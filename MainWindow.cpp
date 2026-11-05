#include "MainWindow.h"
#include "CommunicationProtocol.h"
#include "ConnectionPanel.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "FirmwareUpgradeDialog.h"
#include "ManagementKeyDialog.h"
#include "RotorSettingsPanel.h"
#include "WifiProvisioningDialog.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(tr("GS-232B Open Control"));
    setMinimumSize(1120, 720);
    resize(1440, 880);

    auto *central = new QWidget(this);
    auto *columns = new QHBoxLayout(central);
    columns->setContentsMargins(10, 10, 10, 10);
    columns->setSpacing(12);
    auto *left = new QWidget(central);
    left->setMinimumWidth(340);
    left->setMaximumWidth(390);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);
    m_connection = new ConnectionPanel(left);
    m_control = new ControlPanel(left);
    leftLayout->addWidget(m_connection);
    leftLayout->addWidget(m_control, 1);
    columns->addWidget(left);
    m_console = new ConsolePanel(central);
    columns->addWidget(m_console, 1);
    m_settings = new RotorSettingsPanel(central);
    m_settings->setMinimumWidth(250);
    m_settings->setMaximumWidth(320);
    columns->addWidget(m_settings);
    setCentralWidget(central);

    m_transport = new Communication::Transport(this);

    m_connectionStatus = new QLabel(this);
    m_connectionStatus->setObjectName(QStringLiteral("connectionStatus"));
    m_transportStatus = new QLabel(this);
    m_transportStatus->setObjectName(QStringLiteral("transportStatus"));
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_connectionStatus, 1);
    statusBar()->addPermanentWidget(m_transportStatus);
    createMenus();
    connect(m_connection, &ConnectionPanel::stateChanged,
            this, &MainWindow::updateConnectionState);
    connect(m_connection, &ConnectionPanel::connectRequested,
            this, &MainWindow::openTransport);
    connect(m_connection, &ConnectionPanel::disconnectRequested,
            this, &MainWindow::closeTransport);
    connect(m_transport, &Communication::Transport::connected, this, [this] {
        m_connection->setConnecting(false);
        m_connection->setConnected(true);
    });
    connect(m_transport, &Communication::Transport::disconnected, this, [this] {
        m_connection->setConnecting(false);
        m_connection->setConnected(false);
    });
    connect(m_transport, &Communication::Transport::bytesReceived,
            m_console, &ConsolePanel::appendReceived);
    connect(m_transport, &Communication::Transport::bytesSent,
            m_console, &ConsolePanel::appendSent);
    connect(m_transport, &Communication::Transport::errorOccurred, this,
            [this](const QString &error) {
                m_console->appendError(tr("Transport error: %1 (AI)").arg(error));
                m_connection->setConnecting(false);
            });
    connect(m_console, &ConsolePanel::commandRequested, this,
            [this](const QString &command) {
                m_transport->sendCommand(command);
            });
    connect(m_settings, &RotorSettingsPanel::modelApplyRequested,
            this, &MainWindow::applyRotorModel);
    updateConnectionState();
}

void MainWindow::createMenus()
{
    // NOTE: 菜单固定在窗口内，所有控件和弹窗均沿用系统 Qt Widgets 样式。
    menuBar()->setNativeMenuBar(false);
    auto *files = menuBar()->addMenu(tr("Files"));
    files->addAction(tr("Exit"), qApp, &QCoreApplication::quit);
    auto *network = menuBar()->addMenu(tr("Network"));
    m_wifiAction = network->addAction(tr("Wi-Fi provisioning..."),
                                     this, &MainWindow::openWifiProvisioning);
    m_wifiAction->setObjectName(QStringLiteral("wifiAction"));
    menuBar()->addMenu(tr("Bridge"));
    auto *upgrade = menuBar()->addMenu(tr("Upgrade"));
    m_upgradeAction = upgrade->addAction(tr("Flash firmware..."),
                                        this, &MainWindow::openFirmwareUpgrade);
    m_upgradeAction->setObjectName(QStringLiteral("upgradeAction"));
    auto *about = menuBar()->addMenu(tr("About"));
    auto *aboutAction = about->addAction(tr("GS-232B Open Control"),
                                        this, &MainWindow::openAbout);
    aboutAction->setObjectName(QStringLiteral("aboutAction"));
}

void MainWindow::updateConnectionState()
{
    const bool virtualCom = m_connection->isVirtualComEnabled();
    const bool commandEnabled = m_connection->isConnected() && !virtualCom;
    m_control->setCommandEnabled(commandEnabled);
    m_console->setCommandEnabled(commandEnabled);
    m_settings->setCommandEnabled(commandEnabled);
    m_wifiAction->setEnabled(!virtualCom);
    m_upgradeAction->setEnabled(!virtualCom);
    m_connectionStatus->setText(m_connection->isConnected() ? tr("Connected") : tr("Disconnected"));
    m_transportStatus->setText(m_connection->isNetworkMode() ? tr("WebSocket") : tr("Serial"));
    if (!m_connection->isConnected())
        m_control->setPosition(0, 0);
}

void MainWindow::openTransport()
{
    if (m_transport->isOpen())
        return;

    m_connection->setConnecting(true);
    // NOTE: 主页面终端固定使用控制端点；管理端点由协议模块保留给后续独立调用方。
    const auto options = m_connection->connectionOptions(Communication::WebSocketEndpoint::Gs232b);
    if (!m_transport->open(options))
        m_connection->setConnecting(false);
}

void MainWindow::closeTransport()
{
    m_transport->close();
    m_connection->setConnecting(false);
    m_connection->setConnected(false);
}

void MainWindow::openWifiProvisioning()
{
    auto *dialog = new WifiProvisioningDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setConnectionState(m_connection->isConnected() && !m_connection->isVirtualComEnabled(),
                               m_connection->isNetworkMode());
    dialog->open();
}

void MainWindow::openFirmwareUpgrade()
{
    auto *dialog = new FirmwareUpgradeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::openAbout()
{
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("aboutDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("About"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->resize(360, 150);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(new QLabel(tr("GS-232B Open Control"), dialog));
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(dialog);
    buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog->open();
}

void MainWindow::applyRotorModel()
{
    if (m_connection->isNetworkMode()) {
        auto *dialog = new ManagementKeyDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    }
    // TODO: UART 直接提交 ROTOR SET；网络已认证会话直接提交，否则按需认证后再提交。
}
