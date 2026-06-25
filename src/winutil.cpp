#include "winutil.h"

#include "constants.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <qt_windows.h>
#include <shellapi.h>
#endif

SingleInstance::SingleInstance(const QString &name)
{
#ifdef Q_OS_WIN
    const QString mutexName = QStringLiteral("Local\\%1").arg(name);
    m_handle = CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(mutexName.utf16()));
    if (!m_handle) {
        qCritical().noquote() << QStringLiteral("创建互斥锁失败");
        m_primary = false;
        return;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        qWarning().noquote() << QStringLiteral("互斥锁已存在，程序已在运行");
        CloseHandle(m_handle);
        m_handle = nullptr;
        m_primary = false;
    }
#else
    Q_UNUSED(name)
#endif
}

SingleInstance::~SingleInstance()
{
#ifdef Q_OS_WIN
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
#endif
}

bool SingleInstance::isPrimary() const
{
    return m_primary;
}

namespace {

QString runKeyPath()
{
    return QStringLiteral("HKEY_CURRENT_USER\\%1").arg(Constants::runKey());
}

} // namespace

namespace WinUtil {

QString autostartCommand()
{
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QStringLiteral("\"%1\"").arg(exe);
}

bool autostartEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings(runKeyPath(), QSettings::NativeFormat);
    return settings.value(Constants::appName()).toString() == autostartCommand();
#else
    return false;
#endif
}

bool setAutostart(bool enable, QString *errorMessage)
{
#ifdef Q_OS_WIN
    QSettings settings(runKeyPath(), QSettings::NativeFormat);
    if (enable) {
        const QString command = autostartCommand();
        settings.setValue(Constants::appName(), command);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("写入注册表失败");
            }
            return false;
        }
        qInfo().noquote() << QStringLiteral("已启用开机自启: %1").arg(command);
    } else {
        settings.remove(Constants::appName());
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("删除注册表项失败");
            }
            return false;
        }
        qInfo().noquote() << QStringLiteral("已取消开机自启");
    }
    return true;
#else
    Q_UNUSED(enable)
    if (errorMessage) {
        *errorMessage = QStringLiteral("开机自启仅支持 Windows");
    }
    return false;
#endif
}

void shellOpen(const QString &target)
{
#ifdef Q_OS_WIN
    const QUrl url(target);
    const QString shellTarget = url.isValid() && !url.scheme().isEmpty()
        ? target
        : QDir::toNativeSeparators(target);
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        reinterpret_cast<LPCWSTR>(shellTarget.utf16()),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        qWarning().noquote() << QStringLiteral("打开失败: %1").arg(target);
    }
#else
    const QFileInfo info(target);
    if (info.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    } else {
        QDesktopServices::openUrl(QUrl(target));
    }
#endif
}

} // namespace WinUtil
