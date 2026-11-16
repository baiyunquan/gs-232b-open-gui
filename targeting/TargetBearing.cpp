#include "TargetBearing.h"
#include "DxccDatabase.h"
#include "MaidenheadLocator.h"

#include <QtMath>

#include <cmath>

int TargetBearing::Solution::roundedDegrees() const
{
    return qRound(degrees) % 360;
}

std::optional<TargetBearing::Target> TargetBearing::resolve(const QString &target)
{
    const QString normalized = target.trimmed().toUpper();
    if (const auto center = MaidenheadLocator::center(target))
        return Target{TargetType::Grid, normalized, *center, {}, {}};
    if (const auto entity = DxccDatabase::instance().find(target))
        return Target{TargetType::Dxcc, normalized, entity->coordinates,
                      entity->name, entity->primaryPrefix};
    return std::nullopt;
}

std::optional<double> TargetBearing::initialBearing(const QPointF &station, const QPointF &target)
{
    for (const QPointF &point : {station, target}) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()) ||
            std::abs(point.x()) > 180 || std::abs(point.y()) > 90)
            return std::nullopt;
    }
    if (std::abs(station.y()) == 90)
        return std::nullopt;
    const double latitude = qDegreesToRadians(station.y());
    const double targetLatitude = qDegreesToRadians(target.y());
    const double delta = qDegreesToRadians(target.x() - station.x());
    const double east = std::sin(delta) * std::cos(targetLatitude);
    const double north = std::cos(latitude) * std::sin(targetLatitude) -
                         std::sin(latitude) * std::cos(targetLatitude) * std::cos(delta);
    // NOTE: 同点与对跖点均无唯一初始方位，不能把 atan2(0, 0) 当作正北旋转。
    if (std::hypot(east, north) < 1e-12)
        return std::nullopt;
    return std::fmod(qRadiansToDegrees(std::atan2(east, north)) + 360.0, 360.0);
}

TargetBearing::Result TargetBearing::calculate(const QString &stationGrid, const QString &target)
{
    const auto station = MaidenheadLocator::center(stationGrid);
    if (!station)
        return {Error::InvalidStationGrid, std::nullopt};
    const auto resolved = resolve(target);
    if (!resolved)
        return {Error::UnknownTarget, std::nullopt};
    const auto bearing = initialBearing(*station, resolved->coordinates);
    if (!bearing)
        return {Error::UndefinedBearing, std::nullopt};
    return {Error::None, Solution{*resolved, *bearing}};
}
