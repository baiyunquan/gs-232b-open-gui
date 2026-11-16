#pragma once

#include <QDialog>

class QPushButton;
class QLabel;

/** @brief 集中展示零点校准操作和满量程调节说明，命令由宿主窗口确认并发送。 */
class CalibrationDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建非模态校准窗口，允许用户同时操作主窗口调整旋转器位置。 */
    explicit CalibrationDialog(QWidget *parent = nullptr);
    /** @brief 按设备连接、透传模式和校准进度更新两个校准按钮。 */
    void setCommandEnabled(bool enabled);
    /** @brief 显示应用方位补偿后的连续方位和实测俯仰角，不取模且不发送命令。 */
    void setPosition(qint64 azimuth, int elevation);

signals:
    /** @brief 用户请求方位零点校准，宿主须确认当前位置后再发送命令。 */
    void azimuthZeroCalibrationRequested();
    /** @brief 用户请求俯仰零点校准，宿主须确认当前位置后再发送命令。 */
    void elevationZeroCalibrationRequested();

private:
    QLabel *m_azimuthReading;
    QLabel *m_elevationReading;
    QPushButton *m_azimuthZero;
    QPushButton *m_elevationZero;
};
