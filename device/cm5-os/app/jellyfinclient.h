#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QVariantList>
#include <QUrl>

class JellyfinClient : public QObject {
 Q_OBJECT
 Q_PROPERTY(QString server READ server WRITE setServer NOTIFY connectionChanged)
 Q_PROPERTY(QString token READ token WRITE setToken NOTIFY connectionChanged)
 Q_PROPERTY(QString userId READ userId NOTIFY connectionChanged)
 Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
 Q_PROPERTY(QString error READ error NOTIFY errorChanged)
 Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
 explicit JellyfinClient(QObject *parent=nullptr);
 QString server() const {return m_server;}
 QString token() const {return m_token;}
 QString userId() const {return m_userId;}
 QVariantList items() const {return m_items;}
 QString error() const {return m_error;}
 bool busy() const {return m_busy;}
 void setServer(const QString &s);
 void setToken(const QString &t);
 Q_INVOKABLE void login(const QString &username,const QString &password);
 Q_INVOKABLE void browse(const QString &type);
 Q_INVOKABLE void continueWatching();
 Q_INVOKABLE QString imageUrl(const QString &itemId) const;
 Q_INVOKABLE QString playbackUrl(const QString &itemId) const;
signals:
 void connectionChanged(); void itemsChanged(); void errorChanged(); void busyChanged();
 void loggedIn();
private:
 QNetworkRequest request(const QString &path) const;
 void setError(const QString &message);
 void setBusy(bool b);
 void fetch(const QString &path);
 QNetworkAccessManager m_net;
 QString m_server,m_token,m_userId,m_error;
 QVariantList m_items;
 bool m_busy=false;
};
