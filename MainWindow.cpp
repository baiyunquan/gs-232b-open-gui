#include "MainWindow.h"
#include "CommunicationProtocol.h"
#include "ConnectionPanel.h"
#include "ConsolePanel.h"
#include "ControlPanel.h"
#include "FaultStatusMonitor.h"
#include "FaultStatusPanel.h"
#include "FirmwareUpgradeDialog.h"
#include "ManagementKeyDialog.h"
#include "NetworkPortConfigDialog.h"
#include "PasswordChangeDialog.h"
#include "RotorSettingsPanel.h"
#include "WifiProvisioningDialog.h"
#include "virtual_serial/VirtualSerialBridge.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString managementAuthCommand(const QString &password)
{
    QJsonObject request;
    request.insert(QStringLiteral("password"), password);
    return QStringLiteral("!SESSION AUTH %1")
        .arg(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

QString modelSetCommand(int modelId)
{
    QJsonObject request;
    request.insert(QStringLiteral("model_id"), modelId);
    return QStringLiteral("!ROTOR SET %1")
        .arg(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

} // namespace

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
    auto *right = new QWidget(central);
    right->setMinimumWidth(250);
    // NOTE: 侧栏按内容尺寸限制宽度，避免硬性上限小于布局最小宽度时低估换行文字的高度。
    right->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    m_settings = new RotorSettingsPanel(right);
    m_faultStatus = new FaultStatusMonitor(this);
    rightLayout->addWidget(m_settings);
    rightLayout->addWidget(new FaultStatusPanel(m_faultStatus, right));
    rightLayout->addStretch(1);
    columns->addWidget(right);
    setCentralWidget(central);

    m_transport = new Communication::Transport(this);
    m_positionPollTimer = new QTimer(this);
    m_positionPollTimer->setInterval(1000);
    m_wifiTrialPollTimer = new QTimer(this);
    m_wifiTrialPollTimer->setInterval(1000);
    m_calibrationTimeoutTimer = new QTimer(this);
    m_calibrationTimeoutTimer->setSingleShot(true);
    m_calibrationTimeoutTimer->setInterval(60000);

    m_connectionStatus = new QLabel(this);
    m_connectionStatus->setObjectName(QStringLiteral("connectionStatus"));
    m_transportStatus = new QLabel(this);
    m_transportStatus->setObjectName(QStringLiteral("transportStatus"));
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_connectionStatus, 1);
    statusBar()->addPermanentWidget(m_transportStatus);
    createMenus();
    setupVirtualSerial();
    connect(m_connection, &ConnectionPanel::stateChanged,
            this, &MainWindow::updateConnectionState);
    connect(m_connection, &ConnectionPanel::connectRequested,
            this, &MainWindow::openTransport);
    connect(m_connection, &ConnectionPanel::disconnectRequested,
            this, &MainWindow::closeTransport);
    connect(m_transport, &Communication::Transport::connected, this, [this] {
        if (m_connection->isVirtualComEnabled() &&
            !m_virtualSerial->start(m_connection->virtualSerialOptions())) {
            closeTransport();
            return;
        }
        m_connection->setConnecting(false);
        if (!m_connection->isVirtualComEnabled())
            sendControlCommand(QStringLiteral("!ROTOR GET"));
        m_connection->setConnected(true);
        if (!m_connection->isVirtualComEnabled()) {
            sendControlCommand(QStringLiteral("C2"));
            m_positionPollTimer->start();
        }
    });
    connect(m_transport, &Communication::Transport::disconnected, this, [this] {
        m_virtualSerial->stop();
        m_connection->setVirtualSerialPort({});
        m_connection->setConnecting(false);
        m_connection->setConnected(false);
        m_positionPollTimer->stop();
        m_wifiTrialPollTimer->stop();
        m_calibrationTimeoutTimer->stop();
        m_receiveBuffer.clear();
        m_consoleForwardedBytes = 0;
        m_pendingCalibrationCommand.clear();
        m_faultStatus->setPaused(false);
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingNetworkWifiTarget = true;
        m_pendingWifiSsid.clear();
        m_pendingManagementPassword.clear();
        m_pendingManagementResponseCommand.clear();
        m_managementAuthenticated = false;
        m_pendingManagementContinuation = {};
        m_pendingManagementResponseContinuation = {};
    });
    connect(m_transport, &Communication::Transport::bytesReceived,
            this, &MainWindow::handleReceivedBytes);
    connect(m_transport, &Communication::Transport::bytesSent,
            m_console, &ConsolePanel::appendSent);
    connect(m_transport, &Communication::Transport::errorOccurred, this,
            [this](const QString &error) {
                m_console->appendError(tr("Transport error: %1").arg(error));
                m_connection->setConnecting(false);
            });
    connect(m_console, &ConsolePanel::commandRequested, this,
            [this](const QString &command) {
                sendControlCommand(command);
            });
    connect(m_control, &ControlPanel::commandRequested,
            this, [this](const QString &command) { sendControlCommand(command); });
    connect(m_settings, &RotorSettingsPanel::commandRequested,
            this, [this](const QString &command) { sendControlCommand(command); });
    connect(m_faultStatus, &FaultStatusMonitor::commandRequested,
            this, [this](const QString &command) { sendControlCommand(command); });
    connect(m_settings, &RotorSettingsPanel::modelApplyRequested,
            this, &MainWindow::applyRotorModel);
    connect(m_settings, &RotorSettingsPanel::azimuthZeroCalibrationRequested,
            this, [this] {
                requestCalibration(
                    QStringLiteral("O"),
                    tr("Move the azimuth to the counter-clockwise limit, then continue."));
            });
    connect(m_settings, &RotorSettingsPanel::elevationZeroCalibrationRequested,
            this, [this] {
                requestCalibration(
                    QStringLiteral("O2"),
                    tr("Move the elevation to 0 degrees, then continue."));
            });
    connect(m_positionPollTimer, &QTimer::timeout, this, [this] {
        sendControlCommand(QStringLiteral("C2"));
    });
    connect(m_wifiTrialPollTimer, &QTimer::timeout, this, [this] {
        if (m_networkConfigurationStage != NetworkConfigurationStage::AwaitRuntime ||
            m_pendingNetworkTrial < 0) {
            m_wifiTrialPollTimer->stop();
            return;
        }
        if (++m_wifiTrialPollCount > 300) {
            m_wifiTrialPollTimer->stop();
            m_console->appendError(tr("Network configuration trial timed out; the device will roll it back."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }
        sendControlCommand(QStringLiteral("!NET GET"), true);
    });
    connect(m_calibrationTimeoutTimer, &QTimer::timeout, this, [this] {
        m_pendingCalibrationCommand.clear();
        m_faultStatus->setPaused(false);
        if (m_transport->isOpen())
            m_positionPollTimer->start();
    });
    updateConnectionState();
}

MainWindow::~MainWindow()
{
    // NOTE: 先关闭两侧连接，避免 QObject 子对象析构时回调已销毁的界面。
    const QSignalBlocker transportBlocker(m_transport);
    const QSignalBlocker virtualSerialBlocker(m_virtualSerial);
    m_transport->close();
    m_virtualSerial->stop();
}

void MainWindow::setupVirtualSerial()
{
    m_virtualSerial = new VirtualSerialBridge(this);
    connect(m_virtualSerial, &VirtualSerialBridge::started, this, [this](const QString &port) {
        m_connection->setVirtualSerialPort(port);
        m_console->appendNotice(tr("Virtual serial port ready: %1").arg(port));
    });
    connect(m_virtualSerial, &VirtualSerialBridge::webSocketLineReady, this,
            [this](const QByteArray &line) {
                // NOTE: 直接发送带 CR 的原始字节，不经过终端命令的 UTF-8 编码。
                const bool reportAsSent = !line.trimmed().startsWith('!');
                if (m_transport->isOpen() && !m_transport->sendBytes(line, reportAsSent))
                    closeTransport();
            });
    connect(m_virtualSerial, &VirtualSerialBridge::errorOccurred, this,
            [this](const QString &error) {
                m_console->appendError(tr("Virtual serial error: %1").arg(error));
            });
    connect(m_virtualSerial, &VirtualSerialBridge::stopped, this, &MainWindow::closeTransport);
}

void MainWindow::createMenus()
{
    // NOTE: 菜单固定在窗口内，所有控件和弹窗均沿用系统 Qt Widgets 样式。
    menuBar()->setNativeMenuBar(false);
    auto *files = menuBar()->addMenu(tr("Files"));
    files->addAction(tr("Exit"), qApp, &QCoreApplication::quit);
    auto *management = menuBar()->addMenu(tr("Management"));
    m_authenticateAction = management->addAction(
        tr("Authenticate"), this, &MainWindow::authenticateManagement);
    m_authenticateAction->setObjectName(QStringLiteral("authenticateAction"));
    m_changePasswordAction = management->addAction(
        tr("Change password"), this, &MainWindow::changeManagementPassword);
    m_changePasswordAction->setObjectName(QStringLiteral("changePasswordAction"));
    m_cancelManagementAction = management->addAction(
        tr("Cancel management"), this, &MainWindow::cancelManagement);
    m_cancelManagementAction->setObjectName(QStringLiteral("cancelManagementAction"));
    auto *wifiMenu = management->addMenu(tr("Wi-Fi provisioning"));
    m_startApProvisioningAction = wifiMenu->addAction(
        tr("Start AP provisioning"), this, &MainWindow::startApProvisioning);
    m_startApProvisioningAction->setObjectName(QStringLiteral("startApProvisioningAction"));
    m_manualWifiAction = wifiMenu->addAction(
        tr("Manual provisioning"), this, &MainWindow::openWifiProvisioning);
    m_manualWifiAction->setObjectName(QStringLiteral("wifiAction"));
    m_networkPortAction = management->addAction(
        tr("Network port configuration"), this, &MainWindow::openNetworkPortConfig);
    m_networkPortAction->setObjectName(QStringLiteral("networkPortAction"));
    m_resetNetworkAction = management->addAction(
        tr("Reset network configuration"), this, &MainWindow::resetNetworkConfiguration);
    m_resetNetworkAction->setObjectName(QStringLiteral("resetNetworkAction"));
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
    m_faultStatus->setEnabled(commandEnabled);
    const bool managementEnabled = m_connection->isConnected() && !virtualCom;
    m_authenticateAction->setEnabled(managementEnabled);
    m_changePasswordAction->setEnabled(managementEnabled);
    m_cancelManagementAction->setEnabled(managementEnabled);
    m_startApProvisioningAction->setEnabled(managementEnabled);
    m_manualWifiAction->setEnabled(managementEnabled);
    m_networkPortAction->setEnabled(managementEnabled);
    m_resetNetworkAction->setEnabled(managementEnabled);
    m_upgradeAction->setEnabled(!virtualCom);
    if (commandEnabled)
        m_positionPollTimer->start();
    else
        m_positionPollTimer->stop();
    m_connectionStatus->setText(m_connection->isConnected() ? tr("Connected") : tr("Disconnected"));
    m_transportStatus->setText(m_connection->isNetworkMode() ? tr("WebSocket") : tr("Serial"));
    if (!m_connection->isConnected()) {
        m_control->setPosition(0, 0);
        m_settings->clearModelSelection();
    }
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
    m_virtualSerial->stop();
    m_connection->setVirtualSerialPort({});
    m_connection->setConnecting(false);
    m_connection->setConnected(false);
}

void MainWindow::authenticateManagement()
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;
    if (!m_connection->isNetworkMode()) {
        QMessageBox::information(this, tr("Management authentication"),
                                 tr("Serial connections do not require management authentication."));
        return;
    }
    openManagementKey([this] {
        m_console->appendNotice(tr("Management authentication succeeded."));
    });
}

void MainWindow::changeManagementPassword()
{
    runManagementAction([this] {
        auto *dialog = new PasswordChangeDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QDialog::accepted, this, [this, dialog] {
            QJsonObject request;
            request.insert(QStringLiteral("password"), dialog->password());
            sendManagementCommand(
                QStringLiteral("!ADMIN SET %1")
                    .arg(QString::fromUtf8(
                        QJsonDocument(request).toJson(QJsonDocument::Compact))),
                QStringLiteral("ADMIN SET"), [this](const QJsonObject &) {
                    m_managementAuthenticated = false;
                    m_wifiTrialPollTimer->stop();
                    m_networkConfigurationStage = NetworkConfigurationStage::Idle;
                    m_pendingNetworkPatch = {};
                    m_pendingWifiSsid.clear();
                    m_console->appendNotice(tr("Management password changed."));
                });
        });
        dialog->open();
    });
}

void MainWindow::cancelManagement()
{
    runManagementAction([this] {
        sendManagementCommand(QStringLiteral("!ADMIN DISABLE"), QStringLiteral("ADMIN DISABLE"),
                              [this](const QJsonObject &) {
                                  m_managementAuthenticated = false;
                                  m_wifiTrialPollTimer->stop();
                                  m_networkConfigurationStage = NetworkConfigurationStage::Idle;
                                  m_pendingNetworkPatch = {};
                                  m_pendingWifiSsid.clear();
                                  m_console->appendNotice(tr("Management authentication disabled."));
                              });
    });
}

void MainWindow::startApProvisioning()
{
    runManagementAction([this] {
        sendManagementCommand(
            QStringLiteral("!PROV START"), QStringLiteral("PROV START"),
            [this](const QJsonObject &data) {
                const QJsonValue remaining = data.value(QStringLiteral("remaining_s"));
                const int seconds = remaining.toInt(-1);
                if (remaining.isDouble() && seconds >= 0 && remaining.toDouble() == seconds) {
                    m_console->appendNotice(
                        tr("AP provisioning started. Window remaining: %1 s.").arg(seconds));
                } else {
                    m_console->appendNotice(tr("AP provisioning started."));
                }
            });
    });
}

void MainWindow::openWifiProvisioning()
{
    runManagementAction([this] {
        auto *dialog = new WifiProvisioningDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &WifiProvisioningDialog::configurationRequested,
                this, &MainWindow::configureWifi);
        dialog->setConnectionState(m_connection->isConnected() && !m_connection->isVirtualComEnabled());
        dialog->open();
    });
}

