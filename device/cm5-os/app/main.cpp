#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include "jellyfinclient.h"
int main(int argc,char *argv[]){
 QGuiApplication app(argc,argv);
 JellyfinClient jellyfin;
 QQmlApplicationEngine engine;
 engine.rootContext()->setContextProperty("jellyfin",&jellyfin);
 QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[](){QCoreApplication::exit(-1);},Qt::QueuedConnection);
 engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
 return app.exec();
}
