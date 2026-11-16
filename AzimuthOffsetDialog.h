#pragma once

#include <QDialog>

class QLabel;
class QSpinBox;

/** @brief 编辑应用本地的方位角补偿；成功保存后通知宿主立即刷新方位显示与输入范围。 */
class AzimuthOffsetDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 载入已保存的整数补偿，离线时也可设置。 */
    explicit AzimuthOffsetDialog(QWidget *parent = nullptr);

private:
    void updateRange();
    void saveOffset();

    QSpinBox *m_offset;
    QLabel *m_range;
};
