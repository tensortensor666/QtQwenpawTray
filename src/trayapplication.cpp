#include "trayapplication.h"

#include "config.h"
#include "constants.h"
#include "iconfactory.h"
#include "winutil.h"

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSizeGrip>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QStyle>

namespace {

QIcon standardIcon(QStyle::StandardPixmap pixmap)
{
    return qApp->style()->standardIcon(pixmap);
}

QIcon dotIcon(const QColor &fill)
{
    constexpr int size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::white, 1.4));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(3.0, 3.0, 10.0, 10.0));
    return QIcon(pixmap);
}

void setMenuIcon(QAction *action, const QIcon &icon)
{
    action->setIcon(icon);
    action->setIconVisibleInMenu(true);
}

QString dialogStyleSheet()
{
    return QStringLiteral(R"(
        QDialog {
            background: #f6f8fb;
            color: #1f2328;
            font-size: 13px;
        }
        QLabel#DialogTitle {
            font-size: 19px;
            font-weight: 700;
            color: #0f172a;
        }
        QLabel#DialogSubtitle {
            color: #64748b;
            line-height: 145%;
        }
        QGroupBox {
            border: 1px solid #d8dee8;
            border-radius: 8px;
            margin-top: 14px;
            padding: 14px 12px 12px 12px;
            background: #ffffff;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #334155;
        }
        QLineEdit, QTextEdit {
            border: 1px solid #cbd5e1;
            border-radius: 7px;
            padding: 8px 10px;
            background: #ffffff;
            selection-background-color: #2563eb;
        }
        QLineEdit:focus, QTextEdit:focus {
            border-color: #2563eb;
        }
        QPushButton {
            border: 1px solid #cbd5e1;
            border-radius: 7px;
            padding: 7px 14px;
            background: #ffffff;
            min-width: 76px;
        }
        QPushButton:hover {
            background: #f1f5f9;
        }
        QPushButton:default {
            background: #2563eb;
            border-color: #2563eb;
            color: #ffffff;
            font-weight: 600;
        }
        QPushButton:disabled {
            color: #94a3b8;
            background: #eef2f7;
        }
        QCheckBox {
            spacing: 8px;
        }
    )");
}

QLabel *makeTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("DialogTitle"));
    return label;
}

QLabel *makeSubtitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("DialogSubtitle"));
    label->setWordWrap(true);
    return label;
}

QTextCharFormat updateLogFormat(const QString &line, bool isError)
{
    QTextCharFormat format;
    format.setFontFamily(QStringLiteral("Consolas"));
    format.setFontFixedPitch(true);
    format.setForeground(QColor(226, 232, 240));

    const QString lower = line.toLower();
    if (isError || lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("failed"))
        || lower.contains(QStringLiteral("失败")) || lower.contains(QStringLiteral("异常"))) {
        format.setForeground(QColor(248, 113, 113));
        format.setBackground(QColor(69, 26, 26));
        format.setFontWeight(QFont::DemiBold);
    } else if (lower.contains(QStringLiteral("warning")) || lower.contains(QStringLiteral("warn"))
               || lower.contains(QStringLiteral("retry")) || lower.contains(QStringLiteral("timeout"))
               || lower.contains(QStringLiteral("警告")) || lower.contains(QStringLiteral("超时"))) {
        format.setForeground(QColor(251, 191, 36));
        format.setBackground(QColor(69, 47, 12));
    } else if (lower.contains(QStringLiteral("success")) || lower.contains(QStringLiteral("complete"))
               || lower.contains(QStringLiteral("installed")) || lower.contains(QStringLiteral("完成"))
               || lower.contains(QStringLiteral("已更新")) || lower.contains(QStringLiteral("已是最新"))) {
        format.setForeground(QColor(74, 222, 128));
        format.setBackground(QColor(20, 83, 45));
        format.setFontWeight(QFont::DemiBold);
    } else if (lower.contains(QStringLiteral("download")) || lower.contains(QStringLiteral("collecting"))
               || lower.contains(QStringLiteral("install")) || lower.contains(QStringLiteral("version"))
               || lower.contains(QStringLiteral("版本")) || lower.contains(QStringLiteral("更新"))) {
        format.setForeground(QColor(96, 165, 250));
        format.setBackground(QColor(30, 58, 138));
    }

    return format;
}

} // namespace

