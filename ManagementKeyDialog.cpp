#include "ManagementKeyDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ManagementKeyDialog::ManagementKeyDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("managementKeyDialog"));
    setWindowTitle(tr("Management key"));
    setWindowModality(Qt::WindowModal);
    resize(380, 190);
    auto *layout = new QVBoxLayout(this);
    auto *key = new QLineEdit(this);
    key->setObjectName(QStringLiteral("managementKey"));
    key->setEchoMode(QLineEdit::Password);
    key->setPlaceholderText(tr("Enter key"));
    auto *label = new QLabel(tr("Management key"), this);
    label->setBuddy(key);
    layout->addWidget(label);
    layout->addWidget(key);
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *continueButton = buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    continueButton->setObjectName(QStringLiteral("authorizeButton"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // TODO: 继续按钮发送 SESSION AUTH；仅认证成功后恢复发起操作，关闭窗口时清除密钥。
}
