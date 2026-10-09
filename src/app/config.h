#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

class QSettings;

// The app settings, in the units of the INI file and the command line.
struct Config
{
    int cameraId = 9015;
    QString sizeVariant = QStringLiteral("auto");          // Panomax size variant name, or "auto"
    double autoscrollSpeed = 6;                            // degrees per second
    QString autoscrollDirection = QStringLiteral("right"); // "left" or "right"
    int idleTimeout = 5;                                   // seconds without input before autoscroll resumes
    int pollInterval = 60;                                 // seconds between checks for a newer image
    bool fullscreen = false;                               // false: a 1280x800 window
    QString logRules;                                      // QT_LOGGING_RULES syntax, for appendLoggingRules()

    QString iniPath;
    QHash<QString, QString> sources; // INI key -> "default", iniPath or "command line"
};

// Resolves the defaults above < the INI file < the command line. Invalid values are logged and
// skipped. Like QCommandLineParser::process(), exits on --help, --version and malformed options.
Config resolveConfig(const QSettings &ini, const QStringList &arguments);

// Logs the INI path and one line per setting with its source.
void logConfig(const Config &config);
