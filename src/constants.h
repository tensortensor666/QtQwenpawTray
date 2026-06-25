#pragma once

#include <QString>
#include <QStringList>

namespace Constants {

inline QString appName()
{
    return QStringLiteral("QwenPawTray");
}

inline QString chatUrl()
{
    return QStringLiteral("http://127.0.0.1:8088/chat");
}

inline QString healthUrl()
{
    return chatUrl();
}

inline QString qwenpawBin()
{
    return QStringLiteral("qwenpaw");
}

inline QStringList qwenpawArgs()
{
    return {QStringLiteral("app")};
}

inline QString pipBin()
{
    return QStringLiteral("python");
}

inline QStringList pipUpdateArgs()
{
    return {
        QStringLiteral("-m"),
        QStringLiteral("pip"),
        QStringLiteral("install"),
        QStringLiteral("--upgrade"),
        QStringLiteral("--disable-pip-version-check"),
        QStringLiteral("--no-input"),
        QStringLiteral("--progress-bar"),
        QStringLiteral("off"),
        QStringLiteral("qwenpaw"),
    };
}

inline QString runKey()
{
    return QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

inline int logRetentionDays()
{
    return 7;
}

} // namespace Constants
