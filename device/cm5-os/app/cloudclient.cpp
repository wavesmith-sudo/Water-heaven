#include "cloudclient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSysInfo>
#include <QUrl>

namespace {
QJsonObject parse(QNetworkReply *reply) { return QJsonDocument::fromJson(reply->readAll()).object(); }
int status(QNetworkReply *reply) { return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); }
} // namespace

CloudClient::CloudClient(const QString &firmwareVersion, QObject *parent)
    : QObject(parent), m_settingsStore(QStringLiteral("RetroArcade"), QStringLiteral("MediaOS")), m_firmware(firmwareVersion)
{
    m_apiUrl = m_settingsStore.value("cloud/apiUrl").toString();
    m_deviceId = m_settingsStore.value("cloud/deviceId").toString();
    m_deviceName = m_settingsStore.value("cloud/deviceName").toString();
    m_token = m_settingsStore.value("cloud/token").toString();
    loadCachedBootstrap();
    if (paired())
        m_state = QStringLiteral("paired");

    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, &CloudClient::poll);
    m_heartbeatTimer.setInterval(5 * 60 * 1000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &CloudClient::refresh);
    m_tickTimer.setInterval(1000);
    connect(&m_tickTimer, &QTimer::timeout, this, [this]() {
        emit tick();
        if (m_state == "pairing" && codeSecondsLeft() == 0)
            setState("error", tr("The code expired. Choose Pair to get a new one."));
    });
    if (paired()) {
        m_heartbeatTimer.start();
        QTimer::singleShot(0, this, &CloudClient::refresh);
    }
}

void CloudClient::setTimingForTests(int minPollMs, int heartbeatMs)
{
    m_minPollMs = minPollMs;
    m_heartbeatTimer.setInterval(heartbeatMs);
}

QString CloudClient::token() const { return m_token; }

void CloudClient::setApiUrl(const QString &value)
{
    const QUrl u = QUrl::fromUserInput(value.trimmed());
    if (!u.isValid() || (u.scheme() != "https" && u.scheme() != "http") || u.host().isEmpty() || !u.userInfo().isEmpty()) {
        setState(m_state == "pairing" ? "idle" : m_state, tr("Enter a valid HTTP(S) cloud URL"));
        return;
    }
    const QString normalized = u.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash).toString();
    if (normalized == m_apiUrl)
        return;
    // A device token belongs to the service that issued it.
    unpair();
    m_apiUrl = normalized;
    m_settingsStore.setValue("cloud/apiUrl", m_apiUrl);
    emit configChanged();
}

int CloudClient::codeSecondsLeft() const
{
    if (m_state != "pairing")
        return 0;
    return int(qMax<qint64>(0, (m_codeExpiresAtMs - QDateTime::currentMSecsSinceEpoch() + 999) / 1000));
}

bool CloudClient::featureEnabled(const QString &name) const
{
    const QVariant v = m_features.value(name);
    return !v.isValid() || v.toBool();
}

QNetworkRequest CloudClient::request(const QString &path, bool auth) const
{
    QNetworkRequest r(QUrl(m_apiUrl + path));
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    r.setTransferTimeout(15000);
    if (auth)
        r.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    return r;
}

void CloudClient::setState(const QString &state, const QString &error)
{
    m_state = state;
    m_error = error;
    if (state != "pairing")
        m_tickTimer.stop();
    emit stateChanged();
}

void CloudClient::startPairing()
{
    if (m_apiUrl.isEmpty()) {
        setState("error", tr("Set the cloud URL first"));
        return;
    }
    const quint64 generation = ++m_pairingGeneration;
    m_pollTimer.stop();
    setState("pairing");
    m_userCode.clear();
    QString name = QSysInfo::machineHostName();
    if (name.isEmpty())
        name = QStringLiteral("Retro Arcade CM5");
    const QJsonObject body{{"device_name", name.left(120)}, {"firmware", m_firmware.left(60)}};
    auto *reply = m_net.post(request("/api/v1/pairing/start", false), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        if (generation != m_pairingGeneration)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            setState("error", status(reply) == 429 ? tr("Too many attempts. Wait a minute and try again.")
                                                   : tr("Could not reach the cloud: %1").arg(reply->errorString()));
            return;
        }
        const QJsonObject o = parse(reply);
        m_userCode = o.value("user_code").toString();
        m_deviceCode = o.value("device_code").toString();
        m_codeExpiresAtMs = QDateTime::currentMSecsSinceEpoch() + o.value("expires_in").toInt(600) * 1000LL;
        m_pollTimer.setInterval(qMax(m_minPollMs, o.value("interval").toInt(5) * 1000));
        if (m_userCode.isEmpty() || m_deviceCode.isEmpty()) {
            setState("error", tr("Unexpected response from the cloud"));
            return;
        }
        m_tickTimer.start();
        setState("pairing");
        m_pollTimer.start();
    });
}