TrayApplication::TrayApplication(QObject *parent)
    : QObject(parent)
    , m_service(this)
{
    buildMenu();

    m_tray.setToolTip(Constants::appName());
    m_tray.setIcon(IconFactory::build(false));
    m_tray.setContextMenu(&m_menu);

    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            openChat();
        }
    });

    connect(&m_service, &ServiceManager::serviceStateChanged, this, &TrayApplication::refresh);
    connect(&m_service, &ServiceManager::proxyStateChanged, this, &TrayApplication::refresh);
    connect(&m_service, &ServiceManager::updateStateChanged, this, &TrayApplication::refresh);
    connect(&m_service, &ServiceManager::updateLogChunk, this, &TrayApplication::appendUpdateLog);
    connect(&m_service,
            &ServiceManager::notificationRequested,
            this,
            &TrayApplication::showNotification);
    connect(&m_service, &ServiceManager::updateFinished, this, [this](bool ok, const QString &message) {
        finishUpdateLogWindow(ok, message);
    });

    m_watchdog.setInterval(5000);
    connect(&m_watchdog, &QTimer::timeout, this, &TrayApplication::refresh);
}

void TrayApplication::show()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCritical().noquote() << QStringLiteral("系统托盘不可用，程序退出");
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }

    m_tray.show();
    m_watchdog.start();
    m_service.start();
    refresh();
}

void TrayApplication::buildMenu()
{
    m_openChatAction = m_menu.addAction(QStringLiteral("打开聊天页"), this, &TrayApplication::openChat);
    setMenuIcon(m_openChatAction, standardIcon(QStyle::SP_DriveNetIcon));

    m_menu.addSeparator();
    m_toggleAction = m_menu.addAction(QStringLiteral("启动服务"), this, [this]() {
        if (m_service.isRunning()) {
            m_service.stop();
        } else {
            m_service.start();
        }
        refresh();
    });
    setMenuIcon(m_toggleAction, standardIcon(QStyle::SP_MediaPlay));
    m_restartAction = m_menu.addAction(QStringLiteral("重启服务"), &m_service, &ServiceManager::restart);
    setMenuIcon(m_restartAction, standardIcon(QStyle::SP_BrowserReload));
    m_statusAction = m_menu.addAction(QStringLiteral("已停止"));
    setMenuIcon(m_statusAction, dotIcon(QColor(150, 150, 150)));
    m_statusAction->setEnabled(false);

    m_menu.addSeparator();
    m_autostartAction = m_menu.addAction(QStringLiteral("开机自启"));
    setMenuIcon(m_autostartAction, standardIcon(QStyle::SP_ComputerIcon));
    m_autostartAction->setCheckable(true);
    connect(m_autostartAction, &QAction::triggered, this, [this](bool checked) {
        QString error;
        if (!WinUtil::setAutostart(checked, &error)) {
            showNotification(QStringLiteral("开机自启设置失败"), error);
        }
        refresh();
    });

    m_configureProxyAction = m_menu.addAction(QStringLiteral("配置代理"), this, &TrayApplication::configureProxy);
    setMenuIcon(m_configureProxyAction, standardIcon(QStyle::SP_FileDialogDetailedView));
    m_proxyStatusAction = m_menu.addAction(QStringLiteral("代理: 未启用"));
    setMenuIcon(m_proxyStatusAction, dotIcon(QColor(150, 150, 150)));
    m_proxyStatusAction->setEnabled(false);
    m_viewLogsAction = m_menu.addAction(QStringLiteral("查看日志"), this, &TrayApplication::viewLogs);
    setMenuIcon(m_viewLogsAction, standardIcon(QStyle::SP_DirOpenIcon));
    m_updateAction = m_menu.addAction(QStringLiteral("更新 QwenPaw"), this, &TrayApplication::checkUpdate);
    setMenuIcon(m_updateAction, standardIcon(QStyle::SP_ArrowUp));

    m_menu.addSeparator();
    m_quitAction = m_menu.addAction(QStringLiteral("退出"), this, []() {
        qInfo().noquote() << QStringLiteral("退出托盘程序，停止所有服务");
        qApp->quit();
    });
    setMenuIcon(m_quitAction, standardIcon(QStyle::SP_DialogCloseButton));

}

