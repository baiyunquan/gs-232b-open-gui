#pragma once

#include <QGroupBox>

class QLineEdit;
class QPushButton;

/** @brief 显示真实收发流并预留手动命令输入的控制台。 */
class ConsolePanel final : public QGroupBox
{
    Q_OBJECT

public:
    /** @brief 创建空白、可选中文字的控制台。 */
    explicit ConsolePanel(QWidget *parent = nullptr);
    /** @brief 统一切换需要发送数据的输入框与按钮。 */
    void setCommandEnabled(bool enabled);

private:
    QLineEdit *m_input;
    QPushButton *m_send;
};
