#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

/** @brief 编辑并保存本站 Maidenhead 网格的本地设置窗口。 */
class StationGridDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 恢复已保存的网格；保存成功才关闭窗口，取消不修改配置。 */
    explicit StationGridDialog(QWidget *parent = nullptr);

private:
    void updateControls();
    void saveGrid();

    QLineEdit *m_grid;
    QPushButton *m_save;
};
