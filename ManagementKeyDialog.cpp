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
    m_key = new QLineEdit(this);
    m_key->setObjectName(QStringLiteral("managementKey"));
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setPlaceholderText(tr("Enter key"));
    auto *label = new QLabel(tr("Management key"), this);
    label->setBuddy(m_key);
    layout->addWidget(label);
    layout->addWidget(m_key);
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *continueButton = buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    continueButton->setObjectName(QStringLiteral("authorizeButton"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

QString ManagementKeyDialog::key() const
{
    return m_key->text().trimmed();
}
