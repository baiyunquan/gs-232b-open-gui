#include "GridTrackerController.h"
#include "GridTrackerProtocol.h"
#include "AzimuthScale.h"

#include <QHostAddress>
#include <QNetworkDatagram>

GridTrackerController::GridTrackerController(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &GridTrackerController::readDatagrams);
}

bool GridTrackerController::setEnabled(bool enabled)
{
    if (enabled == isEnabled())
        return true;
    if (!enabled) {
        m_socket.close();
        return true;
    }
    if (!m_socket.bind(QHostAddress::LocalHost, GridTracker::ControlPort, QUdpSocket::DontShareAddress)) {
        m_error = tr("Could not enable GridTracker integration: %1").arg(m_socket.errorString());
        emit errorOccurred(m_error);
        return false;
    }
    m_error.clear();
    return true;
}

bool GridTrackerController::isEnabled() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

QString GridTrackerController::errorString() const
{
    return m_error;
}

void GridTrackerController::setControlAvailable(bool available)
{
    if (available != m_controlAvailable) {
        // NOTE: 切换设备可控状态时清空内核队列，断线或校准前的请求不能在恢复后执行。
        while (isEnabled() && m_socket.hasPendingDatagrams())
            m_socket.receiveDatagram(0);
    }
    m_controlAvailable = available;
    if (!available) {
        m_azimuthAge.invalidate();
        m_elevationAge.invalidate();
    }
}

void GridTrackerController::setAzimuthPosition(int mechanicalAzimuth)
{
    if (!m_controlAvailable || mechanicalAzimuth < 0 || mechanicalAzimuth > AzimuthScale::Maximum) {
        m_azimuthAge.invalidate();
        return;
    }
    m_azimuth = mechanicalAzimuth;
    m_azimuthAge.start();
}

void GridTrackerController::setElevationPosition(int elevation)
{
    if (!m_controlAvailable || elevation < 0 || elevation > 180) {
        m_elevationAge.invalidate();
        return;
    }
    m_elevation = elevation;
    m_elevationAge.start();
}

void GridTrackerController::setAzimuthOffset(int degrees)
{
    m_offset = degrees;
}

void GridTrackerController::setShortestPathEnabled(bool enabled)
{
    m_shortestPath = enabled;
}

bool GridTrackerController::hasFreshPosition(const QElapsedTimer &age) const
{
    return m_controlAvailable && age.isValid() && age.elapsed() < 3000;
}

void GridTrackerController::readDatagrams()
{
    while (isEnabled() && m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram(8193);
        if (!datagram.senderAddress().isLoopback())
            continue;
        const auto requests = GridTracker::parseDatagram(datagram.data());
        if (!requests)
            continue;
        for (const auto &request : *requests) {
            if (!isEnabled() || !m_controlAvailable)
                break;
            if (request.kind != GridTracker::Request::Kind::Turn) {
                reportPosition(request.kind == GridTracker::Request::Kind::QueryAzimuth);
                continue;
            }
            if (!hasFreshPosition(m_azimuthAge)) {
                emit errorOccurred(tr("GridTracker turn command ignored: no recent rotator position is available."));
                continue;
            }
            int target = m_azimuth;
            if (request.azimuth) {
                const AzimuthScale scale(m_offset);
                const auto direct = scale.mechanical(*request.azimuth);
                target = !m_shortestPath && direct ? *direct
                    : scale.mechanicalBearing(*request.azimuth, m_shortestPath ? m_azimuth : 0);
            }
            // NOTE: 仅方位请求使用 M，保留仰角；仅仰角请求在 W 中保持实测机械方位。
            const QString command = request.elevation
                ? QStringLiteral("W%1 %2").arg(target, 3, 10, QLatin1Char('0'))
                      .arg(*request.elevation, 3, 10, QLatin1Char('0'))
                : QStringLiteral("M%1").arg(target, 3, 10, QLatin1Char('0'));
            emit commandRequested(command);
        }
    }
}

void GridTrackerController::reportPosition(bool azimuth)
{
    if (!hasFreshPosition(azimuth ? m_azimuthAge : m_elevationAge))
        return;
    const int angle = azimuth ? AzimuthScale::normalized(AzimuthScale(m_offset).corrected(m_azimuth))
                              : m_elevation;
    const QByteArray packet = (azimuth ? QByteArray("AZ:") : QByteArray("EL:")) + QByteArray::number(angle);
    if (m_socket.writeDatagram(packet, QHostAddress::LocalHost, GridTracker::ReportingPort) != packet.size()) {
        m_error = tr("Could not report the rotator position to GridTracker: %1").arg(m_socket.errorString());
        emit errorOccurred(m_error);
    }
}
