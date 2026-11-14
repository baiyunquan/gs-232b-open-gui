#pragma once

#include "CommandLine.h"
#include "TerminalOutputBuffer.h"

#include <qtpyte/terminalwidget.h>

class QContextMenuEvent;
class QInputMethodEvent;

/** @brief 为原生终端模拟器提供设备命令的本地整行编辑；不启动本机 shell。 */
class TerminalWidget final : public qtpyte::TerminalWidget
{
    Q_OBJECT

public:
    /** @brief 创建可选择、可滚动且默认禁止发送命令的终端。 */
    explicit TerminalWidget(QWidget *parent = nullptr);
    /** @brief 切换终端内输入；关闭时清除草稿和历史。 */
    void setCommandEnabled(bool enabled);
    /**
     * @brief 追加输出，保留未提交输入；查看历史时保持阅读位置。
     * @param text 使用 LF 换行的文本；调用方负责将 CR 和 CRLF 统一为 LF。
     */
    void appendOutput(const QString &text);
    /** @brief 记录已经由调用方确认不含凭据的历史命令。 */
    void rememberCommand(const QString &command);

signals:
    /** @brief 用户按回车提交完整命令；显示回显由真实发送结果驱动。 */
    void commandRequested(const QString &command);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPoint outputCursor();
    void captureOutputCursor();
    void hidePrompt();
    void showPrompt();
    void redrawPrompt();
    void handleTerminalData(const QByteArray &data);

    CommandLine m_command;
    TerminalOutputBuffer m_outputBuffer;
    int m_outputLine = 0;
    int m_outputColumn = 1;
    QPoint m_cursorReply{1, 1};
    bool m_queryingCursor = false;
    bool m_commandEnabled = false;
    bool m_promptVisible = false;
    bool m_softWrapped = false;
};
