#include "AnglePresets.h"
#include "AngleReading.h"
#include "AzimuthScale.h"

#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedLayout>

AnglePresets::AnglePresets(AppSettings::Axis axis, QWidget *parent)
    : QWidget(parent), m_axis(axis)
{
    AppSettings settings;
    const bool azimuth = axis == AppSettings::Axis::Azimuth;
    const int maximum = azimuth ? AzimuthScale::Maximum : 180;
    const QString prefix = azimuth ? QStringLiteral("azimuthPreset")
                                   : QStringLiteral("elevationPreset");
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int index = 0; index < AppSettings::presetCount(axis); ++index) {
        auto *cell = new QWidget(this);
        cell->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *stack = new QStackedLayout(cell);
        stack->setContentsMargins(0, 0, 0, 0);
        auto *button = new QPushButton(cell);
        button->setObjectName(prefix + QString::number(index));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setToolTip(tr("Right-click to edit this preset. Press Enter to save or Esc to cancel."));
        auto *reading = new AngleReading(maximum, cell);
        reading->setEditingEnabled(true);
        reading->setAccessibleName(azimuth ? tr("Edit azimuth preset") : tr("Edit elevation preset"));
        reading->setToolTip(button->toolTip());
        stack->addWidget(button);
        stack->addWidget(reading);
        m_presets.push_back({settings.presetAngle(axis, index), cell, button, reading});
        connect(button, &QPushButton::clicked, this, [this, index] {
            emit angleRequested(m_presets[index].angle);
        });
        connect(button, &QPushButton::customContextMenuRequested, reading,
                [this, index, reading, stack] {
            reading->setAngle(qint64(m_presets[index].angle) + m_offset);
            stack->setCurrentWidget(reading);
            reading->beginEditing();
        });
        connect(reading, &AngleReading::editingFinished, stack, [button, stack] {
            stack->setCurrentWidget(button);
        });
        connect(reading, &AngleReading::angleRequested, this, [this, index](qint64 angle) {
            // NOTE: 取模编辑以原预设选等价机械位置，原值提交不改变重叠行程；存储格式不变。
            const int mechanical = m_shortestPathEnabled
                                       ? AzimuthScale(m_offset).mechanicalBearing(angle, m_presets[index].angle)
                                       : int(angle - m_offset);
            m_presets[index].angle = mechanical;
            updateAngles();
            if (!AppSettings().setPresetAngle(m_axis, index, mechanical))
                QMessageBox::warning(this, tr("Application settings"),
                                     tr("Could not save application settings."));
        });
        grid->addWidget(cell, index / 2, index % 2);
    }
    updateAngles();
}

void AnglePresets::setAzimuthOffset(int degrees)
{
    if (m_axis != AppSettings::Axis::Azimuth || m_offset == degrees)
        return;
    m_offset = degrees;
    updateAngles();
}

void AnglePresets::setShortestPathEnabled(bool enabled)
{
    if (m_axis != AppSettings::Axis::Azimuth)
        return;
    m_shortestPathEnabled = enabled;
    updateAngles();
}

void AnglePresets::updateAngles()
{
    const qint64 minimum = m_shortestPathEnabled ? 0 : m_offset;
    const qint64 maximum = m_shortestPathEnabled
                               ? 359 : minimum + (m_axis == AppSettings::Axis::Azimuth ? AzimuthScale::Maximum : 180);
    for (const Preset &preset : m_presets) {
        const qint64 corrected = qint64(preset.angle) + m_offset;
        preset.button->setText(QStringLiteral("%1").arg(
            m_shortestPathEnabled ? qint64(AzimuthScale::normalized(corrected)) : corrected,
            3, 10, QLatin1Char('0')));
        // NOTE: 为负号及更大的补偿值保留宽度，避免三位数字宽度裁切补偿后的预设。
        const auto metrics = preset.button->fontMetrics();
        const int width = qMax(metrics.horizontalAdvance(QString::number(minimum)),
                               metrics.horizontalAdvance(QString::number(maximum)));
        preset.cell->setFixedWidth(qMax(width, metrics.horizontalAdvance(QStringLiteral("000"))) + 24);
        preset.reading->setRange(minimum, maximum, m_shortestPathEnabled);
        preset.reading->setAngle(corrected);
    }
}
