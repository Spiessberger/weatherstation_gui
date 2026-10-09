#include "config.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

// Readable QStrings in failure messages.
static void PrintTo(const QString &string, std::ostream *os)
{
    *os << '"' << string.toStdString() << '"';
}

namespace {
const QStringList keys = {u"camera"_s,       u"size"_s,          u"autoscroll-speed"_s, u"autoscroll-direction"_s,
                          u"idle-timeout"_s, u"poll-interval"_s, u"fullscreen"_s,       u"log-rules"_s};

// Every setting, with values that differ from the defaults.
constexpr auto fullIni = "camera=1583\nsize=hd\nautoscroll-speed=2.5\nautoscroll-direction=left\n"
                         "idle-timeout=30\npoll-interval=120\nfullscreen=true\n"
                         "log-rules=ws.*.debug=false, qt.*=false\n";

class ConfigTest : public testing::Test
{
protected:
    // Resolves with an INI file of this content and these command line options.
    Config resolve(const QByteArray &ini, const QStringList &options = {})
    {
        QFile file(iniPath);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(ini);
        file.close();
        return resolveConfig(QSettings(iniPath, QSettings::IniFormat), QStringList{u"app"_s} + options);
    }

    QTemporaryDir dir;
    const QString iniPath = dir.filePath(u"settings.ini"_s);
};
} // namespace

TEST_F(ConfigTest, DefaultsWithoutIniFile)
{
    const QString missing = dir.filePath(u"missing.ini"_s);
    const Config config = resolveConfig(QSettings(missing, QSettings::IniFormat), {u"app"_s});
    EXPECT_EQ(config.cameraId, 9015);
    EXPECT_EQ(config.sizeVariant, u"auto"_s);
    EXPECT_EQ(config.autoscrollSpeed, 6.0);
    EXPECT_EQ(config.autoscrollDirection, u"right"_s);
    EXPECT_EQ(config.idleTimeout, 5);
    EXPECT_EQ(config.pollInterval, 60);
    EXPECT_FALSE(config.fullscreen);
    EXPECT_EQ(config.logRules, QString());
    EXPECT_EQ(config.iniPath, missing);
    for (const QString &key : keys)
        EXPECT_EQ(config.sources.value(key), u"default"_s) << key.toStdString();
}

TEST_F(ConfigTest, IniFileOverridesDefaults)
{
    const Config config = resolve(fullIni);
    EXPECT_EQ(config.cameraId, 1583);
    EXPECT_EQ(config.sizeVariant, u"hd"_s);
    EXPECT_EQ(config.autoscrollSpeed, 2.5);
    EXPECT_EQ(config.autoscrollDirection, u"left"_s);
    EXPECT_EQ(config.idleTimeout, 30);
    EXPECT_EQ(config.pollInterval, 120);
    EXPECT_TRUE(config.fullscreen);
    EXPECT_EQ(config.logRules, u"ws.*.debug=false;qt.*=false"_s);
    for (const QString &key : keys)
        EXPECT_EQ(config.sources.value(key), iniPath) << key.toStdString();
}

TEST_F(ConfigTest, CommandLineOverridesIniFile)
{
    const Config config = resolve(fullIni, {u"--camera"_s, u"10035"_s, u"--size=full"_s, u"--autoscroll-speed"_s,
                                            u"12"_s, u"--autoscroll-direction"_s, u"right"_s, u"--idle-timeout"_s,
                                            u"10"_s, u"--poll-interval"_s, u"30"_s, u"--windowed"_s,
                                            u"--log-rules"_s, u"ws.app.debug=false;ws.config.debug=false"_s});
    EXPECT_EQ(config.cameraId, 10035);
    EXPECT_EQ(config.sizeVariant, u"full"_s);
    EXPECT_EQ(config.autoscrollSpeed, 12.0);
    EXPECT_EQ(config.autoscrollDirection, u"right"_s);
    EXPECT_EQ(config.idleTimeout, 10);
    EXPECT_EQ(config.pollInterval, 30);
    EXPECT_FALSE(config.fullscreen);
    EXPECT_EQ(config.logRules, u"ws.app.debug=false;ws.config.debug=false"_s);
    for (const QString &key : keys)
        EXPECT_EQ(config.sources.value(key), u"command line"_s) << key.toStdString();
}

