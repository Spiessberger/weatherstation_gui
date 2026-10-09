#include "config.h"
#include "logging.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSettings>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    setupLogging();
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(u"weatherstation"_s);
    QGuiApplication::setApplicationName(u"weatherstation_gui"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    const QSettings ini(QSettings::IniFormat, QSettings::UserScope, QGuiApplication::organizationName(),
                        QGuiApplication::applicationName());
    const Config config = resolveConfig(ini, app.arguments());
    appendLoggingRules(config.logRules);
    qCInfo(lcApp, "Starting weatherstation_gui %s (Qt %s)", APP_VERSION, qVersion());
    logConfig(config);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {u"cameraId"_s, config.cameraId},
        {u"sizeVariant"_s, config.sizeVariant},
        {u"autoscrollSpeed"_s, config.autoscrollSpeed},
        {u"autoscrollDirection"_s, config.autoscrollDirection},
        {u"idleTimeout"_s, config.idleTimeout * 1000}, // QML timers count in ms
        {u"pollInterval"_s, config.pollInterval * 1000},
        {u"fullscreen"_s, config.fullscreen},
    });
    engine.loadFromModule("App", "Main"); // shows the window, fullscreen or windowed
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE; // the engine has already printed the QML errors
    qCDebug(lcApp) << "Main.qml loaded";

    return app.exec();
}
