#include "ControlPanel.h"
#include "Compass.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QWidget *makePresets(const QString &prefix, QWidget *parent)
{
    auto *presets = new QWidget(parent);
    auto *grid = new QGridLayout(presets);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int index = 0; index < 8; ++index) {
        auto *button = new QPushButton(QStringLiteral("000"), presets);
        button->setObjectName(prefix + QString::number(index));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        grid->addWidget(button, index / 2, index % 2);
        // TODO: 左键执行对应预设角度，右键打开角度编辑；各预设默认值为 000。
    }
    return presets;
}

} // namespace

ControlPanel::ControlPanel(QWidget *parent) : QGroupBox(tr("CONTROL"), parent)
{
    setObjectName(QStringLiteral("controlPanel"));
    auto *layout = new QVBoxLayout(this);
    auto *azimuthRow = new QHBoxLayout;
    auto *azimuthColumn = new QVBoxLayout;
    m_compass = new Compass(this);
    m_azimuth = new QLabel(QStringLiteral("000°"), this);
    m_azimuth->setObjectName(QStringLiteral("azimuthReading"));
    m_azimuth->setAlignment(Qt::AlignCenter);
    azimuthColumn->addWidget(m_compass, 1);
    azimuthColumn->addWidget(m_azimuth);
    azimuthRow->addLayout(azimuthColumn);
    m_azimuthPresets = makePresets(QStringLiteral("azimuthPreset"), this);
    azimuthRow->addWidget(m_azimuthPresets, 1);
    layout->addLayout(azimuthRow, 1);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    auto *elevationRow = new QHBoxLayout;
    auto *elevationColumn = new QVBoxLayout;
    m_slider = new QSlider(Qt::Vertical, this);
    m_slider->setObjectName(QStringLiteral("elevationSlider"));
    m_slider->setRange(0, 180);
    m_slider->setMinimumHeight(120);
    m_elevation = new QLabel(QStringLiteral("000°"), this);
    m_elevation->setObjectName(QStringLiteral("elevationReading"));
    m_elevation->setAlignment(Qt::AlignCenter);
    elevationColumn->addWidget(m_slider, 1, Qt::AlignHCenter);
    elevationColumn->addWidget(m_elevation);
    elevationRow->addLayout(elevationColumn);
    m_elevationPresets = makePresets(QStringLiteral("elevationPreset"), this);
    elevationRow->addWidget(m_elevationPresets, 1);
    layout->addLayout(elevationRow, 1);
    setCommandEnabled(false);
    // TODO: 连接成功后定期读取 C2；纯仰角型号改用 B，并按机型轴能力刷新对应读数。
    // TODO: 拖动俯仰滑条时发起定位；请求目标不能直接替代设备实测角度。
}

void ControlPanel::setCommandEnabled(bool enabled)
{
    m_compass->setEnabled(enabled);
    m_slider->setEnabled(enabled);
    m_azimuthPresets->setEnabled(enabled);
    m_elevationPresets->setEnabled(enabled);
}

void ControlPanel::setPosition(int azimuth, int elevation)
{
    m_azimuth->setText(QStringLiteral("%1°").arg(azimuth, 3, 10, QLatin1Char('0')));
    m_elevation->setText(QStringLiteral("%1°").arg(elevation, 3, 10, QLatin1Char('0')));
    m_compass->setAngle(azimuth);
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(elevation);
}
