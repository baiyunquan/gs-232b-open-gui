#include "ControlPanel.h"
#include "MotionButtons.h"

#include <QTimer>

void ControlPanel::setupMotionButtons()
{
    const auto wireAxis = [this](MotionButtons *buttons, const QString &negative,
                                  const QString &positive, const QString &stop) {
        connect(buttons, &MotionButtons::motionStarted, this,
                [this, negative, positive](bool forward) {
                    m_elevationCommandTimer->stop();
                    emit commandRequested(forward ? positive : negative);
                });
        connect(buttons, &MotionButtons::motionStopped, this, [this, stop] {
            m_elevationCommandTimer->stop();
            emit commandRequested(stop);
        });
        connect(buttons, &MotionButtons::stopAllRequested, this, [this] {
            cancelPendingMotion();
            emit commandRequested(QStringLiteral("S"));
        });
    };
    wireAxis(m_azimuthMotion, QStringLiteral("L"), QStringLiteral("R"), QStringLiteral("A"));
    wireAxis(m_elevationMotion, QStringLiteral("D"), QStringLiteral("U"), QStringLiteral("E"));
}

void ControlPanel::requestAzimuth(int mechanical)
{
    if (!m_commandEnabled)
        return;
    cancelPendingMotion();
    emit commandRequested(QStringLiteral("M%1").arg(mechanical, 3, 10, QLatin1Char('0')));
}

void ControlPanel::requestElevation(int angle)
{
    if (!m_commandEnabled || !m_elevationSupported)
        return;
    cancelPendingMotion();
    // NOTE: 仰角单轴定位保留实测机械方位，不能应用 LP 或方位补偿。
    emit commandRequested(QStringLiteral("W%1 %2")
                              .arg(m_azimuthValue, 3, 10, QLatin1Char('0'))
                              .arg(angle, 3, 10, QLatin1Char('0')));
}

void ControlPanel::cancelPendingMotion()
{
    m_elevationCommandTimer->stop();
    m_azimuthMotion->cancelMotion();
    m_elevationMotion->cancelMotion();
}
