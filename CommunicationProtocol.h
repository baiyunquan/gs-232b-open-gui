#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>

class QSerialPort;
class QWebSocket;

namespace Communication {

/** @brief 可选的底层传输方式。 */
enum class TransportKind {
    Serial,
    WebSocket,
};

/** @brief WebSocket 服务端点；调用方必须显式选择，不按命令内容自动路由。 */
enum class WebSocketEndpoint {
    Gs232b,
    Manage,
};

/** @brief 一次连接的参数。 */
struct ConnectionOptions {
    TransportKind kind = TransportKind::Serial;
    QString serialPort;
    int baudRate = 9600;
    QString address;
    WebSocketEndpoint endpoint = WebSocketEndpoint::Gs232b;
};

/** @brief 将地址和固定服务端点组合成 WebSocket URL。 */
QUrl makeWebSocketUrl(const QString &address, WebSocketEndpoint endpoint);

/** @brief 将控制台的一行命令编码为设备命令，尾部统一使用一个 CR。 */
QByteArray encodeCommand(const QString &command);

/** @brief 串口与 WebSocket 共用的异步原始字节传输接口。 */
class Transport final : public QObject
{
    Q_OBJECT

public:
    explicit Transport(QObject *parent = nullptr);
    ~Transport() override;

    /** @brief 开始连接；WebSocket 连接在 connected 信号后才算成功。 */
    bool open(const ConnectionOptions &options);
    /** @brief 关闭当前传输并释放底层资源。 */
    void close();
    /** @return 当前是否已建立可读写连接。 */
    bool isOpen() const;
    /** @return 当前连接使用的传输方式。 */
    TransportKind kind() const;
    /** @return 当前 WebSocket endpoint 选择。 */
    WebSocketEndpoint endpoint() const;

public slots:
    /** @brief 将一行控制台命令编码后发送。 */
    bool sendCommand(const QString &command);
    /** @brief 发送已编码的原始字节。 */
    bool sendBytes(const QByteArray &bytes);

signals:
    /** @brief 底层连接已真正建立。 */
    void connected();
    /** @brief 底层连接已断开。 */
    void disconnected();
    /** @brief 收到串口字节或 WebSocket 文本/二进制帧。 */
    void bytesReceived(const QByteArray &bytes);
    /** @brief 底层确认已接受待发送字节。 */
    void bytesSent(const QByteArray &bytes);
    /** @brief 传输层错误，参数为可直接展示的底层错误。 */
    void errorOccurred(const QString &error);

private slots:
    void onSerialReadyRead();
    void onSerialError();
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessage(const QString &message);
    void onWebSocketBinaryMessage(const QByteArray &message);
    void onWebSocketError();

private:
    void markConnected();
    void markDisconnected();
    void reportError(const QString &error);

    QSerialPort *m_serial;
    QWebSocket *m_webSocket;
    ConnectionOptions m_options;
    bool m_open = false;
    bool m_opening = false;
};

} // namespace Communication
