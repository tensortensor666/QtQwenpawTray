#pragma once

#include <QString>

struct AppConfig {
    bool proxyEnabled = false;
    QString httpProxy;
    QString httpsProxy;
};

namespace Config {

QString baseDir();
QString logDir();
QString configPath();
QString serviceLogPath();

AppConfig load();
bool save(const AppConfig &config, QString *errorMessage = nullptr);
void ensureConfigFile();

} // namespace Config

