#include "logging.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    setupLogging();
    QGuiApplication app(argc, argv);
    qCInfo(lcApp, "Starting weatherstation_gui %s (Qt %s)", APP_VERSION, qVersion());

    QQmlApplicationEngine engine;
    engine.loadFromModule("App", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE; // the engine has already printed the QML errors
    qCDebug(lcApp) << "Main.qml loaded";

    return app.exec();
}
