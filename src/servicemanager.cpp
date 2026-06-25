#include "servicemanager.h"

#include "constants.h"

#include <QDir>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <qt_windows.h>
#endif

namespace {

constexpr int HealthTimeoutMs = 2000;
constexpr int HealthStartupTimeoutSeconds = 60;
constexpr int HealthRetryDelayMs = 1000;
constexpr int UpdateCommandTimeoutSeconds = 600;
constexpr int UpdateTimeoutSeconds = 240;

bool processSucceeded(const QProcess &process)
{
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

} // namespace

ServiceManager::ServiceManager(QObject *parent)
    : QObject(parent)
    , m_config(Config::load())
{
    m_lastRunning = isRunning();
}

ServiceManager::~ServiceManager()
{
    stop();
}

bool ServiceManager::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

bool ServiceManager::proxyEnabled() const
{
    return m_config.proxyEnabled;
}

bool ServiceManager::updating() const
{
    return m_updating;
}

void ServiceManager::reloadConfig()
{
    const bool oldProxy = m_config.proxyEnabled;
    m_config = Config::load();
    if (oldProxy != m_config.proxyEnabled) {
        emit proxyStateChanged(m_config.proxyEnabled);
    }
}

void ServiceManager::start()
{
    if (isRunning()) {
        qInfo().noquote() << QStringLiteral("服务已在运行，跳过启动 (pid=%1)").arg(m_process->processId());
        return;
    }

    reloadConfig();
    openServiceLog();

    auto *process = new QProcess(this);
    m_process = process;
    setupHiddenConsole(process);
    applyProxyEnvironment(process, m_config);
    process->setProgram(Constants::qwenpawBin());
    process->setArguments(Constants::qwenpawArgs());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (process == m_process) {
            writeServiceLog(process->readAllStandardOutput());
        }
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        if (process == m_process) {
            writeServiceLog(process->readAllStandardError());
        }
    });

    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (process != m_process) {
                    return;
                }
                qInfo().noquote() << QStringLiteral("服务进程退出 exitCode=%1 exitStatus=%2")
                                      .arg(exitCode)
                                      .arg(exitStatus == QProcess::NormalExit ? QStringLiteral("normal")
                                                                              : QStringLiteral("crash"));
                ++m_healthGeneration;
                closeServiceLog();
                m_process = nullptr;
                process->deleteLater();
                emitStateIfChanged();
            });

    connect(process, &QProcess::errorOccurred, this, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCritical().noquote() << QStringLiteral("服务启动失败: %1").arg(process->errorString());
        }
    });

    qInfo().noquote() << QStringLiteral("启动服务: %1 %2")
                          .arg(Constants::qwenpawBin(), Constants::qwenpawArgs().join(QLatin1Char(' ')));

    process->start();
    if (!process->waitForStarted(3000)) {
        const QString message = QStringLiteral("找不到/无法启动 %1: %2")
                                    .arg(Constants::qwenpawBin(), process->errorString());
        qCritical().noquote() << message;
        closeServiceLog();
        m_process = nullptr;
        process->deleteLater();
        emit notificationRequested(QStringLiteral("QwenPaw 启动失败"), message);
        emitStateIfChanged();
        return;
    }

    qInfo().noquote() << QStringLiteral("服务已启动 pid=%1").arg(process->processId());
    emitStateIfChanged();
    startHealthChecks();
}

void ServiceManager::stop()
{
    ++m_healthGeneration;

    QProcess *process = m_process;
    if (!process) {
        qInfo().noquote() << QStringLiteral("服务未运行");
        killAllQwenpaw();
        emitStateIfChanged();
        return;
    }

    qInfo().noquote() << QStringLiteral("停止服务 pid=%1").arg(process->processId());
    m_process = nullptr;
    disconnect(process, nullptr, this, nullptr);

    process->kill();
    if (!process->waitForFinished(8000)) {
        qWarning().noquote() << QStringLiteral("服务未在限时内退出，强制结束");
        process->kill();
        process->waitForFinished(3000);
    }

    process->deleteLater();
    closeServiceLog();
    qInfo().noquote() << QStringLiteral("服务已停止");

    killAllQwenpaw();
    emitStateIfChanged();
}

