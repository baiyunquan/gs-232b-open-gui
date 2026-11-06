#include "VirtualSerialBridge.h"

VirtualSerialBridge::VirtualSerialBridge(QObject *parent)
    : QObject(parent)
{
    connect(&m_endpoint, &VirtualSerialEndpoint::started,
            this, &VirtualSerialBridge::started);
    connect(&m_endpoint, &VirtualSerialEndpoint::stopped,
            this, [this] {
                clearPendingData();
                emit stopped();
            });
    connect(&m_endpoint, &VirtualSerialEndpoint::bytesReceived,
            this, &VirtualSerialBridge::onSerialBytes);
    connect(&m_endpoint, &VirtualSerialEndpoint::errorOccurred,
            this, &VirtualSerialBridge::errorOccurred);
}

VirtualSerialBridge::~VirtualSerialBridge()
{
    stop();
}

bool VirtualSerialBridge::start(const VirtualSerialOptions &options)
{
    clearPendingData();
    return m_endpoint.start(options);
}

void VirtualSerialBridge::stop()
{
    clearPendingData();
    m_endpoint.stop();
}

bool VirtualSerialBridge::isRunning() const
{
    return m_endpoint.isRunning();
}

QString VirtualSerialBridge::clientPortName() const
{
    return m_endpoint.clientPortName();
}

void VirtualSerialBridge::feedWebSocketBytes(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;
    if (!m_endpoint.isRunning()) {
        emit errorOccurred(QStringLiteral("Virtual serial endpoint is not running"));
        return;
    }

    const QList<QByteArray> lines = m_webSocketInput.append(bytes);
    for (const QByteArray &line : lines) {
        if (!m_endpoint.writeToClient(line))
            return;
    }
}

void VirtualSerialBridge::clearPendingData()
{
    m_serialInput.clear();
    m_webSocketInput.clear();
}

void VirtualSerialBridge::onSerialBytes(const QByteArray &bytes)
{
    const QList<QByteArray> lines = m_serialInput.append(bytes);
    for (const QByteArray &line : lines)
        emit webSocketLineReady(line);
}
