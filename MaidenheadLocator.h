#pragma once

#include <QPointF>
#include <QString>

#include <optional>

namespace MaidenheadLocator {

/** @brief 校验 4、6、8 位 Maidenhead 网格，忽略大小写和首尾空白。 */
bool isValid(const QString &grid);

/** @return 网格地理中心（x 为经度、y 为纬度，单位为度）；非法网格返回空值。 */
std::optional<QPointF> center(const QString &grid);

} // namespace MaidenheadLocator