void ServiceManager::restart()
{
    stop();
    QTimer::singleShot(1000, this, [this]() {
        reloadConfig();
        start();
    });
}

void ServiceManager::startUpdate()
{
    if (m_updating) {
        qInfo().noquote() << QStringLiteral("更新已在进行中，忽略重复点击");
        return;
    }

    m_updating = true;
    emit updateStateChanged(true);

    qInfo().noquote() << QStringLiteral("开始更新 QwenPaw: %1 %2")
                          .arg(Constants::pipBin(), Constants::pipUpdateArgs().join(QLatin1Char(' ')));

    stop();
    QTimer::singleShot(1000, this, [this]() {
        m_updateBefore = qwenpawVersion();
        m_updateLastSeen = m_updateBefore;
        runUpdateCommand();
    });
}

void ServiceManager::applyProxyEnvironment(QProcess *process, const AppConfig &config)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (config.proxyEnabled) {
        if (!config.httpProxy.isEmpty()) {
            env.insert(QStringLiteral("HTTP_PROXY"), config.httpProxy);
            env.insert(QStringLiteral("http_proxy"), config.httpProxy);
            qInfo().noquote() << QStringLiteral("设置 HTTP 代理: %1").arg(config.httpProxy);
        }
        if (!config.httpsProxy.isEmpty()) {
            env.insert(QStringLiteral("HTTPS_PROXY"), config.httpsProxy);
            env.insert(QStringLiteral("https_proxy"), config.httpsProxy);
            qInfo().noquote() << QStringLiteral("设置 HTTPS 代理: %1").arg(config.httpsProxy);
        }
    }
    process->setProcessEnvironment(env);
}

void ServiceManager::setupHiddenConsole(QProcess *process)
{
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#else
    Q_UNUSED(process)
#endif
}

void ServiceManager::openServiceLog()
{
    closeServiceLog();
    QDir().mkpath(Config::logDir());
    m_serviceLog.setFileName(Config::serviceLogPath());
    if (!m_serviceLog.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qCritical().noquote() << QStringLiteral("打开服务日志失败 %1: %2")
                                  .arg(Config::serviceLogPath(), m_serviceLog.errorString());
        return;
    }

    const QString header = QStringLiteral("\n===== 服务启动 %1 =====\n")
                               .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    m_serviceLog.write(header.toUtf8());
    m_serviceLog.flush();
}

void ServiceManager::closeServiceLog()
{
    if (m_serviceLog.isOpen()) {
        m_serviceLog.flush();
        m_serviceLog.close();
    }
}

void ServiceManager::writeServiceLog(const QByteArray &data)
{
    if (!m_serviceLog.isOpen() || data.isEmpty()) {
        return;
    }
    m_serviceLog.write(data);
    m_serviceLog.flush();
}

void ServiceManager::startHealthChecks()
{
    ++m_healthGeneration;
    m_healthDeadline = QDateTime::currentDateTime().addSecs(HealthStartupTimeoutSeconds);
    checkHealth(m_healthGeneration);
}

void ServiceManager::checkHealth(int generation)
{
    if (generation != m_healthGeneration) {
        return;
    }
    if (!isRunning()) {
        qInfo().noquote() << QStringLiteral("服务进程已退出，取消健康检查");
        return;
    }
    if (QDateTime::currentDateTime() >= m_healthDeadline) {
        qWarning().noquote() << QStringLiteral("服务在 %1 秒内未就绪").arg(HealthStartupTimeoutSeconds);
        emit notificationRequested(QStringLiteral("QwenPaw 启动超时"),
                                   QStringLiteral("服务未能在预期时间内就绪，请检查日志"));
        return;
    }

    QNetworkRequest request{QUrl(Constants::healthUrl())};
    QNetworkReply *reply = m_network.get(request);

    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout->start(HealthTimeoutMs);

    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        if (generation != m_healthGeneration) {
            return;
        }

        const bool ok = reply->error() == QNetworkReply::NoError;
        if (ok) {
            qInfo().noquote() << QStringLiteral("服务已就绪，可访问 %1").arg(Constants::healthUrl());
            emit notificationRequested(
                QStringLiteral("QwenPaw 启动成功"),
                QStringLiteral("服务已就绪，访问地址：%1").arg(Constants::chatUrl()));
            return;
        }

        QTimer::singleShot(HealthRetryDelayMs, this, [this, generation]() {
            checkHealth(generation);
        });
    });
}

