#pragma once
#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

// Talks to the Retro Arcade cloud API (services/backend): pairing, bootstrap and heartbeat.
//
// Pairing follows the device-authorization pattern: the TV shows a short code, the owner
// approves it, and the device polls until it receives its own token. The token is kept in
// QSettings for now (prototype storage, same caveat as the Jellyfin token).
class CloudClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ apiUrl WRITE setApiUrl NOTIFY configChanged)
    Q_PROPERTY(bool paired READ paired NOTIFY configChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY configChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY configChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)          // idle | pairing | paired | error
    Q_PROPERTY(QString userCode READ userCode NOTIFY stateChanged)
    Q_PROPERTY(int codeSecondsLeft READ codeSecondsLeft NOTIFY tick)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap features READ features NOTIFY bootstrapChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY bootstrapChanged)
    Q_PROPERTY(bool online READ online NOTIFY bootstrapChanged)
public:
    explicit CloudClient(const QString &firmwareVersion, QObject *parent = nullptr);

    QString apiUrl() const { return m_apiUrl; }
    void setApiUrl(const QString &url);
    bool paired() const { return !token().isEmpty() && !m_deviceId.isEmpty(); }
    QString deviceId() const { return m_deviceId; }
    QString deviceName() const { return m_deviceName; }
    QString state() const { return m_state; }
    QString userCode() const { return m_userCode; }
    int codeSecondsLeft() const;
    QString error() const { return m_error; }
    QVariantMap features() const { return m_features; }
    QVariantMap settings() const { return m_settings; }
    bool online() const { return m_online; }

    // True unless the cloud explicitly disabled the feature (offline devices keep working).
    Q_INVOKABLE bool featureEnabled(const QString &name) const;

    Q_INVOKABLE void startPairing();
    Q_INVOKABLE void cancelPairing();
    Q_INVOKABLE void unpair();
    Q_INVOKABLE void refresh(); // bootstrap + heartbeat now

    // Poll/heartbeat intervals, shortened by tests.
    void setTimingForTests(int minPollMs, int heartbeatMs);

signals:
    void configChanged();
    void stateChanged();
    void bootstrapChanged();
    void tick();
    void pairedSuccessfully();

private:
    QString token() const;
    QNetworkRequest request(const QString &path, bool auth) const;
    void setState(const QString &state, const QString &error = {});
    void poll();
    void bootstrap();
    void heartbeat();
    void handleUnauthorized();
    void loadCachedBootstrap();

    QSettings m_settingsStore;
    QNetworkAccessManager m_net;
    QString m_firmware;
    QString m_apiUrl, m_deviceId, m_deviceName, m_token;
    QString m_state = QStringLiteral("idle"), m_error, m_userCode, m_deviceCode;
    qint64 m_codeExpiresAtMs = 0;
    QVariantMap m_features, m_settings;
    bool m_online = false;
    QTimer m_pollTimer, m_heartbeatTimer, m_tickTimer;
    int m_minPollMs = 1000;
    quint64 m_pairingGeneration = 0;
};
