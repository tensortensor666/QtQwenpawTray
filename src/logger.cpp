#include "logger.h"

#include "config.h"
#include "constants.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace {

QMutex g_logMutex;
QFile g_logFile;
QString g_logDate;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

void ensureLogFileOpen()
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    if (g_logFile.isOpen() && g_logDate == today) {
        return;
    }

    if (g_logFile.isOpen()) {
        g_logFile.close();
    }

    QDir dir;
    dir.mkpath(Config::logDir());
    g_logDate = today;
    g_logFile.setFileName(QDir(Config::logDir()).filePath(QStringLiteral("tray.log.%1").arg(today)));
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    QMutexLocker locker(&g_logMutex);
    ensureLogFileOpen();

    if (g_logFile.isOpen()) {
        QTextStream stream(&g_logFile);
        stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
               << " [" << levelName(type) << "] " << message << '\n';
        stream.flush();
        g_logFile.flush();
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

} // namespace

namespace Logger {

void init()
{
    QDir().mkpath(Config::logDir());
    qInstallMessageHandler(messageHandler);
}

void cleanupOldLogs()
{
    QDir dir(Config::logDir());
    if (!dir.exists()) {
        return;
    }

    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-Constants::logRetentionDays());
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    int deleted = 0;
    for (const QFileInfo &fileInfo : files) {
        const QString name = fileInfo.fileName();
        if (!name.endsWith(QStringLiteral(".log")) && !name.contains(QStringLiteral(".log."))) {
            continue;
        }
        if (fileInfo.lastModified() >= cutoff) {
            continue;
        }
        if (QFile::remove(fileInfo.absoluteFilePath())) {
            ++deleted;
            qInfo().noquote() << QStringLiteral("已删除过期日志: %1").arg(name);
        }
    }

    if (deleted > 0) {
        qInfo().noquote() << QStringLiteral("日志清理完成，共删除 %1 个文件").arg(deleted);
    } else {
        qDebug().noquote() << QStringLiteral("没有需要清理的过期日志");
    }
}

} // namespace Logger
