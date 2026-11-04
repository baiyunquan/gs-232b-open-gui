#pragma once

#include <QWidget>

class QComboBox;
class QGroupBox;
class QPushButton;

/** @brief 旋转器型号和方位速度设置视图，不包含网络配置或 M/W 命令表单。 */
class RotorSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 按固件型号表创建设置面板，初始禁用命令控件。 */
    explicit RotorSettingsPanel(QWidget *parent = nullptr);
    /** @brief 按实际连接和透传状态统一切换命令控件。 */
    void setCommandEnabled(bool enabled);

signals:
    /** @brief 用户请求写入所选型号；网络传输的管理认证由宿主窗口按需处理。 */
    void modelApplyRequested();

private:
    QComboBox *m_model;
    QPushButton *m_apply;
    QGroupBox *m_speed;
};
