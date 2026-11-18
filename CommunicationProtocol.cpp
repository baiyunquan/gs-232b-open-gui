#include "CommunicationProtocol.h"

#include <QDeadlineTimer>
#include <QAbstractSocket>
#include <QEventLoop>
#include <QSerialPort>
#include <QSignalBlocker>
#include <QTimer>
#include <QWebSocket>

namespace Communication {

namespace {

QString endpointPath(WebSocketEndpoint endpoint)
{
    return endpoint == WebSocketEndpoint::Manage ? QStringLiteral("/ws/manage")
                                                 : QStringLiteral("/ws/gs232b");
}

QUrl normalizedAddress(const QString &address)
{
    QString value = address.trimmed();
    if (value.isEmpty())
        return {};
    if (!value.contains(QStringLiteral("://")))
        value.prepend(QStringLiteral("ws://"));
    return QUrl::fromUserInput(value);
}

} // namespace

QUrl makeWebSocketUrl(const QString &address, WebSocketEndpoint endpoint)
{
    QUrl url = normalizedAddress(address);
    if (!url.isValid() || url.host().isEmpty())
        return {};

    if (url.scheme() != QStringLiteral("ws") && url.scheme() != QStringLiteral("wss"))
        return {};

    url.setPath(endpointPath(endpoint));
    url.setQuery({});
    url.setFragment({});
    return url;
}

QByteArray encodeCommand(const QString &command)
{
    QByteArray bytes = command.toUtf8();
    while (bytes.endsWith('\r') || bytes.endsWith('\n'))
        bytes.chop(1);
    bytes.append('\r');
    return bytes;
}

Transport::Transport(QObject *parent)
    : QObject(parent), m_serial(new QSerialPort(this)),
      m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(m_serial, &QSerialPort::readyRead, this, &Transport::onSerialReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &Transport::onSerialError);
    connect(m_webSocket, &QWebSocket::connected, this, &Transport::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &Transport::onWebSocketDisconnected);
    connect(m_webSocket, &QWebSocket::bytesWritten, this, [this](qint64 count) {
        Q_UNUSED(count)
        if (m_webSocket->bytesToWrite() == 0)
            m_webSocketWritePending = false;
    });
    connect(m_webSocket, &QWebSocket::textMessageReceived,
            this, &Transport::onWebSocketTextMessage);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived,
            this, &Transport::onWebSocketBinaryMessage);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(m_webSocket, &QWebSocket::errorOccurred, this, &Transport::onWebSocketError);
#else
    connect(m_webSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
            [this](QAbstractSocket::SocketError) { onWebSocketError(); });
#endif
}

Transport::~Transport()
{
    // NOTE: 父窗口析构时界面子控件可能已销毁，关闭传输不再发出状态回调。
    const QSignalBlocker blocker(this);
    close();
}

bool Transport::open(const ConnectionOptions &options)
{
    close();
    m_options = options;
    m_opening = true;

    if (options.kind == TransportKind::Serial) {
        if (options.serialPort.trimmed().isEmpty()) {
            reportError(tr("Serial port is empty"));
            markDisconnected();
            return false;
        }
        m_serial->setPortName(options.serialPort);
        m_serial->setBaudRate(options.baudRate);
        m_serial->setDataBits(QSerialPort::Data8);
        m_serial->setParity(QSerialPort::NoParity);
        m_serial->setStopBits(QSerialPort::OneStop);
        m_serial->setFlowControl(QSerialPort::NoFlowControl);
        if (!m_serial->open(QIODevice::ReadWrite)) {
            reportError(m_serial->errorString());
            markDisconnected();
            return false;
        }
        markConnected();
        return true;
    }

    const QUrl url = makeWebSocketUrl(options.address, options.endpoint);
    if (!url.isValid()) {
        reportError(tr("Invalid WebSocket address"));
        markDisconnected();
        return false;
    }
    m_webSocket->open(url);
    return true;
}

