#pragma once

#include <QMainWindow>

class QAction;
class ConnectionPanel;
class ConsolePanel;
class ControlPanel;
class QLabel;
class RotorSettingsPanel;

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

    ConnectionPanel *m_connection;
    ControlPanel *m_control;
    ConsolePanel *m_console;
    RotorSettingsPanel *m_settings;
    QLabel *m_connectionStatus;
    QLabel *m_transportStatus;
    Communication::Transport *m_transport;
    QAction *m_wifiAction;
    QAction *m_upgradeAction;
};
