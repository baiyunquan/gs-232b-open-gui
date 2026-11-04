#pragma once

#include <QDialog>

/** @brief 仅在具体管理写入需要认证时打开的独立密钥窗口。 */
class ManagementKeyDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建密码回显的密钥输入视图；不在连接时自动调用。 */
    explicit ManagementKeyDialog(QWidget *parent = nullptr);
};
