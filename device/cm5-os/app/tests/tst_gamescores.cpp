#include <QStandardPaths>
#include <QtTest>

#include "gamescores.h"

class TestGameScores : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("cm5-home-test"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_gamescores"));
        QSettings().clear();
    }

    void keepsOnlyTheBest()
    {
        GameScores s;
        QSignalSpy changed(&s, &GameScores::changed);
        QCOMPARE(s.best(QStringLiteral("runner")), 0);
        QVERIFY(s.submit(QStringLiteral("runner"), 120));
        QVERIFY(!s.submit(QStringLiteral("runner"), 80));
        QVERIFY(!s.submit(QStringLiteral("runner"), 120));
        QCOMPARE(s.best(QStringLiteral("runner")), 120);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(GameScores().best(QStringLiteral("runner")), 120); // persisted
        QCOMPARE(s.best(QStringLiteral("orbit")), 0);               // per game
    }

    void rejectsOddKeys()
    {
        GameScores s;
        QVERIFY(!s.submit(QStringLiteral("../token"), 5));
        QVERIFY(!s.submit(QString(), 5));
        QCOMPARE(s.best(QStringLiteral("../token")), 0);
    }
};

QTEST_GUILESS_MAIN(TestGameScores)
#include "tst_gamescores.moc"
