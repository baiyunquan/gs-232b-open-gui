#pragma once

#include <QWidget>

/** @brief 方位或仰角的点动与全轴停止按钮外观，暂不发送运动命令。 */
class MotionButtons final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 横排使用左右旋转图标，竖排使用升降图标。 */
    explicit MotionButtons(Qt::Orientation orientation, QWidget *parent = nullptr);
};
