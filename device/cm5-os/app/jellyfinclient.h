#pragma once
#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

// Jellyfin client for the Retro Arcade home UI.
//
// Security notes (see docs/MEDIA_CENTER.md):
//  * Requests authenticate with the `Authorization: MediaBrowser ... Token="..."` header,
//    never with an api_key query parameter, except the media stream URL handed to
//    Qt Multimedia, which cannot set request headers in Qt 6.4-6.7.
//  * Artwork is served through the `image://jellyfin/<itemId>` provider, which fetches
//    with the auth header, so tokens never appear in QML image URLs or logs.
//  * The token is still persisted with QSettings. That is prototype storage and must be
//    replaced by the device credential service before production.
class JellyfinClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString server READ server WRITE setServer NOTIFY connectionChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY connectionChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY connectionChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY connectionChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    static constexpr auto ClientName = "Retro Arcade";
    static constexpr auto ClientVersion = "0.4.0";

    explicit JellyfinClient(QObject *parent = nullptr);

    QString server() const;
    bool signedIn() const;
    QString userName() const { return m_userName; }
    QString userId() const { return m_userId; }
    QString deviceId() const;
    // Use the cloud-issued id once the device is paired; empty restores the local random id.
    void setDeviceId(const QString &id);
    QVariantList items() const { return m_items; }
    QString error() const { return m_error; }
    bool busy() const { return m_busy; }

    void setServer(const QString &url);

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void browse(const QString &category);
    Q_INVOKABLE void continueWatching();
    Q_INVOKABLE void browseEpisodes(const QString &seriesId);
    Q_INVOKABLE QString imageSource(const QString &itemId) const;
    // Asks the server how this device should play the item (POST /Items/{id}/PlaybackInfo with
    // the CM5 device profile). Emits playbackReady or playbackFailed. allowDirect=false forces a
    // server transcode; the player uses it as the fallback when direct play fails on the device.
    Q_INVOKABLE void requestPlayback(const QString &itemId, const QString &type, bool allowDirect = true);

    // Playback state reporting (Jellyfin /Sessions/Playing*). positionMs is the player position.
    Q_INVOKABLE void reportPlaybackStart(const QString &itemId, qint64 positionMs);
    Q_INVOKABLE void reportPlaybackProgress(const QString &itemId, qint64 positionMs, bool paused);
    Q_INVOKABLE void reportPlaybackStopped(const QString &itemId, qint64 positionMs);

    // Thread-safe snapshot used by the image provider, which runs on the image loader thread.
    struct Auth { QString server; QString header; };
    Auth authSnapshot() const;

    // Builds the MediaBrowser authorization header value. Exposed for tests.
    QString authorizationHeader() const;

signals:
    void connectionChanged();
    void itemsChanged();
    void errorChanged();
    void busyChanged();
    void loggedIn();
    void deviceIdChanged();
    // info: itemId, url, method (DirectPlay|DirectStream|Transcode), mediaSourceId, playSessionId,
    // container, transcodeReasons. The url carries the token: never log it.
    void playbackReady(const QVariantMap &info);
    void playbackFailed(const QString &itemId, const QString &message);

private:
    enum class ItemsMode { Library, Episodes };
    QNetworkRequest request(const QString &pathAndQuery) const;
    void setError(const QString &message);
    void setBusy(bool b);
    void setCredentials(const QString &token, const QString &userId, const QString &userName);
    void fetchItems(const QString &pathAndQuery, ItemsMode mode);
    void postSession(const QString &path, const QString &itemId, qint64 positionMs, bool paused);
    bool requireSignIn();
    void stopTranscode();

    QSettings m_settings;
    QNetworkAccessManager m_net;
    mutable QMutex m_authLock; // guards m_server, m_token and m_deviceId
    QString m_server, m_token, m_deviceId;
    QString m_localDeviceId;
    QString m_userId, m_userName, m_error;
    QVariantList m_items;
    bool m_busy = false;
    quint64 m_itemsGeneration = 0; // drops stale replies when the user navigates quickly
    quint64 m_playGeneration = 0;
    // The session being played, reported with every /Sessions/Playing* call.
    QString m_playItemId, m_playMethod = QStringLiteral("DirectPlay"), m_playSessionId, m_mediaSourceId;
};
