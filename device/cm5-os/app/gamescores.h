#pragma once
#include <QObject>
#include <QSettings>
#include <QString>

// Best score per built-in game, kept on the device (QSettings group "games").
// Scores are local only; nothing is sent to the cloud.
class GameScores : public QObject {
    Q_OBJECT
public:
    explicit GameScores(QObject *parent = nullptr);
    Q_INVOKABLE int best(const QString &game) const;
    // Records a finished run. Returns true when it beats the stored best.
    Q_INVOKABLE bool submit(const QString &game, int score);
signals:
    void changed();

private:
    static bool validName(const QString &game);
    QSettings m_settings;
};
