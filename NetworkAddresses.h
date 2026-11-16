#pragma once

#include <QJsonObject>
#include <QString>

/** @brief 设备正在使用的 IPv4 地址；空字符串表示对应接口未连接或地址无效。 */
struct NetworkAddresses
{
    QString ethernet;
    QString station;
    QString accessPoint;

    /** @brief 从 NET GET 和 PROV STATUS 的成功响应 data 读取实际地址，不使用已保存配置。 */
    static NetworkAddresses fromResponses(const QJsonObject &network,
                                         const QJsonObject &provisioning);
};
