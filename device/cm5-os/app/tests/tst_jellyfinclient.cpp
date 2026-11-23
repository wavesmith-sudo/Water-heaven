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
#include <QUrlQuery>
#include <QtTest>

#include "jellyfinclient.h"
#include "jellyfinimageprovider.h"
#include "playbackprofile.h"

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
        QVERIFY(auth.startsWith("MediaBrowser Client=\"Wave Culture Media\""));
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

    QVariantMap negotiate(JellyfinClient &c, const QString &itemId, const QString &type, bool allowDirect = true)
    {
        QSignalSpy ready(&c, &JellyfinClient::playbackReady);
        QSignalSpy failed(&c, &JellyfinClient::playbackFailed);
        c.requestPlayback(itemId, type, allowDirect);
        if (!ready.wait(5000)) {
            qWarning() << "no playbackReady; failed:" << failed;
            return {};
        }
        return ready.takeFirst().at(0).toMap();
    }

    // The fake server runs the device profile against invented media metadata, so these rows
    // check that the profile asks for what the CM5 image can decode and nothing more.
    void playbackDecision_data()
    {
        QTest::addColumn<QString>("itemId");
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("method");
        QTest::addColumn<QString>("reason");
        QTest::newRow("h264 1080p mp4") << "m1" << "Movie" << "DirectPlay" << "";
        QTest::newRow("hevc 4K mkv") << "m2" << "Movie" << "Transcode" << "VideoResolutionNotSupported";
        QTest::newRow("av1 webm") << "m3" << "Movie" << "Transcode" << "VideoCodecNotSupported";
        QTest::newRow("mpeg4 avi") << "m4" << "Movie" << "Transcode" << "ContainerNotSupported";
        QTest::newRow("10-bit h264") << "m5" << "Movie" << "Transcode" << "VideoBitDepthNotSupported";
        QTest::newRow("vc1 mkv") << "m6" << "Movie" << "Transcode" << "VideoCodecNotSupported";
        QTest::newRow("hevc 1080p episode") << "s1e1" << "Episode" << "DirectPlay" << "";
        QTest::newRow("mp3") << "a1" << "Audio" << "DirectPlay" << "";
        QTest::newRow("flac") << "a2" << "Audio" << "DirectPlay" << "";
        QTest::newRow("wma") << "a3" << "Audio" << "Transcode" << "ContainerNotSupported";
    }

    void playbackDecision()
    {
        QFETCH(QString, itemId);
        QFETCH(QString, type);
        QFETCH(QString, method);
        QFETCH(QString, reason);
        JellyfinClient c;
        signIn(c);
        const QVariantMap info = negotiate(c, itemId, type);
        QCOMPARE(info.value("method").toString(), method);
        QCOMPARE(info.value("itemId").toString(), itemId);
        QVERIFY(!info.value("playSessionId").toString().isEmpty());
        if (!reason.isEmpty())
            QVERIFY2(info.value("transcodeReasons").toString().contains(reason), qPrintable(info.value("transcodeReasons").toString()));
        const QUrl url(info.value("url").toString());
        const QUrlQuery q(url);
        QCOMPARE(q.queryItemValue("api_key"), QStringLiteral("fake-token-0123456789abcdef"));
        QCOMPARE(q.queryItemValue("DeviceId"), c.deviceId());
        const QString kind = type == "Audio" ? "Audio" : "Videos";
        if (method == "DirectPlay") {
            QCOMPARE(url.path(), "/" + kind + "/" + itemId + "/stream");
            QCOMPARE(q.queryItemValue("static"), QStringLiteral("true"));
            QCOMPARE(q.queryItemValue("MediaSourceId"), itemId);
        } else if (type == "Audio") {
            QCOMPARE(url.path(), "/audio/" + itemId + "/stream.mp3");
        } else {
            QCOMPARE(url.path(), "/videos/" + itemId + "/master.m3u8");
            QCOMPARE(q.queryItemValue("VideoCodec"), QStringLiteral("h264"));
        }
        // The PlaybackInfo call itself authenticates with the header and carries the profile.
        QJsonObject call;
        for (const auto &v : requests())
            if (v.toObject().value("path").toString().endsWith("/PlaybackInfo"))
                call = v.toObject();
        QVERIFY(call.value("authorization").toString().contains("Token="));
        QVERIFY(!call.value("query").toString().contains("api_key"));
        const QJsonObject body = QJsonDocument::fromJson(call.value("body").toString().toUtf8()).object();
        QCOMPARE(body.value("DeviceProfile").toObject().value("Name").toString(), QStringLiteral("Wave Culture Media CM5"));
        QCOMPARE(body.value("EnableDirectPlay").toBool(), true);
    }

    void fallbackForcesTranscode()
    {
        JellyfinClient c;
        signIn(c);
        QCOMPARE(negotiate(c, "m1", "Movie").value("method").toString(), QStringLiteral("DirectPlay"));
        const QVariantMap forced = negotiate(c, "m1", "Movie", false);
        QCOMPARE(forced.value("method").toString(), QStringLiteral("Transcode"));
        QVERIFY(forced.value("url").toString().contains("/videos/m1/master.m3u8"));
    }

    void unknownItemFails()
    {
        JellyfinClient c;
        signIn(c);
        QSignalSpy failed(&c, &JellyfinClient::playbackFailed);
        c.requestPlayback("nope", "Movie");
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("nope"));
    }

    void decideHandlesServerVariants()
    {
        // A server that already puts the key in TranscodingUrl, behind a reverse-proxy base path.
        const QJsonObject info{{"PlaySessionId", "ps1"},
                               {"MediaSources", QJsonArray{QJsonObject{
                                    {"Id", "src1"}, {"SupportsDirectPlay", false}, {"SupportsDirectStream", false},
                                    {"SupportsTranscoding", true},
                                    {"TranscodeReasons", QJsonArray{"VideoCodecNotSupported", "AudioCodecNotSupported"}},
                                    {"TranscodingUrl", "/videos/x/master.m3u8?MediaSourceId=src1&ApiKey=k"}}}}};
        auto d = PlaybackProfile::decide(info, "https://h/jellyfin", "x", "Videos", "k", "dev");
        QCOMPARE(d.method, QStringLiteral("Transcode"));
        QCOMPARE(d.transcodeReasons, QStringLiteral("VideoCodecNotSupported,AudioCodecNotSupported"));
        QVERIFY(d.url.startsWith("https://h/jellyfin/videos/x/master.m3u8?"));
        QVERIFY(!QUrlQuery(QUrl(d.url)).hasQueryItem("api_key"));
        QCOMPARE(QUrlQuery(QUrl(d.url)).queryItemValue("DeviceId"), QStringLiteral("dev"));

        QVERIFY(!PlaybackProfile::decide({{"MediaSources", QJsonArray{}}}, "https://h", "x", "Videos", "k", "d").error.isEmpty());
        QVERIFY(!PlaybackProfile::decide({{"ErrorCode", "NotAllowed"}}, "https://h", "x", "Videos", "k", "d").error.isEmpty());
        // Nothing playable and no transcode offered.
        const QJsonObject none{{"MediaSources", QJsonArray{QJsonObject{{"Id", "s"}, {"SupportsTranscoding", false}}}}};
        QVERIFY(!PlaybackProfile::decide(none, "https://h", "x", "Videos", "k", "d").error.isEmpty());
    }

    void profileMatchesImage()
    {
        // Guards against advertising codecs the image has no plugin for (docs/MEDIA_CENTER.md).
        const QJsonObject p = PlaybackProfile::deviceProfile();
        QString all;
        for (const auto &v : p.value("DirectPlayProfiles").toArray())
            all += v.toObject().value("VideoCodec").toString() + "," + v.toObject().value("Container").toString() + ",";
        QVERIFY(!all.contains("av1"));
        QVERIFY(!all.contains("avi"));
        const QJsonObject hls = p.value("TranscodingProfiles").toArray().first().toObject();
        QCOMPARE(hls.value("Protocol").toString(), QStringLiteral("hls"));
        QCOMPARE(hls.value("Container").toString(), QStringLiteral("ts"));
        QCOMPARE(hls.value("VideoCodec").toString(), QStringLiteral("h264"));
    }

    void sessionReportsCarryPlaySession()
    {
        JellyfinClient c;
        signIn(c);
        const QVariantMap info = negotiate(c, "m2", "Movie");
        QCOMPARE(info.value("method").toString(), QStringLiteral("Transcode"));
        c.reportPlaybackStart("m2", 0);
        c.reportPlaybackProgress("m2", 90500, true);
        c.reportPlaybackStopped("m2", 120000);
        // login, PlaybackInfo, three reports and the DELETE that ends the transcode.
        QTRY_COMPARE_WITH_TIMEOUT(requests().size(), 6, 5000);
        // The reports are sent concurrently, so look them up by path, not order.
        QHash<QString, QJsonObject> byPath;
        for (const auto &v : requests())
            byPath.insert(v.toObject().value("path").toString(), v.toObject());
        QVERIFY(byPath.contains("/Sessions/Playing"));
        QVERIFY(byPath.contains("/Sessions/Playing/Stopped"));
        const QJsonObject progress = QJsonDocument::fromJson(byPath.value("/Sessions/Playing/Progress").value("body").toString().toUtf8()).object();
        QCOMPARE(progress.value("ItemId").toString(), QStringLiteral("m2"));
        QCOMPARE(progress.value("PositionTicks").toInteger(), qint64(905000000));
        QCOMPARE(progress.value("IsPaused").toBool(), true);
        QCOMPARE(progress.value("PlayMethod").toString(), QStringLiteral("Transcode"));
        QCOMPARE(progress.value("PlaySessionId").toString(), info.value("playSessionId").toString());
        QCOMPARE(progress.value("MediaSourceId").toString(), QStringLiteral("m2"));
        const QJsonObject del = byPath.value("/Videos/ActiveEncodings");
        QCOMPARE(del.value("method").toString(), QStringLiteral("DELETE"));
        QVERIFY(del.value("query").toString().contains("PlaySessionId=" + info.value("playSessionId").toString()));

        // After stop, reports for another item fall back to plain direct play fields.
        c.reportPlaybackStart("m1", 0);
        QTRY_COMPARE_WITH_TIMEOUT(requests().size(), 7, 5000);
        const QJsonObject start = QJsonDocument::fromJson(requests().last().toObject().value("body").toString().toUtf8()).object();
        QCOMPARE(start.value("PlayMethod").toString(), QStringLiteral("DirectPlay"));
        QVERIFY(!start.contains("PlaySessionId"));
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
