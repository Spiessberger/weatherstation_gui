#include "logging.h"

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcApp, "ws.app")

namespace {
// The default filter, defined only here: "ws.*.debug=false" switches it to info.
constexpr auto defaultRules = "ws.*.debug=true";
// setFilterRules() replaces all earlier API rules, so keep defaults + appended rules.
QString activeRules;
}

void setupLogging()
{
    qSetMessagePattern(u"%{time yyyy-MM-dd HH:mm:ss.zzz} %{type} %{category}: %{message}"_s);
    activeRules = QString::fromLatin1(defaultRules);
    QLoggingCategory::setFilterRules(activeRules);
}

void appendLoggingRules(const QString &rules)
{
    activeRules += u'\n' + QString(rules).replace(u';', u'\n');
    QLoggingCategory::setFilterRules(activeRules);
}
