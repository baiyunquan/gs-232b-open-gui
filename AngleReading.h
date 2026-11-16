#pragma once

#include <QLabel>

#include <functional>
#include <optional>

class QLineEdit;
class QRegularExpressionValidator;

/** @brief 支持原位输入角度的读数控件。 */
class AngleReading final : public QLabel
{
    Q_OBJECT

public:
    /** @brief 扩展输入的解析结果；失败时 angle 为空，error 为可展示的本地化原因。 */
    struct InputResult {
        std::optional<qint64> angle;
        QString error;
    };
    using InputResolver = std::function<InputResult(const QString &)>;

    /** @brief 创建范围为 0 至 maximum 度的读数，默认禁止输入。 */
    explicit AngleReading(int maximum, QWidget *parent = nullptr);
    /**
     * @brief 设置角度闭区间并取消尚未提交的编辑；要求 minimum <= maximum。
     * @param circular 为 true 时须使用 0–359°，读数和整数输入按 360 取模。
     */
    void setRange(qint64 minimum, qint64 maximum, bool circular = false);
    /** @brief 更新显示读数；保留正在编辑的角度。 */
    void setAngle(qint64 angle);
    /** @brief 切换输入权限；禁止输入时取消尚未提交的编辑。 */
    void setEditingEnabled(bool enabled);
    /**
     * @brief 扩展非数字输入；同步调用 resolver，成功角度仍须位于当前闭区间内。
     * @param hint 描述可用输入的本地化提示；传空 resolver 恢复纯数字编辑。
     */
    void setInputResolver(InputResolver resolver, const QString &hint);
    /** @brief 开始原位编辑；禁止输入或已在编辑时不执行操作。 */
    void beginEditing();

signals:
    /** @brief 用户按回车确认合法范围内的角度（含扩展输入解析结果），由调用方决定用途。 */
    void angleRequested(qint64 angle);
    /** @brief 提交输入失败并保留编辑时发出，供宿主持续展示本地化错误。 */
    void inputRejected(const QString &message);
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
    QRegularExpressionValidator *m_numericValidator;
    InputResolver m_inputResolver;
    QString m_inputHint;
    qint64 m_minimum = 0;
    qint64 m_maximum;
    qint64 m_angle = 0;
    bool m_editingEnabled = false;
    bool m_editing = false;
    bool m_circular = false;
};
