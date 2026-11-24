#pragma once
#include <QObject>
#include <QQuickImageProvider>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

// Stand-in for JellyfinClient used by the browser preview. It exposes the same QML-facing
// API (properties, invokables, signals) as device/cm5-os/app/jellyfinclient.h, so the real
// Main.qml and its components run unchanged, but every call is answered from a built-in
// catalogue of made-up titles. Nothing is sent over the network and no credentials exist.
//
// Keep this in step with JellyfinClient's Q_PROPERTY / Q_INVOKABLE list. check_api.py
// fails the preview build if they drift.
class DemoJellyfinClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString server READ server WRITE setServer NOTIFY connectionChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY connectionChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY connectionChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY connectionChanged)
    Q_PROPERTY(QString deviceId READ deviceId CONSTANT)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit DemoJellyfinClient(const QUrl &demoClip, QObject *parent = nullptr);

    QString server() const { return m_server; }
    bool signedIn() const { return !m_userName.isEmpty(); }
    QString userName() const { return m_userName; }
    QString userId() const { return signedIn() ? QStringLiteral("demo-user") : QString(); }
    QString deviceId() const { return QStringLiteral("browser-preview"); }
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
    Q_INVOKABLE QString playbackUrl(const QString &itemId, const QString &type) const;
    Q_INVOKABLE void reportPlaybackStart(const QString &itemId, qint64 positionMs);
    Q_INVOKABLE void reportPlaybackProgress(const QString &itemId, qint64 positionMs, bool paused);
    Q_INVOKABLE void reportPlaybackStopped(const QString &itemId, qint64 positionMs);

    // Used by the poster provider.
    QVariantMap itemById(const QString &id) const;

signals:
    void connectionChanged();
    void itemsChanged();
    void errorChanged();
    void busyChanged();
    void loggedIn();

private:
    void setError(const QString &message);
    // Shows the "LOADING…" state briefly, like a real server round trip, then publishes items.
    void deliver(const QVariantList &items);
    bool requireSignIn();
    QVariantMap withProgress(QVariantMap item) const;

    QUrl m_clip;
    QString m_server = QStringLiteral("https://demo.invalid");
    QString m_userName = QStringLiteral("player1");
    QString m_error;
    QVariantList m_items;
    bool m_busy = false;
    quint64 m_generation = 0;
    QVariantList m_catalogue;                     // every item, flat
    QHash<QString, qint64> m_positions;           // itemId -> resume position (ms)
};

// Serves image://jellyfin/<itemId> with generated synthwave posters, so the preview needs no
// image files and no server.
class DemoPosterProvider : public QQuickImageProvider {
public:
    explicit DemoPosterProvider(const DemoJellyfinClient *client);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    const DemoJellyfinClient *m_client;
};
