#include "ControlPanel.h"
#include "AngleReading.h"
#include "AnglePresets.h"
#include "AppSettings.h"
#include "Compass.h"
#include "targeting/TargetBearing.h"

#include <QFrame>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

QString formatAngle(int angle, int maximum)
{
    return QStringLiteral("%1").arg(qBound(0, angle, maximum), 3, 10, QLatin1Char('0'));
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
        requestAzimuth(m_azimuthScale.mechanicalBearing(angle, m_azimuthValue));
    });
    m_azimuth = new AngleReading(450, this);
    m_azimuth->setObjectName(QStringLiteral("azimuthReading"));
    connect(m_azimuth, &AngleReading::inputRejected, this, &ControlPanel::inputError);
    m_azimuth->setAccessibleName(tr("Azimuth angle, grid or callsign"));
    connect(m_azimuth, &AngleReading::angleRequested, this, [this](qint64 angle) {
        if (m_shortestPathEnabled)
            requestAzimuth(m_azimuthScale.mechanicalBearing(angle, m_azimuthValue));
        else if (const auto mechanical = m_azimuthScale.mechanical(angle))
            requestAzimuth(*mechanical);
    });
    azimuthColumn->addWidget(m_compass, 1);
    azimuthColumn->addWidget(m_azimuth);
    layout->addLayout(azimuthColumn, 0, 0);
    m_azimuthPresets = new AnglePresets(AppSettings::Axis::Azimuth, this);
    layout->addWidget(m_azimuthPresets, 0, 1);

    connect(m_azimuthPresets, &AnglePresets::angleRequested, this, [this](int mechanical) {
        requestAzimuth(m_shortestPathEnabled
                           ? m_azimuthScale.mechanicalBearing(m_azimuthScale.corrected(mechanical), m_azimuthValue)
                           : mechanical);
    });

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
    connect(m_elevation, &AngleReading::inputRejected, this, &ControlPanel::inputError);
    m_elevation->setAccessibleName(tr("Elevation angle"));
    connect(m_elevation, &AngleReading::angleRequested, this, [this](qint64 angle) {
        m_elevationCommandTimer->stop();
        emit commandRequested(QStringLiteral("W%1 %2")
                                  .arg(formatAngle(m_azimuthValue, 450))
                                  .arg(formatAngle(int(angle), 180)));
    });
    elevationColumn->addWidget(m_slider, 1, Qt::AlignHCenter);
    elevationColumn->addWidget(m_elevation);
    layout->addLayout(elevationColumn, 2, 0);
    m_elevationPresets = new AnglePresets(AppSettings::Axis::Elevation, this);
    layout->addWidget(m_elevationPresets, 2, 1);

    connect(m_elevationPresets, &AnglePresets::angleRequested, this, [this](int angle) {
        m_elevationCommandTimer->stop();
        emit commandRequested(QStringLiteral("W%1 %2")
                                  .arg(formatAngle(m_azimuthValue, 450))
                                  .arg(formatAngle(angle, 180)));
    });

    connect(m_slider, &QSlider::valueChanged, m_elevationCommandTimer,
            qOverload<>(&QTimer::start));
    connect(m_elevationCommandTimer, &QTimer::timeout, this, [this] {
        emit commandRequested(QStringLiteral("W%1 %2")
                                  .arg(formatAngle(m_azimuthValue, 450))
                                  .arg(formatAngle(m_slider->value(), 180)));
    });

    const AppSettings preferences;
    m_shortestPathEnabled = preferences.shortestPathEnabled();
    m_azimuthPresets->setShortestPathEnabled(m_shortestPathEnabled);
    setAzimuthOffset(preferences.azimuthOffset());
    setCommandEnabled(false);
}

void ControlPanel::requestAzimuth(int mechanical)
{
    m_elevationCommandTimer->stop();
    emit commandRequested(QStringLiteral("M%1").arg(formatAngle(mechanical, AzimuthScale::Maximum)));
}