void MainWindow::openNetworkPortConfig()
{
    runManagementAction([this] {
        auto *dialog = new NetworkPortConfigDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &NetworkPortConfigDialog::configurationRequested,
                this, &MainWindow::configureEthernet);
        dialog->setConnectionState(m_connection->isConnected() && !m_connection->isVirtualComEnabled());
        dialog->open();
    });
}

void MainWindow::resetNetworkConfiguration()
{
    runManagementAction([this] {
        if (QMessageBox::question(
                this, tr("Reset network configuration"),
                tr("Reset Ethernet and Wi-Fi settings and reopen AP provisioning?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        QJsonObject request;
        request.insert(QStringLiteral("confirm"), QStringLiteral("NETWORK_ONLY"));
        sendManagementCommand(
            QStringLiteral("!NET RESET %1")
                .arg(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))),
            QStringLiteral("NET RESET"), [this](const QJsonObject &) {
                m_managementAuthenticated = false;
                m_wifiTrialPollTimer->stop();
                m_networkConfigurationStage = NetworkConfigurationStage::Idle;
                m_pendingNetworkPatch = {};
                m_pendingWifiSsid.clear();
                m_console->appendNotice(tr("Network configuration reset."));
            });
    });
}

void MainWindow::openFirmwareUpgrade()
{
    auto *dialog = new FirmwareUpgradeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &FirmwareUpgradeDialog::flashRequested,
            this, &MainWindow::flashFirmware);
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
    const QString command = modelSetCommand(m_settings->selectedModelId());
    if (m_connection->isNetworkMode()) {
        openManagementKey([this, command] { sendControlCommand(command); });
        return;
    }
    sendControlCommand(command);
}

