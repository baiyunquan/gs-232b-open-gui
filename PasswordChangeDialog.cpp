#include "PasswordChangeDialog.h"

#include <QByteArray>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

bool validPassword(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    if (bytes.size() < 8 || bytes.size() > 64)
        return false;
    for (const unsigned char byte : bytes) {
        if (byte < 0x20 || byte > 0x7e)
            return false;
    }
    return true;
}

} // namespace

PasswordChangeDialog::PasswordChangeDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("passwordChangeDialog"));
    setWindowTitle(tr("Change management password (AI)"));
    setWindowModality(Qt::WindowModal);
    resize(420, 250);

    auto *layout = new QVBoxLayout(this);
    m_password = new QLineEdit(this);
    m_password->setObjectName(QStringLiteral("newManagementPassword"));
    m_password->setEchoMode(QLineEdit::Password);
    auto *passwordLabel = new QLabel(tr("New management password (AI)"), this);
    passwordLabel->setBuddy(m_password);
    layout->addWidget(passwordLabel);
    layout->addWidget(m_password);

    m_confirmation = new QLineEdit(this);
    m_confirmation->setObjectName(QStringLiteral("confirmManagementPassword"));
    m_confirmation->setEchoMode(QLineEdit::Password);
    auto *confirmationLabel = new QLabel(tr("Confirm password (AI)"), this);
    confirmationLabel->setBuddy(m_confirmation);
    layout->addWidget(confirmationLabel);
    layout->addWidget(m_confirmation);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_change = buttons->addButton(tr("Change password (AI)"), QDialogButtonBox::AcceptRole);
    m_change->setObjectName(QStringLiteral("changeManagementPassword"));
    m_change->setEnabled(false);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_password, &QLineEdit::textChanged, this, &PasswordChangeDialog::updateControls);
    connect(m_confirmation, &QLineEdit::textChanged, this, &PasswordChangeDialog::updateControls);
}

QString PasswordChangeDialog::password() const
{
    return m_password->text();
}

void PasswordChangeDialog::updateControls()
{
    m_change->setEnabled(validPassword(m_password->text()) &&
                         m_password->text() == m_confirmation->text());
}
