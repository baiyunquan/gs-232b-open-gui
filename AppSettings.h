#pragma once

#include <QSettings>

/** @brief 应用本地偏好的统一入口，集中管理存储键、默认值和校验。 */
class AppSettings final
{
public:
    /** @brief 角度预设所属的转台轴。 */
    enum class Axis { Azimuth, Elevation };
    /** @brief 每个轴的预设数量。 */
    static constexpr int PresetCount = 8;

    /** @brief 使用当前用户的系统配置存储，沿用现有角度预设的位置。 */
    AppSettings();
    /** @brief 使用指定的独立 INI 文件，不影响默认配置存储。 */
    explicit AppSettings(const QString &fileName);

    /** @brief 读取控制台显示偏好；缺失或无效时默认隐藏。 */
    bool consoleVisible() const;
    /** @return 立即同步控制台显示偏好到存储是否成功。 */
    bool setConsoleVisible(bool visible);

    /** @brief 读取罗盘大圆图模式；缺失或无效时默认关闭。 */
    bool greatCircleMapEnabled() const;
    /** @return 立即同步大圆图模式偏好到存储是否成功。 */
    bool setGreatCircleMapEnabled(bool enabled);

    /** @brief 读取本站 Maidenhead 网格；返回大写，缺失或无效时返回空字符串。 */
    QString stationGrid() const;
    /**
     * @brief 保存本站网格，自动去除首尾空白并转为大写；空字符串表示清除。
     * @return 网格合法且立即同步到存储成功时返回 true；非法参数不写入。
     */
    bool setStationGrid(const QString &grid);
    /** @brief 校验 4、6、8 位 Maidenhead 网格，忽略大小写和首尾空白；空值无效。 */
    static bool isValidStationGrid(const QString &grid);

    /**
     * @brief 读取角度预设；缺失或无效时返回该位置的默认角度。
     * @param index 从 0 开始的预设位置；越界时返回 0。
     */
    int presetAngle(Axis axis, int index) const;
    /**
     * @brief 保存角度预设，方位范围为 0–450 度，仰角范围为 0–180 度。
     * @param index 从 0 开始的预设位置，必须小于 PresetCount。
     * @return 参数合法且立即同步到存储成功时返回 true；非法参数不写入。
     */
    bool setPresetAngle(Axis axis, int index, int angle);

private:
    bool storeValue(const QString &key, const QVariant &value);

    QSettings m_settings;
};
