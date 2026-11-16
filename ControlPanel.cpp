#include "ControlPanel.h"
#include "AngleReading.h"
#include "AppSettings.h"
#include "Compass.h"

#include <QFrame>
#include <QCoreApplication>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

QString formatAngle(int angle, int maximum)
{
    return QStringLiteral("%1").arg(qBound(0, angle, maximum), 3, 10, QLatin1Char('0'));
}

QWidget *makePresets(AppSettings::Axis axis, QWidget *parent)
{
    AppSettings settings;
    const bool azimuth = axis == AppSettings::Axis::Azimuth;
    const int maximum = azimuth ? 450 : 180;
    const QString prefix = azimuth ? QStringLiteral("azimuthPreset")
                                   : QStringLiteral("elevationPreset");
    auto *presets = new QWidget(parent);
    auto *grid = new QGridLayout(presets);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int index = 0; index < AppSettings::presetCount(axis); ++index) {
        auto *cell = new QWidget(presets);
        cell->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *stack = new QStackedLayout(cell);
        stack->setContentsMargins(0, 0, 0, 0);
        auto *button = new QPushButton(
            formatAngle(settings.presetAngle(axis, index), maximum), cell);
        button->setObjectName(prefix + QString::number(index));
        // NOTE: 预设只显示三位数字，按字体保留留白，将横向空间优先分配给罗盘。
        cell->setFixedWidth(button->fontMetrics().horizontalAdvance(QStringLiteral("000")) + 24);
        button->setProperty("presetIndex", index);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setToolTip(QCoreApplication::translate(
            "ControlPanel", "Right-click to edit this preset. Press Enter to save or Esc to cancel."));
        auto *reading = new AngleReading(maximum, cell);
        reading->setEditingEnabled(true);
        reading->setAccessibleName(QCoreApplication::translate(
            "ControlPanel", azimuth ? "Edit azimuth preset" : "Edit elevation preset"));
        reading->setToolTip(button->toolTip());
        stack->addWidget(button);
        stack->addWidget(reading);
        QObject::connect(button, &QPushButton::customContextMenuRequested, reading,
                         [button, reading, stack] {
            reading->setAngle(button->text().toInt());
            stack->setCurrentWidget(reading);
            reading->beginEditing();
        });
        QObject::connect(reading, &AngleReading::editingFinished, stack, [button, stack] {
            stack->setCurrentWidget(button);
        });
        QObject::connect(reading, &AngleReading::angleRequested, button,
                         [axis, index, maximum, button, parent](int angle) {
            button->setText(formatAngle(angle, maximum));
            if (!AppSettings().setPresetAngle(axis, index, angle))
                QMessageBox::warning(parent,
                    QCoreApplication::translate("ControlPanel", "Application settings"),
                    QCoreApplication::translate("ControlPanel", "Could not save application settings."));
        });
        grid->addWidget(cell, index / 2, index % 2);
    }
    return presets;
}

} // namespace

