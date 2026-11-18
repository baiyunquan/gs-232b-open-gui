#pragma once

#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

// Keep production's native settings backend, but give each test process its own
// storage. QSettings::setPath(NativeFormat, ...) alone cannot redirect Windows.
class TestSettings : public QTemporaryDir
{
public:
    TestSettings()
    {
#ifdef Q_OS_WIN
        m_registryPath = QStringLiteral("Software\\GS232B-Test-") +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_createResult = RegCreateKeyExW(HKEY_CURRENT_USER,
                reinterpret_cast<LPCWSTR>(m_registryPath.utf16()), 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &m_key, nullptr);
        if (m_createResult == ERROR_SUCCESS)
            m_overrideResult = RegOverridePredefKey(HKEY_CURRENT_USER, m_key);
        m_valid = QTemporaryDir::isValid() && m_createResult == ERROR_SUCCESS && m_overrideResult == ERROR_SUCCESS;
#else
        m_valid = QTemporaryDir::isValid();
#endif
    }

    ~TestSettings()
    {
#ifdef Q_OS_WIN
        if (m_valid)
            RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        if (m_key) {
            RegCloseKey(m_key);
            RegDeleteTreeW(HKEY_CURRENT_USER,
                          reinterpret_cast<LPCWSTR>(m_registryPath.utf16()));
        }
#endif
    }

    bool isValid() const
    {
#ifdef Q_OS_WIN
        if (!m_valid)
            qWarning("Test settings: temporary directory=%d, create=%ld, override=%ld",
                     QTemporaryDir::isValid(), m_createResult, m_overrideResult);
#endif
        return m_valid;
    }

    void reset()
    {
        Q_ASSERT(m_valid);
        const QString directory = filePath(QString::number(++m_case));
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory);
        QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, directory);
#ifdef Q_OS_WIN
        QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                           QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
        settings.clear();
        settings.sync();
        if (settings.status() != QSettings::NoError)
            qFatal("Cannot reset isolated test settings");
#endif
    }

private:
    bool m_valid = false;
    int m_case = 0;
#ifdef Q_OS_WIN
    QString m_registryPath;
    HKEY m_key = nullptr;
    LONG m_createResult = -1;
    LONG m_overrideResult = -1;
#endif
};
