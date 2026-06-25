#pragma once

#include "config.h"

#include <QDateTime>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>

class ServiceManager : public QObject
{
    Q_OBJECT

public:
    explicit ServiceManager(QObject *parent = nullptr);
    ~ServiceManager() override;

    bool isRunning() const;
    bool proxyEnabled() const;
    bool updating() const;

public slots:
    void reloadConfig();
    void start();
    void stop();
    void restart();
    void startUpdate();

signals:
    void serviceStateChanged(bool running);
    void proxyStateChanged(bool enabled);
    void notificationRequested(const QString &title, const QString &message);
    void updateStateChanged(bool updating);
    void updateLogChunk(const QString &text, bool isError);
    void updateFinished(bool ok, const QString &message);

private:
    void applyProxyEnvironment(QProcess *process, const AppConfig &config);
    void setupHiddenConsole(QProcess *process);

    void openServiceLog();
    void closeServiceLog();
    void writeServiceLog(const QByteArray &data);

    void startHealthChecks();
    void checkHealth(int generation);
    void emitStateIfChanged();

    void runUpdateCommand();
    void handleUpdateCommandFinished(QProcess *process, int exitCode, QProcess::ExitStatus exitStatus);
    void pollUpdateVersion();
    void finishUpdate(bool ok, const QString &message);

    QString qwenpawVersion();
    QString lastLines(const QString &text, int count) const;
    void killAllQwenpaw();

    QProcess *m_process = nullptr;
    QFile m_serviceLog;
    AppConfig m_config;
    bool m_lastRunning = false;

    QNetworkAccessManager m_network;
    int m_healthGeneration = 0;
    QDateTime m_healthDeadline;

    bool m_updating = false;
    QPointer<QProcess> m_updateProcess;
    QByteArray m_updateStdout;
    QByteArray m_updateStderr;
    QString m_updateBefore;
    QString m_updateTarget;
    QString m_updateLastSeen;
    QDateTime m_updateDeadline;
};
