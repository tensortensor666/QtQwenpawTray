#pragma once

#include "servicemanager.h"

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>

class QDialog;
class QTextEdit;

class TrayApplication : public QObject
{
    Q_OBJECT

public:
    explicit TrayApplication(QObject *parent = nullptr);

    void show();

private:
    void buildMenu();
    void refresh();
    void openChat();
    void configureProxy();
    void viewLogs();
    void checkUpdate();
    void openUpdateLogWindow();
    void appendUpdateLog(const QString &text, bool isError);
    void finishUpdateLogWindow(bool ok, const QString &message);
    void showNotification(const QString &title, const QString &message);

    ServiceManager m_service;
    QSystemTrayIcon m_tray;
    QMenu m_menu;
    QTimer m_watchdog;
    bool m_checkingUpdate = false;
    QPointer<QDialog> m_updateLogDialog;
    QPointer<QTextEdit> m_updateLogView;

    QAction *m_openChatAction = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_restartAction = nullptr;
    QAction *m_statusAction = nullptr;
    QAction *m_autostartAction = nullptr;
    QAction *m_configureProxyAction = nullptr;
    QAction *m_proxyStatusAction = nullptr;
    QAction *m_viewLogsAction = nullptr;
    QAction *m_updateAction = nullptr;
    QAction *m_quitAction = nullptr;
};
