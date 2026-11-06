#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QPushButton;

struct EthernetConfiguration
{
    QString mode;
    QString address;
    QString netmask;
    QString gateway;
    QString dns1;
    QString dns2;
};

/** @brief 编辑以太网 DHCP 或静态 IPv4 配置。 */
class NetworkPortConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkPortConfigDialog(QWidget *parent = nullptr);
    void setConnectionState(bool commandEnabled);

signals:
    /** @brief 用户提交以太网配置，事务命令由主窗口发送。 */
    void configurationRequested(const EthernetConfiguration &configuration);

private:
    void updateControls();

    bool m_commandEnabled = false;
    QComboBox *m_mode;
    QLineEdit *m_address;
    QLineEdit *m_netmask;
    QLineEdit *m_gateway;
    QLineEdit *m_dns1;
    QLineEdit *m_dns2;
    QPushButton *m_apply;
};
