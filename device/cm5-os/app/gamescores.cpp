#include "gamescores.h"

#include <QRegularExpression>

GameScores::GameScores(QObject *parent) : QObject(parent) {}

bool GameScores::validName(const QString &game)
{
    static const QRegularExpression re(QStringLiteral("^[a-z][a-z0-9_]{0,31}$"));
    return re.match(game).hasMatch();
}

int GameScores::best(const QString &game) const
{
    return validName(game) ? m_settings.value(QStringLiteral("games/") + game, 0).toInt() : 0;
}

bool GameScores::submit(const QString &game, int score)
{
    if (!validName(game) || score <= best(game))
        return false;
    m_settings.setValue(QStringLiteral("games/") + game, score);
    m_settings.sync();
    emit changed();
    return true;
}
