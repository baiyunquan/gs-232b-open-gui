#pragma once

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

/** @brief 串口、WebSocket 和虚拟串口连接选项视图。 */
class ConnectionPanel final : public QGroupBox
{
    Q_OBJECT

public:
    /** @brief 创建断开状态的连接面板。 */
    explicit ConnectionPanel(QWidget *parent = nullptr);
    /** @return 是否已经收到传输后端的连接成功状态。 */
    bool isConnected() const;
    /** @return 当前是否选择 WebSocket 传输。 */
    bool isNetworkMode() const;
    /** @return 是否启用 WebSocket 到虚拟串口的透传模式。 */
    bool isVirtualComEnabled() const;
    /** @brief 由传输后端更新实际连接状态；点击连接按钮不直接改变此状态。 */
    void setConnected(bool connected);

signals:
    /** @brief 传输模式、透传选项或实际连接状态变更。 */
    void stateChanged();

private:
    void updateControls();

    bool m_connected = false;
    QRadioButton *m_serialMode;
    QRadioButton *m_networkMode;
    QComboBox *m_serialPort;
    QComboBox *m_baudRate;
    QLineEdit *m_address;
    QCheckBox *m_virtualCom;
    QComboBox *m_virtualPort;
    QPushButton *m_connect;
    QPushButton *m_disconnect;
};
