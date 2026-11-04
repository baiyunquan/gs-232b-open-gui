#include "WifiProvisioningDialog.h"
#include "ManagementKeyDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

WifiProvisioningDialog::WifiProvisioningDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("wifiProvisioningDialog"));
    setWindowTitle(tr("Wi-Fi provisioning"));
    setWindowModality(Qt::WindowModal);
    resize(440, 250);
    auto *layout = new QVBoxLayout(this);
    auto *ssid = new QLineEdit(this);
    ssid->setObjectName(QStringLiteral("wifiSsid"));
    ssid->setPlaceholderText(tr("SSID"));
    auto *ssidLabel = new QLabel(tr("Wi-Fi network"), this);
    ssidLabel->setBuddy(ssid);
    layout->addWidget(ssidLabel);
    layout->addWidget(ssid);
    auto *password = new QLineEdit(this);
    password->setObjectName(QStringLiteral("wifiPassword"));
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(tr("Password"));
    auto *passwordLabel = new QLabel(tr("Wi-Fi password"), this);
    passwordLabel->setBuddy(password);
    layout->addWidget(passwordLabel);
    layout->addWidget(password);
    layout->addWidget(new QLabel(tr("2.4 GHz"), this));
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_configure = buttons->addButton(tr("Configure"), QDialogButtonBox::ActionRole);
    m_configure->setObjectName(QStringLiteral("configureWifi"));
    m_configure->setEnabled(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_configure, &QPushButton::clicked, this, [this] {
        if (m_networkMode) {
            auto *key = new ManagementKeyDialog(this);
            key->setAttribute(Qt::WA_DeleteOnClose);
            key->open();
        }
        // TODO: 通过 PROV/CFG 配网流程接入扫描、试运行和手动确认；UART 无需密钥。
        // TODO: 网络写入仅在尚未认证时请求管理密钥，连接和只读查询均不索要密钥。
    });
}

void WifiProvisioningDialog::setConnectionState(bool commandEnabled, bool networkMode)
{
    m_configure->setEnabled(commandEnabled);
    m_networkMode = networkMode;
}
