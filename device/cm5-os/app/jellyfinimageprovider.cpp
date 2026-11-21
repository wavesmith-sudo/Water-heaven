#include "jellyfinimageprovider.h"
#include "jellyfinclient.h"

#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QThreadStorage>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr int MaxImageBytes = 8 * 1024 * 1024;

// The provider is called on Qt Quick's image loader thread, which runs an event loop.
// Each such thread gets its own network manager; QNetworkAccessManager is not shareable
// across threads.
QNetworkAccessManager *threadNetworkManager()
{
    static QThreadStorage<QNetworkAccessManager *> storage;
    if (!storage.hasLocalData())
        storage.setLocalData(new QNetworkAccessManager);
    return storage.localData();
}

class JellyfinImageResponse : public QQuickImageResponse {
public:
    JellyfinImageResponse(const JellyfinClient::Auth &auth, const QString &itemId, const QSize &requestedSize)
    {
        if (auth.server.isEmpty() || itemId.isEmpty()) {
            fail(QStringLiteral("Jellyfin server not configured"));
            return;
        }
        const int width = requestedSize.width() > 0 ? qMin(requestedSize.width(), 1920) : 400;
        QUrlQuery q;
        q.addQueryItem("maxWidth", QString::number(width));
        q.addQueryItem("quality", "90");
        QUrl url(auth.server + "/Items/" + QUrl::toPercentEncoding(itemId) + "/Images/Primary");
        url.setQuery(q);
        QNetworkRequest req(url);
        req.setRawHeader("Authorization", auth.header.toUtf8());
        req.setTransferTimeout(15000);
        m_reply = threadNetworkManager()->get(req);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this]() { onFinished(); });
    }

    ~JellyfinImageResponse() override
    {
        if (m_reply) {
            m_reply->disconnect(this);
            m_reply->abort();
            m_reply->deleteLater();
        }
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }
    QString errorString() const override { return m_error; }
    void cancel() override
    {
        if (m_reply)
            m_reply->abort();
    }

private:
    void fail(const QString &message)
    {
        m_error = message;
        // Must be emitted after the response is returned to the engine.
        QMetaObject::invokeMethod(this, &QQuickImageResponse::finished, Qt::QueuedConnection);
    }

    void onFinished()
    {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_error = reply->errorString();
        } else {
            const QByteArray data = reply->read(MaxImageBytes + 1);
            if (data.size() > MaxImageBytes || !m_image.loadFromData(data))
                m_error = QStringLiteral("Invalid image data");
        }
        emit finished();
    }

    QPointer<QNetworkReply> m_reply;
    QImage m_image;
    QString m_error;
};
} // namespace

JellyfinImageProvider::JellyfinImageProvider(const JellyfinClient *client) : m_client(client) {}

QQuickImageResponse *JellyfinImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new JellyfinImageResponse(m_client->authSnapshot(), QUrl::fromPercentEncoding(id.toUtf8()), requestedSize);
}
