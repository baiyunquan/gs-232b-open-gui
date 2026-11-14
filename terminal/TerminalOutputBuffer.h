#pragma once

#include <QString>

/** @brief 暂存跨数据块的 ANSI 控制序列，避免本地提示符重绘打断终端解析状态。 */
class TerminalOutputBuffer
{
public:
    /** @brief 追加输出并取出可安全与本地终端控制序列拼接的完整前缀。 */
    QString append(const QString &text);

private:
    QString m_pending;
};
