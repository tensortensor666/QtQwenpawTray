#include "config.h"
#include "constants.h"
#include "logger.h"
#include "servicemanager.h"
#include "trayapplication.h"
#include "winutil.h"

#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);

    QApplication app(argc, argv);
    QApplication::setApplicationName(Constants::appName());
    QApplication::setOrganizationName(QStringLiteral("QwenPaw"));
    QApplication::setQuitOnLastWindowClosed(false);

    Logger::init();
    qInfo().noquote() << QStringLiteral("===== %1 启动 =====").arg(Constants::appName());

    Config::ensureConfigFile();
    Logger::cleanupOldLogs();

    if (app.arguments().contains(QStringLiteral("--self-update"))) {
        qInfo().noquote() << QStringLiteral("--self-update：直接执行更新管线（不启动托盘）");
        ServiceManager service;
        QEventLoop loop;
        bool ok = false;
        QString message;
        QObject::connect(&service, &ServiceManager::updateFinished, &loop, [&](bool result, const QString &msg) {
            ok = result;
            message = msg;
            loop.quit();
        });
        QTimer::singleShot(0, &service, &ServiceManager::startUpdate);
        loop.exec();
        QTextStream(stdout) << "[--self-update] ok=" << (ok ? "true" : "false") << ": " << message << '\n';
        return ok ? 0 : 1;
    }

    SingleInstance instance(QStringLiteral("%1_Mutex").arg(Constants::appName()));
    if (!instance.isPrimary()) {
        qCritical().noquote() << QStringLiteral("检测到已有托盘实例在运行，退出");
        return 0;
    }

    TrayApplication tray;
    tray.show();

    return app.exec();
}
