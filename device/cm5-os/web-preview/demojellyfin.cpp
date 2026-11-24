#include "demojellyfin.h"

#include <QCryptographicHash>
#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QUrl>

namespace {
// All titles, artists and synopses below are invented for the preview.
struct Entry { const char *id, *name, *type, *subtitle, *overview; int minutes; };

const Entry Movies[] = {
    {"m-neon-drift", "Neon Drift", "Movie", "2084", "A courier with a borrowed hover-bike has one night to cross a city that never turns its lights off.", 104},
    {"m-chrome-horizon", "Chrome Horizon", "Movie", "2081", "Two salvage pilots find a signal at the edge of the map that answers back.", 118},
    {"m-pixel-heart", "Pixel Heart", "Movie", "1989", "An arcade technician falls for the high-score holder she has never met.", 96},
    {"m-tape-delay", "Tape Delay", "Movie", "1986", "A late-night radio host starts receiving calls from tomorrow.", 101},
    {"m-laser-tide", "Laser Tide", "Movie", "2079", "Surfers chase a wave of light that rolls in once a century.", 92},
    {"m-last-cartridge", "The Last Cartridge", "Movie", "1993", "A collector races rivals to find the only copy of a game that was never released.", 110},
    {"m-starlight-motel", "Starlight Motel", "Movie", "1984", "Strangers stranded at a desert motel share one very long night.", 99},
    {"m-vector-run", "Vector Run", "Movie", "2090", "A wireframe athlete competes in a race drawn one line at a time.", 88},
    {"m-sunset-protocol", "Sunset Protocol", "Movie", "2077", "The city's weather computer decides the sun should never set again.", 115},
    {"m-midnight-cabinet", "Midnight Cabinet", "Movie", "1991", "Every night at twelve, the oldest machine in the arcade switches itself on.", 94},
};
const Entry Series[] = {
    {"s-night-shift", "Night Shift Static", "Series", "2 seasons", "The overnight crew of a public-access TV station keeps strange things off the air.", 0},
    {"s-coin-op", "Coin-Op Detectives", "Series", "1 season", "Two teenagers solve neighbourhood mysteries from the back of an arcade.", 0},
    {"s-retrograde", "Retrograde", "Series", "1 season", "A synth band on tour discovers each venue is a year further into the past.", 0},
};
const Entry Music[] = {
    {"a-outrun-dawn", "Outrun the Dawn", "Audio", "The Low Res", "", 4},
    {"a-grid-lights", "Grid Lights", "Audio", "The Low Res", "", 5},
    {"a-palm-static", "Palm Static", "Audio", "Velvet Scanline", "", 3},
    {"a-cassette-sky", "Cassette Sky", "Audio", "Velvet Scanline", "", 4},
    {"a-insert-coin", "Insert Coin (Night Mix)", "Audio", "Arcade Ghosts", "", 6},
    {"a-high-score", "High Score Heart", "Audio", "Arcade Ghosts", "", 4},
};
struct EpisodeSeed { const char *series; int season; const char *titles[4]; };
const EpisodeSeed Episodes[] = {
    {"s-night-shift", 1, {"Test Pattern", "Dead Air", "The Graveyard Slot", "Sign Off"}},
    {"s-night-shift", 2, {"Rerun", "Snow", "Colour Bars", nullptr}},
    {"s-coin-op", 1, {"Tilt", "Free Play", "Continue?", "Game Over"}},
    {"s-retrograde", 1, {"Soundcheck", "B-Side", "Encore", nullptr}},
};

QVariantMap toMap(const Entry &e)
{
    return {{"id", e.id}, {"name", e.name}, {"type", e.type}, {"subtitle", e.subtitle},
            {"overview", e.overview}, {"durationMs", qint64(e.minutes) * 60000}};
}
} // namespace

DemoJellyfinClient::DemoJellyfinClient(const QUrl &demoClip, QObject *parent) : QObject(parent), m_clip(demoClip)
{
    for (const auto &e : Movies) m_catalogue.append(toMap(e));
    for (const auto &e : Series) m_catalogue.append(toMap(e));
    for (const auto &e : Music) m_catalogue.append(toMap(e));
    for (const auto &s : Episodes) {
        for (int i = 0; i < 4 && s.titles[i]; ++i) {
            const QString id = QStringLiteral("%1-s%2e%3").arg(s.series).arg(s.season).arg(i + 1);
            m_catalogue.append(QVariantMap{
                {"id", id}, {"name", s.titles[i]}, {"type", "Episode"}, {"seriesId", s.series},
                {"subtitle", QStringLiteral("S%1 · E%2").arg(s.season).arg(i + 1)},
                {"overview", QStringLiteral("Season %1, episode %2 of a show made up for this preview.").arg(s.season).arg(i + 1)},
                {"durationMs", qint64(24 + (i * 7) % 20) * 60000}});
        }
    }
    // A few half-watched items so "Continue Watching" has something in it.
    m_positions.insert("m-chrome-horizon", 47 * 60000);
    m_positions.insert("m-tape-delay", 12 * 60000);
    m_positions.insert("s-night-shift-s1e3", 9 * 60000);
}

