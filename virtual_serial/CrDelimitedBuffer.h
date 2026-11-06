#pragma once

#include <QByteArray>
#include <QList>

/** @brief 按 CR 拆分任意大小字节块，容忍 CRLF，并保留未完成行。 */
class CrDelimitedBuffer final
{
public:
    /** @return 本次输入中所有完整的、包含 CR 的行。 */
    QList<QByteArray> append(const QByteArray &bytes);
    /** @brief 丢弃未完成行和 CRLF 可选后缀状态。 */
    void clear();
    /** @return 当前未完成行的字节数。 */
    qsizetype pendingSize() const;

private:
    QByteArray m_pending;
    bool m_skipLfAfterCr = false;
};
