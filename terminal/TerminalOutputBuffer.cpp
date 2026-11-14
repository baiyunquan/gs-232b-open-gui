#include "TerminalOutputBuffer.h"

QString TerminalOutputBuffer::append(const QString &text)
{
    m_pending += text;
    qsizetype complete = m_pending.size();
    for (qsizetype i = 0; i < m_pending.size(); ++i) {
        if (m_pending.at(i) != QChar(0x1b))
            continue;
        const qsizetype start = i++;
        if (i < m_pending.size()) {
            const QChar type = m_pending.at(i++);
            if (type == QLatin1Char('[')) {
                while (i < m_pending.size() &&
                       (m_pending.at(i) < QLatin1Char('@') || m_pending.at(i) > QLatin1Char('~')))
                    ++i;
            } else if (QStringLiteral("]PX^_").contains(type)) {
                while (i < m_pending.size()) {
                    if (type == QLatin1Char(']') && m_pending.at(i) == QChar(0x07))
                        break;
                    if (m_pending.at(i) == QChar(0x1b) && i + 1 < m_pending.size() &&
                        m_pending.at(i + 1) == QLatin1Char('\\')) {
                        ++i;
                        break;
                    }
                    ++i;
                }
            } else {
                --i;
                while (i < m_pending.size() && m_pending.at(i) >= QLatin1Char(' ') &&
                       m_pending.at(i) <= QLatin1Char('/'))
                    ++i;
            }
        }
        if (i >= m_pending.size()) {
            complete = start;
            break;
        }
    }
    // NOTE: 未终止的控制字符串最多暂存 64 KiB；异常长时显示 ESC 符号，避免无限累积。
    if (m_pending.size() - complete > 65536) {
        m_pending.replace(QChar(0x1b), QChar(0x241b));
        complete = m_pending.size();
    }
    const QString result = m_pending.left(complete);
    m_pending.remove(0, complete);
    return result;
}
