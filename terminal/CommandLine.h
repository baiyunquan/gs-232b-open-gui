#pragma once

#include <QStringList>

class QKeyEvent;

/** @brief 与终端绘制独立的整行命令编辑和有界历史记录。 */
class CommandLine
{
public:
    /** @brief 处理编辑、移动和历史按键；回车由调用方提交。 */
    void edit(const QKeyEvent &event);
    /** @brief 插入文本；换行转为空格，控制字符不作为命令执行。 */
    void insert(QString text);
    /** @brief 取出当前命令并清空草稿。 */
    QString take();
    /** @brief 记录允许保留的命令；调用方必须先排除含凭据的命令。 */
    void remember(const QString &command);
    /** @brief 清空当前输入和历史，供断开连接时调用。 */
    void reset();
    /** @return 当前命令草稿。 */
    const QString &text() const { return m_text; }
    /** @return 光标在 QString 中的位置，位于字素边界。 */
    int cursor() const { return m_cursor; }

private:
    int previousBoundary() const;
    int nextBoundary() const;
    void recall(int direction);

    QString m_text;
    QString m_draft;
    QStringList m_history;
    int m_cursor = 0;
    int m_historyIndex = 0;
};
