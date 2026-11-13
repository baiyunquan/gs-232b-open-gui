#include "ConnectionPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSerialPortInfo>

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QGroupBox(tr("CONNECTION"), parent)
{
    setObjectName(QStringLiteral("connectionPanel"));
    auto *layout = new QGridLayout(this);
    m_serialMode = new QRadioButton(tr("Serial"), this);
    m_serialMode->setObjectName(QStringLiteral("serialMode"));
    m_serialMode->setChecked(true);
    m_serialPort = new QComboBox(this);
    m_serialPort->setObjectName(QStringLiteral("serialPort"));
    m_serialPort->setEditable(true);
    const auto availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : availablePorts)
        m_serialPort->addItem(port.portName(), port.portName());
    if (m_serialPort->count() == 0)
        m_serialPort->addItem(tr("No serial ports found"), QString());
    layout->addWidget(m_serialMode, 0, 0);
    layout->addWidget(m_serialPort, 0, 1);

    m_baudRate = new QComboBox(this);
    m_baudRate->setObjectName(QStringLiteral("baudRate"));
    m_baudRate->addItems({QStringLiteral("1200"), QStringLiteral("2400"),
                          QStringLiteral("4800"), QStringLiteral("9600")});
    m_baudRate->setCurrentIndex(3);
    auto *baudLabel = new QLabel(tr("Baud rate"), this);
    baudLabel->setBuddy(m_baudRate);
    layout->addWidget(baudLabel, 1, 0);
    layout->addWidget(m_baudRate, 1, 1);

    m_networkMode = new QRadioButton(tr("Network"), this);
    m_networkMode->setObjectName(QStringLiteral("networkMode"));
    m_address = new QLineEdit(QStringLiteral("192.168.1.50"), this);
    m_address->setObjectName(QStringLiteral("websocketAddress"));
    layout->addWidget(m_networkMode, 2, 0);
    layout->addWidget(m_address, 2, 1);

    m_virtualCom = new QCheckBox(tr("Virtual COM"), this);
    m_virtualCom->setObjectName(QStringLiteral("virtualCom"));
    layout->addWidget(m_virtualCom, 3, 0, 1, 2);
    m_virtualPortOptions = new QWidget(this);
    auto *virtualLayout = new QFormLayout(m_virtualPortOptions);
    virtualLayout->setContentsMargins(0, 0, 0, 0);
#ifdef Q_OS_WIN
    m_virtualApplicationPort = new QComboBox(m_virtualPortOptions);
    m_virtualApplicationPort->setObjectName(QStringLiteral("virtualApplicationPort"));
    m_virtualClientPort = new QComboBox(m_virtualPortOptions);
    m_virtualClientPort->setObjectName(QStringLiteral("virtualClientPort"));
    for (QComboBox *port : {m_virtualApplicationPort, m_virtualClientPort}) {
        port->setEditable(true);
        for (const QSerialPortInfo &info : availablePorts)
            port->addItem(info.portName());
        port->setCurrentIndex(-1);
        port->lineEdit()->setPlaceholderText(tr("Select a COM port"));
    }
    virtualLayout->addRow(tr("Bridge port"), m_virtualApplicationPort);
    virtualLayout->addRow(tr("Client port"), m_virtualClientPort);
    auto *pairHint = new QLabel(tr("Select the two ports of an existing virtual COM pair."),
                               m_virtualPortOptions);
    pairHint->setWordWrap(true);
    virtualLayout->addRow(pairHint);
#endif
    m_virtualPort = new QLineEdit(m_virtualPortOptions);
    m_virtualPort->setObjectName(QStringLiteral("virtualPort"));
    m_virtualPort->setReadOnly(true);
    m_virtualPort->setPlaceholderText(tr("Assigned after connection"));
    virtualLayout->addRow(tr("Active port"), m_virtualPort);
    layout->addWidget(m_virtualPortOptions, 4, 0, 1, 2);

    m_connect = new QPushButton(tr("Connect"), this);
    m_connect->setObjectName(QStringLiteral("connectButton"));
    m_disconnect = new QPushButton(tr("Disconnect"), this);
    m_disconnect->setObjectName(QStringLiteral("disconnectButton"));
    layout->addWidget(m_connect, 5, 0);
    layout->addWidget(m_disconnect, 5, 1);
    layout->setColumnStretch(1, 1);

    connect(m_networkMode, &QRadioButton::toggled, this, &ConnectionPanel::updateControls);
    connect(m_virtualCom, &QCheckBox::toggled, this, &ConnectionPanel::updateControls);
    connect(m_connect, &QPushButton::clicked, this, &ConnectionPanel::connectRequested);
    connect(m_disconnect, &QPushButton::clicked, this, &ConnectionPanel::disconnectRequested);
    // NOTE: 连接状态只由真实传输后端回写，面板按钮不伪造成功状态。
    updateControls();
}

bool ConnectionPanel::isConnected() const { return m_connected; }
bool ConnectionPanel::isNetworkMode() const { return m_networkMode->isChecked(); }
bool ConnectionPanel::isVirtualComEnabled() const
{
    return isNetworkMode() && m_virtualCom->isChecked();
}

Communication::ConnectionOptions ConnectionPanel::connectionOptions(
    Communication::WebSocketEndpoint endpoint) const
{
    Communication::ConnectionOptions options;
    options.kind = isNetworkMode() ? Communication::TransportKind::WebSocket
                                   : Communication::TransportKind::Serial;
    const bool noPortPlaceholder = m_serialPort->currentIndex() == 0 &&
                                   m_serialPort->itemData(0).toString().isEmpty() &&
                                   m_serialPort->currentText() == m_serialPort->itemText(0);
    if (!noPortPlaceholder)
        options.serialPort = m_serialPort->currentText().trimmed();
    options.baudRate = m_baudRate->currentText().toInt();
    options.address = m_address->text();
    options.endpoint = endpoint;
    return options;
}

VirtualSerialOptions ConnectionPanel::virtualSerialOptions() const
{
    VirtualSerialOptions options;
#ifdef Q_OS_WIN
    options.applicationPort = m_virtualApplicationPort->currentText().trimmed();
    options.clientPort = m_virtualClientPort->currentText().trimmed();
    options.baudRate = m_baudRate->currentText().toInt();
#endif
    return options;
}

void ConnectionPanel::setVirtualSerialPort(const QString &port)
{
    m_virtualPort->setText(port);
}

void ConnectionPanel::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connecting = false;
    m_connected = connected;
    updateControls();
}

void ConnectionPanel::setConnecting(bool connecting)
{
    if (m_connecting == connecting)
        return;
    m_connecting = connecting;
    updateControls();
}

void ConnectionPanel::updateControls()
{
    const bool network = isNetworkMode();
    const bool locked = m_connected || m_connecting;
    m_serialMode->setEnabled(!locked);
    m_networkMode->setEnabled(!locked);
    m_serialPort->setEnabled(!locked && !network);
    bool baudRateEnabled = !network;
#ifdef Q_OS_WIN
    baudRateEnabled = baudRateEnabled || isVirtualComEnabled();
    m_virtualApplicationPort->setEnabled(!locked);
    m_virtualClientPort->setEnabled(!locked);
#endif
    m_baudRate->setEnabled(!locked && baudRateEnabled);
    m_address->setEnabled(!locked && network);
    m_virtualCom->setEnabled(!locked && network);
    m_virtualPortOptions->setVisible(isVirtualComEnabled());
    m_connect->setEnabled(!locked);
    m_disconnect->setEnabled(m_connected);
    emit stateChanged();
}
