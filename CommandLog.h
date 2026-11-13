#pragma once

#include <QByteArray>

namespace Communication {

/**
 * @brief 为单条已编码命令生成日志副本，遮蔽管理命令 JSON 中所有嵌套的 password 字段。
 * @param bytes 以 CR 结尾的单条命令；非管理命令按原始字节保留。
 * @return 可显示的命令；无法解析的管理命令参数整体替换为 ***，保留命令名。
 */
QByteArray commandForLog(const QByteArray &bytes);

} // namespace Communication
