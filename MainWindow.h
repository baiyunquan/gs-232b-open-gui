#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include <functional>

class QAction;
class ConnectionPanel;
class ConsolePanel;
class ControlPanel;
class QLabel;
class RotorSettingsPanel;
class QTimer;

namespace Communication {
class Transport;
}

/** @brief 使用系统样式的纯 Qt Widgets 单窗口上位机原型。 */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief 创建主页面和菜单。 */
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createMenus();
    void updateConnectionState();
    void openTransport();
    void closeTransport();
    void openWifiProvisioning();
    void openFirmwareUpgrade();
    void openAbout();
    void applyRotorModel();
    void flashFirmware(const QString &port, const QString &file);
    void sendControlCommand(const QString &command, bool sensitive = false);
    void requestCalibration(const QString &command, const QString &question);
    void openManagementKey(const std::function<void()> &continuation,
                           const std::function<void()> &cancelled = {});
    void configureWifi(const QString &ssid, const QString &password);
    void handleReceivedBytes(const QByteArray &bytes);
    void handleManagementResponse(const QByteArray &line);

    ConnectionPanel *m_connection;
    ControlPanel *m_control;
    ConsolePanel *m_console;
    RotorSettingsPanel *m_settings;
    QLabel *m_connectionStatus;
    QLabel *m_transportStatus;
    Communication::Transport *m_transport;
    QAction *m_wifiAction;
    QAction *m_upgradeAction;
    QByteArray m_receiveBuffer;
    QString m_pendingWifiSsid;
    QString m_pendingWifiPassword;
    qint64 m_pendingWifiCandidate = -1;
    qint64 m_pendingWifiRevision = -1;
    qint64 m_pendingWifiTrial = -1;
    QString m_pendingCalibrationCommand;
    QString m_pendingManagementToken;
    bool m_managementAuthenticated = false;
    std::function<void()> m_pendingManagementContinuation;
    QTimer *m_positionPollTimer;
    QTimer *m_wifiTrialPollTimer;
    QTimer *m_calibrationTimeoutTimer;
    int m_wifiTrialPollCount = 0;

    enum class WifiConfigurationStage {
        Idle,
        AwaitBegin,
        AwaitSet,
        AwaitCheck,
        AwaitApply,
        AwaitRuntime,
        AwaitConfirm,
        AwaitAbort,
    };
    WifiConfigurationStage m_wifiConfigurationStage = WifiConfigurationStage::Idle;
};
