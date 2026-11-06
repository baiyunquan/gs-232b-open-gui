#include "VirtualSerialEndpoint.h"

#include <QSerialPort>

class VirtualSerialEndpoint::Impl
{
public:
    QSerialPort serial;
};

VirtualSerialEndpoint::VirtualSerialEndpoint(QObject *parent)
    : QObject(parent), m_impl(std::make_unique<Impl>())
{
    connect(&m_impl->serial, &QSerialPort::readyRead, this, [this] {
        const QByteArray bytes = m_impl->serial.readAll();
        if (!bytes.isEmpty())
            emit bytesReceived(bytes);
    });
    connect(&m_impl->serial, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
                if (error == QSerialPort::NoError)
                    return;
                emit errorOccurred(m_impl->serial.errorString());
                if (error == QSerialPort::ResourceError ||
                    error == QSerialPort::DeviceNotFoundError) {
                    const bool wasRunning = m_running;
                    m_impl->serial.close();
                    m_running = false;
                    m_clientPortName.clear();
                    if (wasRunning)
                        emit stopped();
                }
            });
}

VirtualSerialEndpoint::~VirtualSerialEndpoint()
{
    stop();
}

bool VirtualSerialEndpoint::start(const VirtualSerialOptions &options)
{
    stop();
    const QString applicationPort = options.applicationPort.trimmed();
    const QString clientPort = options.clientPort.trimmed();
    if (applicationPort.isEmpty() || clientPort.isEmpty() ||
        applicationPort.compare(clientPort, Qt::CaseInsensitive) == 0) {
        emit errorOccurred(QStringLiteral(
            "Configure distinct application and client ports in the virtual COM pair driver"));
        return false;
    }

    m_impl->serial.setPortName(applicationPort);
    if (!m_impl->serial.setBaudRate(options.baudRate) ||
        !m_impl->serial.setDataBits(QSerialPort::Data8) ||
        !m_impl->serial.setParity(QSerialPort::NoParity) ||
        !m_impl->serial.setStopBits(QSerialPort::OneStop) ||
        !m_impl->serial.setFlowControl(QSerialPort::NoFlowControl) ||
        !m_impl->serial.open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_impl->serial.errorString());
        m_impl->serial.close();
        return false;
    }

    m_clientPortName = clientPort;
    m_running = true;
    emit started(m_clientPortName);
    return true;
}

void VirtualSerialEndpoint::stop()
{
    const bool wasRunning = m_running;
    m_running = false;
    m_clientPortName.clear();
    if (m_impl->serial.isOpen())
        m_impl->serial.close();
    if (wasRunning)
        emit stopped();
}

bool VirtualSerialEndpoint::isRunning() const
{
    return m_running;
}

QString VirtualSerialEndpoint::clientPortName() const
{
    return m_clientPortName;
}

bool VirtualSerialEndpoint::writeToClient(const QByteArray &bytes)
{
    if (!m_running) {
        emit errorOccurred(QStringLiteral("Virtual serial endpoint is not running"));
        return false;
    }
    if (bytes.isEmpty())
        return true;
    if (m_impl->serial.write(bytes) < 0) {
        emit errorOccurred(m_impl->serial.errorString());
        return false;
    }
    return true;
}
