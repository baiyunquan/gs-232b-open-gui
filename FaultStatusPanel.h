#pragma once

#include "FaultStatusMonitor.h"

#include <QGroupBox>

class QLabel;
class QPushButton;

/** @brief 将故障与操作结果显示为短句，不向用户展示固件代码或诊断字段。 */
class FaultStatusPanel final : public QGroupBox
{
    Q_OBJECT

public:
    /** @brief 绑定宿主管理的故障监视器；监视器须覆盖面板的生命周期。 */
    explicit FaultStatusPanel(FaultStatusMonitor *monitor, QWidget *parent = nullptr);

private:
    void updateStatus();
    void showClearResult(FaultStatusMonitor::ClearResult result);
    QString faultDescription() const;

    FaultStatusMonitor *m_monitor;
    QLabel *m_status;
    QLabel *m_result;
    QPushButton *m_clear;
};
