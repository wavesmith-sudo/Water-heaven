// CloudClient against the real backend (services/backend), started per test on a free port.
// Set CM5_TEST_PYTHON to a Python that has services/backend/requirements.txt installed.
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include "cloudclient.h"
#include "jellyfinclient.h"

class TestCloudClient : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    QProcess m_api;
    QString m_base;
    QNetworkAccessManager m_nam;

    QJsonObject admin(const QByteArray &verb, const QString &path, const QJsonObject &body = {}, int *status = nullptr)
    {
        QNetworkRequest r(QUrl(m_base + path));
        r.setRawHeader("X-Admin-Key", "test-admin");
        r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = m_nam.sendCustomRequest(r, verb, QJsonDocument(body).toJson());
        QSignalSpy done(reply, &QNetworkReply::finished);
        done.wait(10000);
        if (status)
            *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        return doc.object();
    }

    void pair(CloudClient &c)
    {
        c.setApiUrl(m_base);
        c.startPairing();
        QTRY_VERIFY_WITH_TIMEOUT(!c.userCode().isEmpty(), 10000);
        int status = 0;
        admin("POST", "/api/v1/admin/pairing/" + c.userCode() + "/approve", {{"name", "Test Arcade"}}, &status);
        QCOMPARE(status, 201);
        QTRY_VERIFY_WITH_TIMEOUT(c.paired(), 10000);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("XDG_CONFIG_HOME", m_dir.filePath("config").toUtf8());
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QSettings("RetroArcade", "MediaOS").clear();
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        const quint16 port = probe.serverPort();
        probe.close();
        m_base = QStringLiteral("http://127.0.0.1:%1").arg(port);

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("DATABASE_URL", "sqlite:///" + m_dir.filePath(QStringLiteral("api-%1.db").arg(port)));
        env.insert("ADMIN_API_KEY", "test-admin");
        m_api.setProcessEnvironment(env);
        m_api.setWorkingDirectory(QStringLiteral(BACKEND_DIR));
        m_api.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        const QString python = qEnvironmentVariable("CM5_TEST_PYTHON", "python3");
        m_api.start(python, {"-m", "uvicorn", "app.main:app", "--host", "127.0.0.1", "--port", QString::number(port), "--log-level", "warning"});
        QVERIFY(m_api.waitForStarted(5000));
        // Wait for /health.
        bool up = false;
        for (int i = 0; i < 100 && !up; ++i) {
            QNetworkReply *r = m_nam.get(QNetworkRequest(QUrl(m_base + "/health")));
            QSignalSpy done(r, &QNetworkReply::finished);
            done.wait(1000);
            up = r->error() == QNetworkReply::NoError;
            r->deleteLater();
            if (!up)
                QTest::qWait(100);
        }
        QVERIFY2(up, "backend did not start; is CM5_TEST_PYTHON set to a Python with the backend deps?");
    }

    void cleanup()
    {
        m_api.terminate();
        if (!m_api.waitForFinished(3000))
            m_api.kill();
    }

    void pairsAndBootstraps()
    {
        CloudClient c(QStringLiteral("0.4.0-test"));
        c.setTimingForTests(200, 60000);
        QSignalSpy paired(&c, &CloudClient::pairedSuccessfully);
        pair(c);
        QCOMPARE(paired.count(), 1);
        QCOMPARE(c.state(), QStringLiteral("paired"));
        QCOMPARE(c.deviceName(), QStringLiteral("Test Arcade"));
        QVERIFY(!c.deviceId().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(c.online(), 10000);
        QCOMPARE(c.settings().value("theme").toString(), QStringLiteral("retro-arcade"));
        QVERIFY(c.featureEnabled("movies"));
        QVERIFY(!c.featureEnabled("retro_games")); // disabled by default on the backend
        QVERIFY(c.featureEnabled("not-a-flag"));    // unknown flags never hide UI

        // Heartbeat reached the backend with our firmware version.
        QTRY_VERIFY_WITH_TIMEOUT(
            [&]() {
                QNetworkRequest r(QUrl(m_base + "/api/v1/admin/devices"));
                r.setRawHeader("X-Admin-Key", "test-admin");
                QNetworkReply *reply = m_nam.get(r);
                QSignalSpy done(reply, &QNetworkReply::finished);
                done.wait(5000);
                const QByteArray body = reply->readAll();
                reply->deleteLater();
                return body.contains("0.4.0-test") && body.contains(c.deviceId().toUtf8());
            }(), 10000);
    }

    void persistsAcrossRestartAndAppliesFlagChanges()
    {
        QString id;
        {
            CloudClient c(QStringLiteral("0.4.0-test"));
            c.setTimingForTests(200, 60000);
            pair(c);
            id = c.deviceId();
        }
        int status = 0;
        admin("PATCH", "/api/v1/admin/features/retro_games", {{"enabled", true}}, &status);
        QCOMPARE(status, 200);
        CloudClient again(QStringLiteral("0.4.0-test"));
        QVERIFY(again.paired());
        QCOMPARE(again.deviceId(), id);
        QTRY_VERIFY_WITH_TIMEOUT(again.online() && again.featureEnabled("retro_games"), 10000);
    }

    void revokedDeviceFallsBackToUnpaired()
    {
        CloudClient c(QStringLiteral("0.4.0-test"));
        c.setTimingForTests(200, 60000);
        pair(c);
        int status = 0;
        admin("POST", "/api/v1/admin/devices/" + c.deviceId() + "/revoke", {}, &status);
        QCOMPARE(status, 200);
        c.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!c.paired(), 10000);
        QCOMPARE(c.state(), QStringLiteral("error"));
        QVERIFY(c.error().contains("removed"));
    }

    void cancelStopsPolling()
    {
        CloudClient c(QStringLiteral("0.4.0-test"));
        c.setTimingForTests(200, 60000);
        c.setApiUrl(m_base);
        c.startPairing();
        QTRY_VERIFY_WITH_TIMEOUT(!c.userCode().isEmpty(), 10000);
        const QString code = c.userCode();
        QVERIFY(c.codeSecondsLeft() > 500);
        c.cancelPairing();
        QCOMPARE(c.state(), QStringLiteral("idle"));
        admin("POST", "/api/v1/admin/pairing/" + code + "/approve");
        QTest::qWait(800);
        QVERIFY(!c.paired());
    }

    void unreachableCloudIsAnError()
    {
        CloudClient c(QStringLiteral("0.4.0-test"));
        c.setApiUrl("http://127.0.0.1:9"); // discard port, nothing listens
        c.startPairing();
        QTRY_COMPARE_WITH_TIMEOUT(c.state(), QStringLiteral("error"), 10000);
        QVERIFY(c.error().contains("Could not reach"));
    }

    void rejectsBadUrlsAndChangingUrlUnpairs()
    {
        CloudClient c(QStringLiteral("0.4.0-test"));
        c.setApiUrl("ftp://example.com");
        QVERIFY(c.apiUrl().isEmpty());
        QVERIFY(!c.error().isEmpty());
        c.setTimingForTests(200, 60000);
        pair(c);
        c.setApiUrl("https://other.example.com");
        QVERIFY(!c.paired());
    }

    void jellyfinUsesCloudDeviceIdWhenPaired()
    {
        JellyfinClient j;
        const QString local = j.deviceId();
        QSignalSpy changed(&j, &JellyfinClient::deviceIdChanged);
        j.setDeviceId("cloud-123");
        QCOMPARE(j.deviceId(), QStringLiteral("cloud-123"));
        QVERIFY(j.authorizationHeader().contains("DeviceId=\"cloud-123\""));
        j.setDeviceId({});
        QCOMPARE(j.deviceId(), local);
        QCOMPARE(changed.count(), 2);
    }
};

QTEST_MAIN(TestCloudClient)
#include "tst_cloudclient.moc"
