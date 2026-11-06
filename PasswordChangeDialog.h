#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

/** @brief 收集并校验新的网络管理密码。 */
class PasswordChangeDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordChangeDialog(QWidget *parent = nullptr);
    /** @return 用户确认后的新管理密码。 */
    QString password() const;

private:
    void updateControls();

    QLineEdit *m_password;
    QLineEdit *m_confirmation;
    QPushButton *m_change;
};
