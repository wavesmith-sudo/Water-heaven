// Plays real streams through PlayerView.qml with the host's GStreamer, against
// tools/fake_jellyfin.py --media. Covers direct play, a server HLS transcode, and the
// fallback from a direct play that fails on the device to a transcode.
//
// This proves the negotiation and fallback logic with a desktop GStreamer. It is not a CM5
// test: the image's plugin set, decode speed and HDMI output are still untested.
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "jellyfinclient.h"

#ifndef FAKE_JELLYFIN
#error "FAKE_JELLYFIN must point at tools/fake_jellyfin.py"
#endif

class TestPlayerView : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    QProcess m_server;
    QString m_base;

    static bool hasElements(const QStringList &elements)
    {
        const QString inspect = QStandardPaths::findExecutable("gst-inspect-1.0");
        if (inspect.isEmpty())
            return false;
        for (const QString &e : elements) {
            if (QProcess::execute(inspect, {"--exists", e}) != 0)
                return false;
        }
        return true;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("XDG_CONFIG_HOME", m_dir.filePath("config").toUtf8());
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty())
            QSKIP("ffmpeg is needed to generate test media");
        if (!hasElements({"playbin", "qtdemux", "hlsdemux", "tsdemux", "avdec_h264", "avdec_aac"}))
            QSKIP("needs GStreamer plugins base, good, bad and libav (hlsdemux, tsdemux, avdec_h264)");
        const QString media = m_dir.filePath("media");
        QCOMPARE(QProcess::execute("bash", {QStringLiteral(MAKE_TEST_MEDIA), media}), 0);

        QSettings("RetroArcade", "MediaOS").clear();
        m_server.setProgram(QStringLiteral("python3"));
        m_server.setArguments({QStringLiteral(FAKE_JELLYFIN), "--port", "0", "--media", media});
        m_server.start();
        QVERIFY(m_server.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(m_server.canReadLine(), 5000);
        const QByteArray line = m_server.readLine().trimmed();
        QVERIFY2(line.startsWith("LISTENING "), line.constData());
        m_base = "http://127.0.0.1:" + QString::fromLatin1(line.mid(10));
    }

    void cleanupTestCase()
    {
        m_server.kill();
        m_server.waitForFinished(3000);
    }

    void plays_data()
    {
        QTest::addColumn<QString>("itemId");
        QTest::addColumn<QString>("method");
        QTest::addColumn<bool>("fellBack");
        QTest::newRow("direct play") << "m1" << "DirectPlay" << false;
        QTest::newRow("hls transcode") << "m2" << "Transcode" << false;
        QTest::newRow("direct play fails, falls back") << "m7" << "Transcode" << true;
    }

    void plays()
    {
        QFETCH(QString, itemId);
        QFETCH(QString, method);
        QFETCH(bool, fellBack);

        // Everything here is deliberately leaked. Destroying a playing Qt 6.4 GStreamer pipeline
        // with a VideoOutput corrupts the heap on the Ubuntu host (seen with the stock qml tool
        // too), which would turn a passing run into a crash. The device runs Qt 6.9.
        auto *jellyfin = new JellyfinClient;
        jellyfin->setServer(m_base);
        QSignalSpy loggedIn(jellyfin, &JellyfinClient::loggedIn);
        jellyfin->login("arcade", "insert-coin");
        QVERIFY(loggedIn.wait(5000));

        // No window: frames reach the VideoOutput's sink but are not rendered, which keeps
        // the test independent of the host's GPU and scene graph backend.
        auto *engine = new QQmlEngine;
        engine->rootContext()->setContextProperty("jellyfin", jellyfin);
        auto *component = new QQmlComponent(engine, QUrl::fromLocalFile(QStringLiteral(QML_DIR "/PlayerView.qml")));
        QVariantMap item{{"id", itemId}, {"name", "Test"}, {"type", "Movie"}, {"resumeMs", 0}};
        QObject *obj = component->createWithInitialProperties({{"item", item}});
        QVERIFY2(obj, qPrintable(component->errorString()));

        // Playback has really started once the position moves past two seconds.
        QTRY_VERIFY_WITH_TIMEOUT(obj->property("lastPosition").toInt() > 2000, 20000);
        QCOMPARE(obj->property("method").toString(), method);
        QCOMPARE(obj->property("fellBack").toBool(), fellBack);
        QMetaObject::invokeMethod(obj, "stop");
    }
};

QTEST_MAIN(TestPlayerView)
#include "tst_playerview.moc"