void DemoJellyfinClient::setServer(const QString &url)
{
    if (url.trimmed() == m_server)
        return;
    m_server = url.trimmed();
    setError({});
    emit connectionChanged();
}

void DemoJellyfinClient::login(const QString &username, const QString &password)
{
    Q_UNUSED(password);
    setError({});
    m_busy = true;
    emit busyChanged();
    QTimer::singleShot(450, this, [this, username]() {
        m_busy = false;
        emit busyChanged();
        m_userName = username.trimmed().isEmpty() ? QStringLiteral("player1") : username.trimmed();
        emit connectionChanged();
        emit loggedIn();
    });
}

void DemoJellyfinClient::logout()
{
    m_userName.clear();
    m_items.clear();
    emit connectionChanged();
    emit itemsChanged();
}

void DemoJellyfinClient::setError(const QString &message)
{
    if (message == m_error)
        return;
    m_error = message;
    emit errorChanged();
}

bool DemoJellyfinClient::requireSignIn()
{
    if (signedIn())
        return true;
    m_items.clear();
    emit itemsChanged();
    setError(tr("Not signed in"));
    return false;
}

QVariantMap DemoJellyfinClient::withProgress(QVariantMap item) const
{
    const QString id = item.value("id").toString();
    const qint64 pos = m_positions.value(id, 0);
    const qint64 dur = item.value("durationMs").toLongLong();
    item["image"] = imageSource(id);
    item["resumeMs"] = pos;
    item["playedPct"] = dur > 0 ? 100.0 * pos / dur : 0.0;
    item["played"] = false;
    return item;
}

void DemoJellyfinClient::deliver(const QVariantList &items)
{
    setError({});
    const quint64 generation = ++m_generation;
    m_busy = true;
    emit busyChanged();
    m_items.clear();
    emit itemsChanged();
    QTimer::singleShot(250, this, [this, items, generation]() {
        if (generation != m_generation)
            return; // the user navigated again before this "reply" arrived
        m_busy = false;
        emit busyChanged();
        m_items = items;
        emit itemsChanged();
    });
}

void DemoJellyfinClient::browse(const QString &category)
{
    if (!requireSignIn())
        return;
    const QString type = category == "Movies" ? "Movie" : category == "TV Shows" ? "Series" : category == "Music" ? "Audio" : QString();
    if (type.isEmpty()) {
        setError(tr("Unsupported library type"));
        return;
    }
    QVariantList out;
    for (const auto &v : m_catalogue)
        if (v.toMap().value("type") == type)
            out.append(withProgress(v.toMap()));
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("name").toString() < b.toMap().value("name").toString();
    });
    deliver(out);
}

void DemoJellyfinClient::continueWatching()
{
    if (!requireSignIn())
        return;
    QVariantList out;
    for (const auto &v : m_catalogue) {
        const QVariantMap m = v.toMap();
        const QString type = m.value("type").toString();
        if ((type == "Movie" || type == "Episode") && m_positions.value(m.value("id").toString()) > 0)
            out.append(withProgress(m));
    }
    deliver(out);
}

void DemoJellyfinClient::browseEpisodes(const QString &seriesId)
{
    if (seriesId.isEmpty() || !requireSignIn())
        return;
    QVariantList out;
    for (const auto &v : m_catalogue)
        if (v.toMap().value("seriesId") == seriesId)
            out.append(withProgress(v.toMap()));
    deliver(out);
}

QString DemoJellyfinClient::imageSource(const QString &itemId) const
{
    return itemId.isEmpty() ? QString() : QStringLiteral("image://jellyfin/") + QString::fromLatin1(QUrl::toPercentEncoding(itemId));
}

QString DemoJellyfinClient::playbackUrl(const QString &itemId, const QString &type) const
{
    Q_UNUSED(type);
    if (itemId.isEmpty() || !signedIn())
        return {};
    // Every title plays the same short generated clip that ships next to the page.
    return m_clip.toString();
}

void DemoJellyfinClient::reportPlaybackStart(const QString &, qint64) {}

