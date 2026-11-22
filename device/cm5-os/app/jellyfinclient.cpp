#include "jellyfinclient.h"

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
    m_deviceId = m_settings.value("deviceId").toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue("deviceId", m_deviceId);
    }
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
    QString token;
    {
        QMutexLocker lock(&m_authLock);
        token = m_token;
    }
    QString device = headerSafe(QSysInfo::machineHostName());
    if (device.isEmpty())
        device = QStringLiteral("CM5");
    QString h = QStringLiteral("MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", Version=\"%4\"")
                    .arg(QLatin1String(ClientName), device, headerSafe(m_deviceId), QLatin1String(ClientVersion));
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

QString JellyfinClient::playbackUrl(const QString &itemId, const QString &type) const
{
    QString token;
    QString base;
    {
        QMutexLocker lock(&m_authLock);
        token = m_token;
        base = m_server;
    }
    if (itemId.isEmpty() || token.isEmpty())
        return {};
    // Direct play only. PlaybackInfo negotiation / transcoding is still TODO (docs/MEDIA_CENTER.md).
    // Qt Multimedia's MediaPlayer cannot send request headers, so this URL is the one
    // place the token travels as a query parameter. Never log it.
    QUrlQuery q;
    q.addQueryItem("static", "true");
    q.addQueryItem("DeviceId", m_deviceId);
    q.addQueryItem("api_key", token);
    const QString kind = type == "Audio" ? QStringLiteral("Audio") : QStringLiteral("Videos");
    return base + "/" + kind + "/" + QUrl::toPercentEncoding(itemId) + "/stream?" + q.toString(QUrl::FullyEncoded);
}

void JellyfinClient::postSession(const QString &path, const QString &itemId, qint64 positionMs, bool paused)
{
    if (itemId.isEmpty() || !signedIn())
        return;
    const QJsonObject body{{"ItemId", itemId},
                           {"PositionTicks", QJsonValue(qMax<qint64>(0, positionMs) * TicksPerMs)},
                           {"IsPaused", paused},
                           {"CanSeek", true},
                           {"PlayMethod", "DirectPlay"}};
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
}
