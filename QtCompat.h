#pragma once

#include <QJsonValue>
#include <QMouseEvent>
#include <QtGlobal>

#include <cmath>
#include <limits>

inline QPointF mouseEventPosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

inline qint64 jsonIntegerValue(const QJsonValue &value, qint64 defaultValue)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return value.toInteger(defaultValue);
#else
    if (!value.isDouble())
        return defaultValue;
    const double number = value.toDouble();
    const double qint64Limit = -static_cast<double>(std::numeric_limits<qint64>::min());
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < static_cast<double>(std::numeric_limits<qint64>::min()) || number >= qint64Limit) {
        return defaultValue;
    }
    return static_cast<qint64>(number);
#endif
}
