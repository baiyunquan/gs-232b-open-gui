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
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString managementAuthCommand(const QString &token)
{
    QJsonObject request;
    request.insert(QStringLiteral("token"), token);
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
    m_settings = new RotorSettingsPanel(central);
    m_settings->setMinimumWidth(250);
    m_settings->setMaximumWidth(320);
    columns->addWidget(m_settings);
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
    connect(m_connection, &ConnectionPanel::stateChanged,
            this, &MainWindow::updateConnectionState);
    connect(m_connection, &ConnectionPanel::connectRequested,
            this, &MainWindow::openTransport);
    connect(m_connection, &ConnectionPanel::disconnectRequested,
            this, &MainWindow::closeTransport);
    connect(m_transport, &Communication::Transport::connected, this, [this] {
        m_connection->setConnecting(false);
        m_connection->setConnected(true);
        if (!m_connection->isVirtualComEnabled()) {
            sendControlCommand(QStringLiteral("C2"));
            m_positionPollTimer->start();
        }
    });
    connect(m_transport, &Communication::Transport::disconnected, this, [this] {
        m_connection->setConnecting(false);
        m_connection->setConnected(false);
        m_positionPollTimer->stop();
        m_wifiTrialPollTimer->stop();
        m_calibrationTimeoutTimer->stop();
        m_receiveBuffer.clear();
        m_pendingCalibrationCommand.clear();
        m_wifiConfigurationStage = WifiConfigurationStage::Idle;
        m_pendingWifiPassword.clear();
        m_pendingManagementToken.clear();
        m_managementAuthenticated = false;
        m_pendingManagementContinuation = {};
    });
    connect(m_transport, &Communication::Transport::bytesReceived,
            m_console, &ConsolePanel::appendReceived);
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
        if (m_wifiConfigurationStage != WifiConfigurationStage::AwaitRuntime ||
            m_pendingWifiTrial < 0) {
            m_wifiTrialPollTimer->stop();
            return;
        }
        if (++m_wifiTrialPollCount > 300) {
            m_wifiTrialPollTimer->stop();
            m_console->appendError(tr("Wi-Fi trial timed out; the device will roll it back."));
            m_wifiConfigurationStage = WifiConfigurationStage::Idle;
            m_pendingWifiSsid.clear();
            m_pendingWifiPassword.clear();
            return;
        }
        sendControlCommand(QStringLiteral("!NET GET"), true);
    });
    connect(m_calibrationTimeoutTimer, &QTimer::timeout, this, [this] {
        m_pendingCalibrationCommand.clear();
        if (m_transport->isOpen())
            m_positionPollTimer->start();
    });
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
    if (commandEnabled)
        m_positionPollTimer->start();
    else
        m_positionPollTimer->stop();
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
    connect(dialog, &WifiProvisioningDialog::configurationRequested,
            this, &MainWindow::configureWifi);
    dialog->setConnectionState(m_connection->isConnected() && !m_connection->isVirtualComEnabled());
    dialog->open();
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
        m_positionPollTimer->stop();
        m_calibrationTimeoutTimer->start();
        sendControlCommand(command);
    }
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
        const QString token = dialog->key();
        if (token.isEmpty())
            return;
        m_pendingManagementToken = token;
        m_pendingManagementContinuation = continuation;
        sendControlCommand(managementAuthCommand(token), true);
    });
    connect(dialog, &QDialog::rejected, this, [cancelled] {
        if (cancelled)
            cancelled();
    });
    dialog->open();
}

void MainWindow::configureWifi(const QString &ssid, const QString &password)
{
    if (ssid.isEmpty() || !m_transport->isOpen() || m_connection->isVirtualComEnabled())
        return;

    m_pendingWifiSsid = ssid;
    m_pendingWifiPassword = password;
    m_pendingWifiCandidate = -1;
    m_pendingWifiRevision = -1;
    m_pendingWifiTrial = -1;

    const auto begin = [this] {
        m_wifiConfigurationStage = WifiConfigurationStage::AwaitBegin;
        sendControlCommand(QStringLiteral("!CFG BEGIN"), true);
    };
    const auto cancel = [this] {
        m_pendingWifiSsid.clear();
        m_pendingWifiPassword.clear();
        m_wifiConfigurationStage = WifiConfigurationStage::Idle;
    };
    if (m_connection->isNetworkMode())
        openManagementKey(begin, cancel);
    else
        begin();
}

