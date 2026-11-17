#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace N1mm {

inline constexpr quint16 ControlPort = 12040;
inline constexpr quint16 ReportingPort = 13010;
inline constexpr int MaximumNameLength = 64;

/** @brief N1MM 的方位控制请求；offset 是天线相对于旋转器的偏移，不包含 GUI 的设备补偿。 */
struct Command {
    enum class Kind { Turn, Stop };
    Kind kind = Kind::Turn;
    QStringList rotors;
    double azimuth = 0;
    double offset = 0;
    bool bidirectional = false;
};

/** @brief 校验去除首尾空白后的旋转器名，禁止逗号、@ 和控制字符等协议分隔符。 */
bool isValidRotorName(const QString &name);
/** @brief 解析官方 N1MMRotor 转向或嵌套 stop 报文；无声明的 XML 同样受支持，损坏报文返回空值。 */
std::optional<Command> parseCommand(const QByteArray &datagram);
/**
 * @brief 将 N1MM 天线目标转换为 0–450° 设备行程，应用天线偏移、本地补偿及双向天线选解。
 * @param current 最近一次设备实测方位；调用方须保证读数新鲜且在 0–450° 内。
 * @param shortestPath 重叠行程优先最近解；关闭时优先直接可达的连续角度，否则取首个等价解。
 */
int mechanicalTarget(const Command &command, int current, int localOffset, bool shortestPath);

} // namespace N1mm
