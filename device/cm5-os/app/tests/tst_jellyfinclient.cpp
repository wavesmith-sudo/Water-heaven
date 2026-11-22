// Host unit tests for JellyfinClient and the authenticated image provider.
// Runs against tools/fake_jellyfin.py (started per test case on a free port).
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "jellyfinclient.h"
#include "jellyfinimageprovider.h"

#ifndef FAKE_JELLYFIN
#error "FAKE_JELLYFIN must point at tools/fake_jellyfin.py"
#endif

class TestJellyfinClient : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    QProcess m_server;
    QString m_base;

    QJsonArray requests()
    {
        QNetworkAccessManager nam;
        QNetworkReply *r = nam.get(QNetworkRequest(QUrl(m_base + "/__requests")));
        QSignalSpy done(r, &QNetworkReply::finished);
        done.wait(5000);
        const QJsonArray log = QJsonDocument::fromJson(r->readAll()).array();
        r->deleteLater();
        return log;
    }

    void signIn(JellyfinClient &c)
    {
        c.setServer(m_base);
        QSignalSpy loggedIn(&c, &JellyfinClient::loggedIn);
        c.login("arcade", "insert-coin");
        QVERIFY(loggedIn.wait(5000));
    }

    void waitIdle(JellyfinClient &c)
    {
        QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 5000);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QSettings("RetroArcade", "MediaOS").clear();
        m_server.setProgram(QStringLiteral("python3"));
        m_server.setArguments({QStringLiteral(FAKE_JELLYFIN), "--port", "0"});
        m_server.start();
        QVERIFY(m_server.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(m_server.canReadLine(), 5000);
        const QByteArray line = m_server.readLine().trimmed();
        QVERIFY2(line.startsWith("LISTENING "), line.constData());
        m_base = "http://127.0.0.1:" + QString::fromLatin1(line.mid(10));
    }

    void cleanup()
    {
        m_server.kill();
        m_server.waitForFinished(3000);
    }

    void rejectsBadServerUrls_data()
    {
        QTest::addColumn<QString>("url");
        QTest::newRow("ftp") << "ftp://example.com";
        QTest::newRow("userinfo") << "https://user:pw@example.com";
        QTest::newRow("empty") << "";
    }
    void rejectsBadServerUrls()
    {
        QFETCH(QString, url);
        JellyfinClient c;
        c.setServer(url);
        QVERIFY(!c.error().isEmpty());
        QVERIFY(c.server().isEmpty());
    }

    void keepsReverseProxyBasePath()
    {
        JellyfinClient c;
        c.setServer("https://media.example.com/jellyfin/");
        QCOMPARE(c.server(), QStringLiteral("https://media.example.com/jellyfin"));
    }

    void deviceIdIsUniqueAndStable()
    {
        QString first;
        {
            JellyfinClient c;
            first = c.deviceId();
        }
        JellyfinClient again;
        QCOMPARE(again.deviceId(), first);
        QVERIFY(first != QStringLiteral("cm5-prototype"));
        QVERIFY(!QUuid::fromString(first).isNull());
        QSettings("RetroArcade", "MediaOS").remove("deviceId");
        JellyfinClient fresh;
        QVERIFY(fresh.deviceId() != first);
    }

    void loginSendsDeviceHeaderAndStoresSession()
    {
        JellyfinClient c;
        signIn(c);
        QVERIFY(c.signedIn());
        QCOMPARE(c.userId(), QStringLiteral("u-arcade"));
        QCOMPARE(c.userName(), QStringLiteral("arcade"));
        const QJsonObject login = requests().first().toObject();
        QCOMPARE(login.value("path").toString(), QStringLiteral("/Users/AuthenticateByName"));
        const QString auth = login.value("authorization").toString();
        QVERIFY(auth.startsWith("MediaBrowser Client=\"Retro Arcade\""));
        QVERIFY(auth.contains("DeviceId=\"" + c.deviceId() + "\""));
        QVERIFY(!auth.contains("Token="));
    }

    void wrongPasswordReportsError()
    {
        JellyfinClient c;
        c.setServer(m_base);
        c.login("arcade", "wrong");
        waitIdle(c);
        QVERIFY(!c.signedIn());
        QVERIFY(c.error().contains("wrong username or password"));
    }

    void browseUsesHeaderAuthAndTokenFreeImages()
    {
        JellyfinClient c;
        signIn(c);
        QSignalSpy changed(&c, &JellyfinClient::itemsChanged);
        c.browse("Movies");
        waitIdle(c);
        QCOMPARE(c.items().size(), 7);
        const QVariantMap first = c.items().first().toMap();
        QCOMPARE(first.value("type").toString(), QStringLiteral("Movie"));
        QCOMPARE(first.value("image").toString(), QStringLiteral("image://jellyfin/m1"));
        for (const auto &v : c.items())
            QVERIFY(!v.toMap().value("image").toString().contains("fake-token"));
        const QJsonObject req = requests().last().toObject();
        QCOMPARE(req.value("path").toString(), QStringLiteral("/Users/u-arcade/Items"));
        QVERIFY(req.value("query").toString().contains("IncludeItemTypes=Movie"));
        QVERIFY(!req.value("query").toString().contains("api_key"));
        QVERIFY(req.value("authorization").toString().contains("Token=\"fake-token-0123456789abcdef\""));
    }

    void continueWatchingCarriesProgress()
    {
        JellyfinClient c;
        signIn(c);
        c.continueWatching();
        waitIdle(c);
        QCOMPARE(c.items().size(), 1);
        const QVariantMap item = c.items().first().toMap();
        QCOMPARE(item.value("playedPct").toDouble(), 42.0);
        QVERIFY(item.value("resumeMs").toLongLong() > 0);
    }

    void browsesEpisodesOfASeries()
    {
        JellyfinClient c;
        signIn(c);
        c.browseEpisodes("s1");
        waitIdle(c);
        QCOMPARE(c.items().size(), 4);
        const QVariantMap ep = c.items().at(1).toMap();
        QCOMPARE(ep.value("name").toString(), QStringLiteral("Dead Air"));
        QCOMPARE(ep.value("subtitle").toString(), QStringLiteral("S1 · E2"));
        QCOMPARE(ep.value("type").toString(), QStringLiteral("Episode"));
    }

    void requiresSignInBeforeBrowsing()
    {
        JellyfinClient c;
        c.setServer(m_base);
        c.browse("Movies");
        QVERIFY(c.error().contains("Sign in"));
        QVERIFY(!c.busy());
    }

    void changingServerDropsToken()
    {
        JellyfinClient c;
        signIn(c);
        c.setServer("https://other.example.com");
        QVERIFY(!c.signedIn());
    }

    void playbackUrlsAndSessionReports()
    {
        JellyfinClient c;
        signIn(c);
        QVERIFY(c.playbackUrl("m1", "Movie").contains("/Videos/m1/stream?"));
        QVERIFY(c.playbackUrl("a1", "Audio").contains("/Audio/a1/stream?"));
        QVERIFY(c.playbackUrl("m1", "Movie").contains("DeviceId=" + c.deviceId()));
        c.reportPlaybackStart("m1", 0);
        c.reportPlaybackProgress("m1", 90500, true);
        c.reportPlaybackStopped("m1", 120000);
        QTRY_COMPARE_WITH_TIMEOUT(requests().size(), 4, 5000);
        // The three reports are sent concurrently, so look them up by path, not order.
        QHash<QString, QJsonObject> byPath;
        for (const auto &v : requests())
            byPath.insert(v.toObject().value("path").toString(), v.toObject());
        QVERIFY(byPath.contains("/Sessions/Playing"));
        QVERIFY(byPath.contains("/Sessions/Playing/Stopped"));
        const QJsonObject progress = QJsonDocument::fromJson(byPath.value("/Sessions/Playing/Progress").value("body").toString().toUtf8()).object();
        QCOMPARE(progress.value("ItemId").toString(), QStringLiteral("m1"));
        QCOMPARE(progress.value("PositionTicks").toInteger(), qint64(905000000));
        QCOMPARE(progress.value("IsPaused").toBool(), true);
    }

    void logoutForgetsSession()
    {
        JellyfinClient c;
        signIn(c);
        c.logout();
        QVERIFY(!c.signedIn());
        JellyfinClient reloaded;
        QVERIFY(!reloaded.signedIn());
        QTRY_VERIFY_WITH_TIMEOUT(requests().last().toObject().value("path").toString() == "/Sessions/Logout", 5000);
    }

    void imageProviderSendsAuthHeaderNotQueryToken()
    {
        JellyfinClient c;
        signIn(c);
        JellyfinImageProvider provider(&c);
        QScopedPointer<QQuickImageResponse> response(provider.requestImageResponse("m2", QSize(200, 300)));
        QSignalSpy finished(response.data(), &QQuickImageResponse::finished);
        QVERIFY(finished.wait(5000));
        QVERIFY2(response->errorString().isEmpty(), qPrintable(response->errorString()));
        QScopedPointer<QQuickTextureFactory> tex(response->textureFactory());
        QCOMPARE(tex->image().size(), QSize(80, 120));
        const QJsonObject req = requests().last().toObject();
        QCOMPARE(req.value("path").toString(), QStringLiteral("/Items/m2/Images/Primary"));
        QVERIFY(!req.value("query").toString().contains("api_key"));
        QVERIFY(req.value("query").toString().contains("maxWidth=200"));
        QVERIFY(req.value("authorization").toString().contains("Token=\"fake-token-0123456789abcdef\""));
    }

    void imageProviderFailsCleanlyWhenSignedOut()
    {
        JellyfinClient c;
        c.setServer(m_base);
        JellyfinImageProvider provider(&c);
        QScopedPointer<QQuickImageResponse> response(provider.requestImageResponse("m2", QSize()));
        QSignalSpy finished(response.data(), &QQuickImageResponse::finished);
        QVERIFY(finished.wait(5000));
        QVERIFY(!response->errorString().isEmpty());
    }
};

QTEST_MAIN(TestJellyfinClient)
#include "tst_jellyfinclient.moc"
