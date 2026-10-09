#pragma once

#include <QLoggingCategory>
#include <QString>

// One category per component, all under "ws." (ws.app, ws.config, ws.panomax.api,
// ws.panomax.image, ws.ui.panorama), each defined in its component's own code.
// QML uses LoggingCategory { name: "ws.ui.panorama" }.
Q_DECLARE_LOGGING_CATEGORY(lcApp)

// Installs the default message pattern and filter rules. Call first in main().
// QT_MESSAGE_PATTERN and QT_LOGGING_RULES still override them.
void setupLogging();

// Appends filter rules after the current ones; later rules win. Same syntax as
// QT_LOGGING_RULES (';' or newline separated), which still takes precedence.
void appendLoggingRules(const QString &rules);
