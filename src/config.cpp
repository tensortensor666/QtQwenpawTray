#include "config.h"

#include "constants.h"

#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QString fallbackHome()
{
    const QString userProfile = qEnvironmentVariable("USERPROFILE");
    if (!userProfile.isEmpty()) {
        return userProfile;
    }

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty()) {
        return home;
    }

    return QDir::homePath();
}

QJsonObject configToJson(const AppConfig &config)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("proxy_enabled"), config.proxyEnabled);
    obj.insert(QStringLiteral("http_proxy"), config.httpProxy);
    obj.insert(QStringLiteral("https_proxy"), config.httpsProxy);
    return obj;
}

} // namespace

namespace Config {

QString baseDir()
{
    QString root = qEnvironmentVariable("LOCALAPPDATA");
    if (root.isEmpty()) {
        root = fallbackHome();
    }
    return QDir(root).filePath(Constants::appName());
}

QString logDir()
{
    return QDir(baseDir()).filePath(QStringLiteral("logs"));
}

QString configPath()
{
    return QDir(baseDir()).filePath(QStringLiteral("tray_config.json"));
}

QString serviceLogPath()
{
    const QString name = QStringLiteral("qwenpaw-%1.log")
                             .arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
    return QDir(logDir()).filePath(name);
}

AppConfig load()
{
    AppConfig config;

    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return config;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote() << QStringLiteral("加载配置失败: %1，使用默认配置")
                                    .arg(parseError.errorString());
        return config;
    }

    const QJsonObject obj = doc.object();
    config.proxyEnabled = obj.value(QStringLiteral("proxy_enabled")).toBool(false);
    config.httpProxy = obj.value(QStringLiteral("http_proxy")).toString();
    config.httpsProxy = obj.value(QStringLiteral("https_proxy")).toString();
    return config;
}

bool save(const AppConfig &config, QString *errorMessage)
{
    QDir dir;
    if (!dir.mkpath(baseDir())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建配置目录: %1").arg(baseDir());
        }
        return false;
    }

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const QJsonDocument doc(configToJson(config));
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.flush()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    qInfo().noquote() << QStringLiteral("配置已保存: %1").arg(configPath());
    return true;
}

void ensureConfigFile()
{
    if (QFile::exists(configPath())) {
        return;
    }

    QDir dir;
    if (!dir.mkpath(baseDir())) {
        qWarning().noquote() << QStringLiteral("无法创建配置目录: %1").arg(baseDir());
        return;
    }

    QJsonObject obj = configToJson(AppConfig{});
    obj.insert(QStringLiteral("_comment"), QStringLiteral("代理配置说明"));
    obj.insert(QStringLiteral("_example_1"), QStringLiteral("启用代理: 将 proxy_enabled 设置为 true"));
    obj.insert(QStringLiteral("_example_2"), QStringLiteral("HTTP 代理: \"http_proxy\": \"http://127.0.0.1:7897\""));
    obj.insert(QStringLiteral("_example_3"), QStringLiteral("HTTPS 代理: \"https_proxy\": \"http://127.0.0.1:7897\""));
    obj.insert(QStringLiteral("_note"), QStringLiteral("修改配置后，需要在托盘菜单中【重启服务】才能生效"));

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote() << QStringLiteral("生成配置模板失败: %1").arg(file.errorString());
        return;
    }

    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.flush()) {
        qWarning().noquote() << QStringLiteral("写入配置模板失败: %1").arg(file.errorString());
        return;
    }

    qInfo().noquote() << QStringLiteral("已生成配置模板: %1").arg(configPath());
}

} // namespace Config
