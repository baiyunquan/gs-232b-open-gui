#pragma once

#include <QGroupBox>

class Compass;
class QLabel;
class QSlider;
class QWidget;

/** @brief 方位与俯仰读数、选角控件及各八个预设的视图。 */
class ControlPanel final : public QGroupBox
{
    Q_OBJECT

public:
    /** @brief 创建零读数、零预设且命令控件禁用的面板。 */
    explicit ControlPanel(QWidget *parent = nullptr);
    /** @brief 按实际连接和透传状态统一切换命令控件。 */
    void setCommandEnabled(bool enabled);
    /** @brief 更新设备实测角度，不触发滑条定位请求。 */
    void setPosition(int azimuth, int elevation);

private:
    Compass *m_compass;
    QLabel *m_azimuth;
    QLabel *m_elevation;
    QSlider *m_slider;
    QWidget *m_azimuthPresets;
    QWidget *m_elevationPresets;
};