void MainWindow::handleReceivedBytes(const QByteArray &bytes)
{
    m_receiveBuffer.append(bytes);
    constexpr qsizetype maximumBuffer = 64 * 1024;
    if (m_receiveBuffer.size() > maximumBuffer) {
        m_receiveBuffer.clear();
        return;
    }

    qsizetype newline = -1;
    while ((newline = m_receiveBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_receiveBuffer.left(newline);
        m_receiveBuffer.remove(0, newline + 1);
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
                m_calibrationTimeoutTimer->stop();
                if (m_transport->isOpen())
                    m_positionPollTimer->start();
            }
        } else if (!m_pendingCalibrationCommand.isEmpty() &&
                   (text.contains(QStringLiteral("Completed"), Qt::CaseInsensitive) ||
                    text.startsWith(QStringLiteral("?>")))) {
            m_pendingCalibrationCommand.clear();
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
    if (!response.value(QStringLiteral("ok")).toBool() &&
        response.value(QStringLiteral("error")).toString() == QStringLiteral("NOT_AUTHORIZED")) {
        m_managementAuthenticated = false;
        m_pendingManagementToken.clear();
    }
    if (command == QStringLiteral("SESSION AUTH")) {
        m_managementAuthenticated = response.value(QStringLiteral("ok")).toBool();
        m_pendingManagementToken.clear();
        if (m_managementAuthenticated) {
            const auto continuation = m_pendingManagementContinuation;
            m_pendingManagementContinuation = {};
            if (continuation)
                continuation();
        } else {
            m_pendingManagementContinuation = {};
            m_console->appendError(tr("Management authentication failed."));
            m_wifiConfigurationStage = WifiConfigurationStage::Idle;
            m_pendingWifiSsid.clear();
            m_pendingWifiPassword.clear();
            return;
        }
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::Idle)
        return;

    QString expectedCommand;
    switch (m_wifiConfigurationStage) {
    case WifiConfigurationStage::AwaitBegin: expectedCommand = QStringLiteral("CFG BEGIN"); break;
    case WifiConfigurationStage::AwaitSet: expectedCommand = QStringLiteral("NET SET"); break;
    case WifiConfigurationStage::AwaitCheck: expectedCommand = QStringLiteral("CFG CHECK"); break;
    case WifiConfigurationStage::AwaitApply: expectedCommand = QStringLiteral("CFG APPLY"); break;
    case WifiConfigurationStage::AwaitRuntime: expectedCommand = QStringLiteral("NET GET"); break;
    case WifiConfigurationStage::AwaitConfirm: expectedCommand = QStringLiteral("CFG CONFIRM"); break;
    case WifiConfigurationStage::AwaitAbort: expectedCommand = QStringLiteral("CFG ABORT"); break;
    case WifiConfigurationStage::Idle: return;
    }
    if (command != expectedCommand)
        return;

    if (!response.value(QStringLiteral("ok")).toBool()) {
        m_console->appendError(
            tr("Wi-Fi configuration failed: %1")
                .arg(response.value(QStringLiteral("error")).toString()));
        m_wifiTrialPollTimer->stop();
        m_wifiConfigurationStage = WifiConfigurationStage::Idle;
        m_pendingWifiSsid.clear();
        m_pendingWifiPassword.clear();
        return;
    }

    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitBegin &&
        command == QStringLiteral("CFG BEGIN")) {
        m_pendingWifiCandidate = data.value(QStringLiteral("candidate_id")).toInteger(-1);
        m_pendingWifiRevision = data.value(QStringLiteral("base_revision")).toInteger(-1);
        if (m_pendingWifiCandidate < 0) {
            m_console->appendError(tr("Device did not return a Wi-Fi candidate ID."));
            m_wifiConfigurationStage = WifiConfigurationStage::Idle;
            m_pendingWifiSsid.clear();
            m_pendingWifiPassword.clear();
            return;
        }

        QJsonObject wifi;
        wifi.insert(QStringLiteral("enabled"), true);
        wifi.insert(QStringLiteral("ssid"), m_pendingWifiSsid);
        wifi.insert(QStringLiteral("password"), m_pendingWifiPassword);
        wifi.insert(QStringLiteral("security"), m_pendingWifiPassword.isEmpty()
                                                    ? QStringLiteral("open")
                                                    : QStringLiteral("auto"));
        QJsonObject request;
        request.insert(QStringLiteral("wifi"), wifi);
        m_wifiConfigurationStage = WifiConfigurationStage::AwaitSet;
        sendControlCommand(QStringLiteral("!NET SET %1")
                               .arg(QString::fromUtf8(
                                   QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        return;
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitSet &&
        command == QStringLiteral("NET SET")) {
        m_pendingWifiPassword.clear();
        m_wifiConfigurationStage = WifiConfigurationStage::AwaitCheck;
        sendControlCommand(QStringLiteral("!CFG CHECK"), true);
        return;
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitCheck &&
        command == QStringLiteral("CFG CHECK")) {
        QJsonObject request;
        request.insert(QStringLiteral("candidate_id"), m_pendingWifiCandidate);
        if (m_pendingWifiRevision >= 0)
            request.insert(QStringLiteral("revision"), m_pendingWifiRevision);
        request.insert(QStringLiteral("timeout_s"), 300);
        m_wifiConfigurationStage = WifiConfigurationStage::AwaitApply;
        sendControlCommand(QStringLiteral("!CFG APPLY %1")
                               .arg(QString::fromUtf8(
                                   QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        return;
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitApply &&
        command == QStringLiteral("CFG APPLY")) {
        m_pendingWifiTrial = data.value(QStringLiteral("trial_id")).toInteger(-1);
        if (m_pendingWifiTrial < 0) {
            m_console->appendError(tr("Device did not return a Wi-Fi trial ID."));
            m_wifiConfigurationStage = WifiConfigurationStage::Idle;
            m_pendingWifiSsid.clear();
            m_pendingWifiPassword.clear();
            return;
        }

        m_wifiConfigurationStage = WifiConfigurationStage::AwaitRuntime;
        m_wifiTrialPollCount = 0;
        m_wifiTrialPollTimer->start();
        sendControlCommand(QStringLiteral("!NET GET"), true);
        return;
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitRuntime &&
        command == QStringLiteral("NET GET")) {
        const QJsonObject runtime = data.value(QStringLiteral("runtime")).toObject();
        const QJsonObject wifi = runtime.value(QStringLiteral("wifi")).toObject();
        if (!wifi.value(QStringLiteral("connected")).toBool() ||
            !wifi.value(QStringLiteral("ipv4_valid")).toBool() ||
            wifi.value(QStringLiteral("ssid")).toString() != m_pendingWifiSsid)
            return;

        m_wifiTrialPollTimer->stop();
        m_wifiConfigurationStage = WifiConfigurationStage::AwaitConfirm;
        const auto answer = QMessageBox::question(
            this, tr("Confirm Wi-Fi configuration"),
            tr("The new Wi-Fi network is connected and has an IP address. Confirm and save it?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            QJsonObject request;
            request.insert(QStringLiteral("trial_id"), m_pendingWifiTrial);
            sendControlCommand(QStringLiteral("!CFG CONFIRM %1")
                                   .arg(QString::fromUtf8(
                                       QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        } else {
            QJsonObject request;
            request.insert(QStringLiteral("trial_id"), m_pendingWifiTrial);
            m_wifiConfigurationStage = WifiConfigurationStage::AwaitAbort;
            sendControlCommand(QStringLiteral("!CFG ABORT %1")
                                   .arg(QString::fromUtf8(
                                       QJsonDocument(request).toJson(QJsonDocument::Compact))), true);
        }
        return;
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitAbort &&
        command == QStringLiteral("CFG ABORT")) {
        m_wifiConfigurationStage = WifiConfigurationStage::Idle;
        m_pendingWifiSsid.clear();
        m_pendingWifiPassword.clear();
        return;
    }

    if (m_wifiConfigurationStage == WifiConfigurationStage::AwaitConfirm &&
        command == QStringLiteral("CFG CONFIRM")) {
        m_wifiConfigurationStage = WifiConfigurationStage::Idle;
        m_pendingWifiSsid.clear();
        m_pendingWifiPassword.clear();
        m_console->appendNotice(tr("Wi-Fi configuration saved."));
    }
}