void DemoJellyfinClient::reportPlaybackProgress(const QString &itemId, qint64 positionMs, bool)
{
    if (positionMs > 0)
        m_positions.insert(itemId, positionMs);
}

void DemoJellyfinClient::reportPlaybackStopped(const QString &itemId, qint64 positionMs)
{
    if (positionMs > 0)
        m_positions.insert(itemId, positionMs);
}

QVariantMap DemoJellyfinClient::itemById(const QString &id) const
{
    for (const auto &v : m_catalogue)
        if (v.toMap().value("id") == id)
            return v.toMap();
    return {};
}

// ---- Posters ------------------------------------------------------------------------------

DemoPosterProvider::DemoPosterProvider(const DemoJellyfinClient *client)
    : QQuickImageProvider(QQuickImageProvider::Image), m_client(client) {}

QImage DemoPosterProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString itemId = QUrl::fromPercentEncoding(id.toUtf8());
    const QVariantMap item = m_client->itemById(itemId);
    const QString type = item.value("type").toString();
    const bool wide = type == "Episode";
    const bool square = type == "Audio";
    const int w = requestedSize.width() > 0 ? qMin(requestedSize.width(), 800) : 400;
    const int h = wide ? w * 9 / 16 : square ? w : w * 3 / 2;

    // Deterministic palette per item.
    const QByteArray hash = QCryptographicHash::hash(itemId.toUtf8(), QCryptographicHash::Md5);
    static const QColor skies[][2] = {
        {"#0d0820", "#ff3fa4"}, {"#120a26", "#e95bff"}, {"#05102a", "#29e8f2"},
        {"#1a0b2e", "#ffd23f"}, {"#0b0420", "#ff6b3f"}, {"#081a2a", "#e95bff"}};
    const auto &sky = skies[uchar(hash[0]) % 6];
    const QColor grid = (uchar(hash[1]) % 2) ? QColor("#29e8f2") : QColor("#e95bff");

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal horizon = h * (wide ? 0.62 : 0.58);

    QLinearGradient g(0, 0, 0, horizon);
    g.setColorAt(0, sky[0]);
    g.setColorAt(1, sky[1]);
    p.fillRect(QRectF(0, 0, w, horizon), g);
    p.fillRect(QRectF(0, horizon, w, h - horizon), QColor("#0d0820"));

    // Striped sun.
    const qreal r = qMin(w, h) * (0.22 + (uchar(hash[2]) % 8) / 100.0);
    const QPointF c(w * (0.35 + (uchar(hash[3]) % 30) / 100.0), horizon - r * 0.35);
    QLinearGradient sun(0, c.y() - r, 0, c.y() + r);
    sun.setColorAt(0, QColor("#ffd23f"));
    sun.setColorAt(1, QColor("#ff3fa4"));
    QPainterPath disc;
    disc.addEllipse(c, r, r);
    QPainterPath clip;
    clip.addRect(QRectF(0, 0, w, horizon));
    for (int i = 0; i < 5; ++i) {
        const qreal y = c.y() + r * (0.05 + i * 0.2);
        QPainterPath band;
        band.addRect(QRectF(0, y, w, r * 0.05 * (i + 1)));
        clip = clip.subtracted(band);
    }
    p.fillPath(disc.intersected(clip), sun);

    // Perspective grid.
    p.setPen(QPen(grid, qMax(1.0, w / 300.0)));
    for (int i = -10; i <= 10; ++i)
        p.drawLine(QPointF(w / 2.0 + i * w * 0.06, horizon), QPointF(w / 2.0 + i * w * 0.45, h));
    for (int i = 1; i <= 8; ++i) {
        const qreal t = i / 8.0;
        const qreal y = horizon + (h - horizon) * t * t;
        p.drawLine(QPointF(0, y), QPointF(w, y));
    }

    // Title.
    QFont f;
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, w / 200.0);
    const QRectF titleRect(w * 0.06, h * 0.06, w * 0.88, horizon * 0.45);
    const QString title = item.value("name", itemId).toString().toUpper();
    // Largest size up to w/9 at which the title fits above the sun.
    for (int px = qMax(12, w / 9); px >= 10; px -= 2) {
        f.setPixelSize(px);
        const QRectF need = QFontMetricsF(f).boundingRect(titleRect, Qt::AlignHCenter | Qt::TextWordWrap, title);
        if (need.width() <= titleRect.width() && need.height() <= titleRect.height())
            break;
    }
    p.setFont(f);
    p.setPen(QColor(0, 0, 0, 140));
    p.drawText(titleRect.translated(2, 2), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, title);
    p.setPen(QColor("#ffffff"));
    p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, title);
    p.end();

    if (size)
        *size = img.size();
    return img;
}
