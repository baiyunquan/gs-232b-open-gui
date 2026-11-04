#include "RotorSettingsPanel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

RotorSettingsPanel::RotorSettingsPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("rotorSettingsPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *modelGroup = new QGroupBox(tr("ROTOR MODEL"), this);
    auto *modelLayout = new QVBoxLayout(modelGroup);
    m_model = new QComboBox(modelGroup);
    m_model->setObjectName(QStringLiteral("rotorModel"));
    const QStringList models = {
        QStringLiteral("G-5500"), QStringLiteral("G-5400B"), QStringLiteral("G-5600B"),
        QStringLiteral("G-5500DC"), QStringLiteral("G-400"), QStringLiteral("G-500A"),
        QStringLiteral("G-550"), QStringLiteral("G-800DXA"), QStringLiteral("G-1000DXA"),
        QStringLiteral("G-2800DXA"), QStringLiteral("G-800DXC"), QStringLiteral("G-1000DXC"),
        QStringLiteral("G-2800DXC"), tr("SDX series"), QStringLiteral("G-400 + G-500A"),
        QStringLiteral("G-400 + G-550")
    };
    for (int index = 0; index < models.size(); ++index)
        m_model->addItem(QStringLiteral("%1  %2").arg(index + 1).arg(models.at(index)), index + 1);
    m_apply = new QPushButton(tr("Apply model"), modelGroup);
    m_apply->setObjectName(QStringLiteral("applyModel"));
    modelLayout->addWidget(m_model);
    modelLayout->addWidget(m_apply);
    layout->addWidget(modelGroup);
    connect(m_apply, &QPushButton::clicked, this, &RotorSettingsPanel::modelApplyRequested);

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
    layout->addWidget(m_speed);
    layout->addStretch(1);
    setCommandEnabled(false);
    // TODO: 连接后查询 ROTOR GET 并按轴能力更新控制面板；速度按钮发送 X1 至 X4。
    // TODO: 型号写入使用 ROTOR SET；提示重启后生效，并在 WS 写入前按需认证管理会话。
}

void RotorSettingsPanel::setCommandEnabled(bool enabled)
{
    m_model->setEnabled(enabled);
    m_apply->setEnabled(enabled);
    m_speed->setEnabled(enabled);
}
