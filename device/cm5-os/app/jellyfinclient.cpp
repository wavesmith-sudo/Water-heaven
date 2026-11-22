#include "jellyfinclient.h"
#include "playbackprofile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QSysInfo>
#include <QUrlQuery>
#include <QUuid>

namespace {
constexpr qint64 TicksPerMs = 10000; // Jellyfin ticks are 100 ns

// Header values are quoted; strip characters that would break the quoting.
QString headerSafe(QString v)
{
    v.remove(QLatin1Char('"'));
    v.remove(QLatin1Char('\r'));
    v.remove(QLatin1Char('\n'));
    return v;
}

QString episodeLabel(const QJsonObject &o)
{
    const int season = o.value("ParentIndexNumber").toInt(-1);
    const int episode = o.value("IndexNumber").toInt(-1);
    if (season < 0 && episode < 0)
        return {};
    QString label;
    if (season >= 0)
        label += QStringLiteral("S%1").arg(season);
    if (episode >= 0)
        label += (label.isEmpty() ? QString() : QStringLiteral(" · ")) + QStringLiteral("E%1").arg(episode);
    return label;
}
} // namespace

JellyfinClient::JellyfinClient(QObject *parent)
    : QObject(parent), m_settings(QStringLiteral("RetroArcade"), QStringLiteral("MediaOS"))
{
    m_server = m_settings.value("server").toString();
    m_token = m_settings.value("token").toString();
    m_userId = m_settings.value("userId").toString();
    m_userName = m_settings.value("userName").toString();

    // A stable, per-installation device id. Jellyfin uses it to track sessions and
    // revoke devices, so it must not be shared between units (the v0.3 prototype
    // hardcoded "cm5-prototype"). Replace with the cloud-provisioned id once pairing lands.
    m_localDeviceId = m_settings.value("deviceId").toString();
    if (m_localDeviceId.isEmpty()) {
        m_localDeviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue("deviceId", m_localDeviceId);
    }
    m_deviceId = m_localDeviceId;
}

QString JellyfinClient::deviceId() const
{
    QMutexLocker lock(&m_authLock);
    return m_deviceId;
}

void JellyfinClient::setDeviceId(const QString &id)
{
    const QString next = id.isEmpty() ? m_localDeviceId : id;
    {
        QMutexLocker lock(&m_authLock);
        if (next == m_deviceId)
            return;
        m_deviceId = next;
    }
    emit deviceIdChanged();
}

QString JellyfinClient::server() const
{
    QMutexLocker lock(&m_authLock);
    return m_server;
}

bool JellyfinClient::signedIn() const
{
    QMutexLocker lock(&m_authLock);
    return !m_token.isEmpty() && !m_userId.isEmpty();
}

void JellyfinClient::setServer(const QString &value)
{
    const QUrl u = QUrl::fromUserInput(value.trimmed());
    if (!u.isValid() || (u.scheme() != "https" && u.scheme() != "http") || u.host().isEmpty()
        || !u.userInfo().isEmpty()) {
        setError(tr("Enter a valid HTTP(S) server URL"));
        return;
    }
    // Keep a base path so reverse-proxied servers (https://host/jellyfin) work.
    QString normalized = u.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash).toString();
    setError({});
    {
        QMutexLocker lock(&m_authLock);
        if (normalized == m_server)
            return;
        m_server = normalized;
    }
    m_settings.setValue("server", normalized);
    // A token is only valid for the server that issued it.
    setCredentials({}, {}, {});
}

void JellyfinClient::setCredentials(const QString &token, const QString &userId, const QString &userName)
{
    {
        QMutexLocker lock(&m_authLock);
        m_token = token;
    }
    m_userId = userId;
    m_userName = userName;
    m_settings.setValue("token", token);
    m_settings.setValue("userId", userId);
    m_settings.setValue("userName", userName);
    emit connectionChanged();
}

void JellyfinClient::setError(const QString &e)
{
    if (m_error == e)
        return;
    m_error = e;
    emit errorChanged();
}

void JellyfinClient::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

QString JellyfinClient::authorizationHeader() const
{
    QString token, deviceId;
    {
        QMutexLocker lock(&m_authLock);
        token = m_token;
        deviceId = m_deviceId;
    }
    QString device = headerSafe(QSysInfo::machineHostName());
    if (device.isEmpty())
        device = QStringLiteral("CM5");
    QString h = QStringLiteral("MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", Version=\"%4\"")
                    .arg(QLatin1String(ClientName), device, headerSafe(deviceId), QLatin1String(ClientVersion));
    if (!token.isEmpty())
        h += QStringLiteral(", Token=\"%1\"").arg(headerSafe(token));
    return h;
}

JellyfinClient::Auth JellyfinClient::authSnapshot() const
{
    const QString header = authorizationHeader();
    QMutexLocker lock(&m_authLock);
    return {m_server, header};
}

