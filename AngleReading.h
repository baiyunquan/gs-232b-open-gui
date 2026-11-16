#pragma once

#include <QLabel>

class QLineEdit;

/** @brief 支持原位输入角度的读数控件。 */
class AngleReading final : public QLabel
{
    Q_OBJECT

public:
    /** @brief 创建范围为 0 至 maximum 度的读数，默认禁止输入。 */
    explicit AngleReading(int maximum, QWidget *parent = nullptr);
    /** @brief 更新显示读数；保留正在编辑的角度。 */
    void setAngle(int angle);
    /** @brief 切换输入权限；禁止输入时取消尚未提交的编辑。 */
    void setEditingEnabled(bool enabled);
    /** @brief 开始原位编辑；禁止输入或已在编辑时不执行操作。 */
    void beginEditing();

signals:
    /** @brief 用户按回车确认合法范围内的角度，由调用方决定用途。 */
    void angleRequested(int angle);
    /** @brief 提交或取消编辑后发出，先于 angleRequested，供宿主恢复显示。 */
    void editingFinished();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void finishEditing(bool submit);

    QLineEdit *m_editor;
    int m_maximum;
    int m_angle = 0;
    bool m_editingEnabled = false;
    bool m_editing = false;
};
