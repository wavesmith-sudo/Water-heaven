#include "jellyfinclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QSettings>

JellyfinClient::JellyfinClient(QObject *parent):QObject(parent) {
 QSettings s("RetroArcade","MediaOS");
 m_server=s.value("server").toString(); m_token=s.value("token").toString(); m_userId=s.value("userId").toString();
}
void JellyfinClient::setServer(const QString &v) {
 QUrl u(v.trimmed());
 if (!u.isValid() || (u.scheme()!="https" && u.scheme()!="http") || u.host().isEmpty()) {setError("Enter a valid HTTP(S) server URL");return;}
 m_server=u.toString(QUrl::RemovePath|QUrl::RemoveQuery|QUrl::RemoveFragment);
 // QUrl::RemovePath leaves the trailing slash; remove only if present.
 if (m_server.endsWith('/')) m_server.chop(1);
 QSettings("RetroArcade","MediaOS").setValue("server",m_server); emit connectionChanged();
}
void JellyfinClient::setToken(const QString &v) {m_token=v;QSettings("RetroArcade","MediaOS").setValue("token",v);emit connectionChanged();}
void JellyfinClient::setError(const QString &e){m_error=e;emit errorChanged();}
void JellyfinClient::setBusy(bool b){m_busy=b;emit busyChanged();}
QNetworkRequest JellyfinClient::request(const QString &path) const {
 QNetworkRequest r(QUrl(m_server+path));
 r.setRawHeader("X-Emby-Authorization","MediaBrowser Client=RetroArcade, Device=CM5, DeviceId=cm5-prototype, Version=0.3");
 if(!m_token.isEmpty())r.setRawHeader("X-Emby-Token",m_token.toUtf8());
 r.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
 return r;
}
void JellyfinClient::login(const QString &username,const QString &password){
 if(m_server.isEmpty()){setError("Set a server URL first");return;}
 setBusy(true);setError("");
 QJsonObject credentials{{"Username",username},{"Pw",password}};
 auto *reply=m_net.post(request("/Users/AuthenticateByName"),QJsonDocument(credentials).toJson());
 connect(reply,&QNetworkReply::finished,this,[this,reply](){
  setBusy(false);auto doc=QJsonDocument::fromJson(reply->readAll());
  if(reply->error()!=QNetworkReply::NoError){setError("Login failed: "+reply->errorString());reply->deleteLater();return;}
  auto obj=doc.object();m_userId=obj.value("User").toObject().value("Id").toString();
  setToken(obj.value("AccessToken").toString());QSettings("RetroArcade","MediaOS").setValue("userId",m_userId);
  emit connectionChanged();emit loggedIn();reply->deleteLater();
 });
}
void JellyfinClient::fetch(const QString &path){
 if(m_token.isEmpty()||m_userId.isEmpty()){setError("Sign in to Jellyfin first");return;}
 setBusy(true);setError("");auto *reply=m_net.get(request(path));
 connect(reply,&QNetworkReply::finished,this,[this,reply](){
  setBusy(false);auto data=reply->readAll();
  if(reply->error()!=QNetworkReply::NoError){setError("Library request failed: "+reply->errorString());reply->deleteLater();return;}
  auto doc=QJsonDocument::fromJson(data);m_items.clear();
  for(const auto &v:doc.object().value("Items").toArray()){
   auto o=v.toObject();QVariantMap item;
   item["id"]=o.value("Id").toString();item["name"]=o.value("Name").toString();
   item["type"]=o.value("Type").toString();item["overview"]=o.value("Overview").toString();
   item["image"]=imageUrl(o.value("Id").toString());m_items.append(item);
  }
  emit itemsChanged();reply->deleteLater();
 });
}
void JellyfinClient::browse(const QString &type){
 QUrlQuery q;q.addQueryItem("Recursive","true");q.addQueryItem("Limit","100");
 q.addQueryItem("SortBy","SortName");q.addQueryItem("Fields","Overview,PrimaryImageAspectRatio");
 if(type=="Movies")q.addQueryItem("IncludeItemTypes","Movie");
 else if(type=="TV Shows")q.addQueryItem("IncludeItemTypes","Series");
 else if(type=="Music")q.addQueryItem("IncludeItemTypes","Audio");
 else {setError("Unsupported library type");return;}
 fetch("/Users/"+m_userId+"/Items?"+q.toString(QUrl::FullyEncoded));
}
void JellyfinClient::continueWatching(){
 QUrlQuery q;q.addQueryItem("Limit","30");q.addQueryItem("MediaTypes","Video");
 fetch("/Users/"+m_userId+"/Items/Resume?"+q.toString(QUrl::FullyEncoded));
}
QString JellyfinClient::imageUrl(const QString &id)const{
 if(id.isEmpty()||m_token.isEmpty())return {};
 QUrlQuery q;q.addQueryItem("maxWidth","300");q.addQueryItem("api_key",m_token);
 return m_server+"/Items/"+id+"/Images/Primary?"+q.toString(QUrl::FullyEncoded);
}
QString JellyfinClient::playbackUrl(const QString &id)const{
 if(id.isEmpty()||m_token.isEmpty())return {};
 QUrlQuery q;q.addQueryItem("Static","true");q.addQueryItem("api_key",m_token);
 return m_server+"/Videos/"+id+"/stream?"+q.toString(QUrl::FullyEncoded);
}