QNetworkRequest JellyfinClient::request(const QString &pathAndQuery) const
{
    QNetworkRequest r(QUrl(server() + pathAndQuery));
    r.setRawHeader("Authorization", authorizationHeader().toUtf8());
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    r.setTransferTimeout(15000);
    return r;
}

bool JellyfinClient::requireSignIn()
{
    if (signedIn())
        return true;
    setError(tr("Sign in to Jellyfin in Settings first"));
    return false;
}

void JellyfinClient::login(const QString &username, const QString &password)
{
    if (server().isEmpty()) {
        setError(tr("Set a server URL first"));
        return;
    }
    if (username.trimmed().isEmpty()) {
        setError(tr("Enter a username"));
        return;
    }
    setCredentials({}, {}, {});
    setBusy(true);
    setError({});
    const QJsonObject credentials{{"Username", username.trimmed()}, {"Pw", password}};
    auto *reply = m_net.post(request("/Users/AuthenticateByName"), QJsonDocument(credentials).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            setError(status == 401 ? tr("Login failed: wrong username or password")
                                   : tr("Login failed: %1").arg(reply->errorString()));
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject user = obj.value("User").toObject();
        const QString token = obj.value("AccessToken").toString();
        const QString userId = user.value("Id").toString();
        if (token.isEmpty() || userId.isEmpty()) {
            setError(tr("Login failed: unexpected server response"));
            return;
        }
        setCredentials(token, userId, user.value("Name").toString());
        emit loggedIn();
    });
}

void JellyfinClient::logout()
{
    if (signedIn()) {
        // Best effort: invalidate the session server-side, then forget it locally.
        auto *reply = m_net.post(request("/Sessions/Logout"), QByteArray());
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    setCredentials({}, {}, {});
    m_items.clear();
    emit itemsChanged();
}

void JellyfinClient::fetchItems(const QString &pathAndQuery, ItemsMode mode)
{
    const quint64 generation = ++m_itemsGeneration;
    m_items.clear();
    emit itemsChanged();
    setBusy(true);
    setError({});
    auto *reply = m_net.get(request(pathAndQuery));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, mode]() {
        reply->deleteLater();
        if (generation != m_itemsGeneration)
            return; // superseded by a newer request
        setBusy(false);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401) {
            setCredentials({}, {}, {});
            setError(tr("Session expired. Sign in again in Settings"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Library request failed: %1").arg(reply->errorString()));
            return;
        }
        const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).object().value("Items").toArray();
        QVariantList items;
        items.reserve(array.size());
        for (const auto &v : array) {
            const QJsonObject o = v.toObject();
            const QString id = o.value("Id").toString();
            if (id.isEmpty())
                continue;
            const QJsonObject userData = o.value("UserData").toObject();
            QVariantMap item;
            item["id"] = id;
            item["name"] = o.value("Name").toString();
            item["type"] = o.value("Type").toString();
            item["overview"] = o.value("Overview").toString();
            item["image"] = imageSource(id);
            QString subtitle = mode == ItemsMode::Episodes ? episodeLabel(o) : o.value("SeriesName").toString();
            if (subtitle.isEmpty())
                subtitle = o.value("AlbumArtist").toString();
            item["subtitle"] = subtitle;
            item["resumeMs"] = userData.value("PlaybackPositionTicks").toVariant().toLongLong() / TicksPerMs;
            item["played"] = userData.value("Played").toBool();
            item["playedPct"] = userData.value("PlayedPercentage").toDouble();
            items.append(item);
        }
        m_items = items;
        emit itemsChanged();
    });
}

void JellyfinClient::browse(const QString &category)
{
    QString types;
    if (category == "Movies")
        types = "Movie";
    else if (category == "TV Shows")
        types = "Series";
    else if (category == "Music")
        types = "Audio";
    else {
        setError(tr("Unsupported library type"));
        return;
    }
    if (!requireSignIn())
        return;
    QUrlQuery q;
    q.addQueryItem("Recursive", "true");
    q.addQueryItem("Limit", "100");
    q.addQueryItem("SortBy", "SortName");
    q.addQueryItem("Fields", "Overview,PrimaryImageAspectRatio");
    q.addQueryItem("IncludeItemTypes", types);
    fetchItems("/Users/" + QUrl::toPercentEncoding(m_userId) + "/Items?" + q.toString(QUrl::FullyEncoded), ItemsMode::Library);
}

void JellyfinClient::continueWatching()
{
    if (!requireSignIn())
        return;
    QUrlQuery q;
    q.addQueryItem("Limit", "30");
    q.addQueryItem("MediaTypes", "Video");
    q.addQueryItem("Fields", "Overview");
    fetchItems("/Users/" + QUrl::toPercentEncoding(m_userId) + "/Items/Resume?" + q.toString(QUrl::FullyEncoded), ItemsMode::Library);
}

