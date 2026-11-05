#pragma once

#include <QString>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;

/** @brief 旋转器型号、方位速度和校准设置视图，不包含网络配置或 M/W 命令表单。 */
class RotorSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 按固件型号表创建设置面板，初始禁用命令控件。 */
    explicit RotorSettingsPanel(QWidget *parent = nullptr);
    /** @brief 按实际连接和透传状态统一切换命令控件。 */
    void setCommandEnabled(bool enabled);
    /** @return 当前下拉框选择的固件型号 ID。 */
    int selectedModelId() const;

signals:
    /** @brief 用户通过速度控件请求发送一条 GS-232B 命令。 */
    void commandRequested(const QString &command);
    /** @brief 用户请求写入所选型号；网络传输的管理认证由宿主窗口按需处理。 */
    void modelApplyRequested();
    /** @brief 用户请求方位零点校准。 */
    void azimuthZeroCalibrationRequested();
    /** @brief 用户请求俯仰零点校准。 */
    void elevationZeroCalibrationRequested();

private:
    QComboBox *m_model;
    QPushButton *m_apply;
    QPushButton *m_azimuthZero;
    QPushButton *m_elevationZero;
    QGroupBox *m_speed;
};
