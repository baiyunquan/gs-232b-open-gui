#include "CrDelimitedBuffer.h"

QList<QByteArray> CrDelimitedBuffer::append(const QByteArray &bytes)
{
    QList<QByteArray> completed;
    for (const char byte : bytes) {
        if (m_skipLfAfterCr) {
            m_skipLfAfterCr = false;
            if (byte == '\n')
                continue;
        }

        if (byte == '\r') {
            m_pending.append(byte);
            completed.append(m_pending);
            m_pending.clear();
            m_skipLfAfterCr = true;
        } else {
            m_pending.append(byte);
        }
    }
    return completed;
}

void CrDelimitedBuffer::clear()
{
    m_pending.clear();
    m_skipLfAfterCr = false;
}

qsizetype CrDelimitedBuffer::pendingSize() const
{
    return m_pending.size();
}
