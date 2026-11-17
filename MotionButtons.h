#pragma once

#include <QWidget>

class QToolButton;

/** @brief 方位或仰角的按住点动与全轴停止按钮；失焦、隐藏或禁用时结束点动。 */
class MotionButtons final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 横排使用左右旋转图标，竖排使用升降图标。 */
    explicit MotionButtons(Qt::Orientation orientation, QWidget *parent = nullptr);
    /** @brief 取消当前按住状态，正在点动时发出一次 motionStopped；不再响应旧按键的松开。 */
    void cancelMotion();

signals:
    /** @brief 开始点动；true 为顺时针或向上，false 为逆时针或向下。 */
    void motionStarted(bool positive);
    /** @brief 松开、失焦或取消时停止本轴；宿主须在连接可用时立即发送停止命令。 */
    void motionStopped();
    /** @brief 请求停止所有轴；宿主须取消其他点动及待发定位命令。 */
    void stopAllRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QToolButton *m_activeButton = nullptr;
    QToolButton *m_cancelledButton = nullptr;
};
