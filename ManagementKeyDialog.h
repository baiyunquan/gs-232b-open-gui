#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

/** @brief 仅在网络管理操作需要认证时打开的独立密码窗口。 */
class ManagementKeyDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建密码输入视图；不在连接时自动调用。 */
    explicit ManagementKeyDialog(QWidget *parent = nullptr);
    /** @return 用户输入的当前会话管理密码。 */
    QString password() const;

private:
    void updateControls();

    QLineEdit *m_password;
    QPushButton *m_continue;
};
