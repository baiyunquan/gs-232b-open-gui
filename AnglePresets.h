#pragma once

#include "AppSettings.h"

#include <QWidget>

#include <vector>

class AngleReading;
class QPushButton;

/** @brief 按机械角度保存预设，按方位补偿及最短路径模式显示和编辑。 */
class AnglePresets final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 载入指定轴的预设，离线时仍可编辑。 */
    explicit AnglePresets(AppSettings::Axis axis, QWidget *parent = nullptr);
    /** @brief 更新方位补偿及编辑范围，不改写已保存的机械角度；仰角忽略此设置。 */
    void setAzimuthOffset(int degrees);
    /** @brief 切换方位预设的取模显示和编辑，不改写存储；仰角忽略此设置。 */
    void setShortestPathEnabled(bool enabled);

signals:
    /** @brief 请求转到预设的机械角度，调用方不得再次应用补偿。 */
    void angleRequested(int mechanicalAngle);

private:
    void updateAngles();

    struct Preset {
        int angle;
        QWidget *cell;
        QPushButton *button;
        AngleReading *reading;
    };
    AppSettings::Axis m_axis;
    int m_offset = 0;
    bool m_shortestPathEnabled = false;
    std::vector<Preset> m_presets;
};
