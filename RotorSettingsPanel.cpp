#include "RotorSettingsPanel.h"

#include <QButtonGroup>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

RotorSettingsPanel::RotorSettingsPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("rotorSettingsPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_speed = new QGroupBox(tr("AZIMUTH SPEED"), this);
    auto *speedLayout = new QHBoxLayout(m_speed);
    auto *speeds = new QButtonGroup(this);
    speeds->setExclusive(true);
    for (int speed = 1; speed <= 4; ++speed) {
        auto *button = new QPushButton(QStringLiteral("X%1").arg(speed), m_speed);
        button->setObjectName(QStringLiteral("speed%1").arg(speed));
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        speeds->addButton(button, speed);
        speedLayout->addWidget(button);
    }
    connect(speeds, &QButtonGroup::idClicked, this, [this](int speed) {
        emit commandRequested(QStringLiteral("X%1").arg(speed));
    });
    layout->addWidget(m_speed);

    layout->addStretch(1);
    setCommandEnabled(false);
}

void RotorSettingsPanel::setCommandEnabled(bool enabled)
{
    m_speed->setEnabled(enabled);
}
