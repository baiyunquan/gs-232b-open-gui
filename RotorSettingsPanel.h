#pragma once

#include <QString>
#include <QWidget>

class QGroupBox;

/** @brief 主界面的方位速度设置视图。 */
class RotorSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 创建 X1–X4 方位速度按钮，初始禁用命令控件。 */
    explicit RotorSettingsPanel(QWidget *parent = nullptr);
    /** @brief 按实际连接和透传状态统一切换命令控件。 */
    void setCommandEnabled(bool enabled);

signals:
    /** @brief 用户通过速度控件请求发送一条 GS-232B 命令。 */
    void commandRequested(const QString &command);

private:
    QGroupBox *m_speed;
};