void Transport::close()
{
    QString serialError;
    if (m_serial->isOpen()) {
        // NOTE: close 会丢弃 Qt 写缓存；最多等待一秒，确保点动停止字节在断开前发出。
        // NOTE: 排空期间不向界面交付串口回调，避免关闭流程被读数、错误或弹窗重入。
        const QSignalBlocker blocker(m_serial);
        QDeadlineTimer deadline(1000);
        while (m_serial->isOpen() && m_serial->bytesToWrite() > 0) {
            if (!m_serial->waitForBytesWritten(int(deadline.remainingTime()))) {
                serialError = m_serial->errorString();
                break;
            }
        }
        m_serial->close();
    }
    if (m_webSocket->isValid() || m_webSocket->state() != QAbstractSocket::UnconnectedState) {
        // Drain a final stop command before closing, up to one second for a stalled peer.
        const bool hadPendingWrite = m_webSocketWritePending || m_webSocket->bytesToWrite() > 0;
        QDeadlineTimer deadline(1000);
        while ((m_webSocketWritePending || m_webSocket->bytesToWrite() > 0) &&
               deadline.remainingTime() > 0) {
            QEventLoop waitForWrite;
            QTimer timeout;
            timeout.setSingleShot(true);
            connect(m_webSocket, &QWebSocket::bytesWritten,
                    &waitForWrite, &QEventLoop::quit);
            connect(m_webSocket, &QWebSocket::disconnected,
                    &waitForWrite, &QEventLoop::quit);
            connect(&timeout, &QTimer::timeout,
                    &waitForWrite, &QEventLoop::quit);
            m_webSocket->flush();
            if (!m_webSocketWritePending && m_webSocket->bytesToWrite() == 0)
                break;
            timeout.start(qMax(1, int(deadline.remainingTime())));
            waitForWrite.exec(QEventLoop::ExcludeUserInputEvents);
        }
        if (hadPendingWrite && !m_webSocketWritePending && m_webSocket->bytesToWrite() == 0) {
            // Let the peer's event loop consume the stop frame before Qt closes the TCP socket.
            QEventLoop deliverPendingFrame;
            QTimer settleDelay;
            settleDelay.setSingleShot(true);
            connect(&settleDelay, &QTimer::timeout,
                    &deliverPendingFrame, &QEventLoop::quit);
            settleDelay.start(50);
            deliverPendingFrame.exec(QEventLoop::ExcludeUserInputEvents);
        }
        m_webSocket->close();
    }
    markDisconnected();
    if (!serialError.isEmpty())
        reportError(serialError);
}

bool Transport::isOpen() const
{
    return m_open;
}

TransportKind Transport::kind() const
{
    return m_options.kind;
}

WebSocketEndpoint Transport::endpoint() const
{
    return m_options.endpoint;
}

bool Transport::sendCommand(const QString &command, bool reportAsSent)
{
    const QByteArray bytes = encodeCommand(command);
    if (m_options.kind != TransportKind::WebSocket)
        return sendBytes(bytes, reportAsSent);
    if (!m_open) {
        reportError(QStringLiteral("Transport is not connected"));
        return false;
    }

    // NOTE: 终端命令使用文本帧；sendBytes 保留原始二进制帧给其他协议调用方。
    m_webSocketWritePending = true;
    const qint64 written = m_webSocket->sendTextMessage(QString::fromUtf8(bytes));
    if (written < 0) {
        m_webSocketWritePending = false;
        reportError(QStringLiteral("WebSocket send failed"));
        return false;
    }
    if (reportAsSent)
        emit bytesSent(bytes);
    return true;
}

bool Transport::sendBytes(const QByteArray &bytes, bool reportAsSent)
{
    if (!m_open) {
        reportError(QStringLiteral("Transport is not connected"));
        return false;
    }

    if (m_options.kind == TransportKind::Serial) {
        if (m_serial->write(bytes) < 0) {
            reportError(m_serial->errorString());
            return false;
        }
        if (reportAsSent)
            emit bytesSent(bytes);
        return true;
    }

    m_webSocketWritePending = true;
    if (m_webSocket->sendBinaryMessage(bytes) < 0) {
        m_webSocketWritePending = false;
        reportError(QStringLiteral("WebSocket send failed"));
        return false;
    }
    if (reportAsSent)
        emit bytesSent(bytes);
    return true;
}

void Transport::onSerialReadyRead()
{
    const QByteArray bytes = m_serial->readAll();
    if (!bytes.isEmpty())
        emit bytesReceived(bytes);
}

void Transport::onSerialError()
{
    const auto error = m_serial->error();
    if (error == QSerialPort::NoError)
        return;
    reportError(m_serial->errorString());
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        if (m_serial->isOpen())
            m_serial->close();
        markDisconnected();
    }
}

void Transport::onWebSocketConnected()
{
    markConnected();
}

void Transport::onWebSocketDisconnected()
{
    markDisconnected();
}

void Transport::onWebSocketTextMessage(const QString &message)
{
    const QByteArray bytes = message.toUtf8();
    if (!bytes.isEmpty())
        emit bytesReceived(bytes);
}

void Transport::onWebSocketBinaryMessage(const QByteArray &message)
{
    if (!message.isEmpty())
        emit bytesReceived(message);
}

void Transport::onWebSocketError()
{
    reportError(m_webSocket->errorString());
}

void Transport::markConnected()
{
    m_opening = false;
    if (m_open)
        return;
    m_open = true;
    emit connected();
}

void Transport::markDisconnected()
{
    const bool wasActive = m_open || m_opening;
    m_open = false;
    m_opening = false;
    m_webSocketWritePending = false;
    if (wasActive)
        emit disconnected();
}

void Transport::reportError(const QString &error)
{
    if (!error.isEmpty())
        emit errorOccurred(error);
}

} // namespace Communication
