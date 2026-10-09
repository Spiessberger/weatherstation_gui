#include "config.h"

#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QSettings>

#include <limits>

using namespace Qt::StringLiterals;

Q_STATIC_LOGGING_CATEGORY(lcConfig, "ws.config")

namespace {
// Each parse() stores the value of text and returns an empty string, or returns what it expected.
QString parse(const QString &text, int &value, int min, int max)
{
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok || parsed < min || parsed > max)
        return u"an integer from %1 to %2"_s.arg(min).arg(max);
    value = parsed;
    return {};
}

QString parse(const QString &text, double &value, double min, double max)
{
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (!ok || !(parsed >= min && parsed <= max)) // negated so that nan fails too
        return u"a number from %1 to %2"_s.arg(min).arg(max);
    value = parsed;
    return {};
}

QString parse(const QString &text, bool &value)
{
    if (text != u"true" && text != u"false")
        return u"true or false"_s;
    value = text == u"true";
    return {};
}

QString parse(const QString &text, QString &value, const QStringList &choices = {})
{
    if (!choices.isEmpty() && !choices.contains(text))
        return choices.join(u" or "_s);
    value = text;
    return {};
}
} // namespace

Config resolveConfig(const QSettings &ini, const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        u"Shows the latest 360-degree panorama of a Panomax webcam.\n\n"
        "Every setting can also go in %1\n"
        "as option=value, for example camera=9015 or fullscreen=true. Options override the file."_s
            .arg(ini.fileName()));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {u"camera"_s, u"Panomax camera id (default: 9015)."_s, u"id"_s},
        {u"size"_s,
         u"Image size variant: thumb, small, optimized, reduced, default, hd, full, or auto for the "
         "smallest one at least 800 px high (default: auto)."_s,
         u"name"_s},
        {u"autoscroll-speed"_s, u"Autoscroll speed in degrees per second, 0.1 to 360 (default: 6)."_s,
         u"deg/s"_s},
        {u"autoscroll-direction"_s, u"Autoscroll direction (default: right)."_s, u"left|right"_s},
        {u"idle-timeout"_s,
         u"Seconds without input before autoscroll resumes, 1 to 3600 (default: 5)."_s, u"seconds"_s},
        {u"poll-interval"_s, u"Seconds between checks for a newer image, 10 to 3600 (default: 60)."_s,
         u"seconds"_s},
        {u"fullscreen"_s, u"Show the window fullscreen."_s},
        {u"windowed"_s, u"Show a 1280x800 window (default)."_s},
        {u"log-rules"_s,
         u"Logging rules in QT_LOGGING_RULES syntax, added to the defaults. In the INI file, "
         "separate rules with commas, since ';' starts a comment there."_s,
         u"rules"_s},
    });
    parser.process(arguments);

    // The command line as INI keys and values; the window mode flags become fullscreen=true/false.
    QHash<QString, QString> commandLine;
    for (const QString &name : parser.optionNames()) {
        if (name == u"fullscreen" || name == u"windowed")
            commandLine[u"fullscreen"_s] = name == u"fullscreen" ? u"true"_s : u"false"_s;
        else
            commandLine[name] = parser.value(name);
    }

    Config config;
    config.iniPath = ini.fileName();
    if (ini.status() != QSettings::NoError)
        qCWarning(lcConfig).noquote() << "Cannot fully read" << config.iniPath << ini.status();

    // Applies the INI file, then the command line. An invalid value keeps the one below it.
    const auto resolve = [&](const QString &key, auto &value, const auto &...limits) {
        config.sources[key] = u"default"_s;
        const auto apply = [&](const QString &text, const QString &source) {
            const QString expected = parse(text, value, limits...);
            if (expected.isEmpty())
                config.sources[key] = source;
            else
                qCWarning(lcConfig).noquote()
                    << u"Ignoring %1 \"%2\" from %3: expected %4"_s.arg(key, text, source, expected);
        };
        if (ini.contains(key)) // QSettings splits unquoted INI values at commas
            apply(ini.value(key).toStringList().join(u';'), config.iniPath);
        if (commandLine.contains(key))
            apply(commandLine.value(key), u"command line"_s);
    };
    resolve(u"camera"_s, config.cameraId, 1, std::numeric_limits<int>::max());
    resolve(u"size"_s, config.sizeVariant);
    resolve(u"autoscroll-speed"_s, config.autoscrollSpeed, 0.1, 360.0);
    resolve(u"autoscroll-direction"_s, config.autoscrollDirection, QStringList{u"left"_s, u"right"_s});
    resolve(u"idle-timeout"_s, config.idleTimeout, 1, 3600);
    resolve(u"poll-interval"_s, config.pollInterval, 10, 3600);
    resolve(u"fullscreen"_s, config.fullscreen);
    resolve(u"log-rules"_s, config.logRules);
    return config;
}

void logConfig(const Config &config)
{
    qCInfo(lcConfig).noquote() << "INI file:" << config.iniPath;
    const auto log = [&config](const QString &key, const auto &value) {
        qCInfo(lcConfig).nospace() << qUtf8Printable(key) << " = " << value << " ("
                                   << qUtf8Printable(config.sources.value(key)) << ')';
    };
    log(u"camera"_s, config.cameraId);
    log(u"size"_s, config.sizeVariant);
    log(u"autoscroll-speed"_s, config.autoscrollSpeed);
    log(u"autoscroll-direction"_s, config.autoscrollDirection);
    log(u"idle-timeout"_s, config.idleTimeout);
    log(u"poll-interval"_s, config.pollInterval);
    log(u"fullscreen"_s, config.fullscreen);
    log(u"log-rules"_s, config.logRules);
}
