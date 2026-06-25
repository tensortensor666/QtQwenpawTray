#pragma once

#include <QString>

class SingleInstance
{
public:
    explicit SingleInstance(const QString &name);
    ~SingleInstance();

    SingleInstance(const SingleInstance &) = delete;
    SingleInstance &operator=(const SingleInstance &) = delete;

    bool isPrimary() const;

private:
    bool m_primary = true;
#ifdef Q_OS_WIN
    void *m_handle = nullptr;
#endif
};

namespace WinUtil {

QString autostartCommand();
bool autostartEnabled();
bool setAutostart(bool enable, QString *errorMessage = nullptr);
void shellOpen(const QString &target);

} // namespace WinUtil