void TrayApplication::refresh()
{
    const bool running = m_service.isRunning();
    const bool updating = m_service.updating() || m_checkingUpdate;
    m_toggleAction->setText(running ? QStringLiteral("停止服务") : QStringLiteral("启动服务"));
    m_toggleAction->setIcon(standardIcon(running ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
    m_toggleAction->setEnabled(!updating);
    m_restartAction->setEnabled(running && !updating);
    m_statusAction->setText(running ? QStringLiteral("运行中") : QStringLiteral("已停止"));
    m_statusAction->setIcon(dotIcon(running ? QColor(46, 160, 67) : QColor(150, 150, 150)));
    m_autostartAction->setEnabled(!updating);
    m_autostartAction->setChecked(WinUtil::autostartEnabled());
    m_configureProxyAction->setEnabled(!updating);
    m_proxyStatusAction->setText(m_service.proxyEnabled() ? QStringLiteral("代理: 已启用")
                                                          : QStringLiteral("代理: 未启用"));
    m_proxyStatusAction->setIcon(dotIcon(m_service.proxyEnabled() ? QColor(31, 111, 235)
                                                                  : QColor(150, 150, 150)));
    m_updateAction->setText(updating ? QStringLiteral("更新中...") : QStringLiteral("更新 QwenPaw"));
    m_updateAction->setEnabled(!updating);
    m_tray.setIcon(IconFactory::build(running));
}

void TrayApplication::openChat()
{
    qInfo().noquote() << QStringLiteral("打开聊天页: %1").arg(Constants::chatUrl());
    WinUtil::shellOpen(Constants::chatUrl());
}

void TrayApplication::configureProxy()
{
    qInfo().noquote() << QStringLiteral("打开代理配置");
    Config::ensureConfigFile();

    AppConfig config = Config::load();

    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("代理设置"));
    dialog.setMinimumWidth(520);
    dialog.setStyleSheet(dialogStyleSheet());

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = makeTitle(QStringLiteral("代理设置"), &dialog);
    auto *subtitle = makeSubtitle(QStringLiteral("配置会保存到本机托盘配置文件，服务和更新流程都会使用这里的代理。"), &dialog);

    auto *group = new QGroupBox(QStringLiteral("网络代理"), &dialog);
    auto *form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);

    auto *enabled = new QCheckBox(QStringLiteral("启用代理"), group);
    enabled->setChecked(config.proxyEnabled);

    auto *httpProxy = new QLineEdit(config.httpProxy, group);
    httpProxy->setPlaceholderText(QStringLiteral("http://127.0.0.1:7897"));
    httpProxy->setClearButtonEnabled(true);

    auto *httpsProxy = new QLineEdit(config.httpsProxy, group);
    httpsProxy->setPlaceholderText(QStringLiteral("http://127.0.0.1:7897"));
    httpsProxy->setClearButtonEnabled(true);

    form->addRow(QString(), enabled);
    form->addRow(QStringLiteral("HTTP"), httpProxy);
    form->addRow(QStringLiteral("HTTPS"), httpsProxy);

    auto *hint = makeSubtitle(QStringLiteral("HTTPS 代理通常也填写 HTTP 代理地址，例如 http://127.0.0.1:7897。保存后运行中的服务需要重启才会应用新环境变量。"),
                              group);
    form->addRow(QString(), hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(group);
    layout->addStretch(1);
    layout->addWidget(buttons);

    connect(enabled, &QCheckBox::toggled, &dialog, [httpProxy, httpsProxy](bool checked) {
        httpProxy->setEnabled(checked);
        httpsProxy->setEnabled(checked);
    });
    enabled->toggled(enabled->isChecked());

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, [&]() {
        AppConfig next;
        next.proxyEnabled = enabled->isChecked();
        next.httpProxy = httpProxy->text().trimmed();
        next.httpsProxy = httpsProxy->text().trimmed();

        if (next.proxyEnabled && next.httpProxy.isEmpty() && next.httpsProxy.isEmpty()) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("代理设置"),
                                 QStringLiteral("已启用代理，但 HTTP 和 HTTPS 代理都为空。请至少填写一个代理地址。"));
            return;
        }

        QString error;
        if (!Config::save(next, &error)) {
            QMessageBox::critical(&dialog,
                                  QStringLiteral("保存失败"),
                                  QStringLiteral("无法保存代理设置：%1").arg(error));
            return;
        }

        m_service.reloadConfig();
        refresh();
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (m_service.isRunning()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            nullptr,
            QStringLiteral("代理设置已保存"),
            QStringLiteral("代理设置已保存。是否现在重启服务以立即生效？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            m_service.restart();
        }
    } else {
        showNotification(QStringLiteral("代理设置已保存"), QStringLiteral("下次启动服务时会使用新的代理配置"));
    }
}

void TrayApplication::viewLogs()
{
    QDir().mkpath(Config::logDir());
    qInfo().noquote() << QStringLiteral("打开日志目录: %1").arg(Config::logDir());
    WinUtil::shellOpen(Config::logDir());
}