void MainWindow::flashFirmware(const QString &port, const QString &file)
{
    if (port.isEmpty() || file.isEmpty())
        return;

    if (!m_connection->isNetworkMode() &&
        m_connection->connectionOptions().serialPort == port && m_transport->isOpen())
        closeTransport();

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        const QByteArray output = process->readAllStandardOutput();
        if (!output.isEmpty())
            m_console->appendProcessOutput(output);
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
                m_console->appendError(tr("Firmware flashing process failed to start."));
                if (error == QProcess::FailedToStart)
                    process->deleteLater();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && exitCode == 0) {
                    m_console->appendNotice(tr("Firmware flashing completed."));
                } else {
                    m_console->appendError(
                        tr("Firmware flashing failed with exit code %1.").arg(exitCode));
                }
                process->deleteLater();
            });

    const QStringList arguments = {
        QStringLiteral("--chip"), QStringLiteral("esp32c3"),
        QStringLiteral("--port"), port,
        QStringLiteral("--baud"), QStringLiteral("460800"),
        QStringLiteral("write-flash"), QStringLiteral("0x10000"), file};
    process->start(QStringLiteral("esptool"), arguments);
}

void MainWindow::sendControlCommand(const QString &command, bool sensitive)
{
    if (command.trimmed().isEmpty() || !m_transport->isOpen())
        return;
    if (m_connection->isVirtualComEnabled()) {
        m_console->appendError(tr("Direct device commands are disabled in Virtual COM mode."));
        return;
    }
    const bool redact = sensitive || command.trimmed().startsWith(QLatin1Char('!'));
    m_transport->sendCommand(command, !redact);
}

