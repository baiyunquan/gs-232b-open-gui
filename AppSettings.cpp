#include "AppSettings.h"
#include "MaidenheadLocator.h"

#include <array>

namespace {

const QString ConsoleVisibleKey = QStringLiteral("view/consoleVisible");
const QString GreatCircleMapEnabledKey = QStringLiteral("view/greatCircleMapEnabled");
const QString StationGridKey = QStringLiteral("station/grid");

bool readBoolean(const QVariant &stored)
{
    // NOTE: INI 重启后布尔值可能作为字符串读取，拒绝将任意非空损坏值解释为 true。
    const QString value = stored.toString();
    return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
           value == QStringLiteral("1");
}

QString presetKey(AppSettings::Axis axis, int index)
{
    // NOTE: 预设键沿用既有格式，与控件 objectName 解耦后仍可直接读取用户数据。
    const QString prefix = axis == AppSettings::Axis::Azimuth
                               ? QStringLiteral("azimuthPreset")
                               : QStringLiteral("elevationPreset");
    return prefix + QString::number(index);
}

int maximumAngle(AppSettings::Axis axis)
{
    return axis == AppSettings::Axis::Azimuth ? 450 : 180;
}

int defaultAngle(AppSettings::Axis axis, int index)
{
    constexpr std::array<int, AppSettings::PresetCount> elevation{0, 25, 50, 75, 100, 125, 150, 180};
    return axis == AppSettings::Axis::Azimuth ? index * 45 : elevation[index];
}

} // namespace

AppSettings::AppSettings()
    // NOTE: 保留既有组织名、应用名和原生存储格式，集中读写无需迁移已保存的预设。
    : m_settings(QSettings::NativeFormat, QSettings::UserScope,
                 QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"))
{
}

AppSettings::AppSettings(const QString &fileName)
    : m_settings(fileName, QSettings::IniFormat)
{
}

bool AppSettings::consoleVisible() const
{
    return readBoolean(m_settings.value(ConsoleVisibleKey));
}

bool AppSettings::setConsoleVisible(bool visible)
{
    return storeValue(ConsoleVisibleKey, visible);
}

bool AppSettings::greatCircleMapEnabled() const
{
    return readBoolean(m_settings.value(GreatCircleMapEnabledKey));
}

bool AppSettings::setGreatCircleMapEnabled(bool enabled)
{
    return storeValue(GreatCircleMapEnabledKey, enabled);
}

QString AppSettings::stationGrid() const
{
    const QString grid = m_settings.value(StationGridKey).toString().trimmed();
    return isValidStationGrid(grid) ? grid.toUpper() : QString();
}

bool AppSettings::setStationGrid(const QString &grid)
{
    const QString trimmed = grid.trimmed();
    if (!trimmed.isEmpty() && !isValidStationGrid(trimmed))
        return false;

    return storeValue(StationGridKey, trimmed.toUpper());
}

bool AppSettings::isValidStationGrid(const QString &grid)
{
    return MaidenheadLocator::isValid(grid);
}

int AppSettings::presetAngle(Axis axis, int index) const
{
    if (index < 0 || index >= PresetCount)
        return 0;

    bool ok = false;
    const int angle = m_settings.value(presetKey(axis, index)).toString().toInt(&ok);
    return ok && angle >= 0 && angle <= maximumAngle(axis) ? angle : defaultAngle(axis, index);
}

bool AppSettings::setPresetAngle(Axis axis, int index, int angle)
{
    if (index < 0 || index >= PresetCount || angle < 0 || angle > maximumAngle(axis))
        return false;

    return storeValue(presetKey(axis, index), angle);
}

bool AppSettings::storeValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    // NOTE: 偏好在交互时立即落盘，不依赖正常退出；保留 QSettings 的原子写入机制。
    m_settings.sync();
    return m_settings.status() == QSettings::NoError;
}
