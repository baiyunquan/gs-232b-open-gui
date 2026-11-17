#include "MotionButtons.h"

#include <QBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QtMath>

namespace {

enum class Symbol { Counterclockwise, Clockwise, Up, Down, Stop };

QIcon motionIcon(Symbol symbol, const QPalette &palette)
{
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
        for (const int size : {24, 48, 72}) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.scale(size / 24.0, size / 24.0);
            const QColor ink = palette.color(mode == QIcon::Normal ? QPalette::Active
                                                                   : QPalette::Disabled,
                                             QPalette::ButtonText);
            painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (symbol == Symbol::Stop) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(mode == QIcon::Normal ? QColor(0xd8, 0x3a, 0x2e)
                                                      : QColor(0xb8, 0x70, 0x68));
                // NOTE: 用正六边形的精确比例构造上下尖角，保证左右边严格竖直。
                constexpr qreal radius = 11;
                const qreal halfWidth = qSqrt(3.0) * radius / 2;
                painter.translate(12, 12);
                painter.drawPolygon(QPolygonF{{0, -radius}, {halfWidth, -radius / 2},
                                              {halfWidth, radius / 2}, {0, radius},
                                              {-halfWidth, radius / 2}, {-halfWidth, -radius / 2}});
            } else if (symbol == Symbol::Up || symbol == Symbol::Down) {
                if (symbol == Symbol::Down) {
                    painter.translate(24, 24);
                    painter.rotate(180);
                }
                painter.drawLine(QPointF(12, 20), QPointF(12, 4));
                painter.drawPolyline(QPolygonF{{6, 10}, {12, 4}, {18, 10}});
            } else {
                if (symbol == Symbol::Clockwise) {
                    painter.translate(24, 0);
                    painter.scale(-1, 1);
                }
                QPainterPath arc;
                arc.moveTo(20, 17);
                arc.cubicTo(20, 6, 10, 4, 5, 12);
                painter.drawPath(arc);
                painter.drawPolyline(QPolygonF{{5, 5}, {5, 12}, {12, 12}});
            }
            painter.end();
            icon.addPixmap(pixmap, mode);
        }
    }
    return icon;
}

} // namespace

MotionButtons::MotionButtons(Qt::Orientation orientation, QWidget *parent) : QWidget(parent)
{
    const bool vertical = orientation == Qt::Vertical;
    auto *layout = new QBoxLayout(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight,
                                  this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    const auto addButton = [this, layout](Symbol symbol, const QString &name,
                                          const QString &hint) {
        auto *button = new QToolButton(this);
        button->setObjectName(name);
        button->setIcon(motionIcon(symbol, palette()));
        button->setIconSize(QSize(24, 24));
        button->setMinimumSize(34, 34);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setAccessibleName(hint);
        button->setToolTip(hint);
        button->setAutoRepeat(false);
        layout->addWidget(button);
    };
    addButton(vertical ? Symbol::Up : Symbol::Counterclockwise,
              vertical ? QStringLiteral("elevationUp") : QStringLiteral("azimuthCounterclockwise"),
              vertical ? tr("Hold to move up; release to stop")
                       : tr("Hold to rotate counterclockwise; release to stop"));
    addButton(vertical ? Symbol::Down : Symbol::Clockwise,
              vertical ? QStringLiteral("elevationDown") : QStringLiteral("azimuthClockwise"),
              vertical ? tr("Hold to move down; release to stop")
                       : tr("Hold to rotate clockwise; release to stop"));
    addButton(Symbol::Stop, vertical ? QStringLiteral("elevationStopAll")
                                     : QStringLiteral("azimuthStopAll"),
              tr("Stop all motion"));
    // TODO: 实现点动：按下开始对应轴运动，松开、失焦或断连时停止；用设备回读验证。
    // TODO: 将两个六边形停止按钮的单击接入全轴停止，并取消待提交的定位命令。
}
