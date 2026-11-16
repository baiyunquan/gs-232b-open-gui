#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

/** @brief 编辑并保存 N1MM 精确匹配的转台名；保存失败保留输入，取消不修改配置。 */
class N1mmNameDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 载入当前名称，离线时也可配置。 */
    explicit N1mmNameDialog(QWidget *parent = nullptr);

private:
    void saveName();

    QLineEdit *m_name;
    QPushButton *m_save;
};