void MainWindow::requestCalibration(const QString &command, const QString &question)
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;
    if (QMessageBox::question(this, tr("Calibration confirmation"), question,
                              QMessageBox::Ok | QMessageBox::Cancel,
                              QMessageBox::Ok) == QMessageBox::Ok) {
        m_pendingCalibrationCommand = command;
        m_faultStatus->setPaused(true);
        m_positionPollTimer->stop();
        m_calibrationTimeoutTimer->start();
        sendControlCommand(command);
    }
}

void MainWindow::runManagementAction(const std::function<void()> &action,
                                     const std::function<void()> &cancelled)
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;
    if (m_connection->isNetworkMode())
        openManagementKey(action, cancelled);
    else
        action();
}

void MainWindow::openManagementKey(const std::function<void()> &continuation,
                                   const std::function<void()> &cancelled)
{
    if (m_managementAuthenticated) {
        continuation();
        return;
    }

    auto *dialog = new ManagementKeyDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog, continuation] {
        const QString password = dialog->password();
        if (password.isEmpty())
            return;
        m_pendingManagementPassword = password;
        m_pendingManagementResponseCommand.clear();
        m_pendingManagementResponseContinuation = {};
        m_pendingManagementContinuation = continuation;
        sendControlCommand(managementAuthCommand(password), true);
    });
    connect(dialog, &QDialog::rejected, this, [cancelled] {
        if (cancelled)
            cancelled();
    });
    dialog->open();
}