void CloudClient::cancelPairing()
{
    ++m_pairingGeneration;
    m_pollTimer.stop();
    m_deviceCode.clear();
    m_userCode.clear();
    setState(paired() ? "paired" : "idle");
}

void CloudClient::poll()
{
    if (m_state != "pairing" || m_deviceCode.isEmpty())
        return;
    const quint64 generation = m_pairingGeneration;
    const QJsonObject body{{"device_code", m_deviceCode}};
    auto *reply = m_net.post(request("/api/v1/pairing/poll", false), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        if (generation != m_pairingGeneration || m_state != "pairing")
            return;
        const int code = status(reply);
        if (code == 202 || (reply->error() != QNetworkReply::NoError && code == 0)) {
            m_pollTimer.start(); // pending, or a transient network error: keep polling until expiry
            return;
        }
        if (code == 410) {
            setState("error", tr("The code expired. Choose Pair to get a new one."));
            return;
        }
        if (code != 200) {
            setState("error", tr("Pairing failed (%1)").arg(code));
            return;
        }
        const QJsonObject o = parse(reply);
        m_token = o.value("device_token").toString();
        m_deviceId = o.value("device_id").toString();
        m_deviceName = o.value("device_name").toString();
        m_deviceCode.clear();
        m_userCode.clear();
        m_settingsStore.setValue("cloud/token", m_token);
        m_settingsStore.setValue("cloud/deviceId", m_deviceId);
        m_settingsStore.setValue("cloud/deviceName", m_deviceName);
        setState("paired");
        emit configChanged();
        emit pairedSuccessfully();
        m_heartbeatTimer.start();
        refresh();
    });
}

void CloudClient::unpair()
{
    cancelPairing();
    m_token.clear();
    m_deviceId.clear();
    m_deviceName.clear();
    m_features.clear();
    m_settings.clear();
    m_online = false;
    m_heartbeatTimer.stop();
    for (const char *k : {"cloud/token", "cloud/deviceId", "cloud/deviceName", "cloud/features", "cloud/settings"})
        m_settingsStore.remove(k);
    setState("idle");
    emit configChanged();
    emit bootstrapChanged();
}

void CloudClient::refresh()
{
    if (!paired())
        return;
    bootstrap();
    heartbeat();
}

void CloudClient::handleUnauthorized()
{
    // Revoked or rotated elsewhere: forget the credential so the UI offers pairing again.
    unpair();
    setState("error", tr("This device was removed from the cloud. Pair it again."));
}

void CloudClient::bootstrap()
{
    auto *reply = m_net.get(request("/api/v1/device/bootstrap", true));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (status(reply) == 401) {
            handleUnauthorized();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            m_online = false; // keep cached settings; the device stays usable offline
            emit bootstrapChanged();
            return;
        }
        const QJsonObject o = parse(reply);
        m_features = o.value("features").toObject().toVariantMap();
        m_settings = o.value("settings").toObject().toVariantMap();
        m_online = true;
        const QString name = o.value("device_name").toString();
        if (!name.isEmpty() && name != m_deviceName) {
            m_deviceName = name;
            m_settingsStore.setValue("cloud/deviceName", name);
            emit configChanged();
        }
        m_settingsStore.setValue("cloud/features", QJsonDocument(QJsonObject::fromVariantMap(m_features)).toJson(QJsonDocument::Compact));
        m_settingsStore.setValue("cloud/settings", QJsonDocument(QJsonObject::fromVariantMap(m_settings)).toJson(QJsonDocument::Compact));
        emit bootstrapChanged();
    });
}

void CloudClient::heartbeat()
{
    const QJsonObject body{{"firmware", m_firmware.left(60)}};
    auto *reply = m_net.post(request("/api/v1/device/heartbeat", true), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void CloudClient::loadCachedBootstrap()
{
    m_features = QJsonDocument::fromJson(m_settingsStore.value("cloud/features").toByteArray()).object().toVariantMap();
    m_settings = QJsonDocument::fromJson(m_settingsStore.value("cloud/settings").toByteArray()).object().toVariantMap();
}
