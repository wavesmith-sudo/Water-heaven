// Browser preview of the Retro Arcade home UI (Qt for WebAssembly).
// Loads the device's own QML from ../app/qml with DemoJellyfinClient in place of the real
// Jellyfin client. Also builds on desktop Qt for local checks (see README.md).
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <cstdio>

#ifdef Q_OS_WASM
#include <emscripten/val.h>
#endif

#include "demojellyfin.h"

namespace {
QUrl demoClipUrl()
{
#ifdef Q_OS_WASM
    // Resolve against the page, so the preview works under any Pages hostname or path.
    const auto href = QString::fromStdString(emscripten::val::global("location")["href"].as<std::string>());
    return QUrl(href).resolved(QUrl(QStringLiteral("demo-clip.mp4")));
#else
    const QString path = qEnvironmentVariable("CM5_PREVIEW_CLIP");
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
#endif
}

int keyFromName(const QString &name)
{
    static const QHash<QString, int> keys{{"Left", Qt::Key_Left}, {"Right", Qt::Key_Right}, {"Up", Qt::Key_Up},
                                         {"Down", Qt::Key_Down}, {"Return", Qt::Key_Return}, {"Escape", Qt::Key_Escape}};
    return keys.value(name, 0);
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("cm5-home-preview"));

    DemoJellyfinClient jellyfin(demoClipUrl());
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("jellyfin"), new DemoPosterProvider(&jellyfin));
    engine.rootContext()->setContextProperty(QStringLiteral("jellyfin"), &jellyfin);

    int qmlWarnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&qmlWarnings](const QList<QQmlError> &w) { qmlWarnings += w.size(); });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

#ifndef Q_OS_WASM
    // Desktop-only smoke mode, same variables as the device app (CM5_HOME_SMOKE_MS, _KEYS,
    // _SCREENSHOT, _SIZE), so the preview can be checked headless before it is deployed.
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    const QStringList size = qEnvironmentVariable("CM5_HOME_SIZE").split('x');
    if (window && size.size() == 2) {
        window->setVisibility(QWindow::Windowed);
        window->resize(size[0].toInt(), size[1].toInt());
    }
    bool ok = false;
    const int smokeMs = qEnvironmentVariableIntValue("CM5_HOME_SMOKE_MS", &ok);
    if (ok && smokeMs > 0) {
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
#endif
    return app.exec();
}
