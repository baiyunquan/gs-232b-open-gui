#include "ConnectionPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

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
    m_serialPort->addItems({QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5")});
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
    // TODO: 按命令种类自动选择控制或管理 WebSocket 路径；完成条件：两类命令均路由到对应路径。
    m_address->setObjectName(QStringLiteral("websocketAddress"));
    layout->addWidget(m_networkMode, 2, 0);
    layout->addWidget(m_address, 2, 1);

    m_virtualCom = new QCheckBox(tr("Virtual COM"), this);
    m_virtualCom->setObjectName(QStringLiteral("virtualCom"));
    m_virtualPort = new QComboBox(this);
    m_virtualPort->setObjectName(QStringLiteral("virtualPort"));
    m_virtualPort->addItems({QStringLiteral("COM8"), QStringLiteral("COM9"), QStringLiteral("COM10")});
    layout->addWidget(m_virtualCom, 3, 0);
    layout->addWidget(m_virtualPort, 3, 1);

    m_connect = new QPushButton(tr("Connect"), this);
    m_connect->setObjectName(QStringLiteral("connectButton"));
    m_disconnect = new QPushButton(tr("Disconnect"), this);
    m_disconnect->setObjectName(QStringLiteral("disconnectButton"));
    layout->addWidget(m_connect, 4, 0);
    layout->addWidget(m_disconnect, 4, 1);
    layout->setColumnStretch(1, 1);

    connect(m_networkMode, &QRadioButton::toggled, this, &ConnectionPanel::updateControls);
    connect(m_virtualCom, &QCheckBox::toggled, this, &ConnectionPanel::updateControls);
    // TODO: 枚举实际串口并接入串口/WebSocket；仅连接成功后调用 setConnected(true)。
    // TODO: Virtual COM 创建虚拟串口并透传 WS 原始字节；断开时释放传输资源。
    updateControls();
}

bool ConnectionPanel::isConnected() const { return m_connected; }
bool ConnectionPanel::isNetworkMode() const { return m_networkMode->isChecked(); }
bool ConnectionPanel::isVirtualComEnabled() const
{
    return isNetworkMode() && m_virtualCom->isChecked();
}

void ConnectionPanel::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    updateControls();
}

void ConnectionPanel::updateControls()
{
    const bool network = isNetworkMode();
    m_serialMode->setEnabled(!m_connected);
    m_networkMode->setEnabled(!m_connected);
    m_serialPort->setEnabled(!m_connected && !network);
    m_baudRate->setEnabled(!m_connected && !network);
    m_address->setEnabled(!m_connected && network);
    m_virtualCom->setEnabled(!m_connected && network);
    m_virtualPort->setEnabled(!m_connected && isVirtualComEnabled());
    m_connect->setEnabled(!m_connected);
    m_disconnect->setEnabled(m_connected);
    emit stateChanged();
}
