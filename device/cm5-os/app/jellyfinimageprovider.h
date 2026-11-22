#pragma once
#include <QQuickAsyncImageProvider>

class JellyfinClient;

// Serves `image://jellyfin/<itemId>` by fetching the item's primary image with the
// Authorization header, so access tokens never appear in image URLs (v0.3 put them in
// an api_key query parameter, which leaks through logs and proxies).
class JellyfinImageProvider : public QQuickAsyncImageProvider {
public:
    explicit JellyfinImageProvider(const JellyfinClient *client);
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    const JellyfinClient *m_client;
};
