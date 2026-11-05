#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QPushButton;

/** @brief 选择下载串口和固件并进行二次确认的升级窗口原型。 */
class FirmwareUpgradeDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建独立升级窗口；不会枚举设备或执行烧录。 */
    explicit FirmwareUpgradeDialog(QWidget *parent = nullptr);

signals:
    /** @brief 用户确认烧录后请求主窗口执行下载工具。 */
    void flashRequested(const QString &port, const QString &file);

private:
    void selectFirmware();
    void confirmFlash();

    QComboBox *m_port;
    QLineEdit *m_file;
    QPushButton *m_flash;
};
