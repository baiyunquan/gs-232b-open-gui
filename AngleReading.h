#pragma once

#include <QLabel>

class QLineEdit;

/** @brief 点击后可原位输入目标角度的设备实测读数。 */
class AngleReading final : public QLabel
{
    Q_OBJECT

public:
    /** @brief 创建范围为 0 至 maximum 度的读数，默认禁止输入。 */
    explicit AngleReading(int maximum, QWidget *parent = nullptr);
    /** @brief 更新实测读数；保留正在编辑的目标角度。 */
    void setAngle(int angle);
    /** @brief 切换输入权限；禁止输入时取消尚未提交的编辑。 */
    void setEditingEnabled(bool enabled);

signals:
    /** @brief 用户按回车确认合法范围内的目标角度。 */
    void angleRequested(int angle);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void beginEditing();
    void finishEditing(bool submit);

    QLineEdit *m_editor;
    int m_maximum;
    int m_angle = 0;
    bool m_editingEnabled = false;
    bool m_editing = false;
};
