#pragma once

#include <QByteArray>
#include <QGroupBox>

class TerminalWidget;

/** @brief 显示真实收发流并提供手动命令输入的控制台。 */
class ConsolePanel final : public QGroupBox
{
    Q_OBJECT

public:
    /** @brief 创建空白、可选中文字的控制台。 */
    explicit ConsolePanel(QWidget *parent = nullptr);
    /** @brief 切换终端内的整行命令输入。 */
    void setCommandEnabled(bool enabled);

public slots:
    /** @brief 将收到的真实字节追加到终端。 */
    void appendReceived(const QByteArray &bytes);
    /** @brief 将底层接受发送的字节追加到终端；敏感命令由调用方传入脱敏副本。 */
    void appendSent(const QByteArray &bytes);
    /** @brief 将传输错误追加到终端，不伪装成设备响应。 */
    void appendError(const QString &error);
    /** @brief 将外部工具输出追加到终端，使用独立标记避免冒充设备数据。 */
    void appendProcessOutput(const QByteArray &bytes);
    /** @brief 将应用状态消息追加到终端，不标记成设备收发或错误。 */
    void appendNotice(const QString &message);

signals:
    /** @brief 用户请求发送一行命令，具体编码由传输协议模块负责。 */
    void commandRequested(const QString &command);

private:
    void appendTraffic(const QByteArray &bytes, bool outgoing);

    TerminalWidget *m_output;
    bool m_lineStart = true;
};