void MainWindow::sendManagementCommand(
    const QString &command, const QString &responseCommand,
    const std::function<void(const QJsonObject &)> &onSuccess)
{
    if (command.trimmed().isEmpty() || responseCommand.trimmed().isEmpty())
        return;
    m_pendingManagementResponseCommand = responseCommand.toUpper();
    m_pendingManagementResponseContinuation = onSuccess;
    sendControlCommand(command, true);
}

void MainWindow::configureWifi(const QString &ssid, const QString &password)
{
    if (ssid.isEmpty() || !m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    QJsonObject wifi;
    wifi.insert(QStringLiteral("enabled"), true);
    wifi.insert(QStringLiteral("ssid"), ssid);
    wifi.insert(QStringLiteral("password"), password);
    wifi.insert(QStringLiteral("security"), password.isEmpty()
                                             ? QStringLiteral("open")
                                             : QStringLiteral("auto"));
    QJsonObject patch;
    patch.insert(QStringLiteral("wifi"), wifi);
    beginNetworkConfiguration(patch, true, ssid);
}

void MainWindow::configureEthernet(const EthernetConfiguration &configuration)
{
    if (!m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    QJsonObject ethernet;
    ethernet.insert(QStringLiteral("mode"), configuration.mode);
    if (configuration.mode == QStringLiteral("static")) {
        ethernet.insert(QStringLiteral("address"), configuration.address);
        ethernet.insert(QStringLiteral("netmask"), configuration.netmask);
        const auto insertIfSet = [&ethernet](const QString &name, const QString &value) {
            if (!value.isEmpty())
                ethernet.insert(name, value);
        };
        insertIfSet(QStringLiteral("gateway"), configuration.gateway);
        insertIfSet(QStringLiteral("dns1"), configuration.dns1);
        insertIfSet(QStringLiteral("dns2"), configuration.dns2);
    }
    QJsonObject patch;
    patch.insert(QStringLiteral("eth"), ethernet);
    beginNetworkConfiguration(patch, false);
}

void MainWindow::beginNetworkConfiguration(const QJsonObject &patch, bool wifiTarget,
                                           const QString &wifiSsid)
{
    if (patch.isEmpty() || !m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    m_pendingNetworkPatch = patch;
    m_pendingNetworkWifiTarget = wifiTarget;
    m_pendingWifiSsid = wifiSsid;
    m_pendingNetworkCandidate = -1;
    m_pendingNetworkRevision = -1;
    m_pendingNetworkTrial = -1;

    const auto begin = [this] {
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitBegin;
        sendControlCommand(QStringLiteral("!CFG BEGIN"), true);
    };
    const auto cancel = [this] {
        m_pendingWifiSsid.clear();
        m_pendingNetworkPatch = {};
        m_pendingNetworkWifiTarget = true;
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
    };
    runManagementAction(begin, cancel);
}

void MainWindow::handleReceivedBytes(const QByteArray &bytes)
{
    if (m_connection->isVirtualComEnabled()) {
        if (!m_virtualSerial->feedWebSocketBytes(bytes))
            closeTransport();
        m_console->appendReceived(bytes);
        return;
    }
    m_receiveBuffer.append(bytes);
    constexpr qsizetype maximumBuffer = 64 * 1024;
    if (m_receiveBuffer.size() > maximumBuffer) {
        m_console->appendReceived(m_receiveBuffer.mid(m_consoleForwardedBytes));
        m_receiveBuffer.clear();
        m_consoleForwardedBytes = 0;
        return;
    }

    qsizetype newline = -1;
    while ((newline = m_receiveBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_receiveBuffer.left(newline);
        m_receiveBuffer.remove(0, newline + 1);
        const auto response = QJsonDocument::fromJson(line.trimmed().mid(1));
        const bool faultResponse = line.trimmed().startsWith('!') && response.isObject() &&
                                   m_faultStatus->handleResponse(response.object());
        // NOTE: 自动故障查询只更新状态面板，避免原始诊断字段淹没控制台。
        if (!faultResponse)
            m_console->appendReceived((line + '\n').mid(m_consoleForwardedBytes));
        m_consoleForwardedBytes = 0;
        handleManagementResponse(line);

        const QString text = QString::fromUtf8(line).trimmed();
        if (!m_pendingCalibrationCommand.isEmpty() &&
            text.contains(QStringLiteral("are you sure?"), Qt::CaseInsensitive)) {
            const auto answer = QMessageBox::question(
                this, tr("Confirm calibration"),
                tr("The device is ready to store this calibration point. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                sendControlCommand(QStringLiteral("YES"));
                m_calibrationTimeoutTimer->start();
            } else {
                sendControlCommand(QStringLiteral("NO"));
                m_pendingCalibrationCommand.clear();
                m_faultStatus->setPaused(false);
                m_calibrationTimeoutTimer->stop();
                if (m_transport->isOpen())
                    m_positionPollTimer->start();
            }
        } else if (!m_pendingCalibrationCommand.isEmpty() &&
                   (text.contains(QStringLiteral("Completed"), Qt::CaseInsensitive) ||
                    text.startsWith(QStringLiteral("?>")))) {
            m_pendingCalibrationCommand.clear();
            m_faultStatus->setPaused(false);
            m_calibrationTimeoutTimer->stop();
            if (m_transport->isOpen())
                m_positionPollTimer->start();
        }
        static const QRegularExpression azimuthElevation(
            QStringLiteral("^AZ\\s*=\\s*(\\d{1,3})\\s+EL\\s*=\\s*(\\d{1,3})$"));
        static const QRegularExpression azimuthOnly(
            QStringLiteral("^AZ\\s*=\\s*(\\d{1,3})$"));
        static const QRegularExpression elevationOnly(
            QStringLiteral("^EL\\s*=\\s*(\\d{1,3})$"));
        const auto azElMatch = azimuthElevation.match(text);
        if (azElMatch.hasMatch()) {
            m_control->setPosition(azElMatch.captured(1).toInt(),
                                   azElMatch.captured(2).toInt());
        } else if (const auto azMatch = azimuthOnly.match(text); azMatch.hasMatch()) {
            m_control->setPosition(azMatch.captured(1).toInt(), m_control->elevation());
        } else if (const auto elMatch = elevationOnly.match(text); elMatch.hasMatch()) {
            m_control->setPosition(m_control->azimuth(), elMatch.captured(1).toInt());
        }
    }
    // NOTE: 仅缓冲扩展响应以便按命令过滤；普通终端输出仍随字节到达立即显示。
    if (!m_receiveBuffer.trimmed().startsWith('!')) {
        m_console->appendReceived(m_receiveBuffer.mid(m_consoleForwardedBytes));
        m_consoleForwardedBytes = m_receiveBuffer.size();
    }
}

void MainWindow::handleManagementResponse(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (!trimmed.startsWith('!'))
        return;

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(trimmed.mid(1), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    const QJsonObject response = document.object();
    const QString command = response.value(QStringLiteral("cmd")).toString().toUpper();
    const bool ok = response.value(QStringLiteral("ok")).toBool();
    if (command == QStringLiteral("ROTOR GET") || command == QStringLiteral("ROTOR SET")) {
        if (ok && m_connection->isConnected() && !m_connection->isVirtualComEnabled()) {
            // NOTE: model_id 是已保存选择；active_model_id 可能仍是重启前的旧型号。
            const QJsonValue modelId = response.value(QStringLiteral("data")).toObject()
                                          .value(QStringLiteral("model_id"));
            const int id = modelId.toInt(-1);
            if (modelId.isDouble() && modelId.toDouble() == id)
                m_settings->setSelectedModelId(id);
        }
    }
    if (!ok && response.value(QStringLiteral("error")).toString() ==
                  QStringLiteral("NOT_AUTHORIZED")) {
        m_managementAuthenticated = false;
        m_pendingManagementPassword.clear();
        m_pendingManagementContinuation = {};
    }
    if (command == QStringLiteral("SESSION AUTH")) {
        m_managementAuthenticated = ok;
        m_pendingManagementPassword.clear();
        if (m_managementAuthenticated) {
            const auto continuation = m_pendingManagementContinuation;
            m_pendingManagementContinuation = {};
            if (continuation)
                continuation();
        } else {
            m_pendingManagementContinuation = {};
            m_pendingManagementResponseCommand.clear();
            m_pendingManagementResponseContinuation = {};
            m_console->appendError(tr("Management authentication failed."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }
    }

    if (!m_pendingManagementResponseCommand.isEmpty() &&
        command == m_pendingManagementResponseCommand) {
        const auto continuation = m_pendingManagementResponseContinuation;
        m_pendingManagementResponseCommand.clear();
        m_pendingManagementResponseContinuation = {};
        if (!ok) {
            m_console->appendError(
                tr("Management command failed: %1")
                    .arg(response.value(QStringLiteral("error")).toString()));
            return;
        }
        if (continuation)
            continuation(response.value(QStringLiteral("data")).toObject());
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::Idle)
        return;

    QString expectedCommand;
    switch (m_networkConfigurationStage) {
    case NetworkConfigurationStage::AwaitBegin: expectedCommand = QStringLiteral("CFG BEGIN"); break;
    case NetworkConfigurationStage::AwaitSet: expectedCommand = QStringLiteral("NET SET"); break;
    case NetworkConfigurationStage::AwaitCheck: expectedCommand = QStringLiteral("CFG CHECK"); break;
    case NetworkConfigurationStage::AwaitApply: expectedCommand = QStringLiteral("CFG APPLY"); break;
    case NetworkConfigurationStage::AwaitRuntime: expectedCommand = QStringLiteral("NET GET"); break;
    case NetworkConfigurationStage::AwaitConfirm: expectedCommand = QStringLiteral("CFG CONFIRM"); break;
    case NetworkConfigurationStage::AwaitAbort: expectedCommand = QStringLiteral("CFG ABORT"); break;
    case NetworkConfigurationStage::Idle: return;
    }
    if (command != expectedCommand)
        return;

    if (!ok) {
        m_console->appendError(
            tr("Network configuration failed: %1")
                .arg(response.value(QStringLiteral("error")).toString()));
        m_wifiTrialPollTimer->stop();
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingWifiSsid.clear();
        return;
    }

    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitBegin &&
        command == QStringLiteral("CFG BEGIN")) {
        m_pendingNetworkCandidate = data.value(QStringLiteral("candidate_id")).toInteger(-1);
        m_pendingNetworkRevision = data.value(QStringLiteral("base_revision")).toInteger(-1);
        if (m_pendingNetworkCandidate < 0) {
            m_console->appendError(tr("Device did not return a network candidate ID."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }

        m_networkConfigurationStage = NetworkConfigurationStage::AwaitSet;
        sendControlCommand(QStringLiteral("!NET SET %1")
                               .arg(QString::fromUtf8(
                                   QJsonDocument(m_pendingNetworkPatch)
                                       .toJson(QJsonDocument::Compact))), true);
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitSet &&
        command == QStringLiteral("NET SET")) {
        m_pendingNetworkPatch = {};
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitCheck;
        sendControlCommand(QStringLiteral("!CFG CHECK"), true);
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitCheck &&
        command == QStringLiteral("CFG CHECK")) {
        QJsonObject request;
        request.insert(QStringLiteral("candidate_id"), m_pendingNetworkCandidate);
        if (m_pendingNetworkRevision >= 0)
            request.insert(QStringLiteral("revision"), m_pendingNetworkRevision);
        request.insert(QStringLiteral("timeout_s"), 300);
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitApply;
        sendControlCommand(QStringLiteral("!CFG APPLY %1")
                               .arg(QString::fromUtf8(
                                   QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitApply &&
        command == QStringLiteral("CFG APPLY")) {
        m_pendingNetworkTrial = data.value(QStringLiteral("trial_id")).toInteger(-1);
        if (m_pendingNetworkTrial < 0) {
            m_console->appendError(tr("Device did not return a network trial ID."));
            m_networkConfigurationStage = NetworkConfigurationStage::Idle;
            m_pendingNetworkPatch = {};
            m_pendingWifiSsid.clear();
            return;
        }

        m_networkConfigurationStage = NetworkConfigurationStage::AwaitRuntime;
        m_wifiTrialPollCount = 0;
        m_wifiTrialPollTimer->start();
        sendControlCommand(QStringLiteral("!NET GET"), true);
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitRuntime &&
        command == QStringLiteral("NET GET")) {
        const QJsonObject runtime = data.value(QStringLiteral("runtime")).toObject();
        bool ready = false;
        if (m_pendingNetworkWifiTarget) {
            const QJsonObject wifi = runtime.value(QStringLiteral("wifi")).toObject();
            ready = wifi.value(QStringLiteral("connected")).toBool() &&
                    wifi.value(QStringLiteral("ipv4_valid")).toBool() &&
                    wifi.value(QStringLiteral("ssid")).toString() == m_pendingWifiSsid;
        } else {
            const QJsonObject ethernet = runtime.value(QStringLiteral("eth")).toObject();
            ready = ethernet.value(QStringLiteral("ipv4_valid")).toBool();
        }
        if (!ready)
            return;

        m_wifiTrialPollTimer->stop();
        m_networkConfigurationStage = NetworkConfigurationStage::AwaitConfirm;
        const QString title = m_pendingNetworkWifiTarget
            ? tr("Confirm Wi-Fi configuration")
            : tr("Confirm network port configuration");
        const QString question = m_pendingNetworkWifiTarget
            ? tr("The new Wi-Fi network is connected and has an IP address. Confirm and save it?")
            : tr("The network port has a valid IPv4 address. Confirm and save it?");
        const auto answer = QMessageBox::question(
            this, title, question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            QJsonObject request;
            request.insert(QStringLiteral("trial_id"), m_pendingNetworkTrial);
            sendControlCommand(QStringLiteral("!CFG CONFIRM %1")
                                   .arg(QString::fromUtf8(
                                       QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        } else {
            QJsonObject request;
            request.insert(QStringLiteral("trial_id"), m_pendingNetworkTrial);
            m_networkConfigurationStage = NetworkConfigurationStage::AwaitAbort;
            sendControlCommand(QStringLiteral("!CFG ABORT %1")
                                   .arg(QString::fromUtf8(
                                       QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        }
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitAbort &&
        command == QStringLiteral("CFG ABORT")) {
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingWifiSsid.clear();
        return;
    }

    if (m_networkConfigurationStage == NetworkConfigurationStage::AwaitConfirm &&
        command == QStringLiteral("CFG CONFIRM")) {
        m_networkConfigurationStage = NetworkConfigurationStage::Idle;
        m_pendingNetworkPatch = {};
        m_pendingWifiSsid.clear();
        m_console->appendNotice(m_pendingNetworkWifiTarget
            ? tr("Wi-Fi configuration saved.")
            : tr("Network port configuration saved."));
    }
}