void ControlPanel::setShortestPathEnabled(bool enabled)
{
    m_shortestPathEnabled = enabled;
    m_elevationCommandTimer->stop();
    updateAzimuthInput();
    m_azimuthPresets->setShortestPathEnabled(enabled);
    setPosition(m_azimuthValue, m_elevationValue);
}

void ControlPanel::setAzimuthOffset(int degrees)
{
    m_azimuthScale = AzimuthScale(degrees);
    m_elevationCommandTimer->stop();
    updateAzimuthInput();
    m_azimuthPresets->setAzimuthOffset(degrees);
    setPosition(m_azimuthValue, m_elevationValue);
}

void ControlPanel::updateAzimuthInput()
{
    m_azimuth->setRange(m_shortestPathEnabled ? 0 : m_azimuthScale.minimum(),
                       m_shortestPathEnabled ? 359 : m_azimuthScale.maximum(), m_shortestPathEnabled);
    m_azimuth->setInputResolver([this](const QString &text) -> AngleReading::InputResult {
        const auto result = TargetBearing::calculate(m_stationGrid, text);
        if (result.solution) {
            const int bearing = result.solution->roundedDegrees();
            if (m_shortestPathEnabled)
                return {bearing, {}};
            const int mechanical = m_azimuthScale.mechanicalBearing(
                bearing, m_azimuthValue);
            return {m_azimuthScale.corrected(mechanical), {}};
        }
        switch (result.error) {
        case TargetBearing::Error::InvalidStationGrid:
            return {std::nullopt, tr("Set your grid in Settings → My grid… before using a grid or callsign.")};
        case TargetBearing::Error::UndefinedBearing:
            return {std::nullopt, tr("This target has no unique bearing from your grid.")};
        case TargetBearing::Error::UnknownTarget:
        case TargetBearing::Error::None:
            return {std::nullopt, tr("Unknown target. Enter a 4, 6 or 8 character grid, callsign or DXCC prefix.")};
        }
        return {};
    }, m_shortestPathEnabled
           ? tr("Click to enter an integer angle, grid, callsign or prefix. Angles wrap at 360° and use the shortest reachable rotation. Callsigns use DXCC coordinates. Press Enter to move or Esc to cancel.")
           : tr("Click to enter an angle (%1–%2°), grid, callsign or prefix. Callsigns use DXCC coordinates. Press Enter to move or Esc to cancel.")
                 .arg(m_azimuthScale.minimum()).arg(m_azimuthScale.maximum()));
}

void ControlPanel::setCommandEnabled(bool enabled)
{
    if (!enabled)
        cancelPendingMotion();
    m_compass->setEnabled(enabled);
    m_slider->setEnabled(enabled);
    m_azimuth->setEditingEnabled(enabled);
    m_elevation->setEditingEnabled(enabled);
    // NOTE: 预设编辑属于本地偏好设置，断开或透传时仍可编辑；发送端会校验真实连接状态。
    m_azimuthPresets->setEnabled(true);
    m_elevationPresets->setEnabled(true);
}

void ControlPanel::cancelPendingMotion()
{
    m_elevationCommandTimer->stop();
}

void ControlPanel::setPosition(int azimuth, int elevation)
{
    m_azimuthValue = qBound(0, azimuth, 450);
    m_elevationValue = qBound(0, elevation, 180);
    m_azimuth->setAngle(m_azimuthScale.corrected(m_azimuthValue));
    m_elevation->setAngle(m_elevationValue);
    // NOTE: 取模仅影响方位显示与目标选解，内部反馈和 W 命令始终保留机械行程。
    const qint64 corrected = m_azimuthScale.corrected(m_azimuthValue);
    m_compass->setAngle(AzimuthScale::normalized(corrected));
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
    m_stationGrid = grid.trimmed().toUpper();
    m_compass->setStationGrid(grid);
}

int ControlPanel::elevation() const
{
    return m_elevationValue;
}
