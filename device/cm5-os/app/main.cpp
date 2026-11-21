#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QKeyEvent>
#include <QTimer>
#include <QUrl>
#include <cstdio>

#include "jellyfinclient.h"
#include "jellyfinimageprovider.h"

namespace {
int keyFromName(const QString &name)
{
    static const QHash<QString, int> keys{{"Left", Qt::Key_Left}, {"Right", Qt::Key_Right}, {"Up", Qt::Key_Up},
                                         {"Down", Qt::Key_Down}, {"Return", Qt::Key_Return}, {"Escape", Qt::Key_Escape},
                                         {"Home", Qt::Key_Home}};
    return keys.value(name, 0);
}
} // namespace

// Development aid (host builds, CI):
//   CM5_HOME_SMOKE_MS=<ms>    load the UI, then exit after <ms>. Exit code is non-zero if QML
//                             failed to load or produced warnings.
//   CM5_HOME_SIZE=<w>x<h>     run windowed at this size instead of full screen.
//   CM5_HOME_KEYS=Right,Return,...  remote presses to replay (one every 400 ms) before exiting.
//   CM5_HOME_SCREENSHOT=<png> save a screenshot just before exiting.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("cm5-home"));
    QGuiApplication::setApplicationVersion(QLatin1String(JellyfinClient::ClientVersion));

    JellyfinClient jellyfin;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("jellyfin"), new JellyfinImageProvider(&jellyfin));
    engine.rootContext()->setContextProperty(QStringLiteral("jellyfin"), &jellyfin);

    int qmlWarnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&qmlWarnings](const QList<QQmlError> &w) { qmlWarnings += w.size(); });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    bool okSmoke = false;
    const int smokeMs = qEnvironmentVariableIntValue("CM5_HOME_SMOKE_MS", &okSmoke);
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    const QStringList size = qEnvironmentVariable("CM5_HOME_SIZE").split('x');
    if (window && size.size() == 2) {
        window->setVisibility(QWindow::Windowed);
        window->resize(size[0].toInt(), size[1].toInt());
    }
    if (okSmoke && smokeMs > 0) {
        const QStringList keys = qEnvironmentVariable("CM5_HOME_KEYS").split(',', Qt::SkipEmptyParts);
        for (int i = 0; i < keys.size(); ++i) {
            QTimer::singleShot(smokeMs + 400 * i, &app, [window, key = keyFromName(keys[i].trimmed())]() {
                if (!window || !key)
                    return;
                QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
                QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &press);
                QCoreApplication::sendEvent(window, &release);
            });
        }
        QTimer::singleShot(smokeMs + 400 * keys.size() + 1500, &app, [&, window]() {
            const QString shot = qEnvironmentVariable("CM5_HOME_SCREENSHOT");
            if (window && !shot.isEmpty())
                window->grabWindow().save(shot);
            std::fprintf(stderr, "smoke: window=%s qmlWarnings=%d\n", window ? "ok" : "missing", qmlWarnings);
            QCoreApplication::exit(window && qmlWarnings == 0 ? 0 : 2);
        });
    }
    return app.exec();
}
