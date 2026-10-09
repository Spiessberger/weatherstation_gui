#include "logging.h"

#include <QRegularExpression>
#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace {
// Qt reads both variables once, on first use of logging, so set them before any test runs.
// This also keeps a developer's own settings out of the tests.
[[maybe_unused]] const bool envReady = [] {
    qunsetenv("QT_MESSAGE_PATTERN");
    return qputenv("QT_LOGGING_RULES", "ws.env.debug=false");
}();
}

// The test categories default to info, so only a filter rule can turn their debug output on.
TEST(Logging, DefaultRulesEnableDebugForWsOnly)
{
    setupLogging();
    EXPECT_TRUE(QLoggingCategory("ws.test", QtInfoMsg).isDebugEnabled());
    EXPECT_FALSE(QLoggingCategory("other.test", QtInfoMsg).isDebugEnabled());
}

TEST(Logging, AppendedRulesOverrideDefaults)
{
    setupLogging();
    appendLoggingRules(u"ws.test.debug=false;ws.other.debug=false"_s);
    EXPECT_FALSE(QLoggingCategory("ws.test", QtInfoMsg).isDebugEnabled());
    EXPECT_FALSE(QLoggingCategory("ws.other", QtInfoMsg).isDebugEnabled());
    EXPECT_TRUE(QLoggingCategory("ws.third", QtInfoMsg).isDebugEnabled());
}

TEST(Logging, EnvironmentRulesWin)
{
    setupLogging();
    EXPECT_FALSE(QLoggingCategory("ws.env").isDebugEnabled());
    appendLoggingRules(u"ws.env.debug=true"_s);
    EXPECT_FALSE(QLoggingCategory("ws.env").isDebugEnabled());
}

TEST(Logging, PatternShowsTimeLevelAndCategory)
{
    setupLogging();
    const QMessageLogContext context(nullptr, 0, nullptr, "ws.test");
    const QString line = qFormatLogMessage(QtInfoMsg, context, u"hello"_s);
    const QRegularExpression expected(uR"(^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3} info ws\.test: hello$)"_s);
    EXPECT_TRUE(expected.match(line).hasMatch()) << line.toStdString();
}