void TrayApplication::checkUpdate()
{
    if (m_service.updating() || m_checkingUpdate) {
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        nullptr,
        QStringLiteral("更新 QwenPaw"),
        QStringLiteral("将停止当前服务并执行 pip 更新。更新完成后会自动重启服务，是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_checkingUpdate = true;
    refresh();
    openUpdateLogWindow();
    showNotification(QStringLiteral("正在更新 QwenPaw"),
                     QStringLiteral("正在停止服务并执行 pip 更新，更新输出会显示在窗口里……"));
    m_service.startUpdate();
}

void TrayApplication::openUpdateLogWindow()
{
    if (m_updateLogDialog) {
        m_updateLogDialog->raise();
        m_updateLogDialog->activateWindow();
        return;
    }

    auto *dialog = new QDialog(nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setSizeGripEnabled(true);
    dialog->setWindowFlags(dialog->windowFlags() | Qt::WindowMaximizeButtonHint);
    dialog->setWindowTitle(QStringLiteral("更新进度"));
    dialog->setMinimumSize(720, 460);
    dialog->resize(920, 640);
    dialog->setStyleSheet(dialogStyleSheet());

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = makeTitle(QStringLiteral("更新 QwenPaw"), dialog);
    auto *subtitle = makeSubtitle(QStringLiteral("正在停止服务并执行 pip 更新，更新输出会实时显示在这里。"), dialog);
    auto *browser = new QTextEdit(dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    auto *sizeGrip = new QSizeGrip(dialog);

    browser->setReadOnly(true);
    browser->setLineWrapMode(QTextEdit::WidgetWidth);
    browser->setPlaceholderText(QStringLiteral("等待更新输出..."));
    browser->document()->setMaximumBlockCount(4000);
    browser->setStyleSheet(QStringLiteral(R"(
        QTextEdit {
            background: #0f172a;
            color: #e2e8f0;
            border-color: #1e293b;
            font-family: Consolas;
            font-size: 12px;
        }
    )"));

    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    buttons->button(QDialogButtonBox::Close)->setEnabled(false);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(browser, 1);

    auto *footer = new QHBoxLayout();
    footer->addWidget(buttons);
    footer->addStretch(1);
    footer->addWidget(sizeGrip, 0, Qt::AlignRight | Qt::AlignBottom);
    layout->addLayout(footer);

    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this]() {
        m_updateLogDialog = nullptr;
        m_updateLogView = nullptr;
    });
    connect(dialog, &QDialog::destroyed, this, [this]() {
        m_updateLogDialog = nullptr;
        m_updateLogView = nullptr;
    });

    m_updateLogDialog = dialog;
    m_updateLogView = browser;
    dialog->show();
    appendUpdateLog(QStringLiteral("[开始] python -m pip install --upgrade qwenpaw\n"), false);
}

void TrayApplication::appendUpdateLog(const QString &text, bool isError)
{
    if (!m_updateLogView) {
        return;
    }

    const QString chunk = text.isEmpty() ? QString() : text;
    if (chunk.isEmpty()) {
        return;
    }

    QTextCursor cursor(m_updateLogView->document());
    cursor.movePosition(QTextCursor::End);

    const QStringList lines = chunk.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i);
        cursor.insertText(line, updateLogFormat(line, isError));
        if (i + 1 < lines.size()) {
            cursor.insertText(QStringLiteral("\n"));
        }
    }

    m_updateLogView->setTextCursor(cursor);
    m_updateLogView->ensureCursorVisible();
}

void TrayApplication::finishUpdateLogWindow(bool ok, const QString &message)
{
    if (m_updateLogView) {
        appendUpdateLog(QStringLiteral("\n[%1] %2\n").arg(ok ? QStringLiteral("完成") : QStringLiteral("失败"), message),
                        !ok);
        m_updateLogView->ensureCursorVisible();
    }

    if (m_updateLogDialog) {
        auto *buttons = m_updateLogDialog->findChild<QDialogButtonBox *>();
        if (buttons) {
            if (auto *closeButton = buttons->button(QDialogButtonBox::Close)) {
                closeButton->setEnabled(true);
            }
        }
    }

    showNotification(ok ? QStringLiteral("更新完成") : QStringLiteral("更新失败"), message);
    m_checkingUpdate = false;
    refresh();
}

void TrayApplication::showNotification(const QString &title, const QString &message)
{
    qInfo().noquote() << QStringLiteral("通知: %1 - %2").arg(title, message);
    if (m_tray.supportsMessages()) {
        m_tray.showMessage(title, message, QSystemTrayIcon::Information, 5000);
    }
}