TEST_F(ConfigTest, EachSettingComesFromItsHighestLayer)
{
    const Config config = resolve("camera=1583\nsize=hd\n", {u"--camera"_s, u"4480"_s});
    EXPECT_EQ(config.cameraId, 4480);
    EXPECT_EQ(config.sources.value(u"camera"_s), u"command line"_s);
    EXPECT_EQ(config.sizeVariant, u"hd"_s);
    EXPECT_EQ(config.sources.value(u"size"_s), iniPath);
    EXPECT_EQ(config.pollInterval, 60);
    EXPECT_EQ(config.sources.value(u"poll-interval"_s), u"default"_s);
}

TEST_F(ConfigTest, InvalidIniValuesFallBackToDefaults)
{
    const Config config = resolve("camera=abc\nautoscroll-speed=0\nautoscroll-direction=up\n"
                                  "idle-timeout=5.5\npoll-interval=9\nfullscreen=yes\n");
    EXPECT_EQ(config.cameraId, 9015);
    EXPECT_EQ(config.autoscrollSpeed, 6.0);
    EXPECT_EQ(config.autoscrollDirection, u"right"_s);
    EXPECT_EQ(config.idleTimeout, 5);
    EXPECT_EQ(config.pollInterval, 60);
    EXPECT_FALSE(config.fullscreen);
    for (const QString &key : keys)
        EXPECT_EQ(config.sources.value(key), u"default"_s) << key.toStdString();
}

TEST_F(ConfigTest, InvalidOptionsFallBackToIniFile)
{
    const Config config = resolve(fullIni, {u"--camera"_s, u"0"_s, u"--autoscroll-speed"_s, u"nan"_s,
                                            u"--idle-timeout"_s, u"3601"_s, u"--poll-interval"_s, u"-60"_s});
    EXPECT_EQ(config.cameraId, 1583);
    EXPECT_EQ(config.autoscrollSpeed, 2.5);
    EXPECT_EQ(config.idleTimeout, 30);
    EXPECT_EQ(config.pollInterval, 120);
    for (const QString &key : {u"camera"_s, u"autoscroll-speed"_s, u"idle-timeout"_s, u"poll-interval"_s})
        EXPECT_EQ(config.sources.value(key), iniPath) << key.toStdString();

    EXPECT_EQ(resolve({}, {u"--camera"_s, u"9015x"_s}).sources.value(u"camera"_s), u"default"_s);
}

TEST_F(ConfigTest, FullscreenSwitchesBothWays)
{
    Config config = resolve("fullscreen=true\n", {u"--windowed"_s});
    EXPECT_FALSE(config.fullscreen);
    EXPECT_EQ(config.sources.value(u"fullscreen"_s), u"command line"_s);

    config = resolve("fullscreen=false\n", {u"--fullscreen"_s});
    EXPECT_TRUE(config.fullscreen);
    EXPECT_EQ(config.sources.value(u"fullscreen"_s), u"command line"_s);

    EXPECT_FALSE(resolve("fullscreen=false\n").fullscreen);
    EXPECT_FALSE(resolve({}, {u"--fullscreen"_s, u"--windowed"_s}).fullscreen); // the last flag wins
    EXPECT_TRUE(resolve({}, {u"--windowed"_s, u"--fullscreen"_s}).fullscreen);
}

TEST_F(ConfigTest, DirectionIsLeftOrRight)
{
    EXPECT_EQ(resolve({}, {u"--autoscroll-direction"_s, u"left"_s}).autoscrollDirection, u"left"_s);
    EXPECT_EQ(resolve("autoscroll-direction=left\n", {u"--autoscroll-direction=right"_s}).autoscrollDirection,
              u"right"_s);
    for (const QString &invalid : {u"up"_s, u"Left"_s, u""_s}) {
        const Config config = resolve("autoscroll-direction=left\n", {u"--autoscroll-direction"_s, invalid});
        EXPECT_EQ(config.autoscrollDirection, u"left"_s) << invalid.toStdString();
        EXPECT_EQ(config.sources.value(u"autoscroll-direction"_s), iniPath) << invalid.toStdString();
    }
}