void JellyfinClient::browseEpisodes(const QString &seriesId)
{
    if (seriesId.isEmpty() || !requireSignIn())
        return;
    QUrlQuery q;
    q.addQueryItem("UserId", m_userId);
    q.addQueryItem("Fields", "Overview");
    fetchItems("/Shows/" + QUrl::toPercentEncoding(seriesId) + "/Episodes?" + q.toString(QUrl::FullyEncoded), ItemsMode::Episodes);
}

QString JellyfinClient::imageSource(const QString &itemId) const
{
    if (itemId.isEmpty())
        return {};
    return QStringLiteral("image://jellyfin/") + QString::fromLatin1(QUrl::toPercentEncoding(itemId));
}

void JellyfinClient::requestPlayback(const QString &itemId, const QString &type, bool allowDirect)
{
    if (itemId.isEmpty() || !requireSignIn())
        return;
    const quint64 generation = ++m_playGeneration;
    const QString kind = type == "Audio" ? QStringLiteral("Audio") : QStringLiteral("Videos");
    QUrlQuery q;
    q.addQueryItem("UserId", m_userId);
    const QJsonObject body = PlaybackProfile::playbackInfoRequest(m_userId, allowDirect);
    auto *reply = m_net.post(request("/Items/" + QUrl::toPercentEncoding(itemId) + "/PlaybackInfo?" + q.toString(QUrl::FullyEncoded)),
                             QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, itemId, kind]() {
        reply->deleteLater();
        if (generation != m_playGeneration)
            return; // the user started something else
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401) {
            setCredentials({}, {}, {});
            emit playbackFailed(itemId, tr("Session expired. Sign in again in Settings"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit playbackFailed(itemId, tr("Playback request failed: %1").arg(reply->errorString()));
            return;
        }
        QString token, base, deviceId;
        {
            QMutexLocker lock(&m_authLock);
            token = m_token;
            base = m_server;
            deviceId = m_deviceId;
        }
        const auto d = PlaybackProfile::decide(QJsonDocument::fromJson(reply->readAll()).object(), base, itemId, kind,
                                               token, deviceId);
        if (!d.error.isEmpty()) {
            emit playbackFailed(itemId, d.error);
            return;
        }
        // Switching sources (direct play -> transcode fallback) ends the previous server session.
        if (m_playItemId == itemId && m_playMethod == "Transcode" && m_playSessionId != d.playSessionId)
            stopTranscode();
        m_playItemId = itemId;
        m_playMethod = d.method;
        m_playSessionId = d.playSessionId;
        m_mediaSourceId = d.mediaSourceId;
        emit playbackReady({{"itemId", itemId},
                            {"url", d.url},
                            {"method", d.method},
                            {"mediaSourceId", d.mediaSourceId},
                            {"playSessionId", d.playSessionId},
                            {"container", d.container},
                            {"transcodeReasons", d.transcodeReasons}});
    });
}

void JellyfinClient::stopTranscode()
{
    if (m_playSessionId.isEmpty() || !signedIn())
        return;
    // Tells the server to kill the ffmpeg job instead of waiting for it to time out.
    QUrlQuery q;
    q.addQueryItem("DeviceId", deviceId());
    q.addQueryItem("PlaySessionId", m_playSessionId);
    auto *reply = m_net.sendCustomRequest(request("/Videos/ActiveEncodings?" + q.toString(QUrl::FullyEncoded)), "DELETE");
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void JellyfinClient::postSession(const QString &path, const QString &itemId, qint64 positionMs, bool paused)
{
    if (itemId.isEmpty() || !signedIn())
        return;
    QJsonObject body{{"ItemId", itemId},
                     {"PositionTicks", QJsonValue(qMax<qint64>(0, positionMs) * TicksPerMs)},
                     {"IsPaused", paused},
                     {"CanSeek", true},
                     {"PlayMethod", "DirectPlay"}};
    if (itemId == m_playItemId) {
        body["PlayMethod"] = m_playMethod;
        if (!m_playSessionId.isEmpty())
            body["PlaySessionId"] = m_playSessionId;
        if (!m_mediaSourceId.isEmpty())
            body["MediaSourceId"] = m_mediaSourceId;
    }
    auto *reply = m_net.post(request(path), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void JellyfinClient::reportPlaybackStart(const QString &itemId, qint64 positionMs)
{
    postSession("/Sessions/Playing", itemId, positionMs, false);
}

void JellyfinClient::reportPlaybackProgress(const QString &itemId, qint64 positionMs, bool paused)
{
    postSession("/Sessions/Playing/Progress", itemId, positionMs, paused);
}

void JellyfinClient::reportPlaybackStopped(const QString &itemId, qint64 positionMs)
{
    postSession("/Sessions/Playing/Stopped", itemId, positionMs, false);
    if (itemId == m_playItemId) {
        if (m_playMethod == "Transcode")
            stopTranscode();
        ++m_playGeneration; // a PlaybackInfo reply still in flight is no longer wanted
        m_playItemId.clear();
        m_playMethod = QStringLiteral("DirectPlay");
        m_playSessionId.clear();
        m_mediaSourceId.clear();
    }
}
