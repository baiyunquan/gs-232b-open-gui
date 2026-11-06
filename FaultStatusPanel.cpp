#include "FaultStatusPanel.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

FaultStatusPanel::FaultStatusPanel(FaultStatusMonitor *monitor, QWidget *parent)
    : QGroupBox(tr("FAULT STATUS"), parent), m_monitor(monitor)
{
    setObjectName(QStringLiteral("faultStatusPanel"));
    auto *layout = new QVBoxLayout(this);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("faultStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_clear = new QPushButton(tr("Clear fault"), this);
    m_clear->setObjectName(QStringLiteral("clearFault"));
    layout->addWidget(m_clear);
    m_result = new QLabel(this);
    m_result->setObjectName(QStringLiteral("faultClearResult"));
    m_result->setTextFormat(Qt::PlainText);
    m_result->setWordWrap(true);
    layout->addWidget(m_result);
    connect(m_clear, &QPushButton::clicked, this, [this] {
        m_result->clear();
        m_result->hide();
        m_monitor->clearFault();
    });
    connect(m_monitor, &FaultStatusMonitor::stateChanged, this, &FaultStatusPanel::updateStatus);
    connect(m_monitor, &FaultStatusMonitor::clearFinished, this, &FaultStatusPanel::showClearResult);
    updateStatus();
}

void FaultStatusPanel::updateStatus()
{
    if (!m_monitor->isEnabled()) {
        m_status->setText(tr("Connect directly to read status"));
        m_result->clear();
        m_result->hide();
    } else if (m_monitor->isClearing()) {
        m_status->setText(tr("Clearing fault…"));
    } else if (m_monitor->fault() == FaultStatusMonitor::Fault::Unknown) {
        m_status->setText(m_monitor->isReading() ? tr("Reading status…")
                                                : tr("Unable to read device status"));
    } else {
        const QString description = faultDescription();
        const int axes = m_monitor->axisMask();
        if (m_monitor->fault() == FaultStatusMonitor::Fault::None || axes == 0)
            m_status->setText(tr("%1").arg(description));
        else if (axes == 1)
            m_status->setText(tr("Azimuth: %1").arg(description));
        else if (axes == 2)
            m_status->setText(tr("Elevation: %1").arg(description));
        else
            m_status->setText(tr("Both axes: %1").arg(description));
    }
    m_clear->setEnabled(m_monitor->canClear());
}

QString FaultStatusPanel::faultDescription() const
{
    using Fault = FaultStatusMonitor::Fault;
    switch (m_monitor->fault()) {
    case Fault::None: return tr("No faults");
    case Fault::Sample: return tr("Position feedback unavailable");
    case Fault::Output: return tr("Motor output fault");
    case Fault::Limit: return tr("Travel limit fault");
    case Fault::FeedbackDirection: return tr("Unexpected rotation direction");
    case Fault::NoProgress: return tr("Rotator is not moving");
    case Fault::RunTimeout: return tr("Motion timed out");
    case Fault::SequenceLate: return tr("Sequence timing fault");
    case Fault::SequenceTarget: return tr("Sequence target unavailable");
    default: return tr("Device fault");
    }
}

void FaultStatusPanel::showClearResult(FaultStatusMonitor::ClearResult result)
{
    using Result = FaultStatusMonitor::ClearResult;
    switch (result) {
    case Result::Succeeded:
        m_result->clear();
        m_result->hide();
        return;
    case Result::Busy:
        m_result->setText(tr("Stop motion and finish pending operations before clearing"));
        break;
    case Result::NotOwner:
        m_result->setText(tr("Another controller is using the device"));
        break;
    case Result::NotAuthorized:
        m_result->setText(tr("Clearing is not permitted on this connection"));
        break;
    case Result::FaultPersists:
        m_result->setText(tr("Fault persists; check feedback and hardware"));
        break;
    case Result::TimedOut:
        m_result->setText(tr("Clear timed out; checking device status"));
        break;
    case Result::Failed:
        m_result->setText(tr("Could not clear the fault"));
        break;
    }
    m_result->show();
}
