#include "ControlPanel.h"
#include "Compass.h"

#include <QFrame>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

#include <array>

namespace {

QString formatAngle(int angle, int maximum)
{
    return QStringLiteral("%1").arg(qBound(0, angle, maximum), 3, 10, QLatin1Char('0'));
}

QWidget *makePresets(const QString &prefix, QWidget *parent)
{
    auto *presets = new QWidget(parent);
    auto *grid = new QGridLayout(presets);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int index = 0; index < 8; ++index) {
        const int defaultAngle = prefix == QStringLiteral("azimuthPreset")
                                     ? index * 45
                                     : std::array<int, 8>{0, 25, 50, 75, 100, 125, 150, 180}[index];
        QSettings settings(QStringLiteral("BG5GEB"), QStringLiteral("GS-232B Open Control"));
        auto *button = new QPushButton(
            formatAngle(settings.value(prefix + QString::number(index), defaultAngle).toInt(),
                        prefix == QStringLiteral("azimuthPreset") ? 450 : 180),
            presets);
        button->setObjectName(prefix + QString::number(index));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        button->setProperty("presetIndex", index);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setToolTip(QCoreApplication::translate(
            "ControlPanel", "Right-click to edit this preset (AI)"));
        grid->addWidget(button, index / 2, index % 2);
    }
    return presets;
}

} // namespace

ControlPanel::ControlPanel(QWidget *parent) : QGroupBox(tr("CONTROL"), parent)
{
    setObjectName(QStringLiteral("controlPanel"));
    auto *layout = new QVBoxLayout(this);
    auto *azimuthRow = new QHBoxLayout;
    auto *azimuthColumn = new QVBoxLayout;
    m_compass = new Compass(this);
    connect(m_compass, &Compass::angleRequested, this, [this](int angle) {
        emit commandRequested(QStringLiteral("M%1").arg(formatAngle(angle, 450)));
    });
    m_azimuth = new QLabel(QStringLiteral("000°"), this);
    m_azimuth->setObjectName(QStringLiteral("azimuthReading"));
    m_azimuth->setAlignment(Qt::AlignCenter);
    azimuthColumn->addWidget(m_compass, 1);
    azimuthColumn->addWidget(m_azimuth);
    azimuthRow->addLayout(azimuthColumn);
    m_azimuthPresets = makePresets(QStringLiteral("azimuthPreset"), this);
    azimuthRow->addWidget(m_azimuthPresets, 1);
    layout->addLayout(azimuthRow, 1);

    const auto azimuthPresets = m_azimuthPresets->findChildren<QPushButton *>();
    for (auto *button : azimuthPresets) {
        connect(button, &QPushButton::customContextMenuRequested, this, [this, button] {
            bool ok = false;
            const int current = button->text().toInt();
            const int angle = QInputDialog::getInt(
                this, tr("Edit azimuth preset (AI)"), tr("Azimuth angle (AI)"),
                current, 0, 450, 1, &ok);
            if (!ok)
                return;
            button->setText(formatAngle(angle, 450));
            QSettings settings(QStringLiteral("BG5GEB"),
                               QStringLiteral("GS-232B Open Control"));
            settings.setValue(button->objectName(), angle);
        });
        connect(button, &QPushButton::clicked, this, [this, button] {
            bool ok = false;
            const int angle = button->text().trimmed().toInt(&ok);
            if (ok)
                emit commandRequested(QStringLiteral("M%1").arg(formatAngle(angle, 450)));
        });
    }

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    auto *elevationRow = new QHBoxLayout;
    auto *elevationColumn = new QVBoxLayout;
    m_slider = new QSlider(Qt::Vertical, this);
    m_slider->setObjectName(QStringLiteral("elevationSlider"));
    m_slider->setRange(0, 180);
    m_slider->setMinimumHeight(120);
    m_elevationCommandTimer = new QTimer(this);
    m_elevationCommandTimer->setSingleShot(true);
    m_elevationCommandTimer->setInterval(150);
    m_elevation = new QLabel(QStringLiteral("000°"), this);
    m_elevation->setObjectName(QStringLiteral("elevationReading"));
    m_elevation->setAlignment(Qt::AlignCenter);
    elevationColumn->addWidget(m_slider, 1, Qt::AlignHCenter);
    elevationColumn->addWidget(m_elevation);
    elevationRow->addLayout(elevationColumn);
    m_elevationPresets = makePresets(QStringLiteral("elevationPreset"), this);
    elevationRow->addWidget(m_elevationPresets, 1);
    layout->addLayout(elevationRow, 1);

    const auto elevationPresets = m_elevationPresets->findChildren<QPushButton *>();
    for (auto *button : elevationPresets) {
        connect(button, &QPushButton::customContextMenuRequested, this, [this, button] {
            bool ok = false;
            const int current = button->text().toInt();
            const int angle = QInputDialog::getInt(
                this, tr("Edit elevation preset (AI)"), tr("Elevation angle (AI)"),
                current, 0, 180, 1, &ok);
            if (!ok)
                return;
            button->setText(formatAngle(angle, 180));
            QSettings settings(QStringLiteral("BG5GEB"),
                               QStringLiteral("GS-232B Open Control"));
            settings.setValue(button->objectName(), angle);
        });
        connect(button, &QPushButton::clicked, this, [this, button] {
            bool ok = false;
            const int angle = button->text().trimmed().toInt(&ok);
            if (ok) {
                emit commandRequested(QStringLiteral("W%1 %2")
                                          .arg(formatAngle(m_azimuthValue, 450))
                                          .arg(formatAngle(angle, 180)));
            }
        });
    }

    connect(m_slider, &QSlider::valueChanged, m_elevationCommandTimer,
            qOverload<>(&QTimer::start));
    connect(m_elevationCommandTimer, &QTimer::timeout, this, [this] {
        emit commandRequested(QStringLiteral("W%1 %2")
                                  .arg(formatAngle(m_azimuthValue, 450))
                                  .arg(formatAngle(m_slider->value(), 180)));
    });

    setCommandEnabled(false);
}

void ControlPanel::setCommandEnabled(bool enabled)
{
    m_compass->setEnabled(enabled);
    m_slider->setEnabled(enabled);
    // NOTE: 预设编辑属于本地偏好设置，断开或透传时仍可编辑；发送端会校验真实连接状态。
    m_azimuthPresets->setEnabled(true);
    m_elevationPresets->setEnabled(true);
}

void ControlPanel::setPosition(int azimuth, int elevation)
{
    m_azimuthValue = qBound(0, azimuth, 450);
    m_elevationValue = qBound(0, elevation, 180);
    m_azimuth->setText(QStringLiteral("%1°").arg(formatAngle(m_azimuthValue, 450)));
    m_elevation->setText(QStringLiteral("%1°").arg(formatAngle(m_elevationValue, 180)));
    m_compass->setAngle(m_azimuthValue);
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(m_elevationValue);
}

int ControlPanel::azimuth() const
{
    return m_azimuthValue;
}

int ControlPanel::elevation() const
{
    return m_elevationValue;
}
