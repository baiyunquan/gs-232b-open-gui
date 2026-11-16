#pragma once

#include <QtGlobal>

#include <optional>

/** @brief 在设备的 0–450° 行程与补偿后的连续方位之间换算，圆周方位单独选解。 */
class AzimuthScale final
{
public:
    static constexpr int Maximum = 450;

    /** @brief 补偿值为带符号整数度；中间运算使用 64 位，保留完整行程而不取模。 */
    explicit AzimuthScale(int offset = 0) : m_offset(offset) {}

    /** @return 对应设备行程的连续方位，可为负数或超过 360°。 */
    qint64 corrected(int mechanical) const { return qint64(mechanical) + m_offset; }
    /** @return 补偿后的逆时针端点。 */
    qint64 minimum() const { return corrected(0); }
    /** @return 补偿后的顺时针端点。 */
    qint64 maximum() const { return corrected(Maximum); }

    /** @return 连续方位对应的设备目标；越过任一端点时返回空值，不钳位或绕回。 */
    std::optional<int> mechanical(qint64 corrected) const
    {
        if (corrected < minimum() || corrected > maximum())
            return std::nullopt;
        return int(corrected - m_offset);
    }

    /** @brief 把圆周方位转换为可达行程；重叠区域取距当前位置最近的解，等距取较小值。 */
    int mechanicalBearing(int bearing, int current) const
    {
        // NOTE: 先在 64 位中减补偿，再修正 C++ 负余数；450° 行程保证至少有一个解。
        const int first = int(((qint64(bearing) - m_offset) % 360 + 360) % 360);
        const int second = first + 360;
        if (second <= Maximum && qAbs(qint64(second) - current) < qAbs(qint64(first) - current))
            return second;
        return first;
    }

private:
    int m_offset;
};
