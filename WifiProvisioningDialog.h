#pragma once

#include <QDialog>

class QPushButton;

/** @brief Wi-Fi 配网的独立 Widgets 对话框原型。 */
class WifiProvisioningDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建 SSID/口令输入视图，不建立网络连接。 */
    explicit WifiProvisioningDialog(QWidget *parent = nullptr);
    /** @brief 按连接/透传状态及传输类型更新配置按钮与按需认证路径。 */
    void setConnectionState(bool commandEnabled, bool networkMode);

private:
    bool m_networkMode = false;
    QPushButton *m_configure;
};
