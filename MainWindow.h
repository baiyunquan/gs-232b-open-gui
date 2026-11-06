#pragma once

#include <QByteArray>
#include <QJsonObject>
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
struct EthernetConfiguration;

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
    void authenticateManagement();
    void changeManagementPassword();
    void cancelManagement();
    void startApProvisioning();
    void openWifiProvisioning();
    void openNetworkPortConfig();
    void resetNetworkConfiguration();
    void openFirmwareUpgrade();
    void openAbout();
    void applyRotorModel();
    void flashFirmware(const QString &port, const QString &file);
    void sendControlCommand(const QString &command, bool sensitive = false);
    void requestCalibration(const QString &command, const QString &question);
    void runManagementAction(const std::function<void()> &action,
                             const std::function<void()> &cancelled = {});
    void openManagementKey(const std::function<void()> &continuation,
                           const std::function<void()> &cancelled = {});
    void configureWifi(const QString &ssid, const QString &password);
    void configureEthernet(const EthernetConfiguration &configuration);
    void beginNetworkConfiguration(const QJsonObject &patch, bool wifiTarget,
                                   const QString &wifiSsid = {});
    void sendManagementCommand(const QString &command, const QString &responseCommand,
                               const std::function<void()> &onSuccess = {});
    void handleReceivedBytes(const QByteArray &bytes);
    void handleManagementResponse(const QByteArray &line);

    ConnectionPanel *m_connection;
    ControlPanel *m_control;
    ConsolePanel *m_console;
    RotorSettingsPanel *m_settings;
    QLabel *m_connectionStatus;
    QLabel *m_transportStatus;
    Communication::Transport *m_transport;
    QAction *m_authenticateAction;
    QAction *m_changePasswordAction;
    QAction *m_cancelManagementAction;
    QAction *m_startApProvisioningAction;
    QAction *m_manualWifiAction;
    QAction *m_networkPortAction;
    QAction *m_resetNetworkAction;
    QAction *m_upgradeAction;
    QByteArray m_receiveBuffer;
    QString m_pendingWifiSsid;
    QJsonObject m_pendingNetworkPatch;
    qint64 m_pendingNetworkCandidate = -1;
    qint64 m_pendingNetworkRevision = -1;
    qint64 m_pendingNetworkTrial = -1;
    QString m_pendingCalibrationCommand;
    QString m_pendingManagementPassword;
    QString m_pendingManagementResponseCommand;
    bool m_managementAuthenticated = false;
    std::function<void()> m_pendingManagementContinuation;
    std::function<void()> m_pendingManagementResponseContinuation;
    QTimer *m_positionPollTimer;
    QTimer *m_wifiTrialPollTimer;
    QTimer *m_calibrationTimeoutTimer;
    int m_wifiTrialPollCount = 0;

    enum class NetworkConfigurationStage {
        Idle,
        AwaitBegin,
        AwaitSet,
        AwaitCheck,
        AwaitApply,
        AwaitRuntime,
        AwaitConfirm,
        AwaitAbort,
    };
    NetworkConfigurationStage m_networkConfigurationStage = NetworkConfigurationStage::Idle;
    bool m_pendingNetworkWifiTarget = true;
};