void ServiceManager::emitStateIfChanged()
{
    const bool running = isRunning();
    if (m_lastRunning == running) {
        return;
    }
    m_lastRunning = running;
    emit serviceStateChanged(running);
}

void ServiceManager::runUpdateCommand()
{
    reloadConfig();
    m_updateStdout.clear();
    m_updateStderr.clear();

    auto *process = new QProcess(this);
    m_updateProcess = process;
    setupHiddenConsole(process);
    applyProxyEnvironment(process, m_config);
    process->setProgram(Constants::pipBin());
    process->setArguments(Constants::pipUpdateArgs());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (process == m_updateProcess) {
            const QByteArray chunk = process->readAllStandardOutput();
            m_updateStdout += chunk;
            if (!chunk.isEmpty()) {
                emit updateLogChunk(QString::fromUtf8(chunk), false);
            }
        }
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        if (process == m_updateProcess) {
            const QByteArray chunk = process->readAllStandardError();
            m_updateStderr += chunk;
            if (!chunk.isEmpty()) {
                emit updateLogChunk(QString::fromUtf8(chunk), true);
            }
        }
    });

    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                handleUpdateCommandFinished(process, exitCode, exitStatus);
            });

    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this, process]() {
        if (process != m_updateProcess || process->state() == QProcess::NotRunning) {
            return;
        }

        const QString message = QStringLiteral("更新命令超过 %1 秒未完成，已终止")
                                    .arg(UpdateCommandTimeoutSeconds);
        qWarning().noquote() << message;
        m_updateStderr += message.toUtf8();
        m_updateStderr += '\n';
        emit updateLogChunk(message + QStringLiteral("\n"), true);
        process->kill();
    });

    process->start();
    if (!process->waitForStarted(3000)) {
        const QString message = QStringLiteral("无法启动 %1: %2")
                                    .arg(Constants::pipBin(), process->errorString());
        qCritical().noquote() << message;
        m_updateProcess = nullptr;
        process->deleteLater();
        start();
        finishUpdate(false, message);
        return;
    }

    timeout->start(UpdateCommandTimeoutSeconds * 1000);
}

void ServiceManager::handleUpdateCommandFinished(QProcess *process, int exitCode, QProcess::ExitStatus exitStatus)
{
    if (process != m_updateProcess) {
        return;
    }

    m_updateStdout += process->readAllStandardOutput();
    m_updateStderr += process->readAllStandardError();
    process->deleteLater();
    m_updateProcess = nullptr;

    const QString stdoutText = QString::fromUtf8(m_updateStdout);
    const QString stderrText = QString::fromUtf8(m_updateStderr);
    const QString combined = stdoutText + QLatin1Char('\n') + stderrText;

    if (!stdoutText.trimmed().isEmpty()) {
        qInfo().noquote() << QStringLiteral("更新 stdout:\n%1").arg(stdoutText.trimmed());
    }
    if (!stderrText.trimmed().isEmpty()) {
        qInfo().noquote() << QStringLiteral("更新 stderr:\n%1").arg(stderrText.trimmed());
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        const QString code = exitStatus == QProcess::NormalExit
            ? QString::number(exitCode)
            : QStringLiteral("进程异常退出");
        const QString message = QStringLiteral("更新命令失败 (退出码 %1)\n%2")
                                    .arg(code, lastLines(combined, 4));
        qCritical().noquote() << message;
        start();
        finishUpdate(false, message);
        return;
    }

    const QString current = qwenpawVersion();
    qInfo().noquote() << QStringLiteral("更新后版本检查: before=%1 after=%2")
                          .arg(m_updateBefore.isEmpty() ? QStringLiteral("<unknown>") : m_updateBefore,
                               current.isEmpty() ? QStringLiteral("<unknown>") : current);

    if (!current.isEmpty() && !m_updateBefore.isEmpty() && current == m_updateBefore) {
        const QString lowered = combined.toLower();
        const bool alreadySatisfied = lowered.contains(QStringLiteral("already satisfied"))
            || lowered.contains(QStringLiteral("requirement already satisfied"));
        const QString message = alreadySatisfied
            ? QStringLiteral("QwenPaw 已是最新版本（%1）").arg(current)
            : QStringLiteral("QwenPaw 版本未变化（%1）").arg(current);
        qInfo().noquote() << message;
        start();
        finishUpdate(true, message);
        return;
    }

    m_updateTarget = current;
    m_updateDeadline = QDateTime::currentDateTime().addSecs(UpdateTimeoutSeconds);
    pollUpdateVersion();
}

