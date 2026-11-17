#pragma once

#include <QHBoxLayout>

/** @brief 将罗盘限制在左侧正方形内，其余宽度交给右侧控制区。 */
class CompassLayout final : public QHBoxLayout
{
public:
    /** @brief 接管两个控件的布局；正方形边长同时受高度和控制区最小宽度约束。 */
    CompassLayout(QWidget *compass, QWidget *controls, QWidget *parent)
        : QHBoxLayout(parent)
    {
        setContentsMargins(0, 0, 0, 0);
        setSpacing(10);
        addWidget(compass);
        addWidget(controls, 1);
    }

    /** @brief 先更新标准布局状态，再按可用高度划分正方形与控制区。 */
    void setGeometry(const QRect &rect) override
    {
        QHBoxLayout::setGeometry(rect);
        const QRect bounds = contentsRect();
        const int gap = spacing();
        const int side = qMin(bounds.height(),
                              qMax(0, bounds.width() - gap - itemAt(1)->minimumSize().width()));
        itemAt(0)->setGeometry(QRect(bounds.x(), bounds.y() + (bounds.height() - side) / 2,
                                     side, side));
        itemAt(1)->setGeometry(QRect(bounds.x() + side + gap, bounds.y(),
                                     qMax(0, bounds.width() - side - gap), bounds.height()));
    }
};
