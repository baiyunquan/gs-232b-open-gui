#pragma once

#include <QGroupBox>
#include <QString>

class AngleReading;
class Compass;
class QSlider;
class QTimer;
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
    /** @brief 切换方位罗盘的大圆图显示，不影响设备连接或角度读数。 */
    void setGreatCircleMapEnabled(bool enabled);
    /** @brief 更新大圆图使用的本站网格。 */
    void setStationGrid(const QString &grid);
    /** @return 当前界面保留的方位读数。 */
    int azimuth() const;
    /** @return 当前界面保留的仰角读数。 */
    int elevation() const;

signals:
    /** @brief 用户通过控制控件请求发送一条 GS-232B 命令。 */
    void commandRequested(const QString &command);
    /** @brief 大圆图生成或缓存失败，供主窗口显示状态提示。 */
    void mapError(const QString &message);

private:
    Compass *m_compass;
    AngleReading *m_azimuth;
    AngleReading *m_elevation;
    QSlider *m_slider;
    QTimer *m_elevationCommandTimer;
    QWidget *m_azimuthPresets;
    QWidget *m_elevationPresets;
    int m_azimuthValue = 0;
    int m_elevationValue = 0;
};