ControlPanel::ControlPanel(QWidget *parent) : QGroupBox(tr("CONTROL"), parent)
{
    setObjectName(QStringLiteral("controlPanel"));
    // NOTE: 共用紧凑预设列，按五行方位、三行仰角分配高度，扩大罗盘并保持滑条对齐。
    auto *layout = new QGridLayout(this);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(0, 5);
    layout->setRowStretch(2, 3);
    auto *azimuthColumn = new QVBoxLayout;
    m_compass = new Compass(this);
    connect(m_compass, &Compass::mapError, this, &ControlPanel::mapError);
    connect(m_compass, &Compass::angleRequested, this, [this](int angle) {
        emit commandRequested(QStringLiteral("M%1").arg(formatAngle(angle, 450)));
    });
    m_azimuth = new AngleReading(450, this);
    m_azimuth->setObjectName(QStringLiteral("azimuthReading"));
    m_azimuth->setAccessibleName(tr("Azimuth angle"));
    connect(m_azimuth, &AngleReading::angleRequested, this, [this](int angle) {
        m_elevationCommandTimer->stop();
        emit commandRequested(QStringLiteral("M%1").arg(formatAngle(angle, 450)));
    });
    azimuthColumn->addWidget(m_compass, 1);
    azimuthColumn->addWidget(m_azimuth);
    layout->addLayout(azimuthColumn, 0, 0);
    m_azimuthPresets = makePresets(AppSettings::Axis::Azimuth, this);
    layout->addWidget(m_azimuthPresets, 0, 1);

    const auto azimuthPresets = m_azimuthPresets->findChildren<QPushButton *>();
    for (auto *button : azimuthPresets) {
        connect(button, &QPushButton::clicked, this, [this, button] {
            bool ok = false;
            const int angle = button->text().trimmed().toInt(&ok);
            if (ok)
                emit commandRequested(QStringLiteral("M%1").arg(formatAngle(angle, 450)));
        });
    }

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator, 1, 0, 1, 2);

    auto *elevationColumn = new QVBoxLayout;
    m_slider = new QSlider(Qt::Vertical, this);
    m_slider->setObjectName(QStringLiteral("elevationSlider"));
    m_slider->setRange(0, 180);
    m_slider->setMinimumHeight(90);
    m_elevationCommandTimer = new QTimer(this);
    m_elevationCommandTimer->setSingleShot(true);
    m_elevationCommandTimer->setInterval(150);
    m_elevation = new AngleReading(180, this);
    m_elevation->setObjectName(QStringLiteral("elevationReading"));
    m_elevation->setAccessibleName(tr("Elevation angle"));
    connect(m_elevation, &AngleReading::angleRequested, this, [this](int angle) {
        m_elevationCommandTimer->stop();
        emit commandRequested(QStringLiteral("W%1 %2")
                                  .arg(formatAngle(m_azimuthValue, 450))
                                  .arg(formatAngle(angle, 180)));
    });
    elevationColumn->addWidget(m_slider, 1, Qt::AlignHCenter);
    elevationColumn->addWidget(m_elevation);
    layout->addLayout(elevationColumn, 2, 0);
    m_elevationPresets = makePresets(AppSettings::Axis::Elevation, this);
    layout->addWidget(m_elevationPresets, 2, 1);

    const auto elevationPresets = m_elevationPresets->findChildren<QPushButton *>();
    for (auto *button : elevationPresets) {
        connect(button, &QPushButton::clicked, this, [this, button] {
            bool ok = false;
            const int angle = button->text().trimmed().toInt(&ok);
            if (ok) {
                emit commandRequested(QStringLiteral("W%1 %2")
                                          .arg(formatAngle(m_azimuthValue, 450))
                                          .arg(formatAngle(angle, 180)));
            }
        });
    }

    connect(m_slider, &QSlider::valueChanged, m_elevationCommandTimer,
            qOverload<>(&QTimer::start));
    connect(m_elevationCommandTimer, &QTimer::timeout, this, [this] {
        emit commandRequested(QStringLiteral("W%1 %2")
                                  .arg(formatAngle(m_azimuthValue, 450))
                                  .arg(formatAngle(m_slider->value(), 180)));
    });

    setCommandEnabled(false);
}

void ControlPanel::setCommandEnabled(bool enabled)
{
    m_compass->setEnabled(enabled);
    m_slider->setEnabled(enabled);
    m_azimuth->setEditingEnabled(enabled);
    m_elevation->setEditingEnabled(enabled);
    // NOTE: 预设编辑属于本地偏好设置，断开或透传时仍可编辑；发送端会校验真实连接状态。
    m_azimuthPresets->setEnabled(true);
    m_elevationPresets->setEnabled(true);
}

void ControlPanel::setPosition(int azimuth, int elevation)
{
    m_azimuthValue = qBound(0, azimuth, 450);
    m_elevationValue = qBound(0, elevation, 180);
    m_azimuth->setAngle(m_azimuthValue);
    m_elevation->setAngle(m_elevationValue);
    m_compass->setAngle(m_azimuthValue);
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(m_elevationValue);
}

int ControlPanel::azimuth() const
{
    return m_azimuthValue;
}

void ControlPanel::setGreatCircleMapEnabled(bool enabled)
{
    m_compass->setGreatCircleMapEnabled(enabled);
}

void ControlPanel::setStationGrid(const QString &grid)
{
    m_compass->setStationGrid(grid);
}

int ControlPanel::elevation() const
{
    return m_elevationValue;
}
