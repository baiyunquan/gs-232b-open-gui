#include "Compass.h"

#include <QPainter>
#include <QtMath>

Compass::Compass(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("azimuthCompass"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void Compass::setAngle(int degrees)
{
    m_angle = degrees;
    update();
}

QSize Compass::sizeHint() const { return QSize(154, 154); }
QSize Compass::minimumSizeHint() const { return QSize(120, 120); }

void Compass::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor ink = palette().color(group, QPalette::Text);
    const QColor needle = palette().color(group, QPalette::Highlight);
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal radius = qMin(width(), height()) * 0.43;
    painter.translate(center);
    painter.setPen(QPen(ink, 1.4));
    painter.drawEllipse(QPointF(), radius, radius);

    for (int degrees = 0; degrees < 360; degrees += 5) {
        const qreal radians = qDegreesToRadians(qreal(degrees - 90));
        const QPointF direction(qCos(radians), qSin(radians));
        const bool major = degrees % 30 == 0;
        painter.setPen(QPen(ink, major ? 1.3 : 0.8));
        painter.drawLine(direction * (radius - (major ? 9 : 4)), direction * radius);
    }

    painter.setPen(QPen(needle, 2.5));
    const qreal radians = qDegreesToRadians(qreal(m_angle % 360 - 90));
    painter.drawLine(QPointF(), QPointF(qCos(radians), qSin(radians)) * radius * 0.63);
    painter.setBrush(needle);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(), 3, 3);
    // TODO: 接入罗盘选角与定位请求；实测指针仍只由设备反馈更新。
}