void ServiceManager::pollUpdateVersion()
{
    if (QDateTime::currentDateTime() >= m_updateDeadline) {
        const QString current = m_updateLastSeen;
        const QString message = (m_updateBefore == current)
            ? QStringLiteral("更新仍在后台进行（已等待 240 秒，当前 %1），可稍后手动重启服务").arg(current)
            : QStringLiteral("QwenPaw 可能已更新到 %1（已重启服务）").arg(current);
        qWarning().noquote() << message;
        reloadConfig();
        start();
        finishUpdate(false, message);
        return;
    }

    QTimer::singleShot(3000, this, [this]() {
        const QString current = qwenpawVersion();
        if (!current.isEmpty()) {
            m_updateLastSeen = current;
            const bool reached = !m_updateTarget.isEmpty()
                ? current == m_updateTarget
                : (!m_updateBefore.isEmpty() && current != m_updateBefore);

            if (reached) {
                QTimer::singleShot(2000, this, [this, current]() {
                    reloadConfig();
                    start();
                    const QString message = QStringLiteral("QwenPaw 已更新到 %1，并重启服务").arg(current);
                    qInfo().noquote() << message;
                    finishUpdate(true, message);
                });
                return;
            }
        }

        pollUpdateVersion();
    });
}

void ServiceManager::finishUpdate(bool ok, const QString &message)
{
    if (!m_updating) {
        return;
    }
    m_updating = false;
    emit updateStateChanged(false);
    emit updateFinished(ok, message);
}

QString ServiceManager::qwenpawVersion()
{
    QProcess process;
    setupHiddenConsole(&process);
    process.setProgram(Constants::qwenpawBin());
    process.setArguments({QStringLiteral("--version")});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForStarted(3000) || !process.waitForFinished(10000) || !processSucceeded(process)) {
        return {};
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput())
        + QString::fromUtf8(process.readAllStandardError());

    const QRegularExpression regex(QStringLiteral(
        R"((?:version\s*[:=]?\s*)?([0-9]+(?:\.[0-9A-Za-z][0-9A-Za-z.-]*)+(?:[+-][0-9A-Za-z.-]+)?))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = regex.match(output);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return {};
}

QString ServiceManager::lastLines(const QString &text, int count) const
{
    QStringList lines;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty()) {
            lines.append(line.trimmed());
        }
    }

    while (lines.size() > count) {
        lines.removeFirst();
    }
    return lines.join(QLatin1Char('\n'));
}

void ServiceManager::killAllQwenpaw()
{
#ifdef Q_OS_WIN
    QProcess killer;
    setupHiddenConsole(&killer);
    killer.setProgram(QStringLiteral("taskkill"));
    killer.setArguments({QStringLiteral("/F"), QStringLiteral("/IM"), QStringLiteral("qwenpaw.exe")});
    killer.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    killer.start();
    if (killer.waitForStarted(1000)) {
        killer.waitForFinished(5000);
        qInfo().noquote() << QStringLiteral("已执行 taskkill 清理 qwenpaw 进程");
    } else {
        qWarning().noquote() << QStringLiteral("taskkill 执行失败: %1").arg(killer.errorString());
    }
#endif
}
