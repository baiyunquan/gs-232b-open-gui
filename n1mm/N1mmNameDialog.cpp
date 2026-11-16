#include "N1mmNameDialog.h"
#include "N1mmProtocol.h"
#include "AppSettings.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

N1mmNameDialog::N1mmNameDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("n1mmNameDialog"));
    setWindowTitle(tr("N1MM rotator name"));
    setWindowModality(Qt::WindowModal);
    resize(440, 220);

    auto *layout = new QVBoxLayout(this);
    m_name = new QLineEdit(AppSettings().n1mmRotorName(), this);
    m_name->setObjectName(QStringLiteral("n1mmRotorName"));
    m_name->setMaxLength(N1mm::MaximumNameLength);
    m_name->setClearButtonEnabled(true);
    m_name->selectAll();
    auto *label = new QLabel(tr("Rotator name"), this);
    label->setBuddy(m_name);
    layout->addWidget(label);
    layout->addWidget(m_name);

    auto *hint = new QLabel(
        tr("Use exactly the same name in N1MM Logger+ → Configurer → Antennas → Rotor Description. Names are case-sensitive."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *rules = new QLabel(tr("Enter 1–64 characters. Commas, @ and control characters are not allowed."), this);
    rules->setWordWrap(true);
    layout->addWidget(rules);
    m_name->setAccessibleDescription(hint->text() + QLatin1Char(' ') + rules->text());
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    m_save->setDefault(true);
    m_save->setObjectName(QStringLiteral("saveN1mmRotorName"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &N1mmNameDialog::saveName);
    connect(m_name, &QLineEdit::textChanged, this, [this] {
        m_save->setEnabled(N1mm::isValidRotorName(m_name->text()));
    });
    m_save->setEnabled(N1mm::isValidRotorName(m_name->text()));
}

void N1mmNameDialog::saveName()
{
    if (!N1mm::isValidRotorName(m_name->text()))
        return;
    if (!AppSettings().setN1mmRotorName(m_name->text())) {
        QMessageBox::warning(this, tr("N1MM rotator name"), tr("Could not save the rotator name."));
        return;
    }
    accept();
}
