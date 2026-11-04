#pragma once

#include <QWidget>

/** @brief 使用系统调色板绘制的方位角罗盘。 */
class Compass final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 创建指向 0° 的罗盘。 */
    explicit Compass(QWidget *parent = nullptr);
    /** @brief 更新设备实测方位；允许保留超过 360° 的机械行程。 */
    void setAngle(int degrees);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_angle = 0;
};
