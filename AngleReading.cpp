#include "AngleReading.h"

#include <QApplication>
#include <QFocusEvent>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QResizeEvent>

AngleReading::AngleReading(int maximum, QWidget *parent)
    : QLabel(parent), m_editor(new QLineEdit(this)), m_maximum(maximum)
{
    setAlignment(Qt::AlignCenter);
    m_editor->setObjectName(QStringLiteral("angleEditor"));
    m_editor->setAlignment(Qt::AlignCenter);
    m_editor->setMaxLength(QString::number(maximum).size());
    m_editor->setValidator(new QIntValidator(0, maximum, m_editor));
    m_editor->hide();
    setMinimumHeight(m_editor->sizeHint().height());
    setAngle(0);
    setEditingEnabled(false);
}

void AngleReading::setAngle(int angle)
{
    m_angle = qBound(0, angle, m_maximum);
    setText(QStringLiteral("%1°").arg(m_angle, 3, 10, QLatin1Char('0')));
}

void AngleReading::setEditingEnabled(bool enabled)
{
    m_editingEnabled = enabled;
    if (!enabled)
        finishEditing(false);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus);
    setToolTip(enabled
                   ? tr("Click to enter an angle (0–%1°). Press Enter to move or Esc to cancel.")
                         .arg(m_maximum)
                   : QString());
}

bool AngleReading::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget != m_editor && !m_editor->isAncestorOf(widget))
            finishEditing(false);
    }
    if (watched == m_editor) {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter) {
                const bool submit = key != Qt::Key_Escape;
                if (!submit || m_editor->hasAcceptableInput())
                    finishEditing(submit);
                event->accept();
                return true;
            }
        }
        // NOTE: 离开输入框只取消编辑，避免点击其他控制项时额外发送定位命令。
        if (event->type() == QEvent::FocusOut &&
            static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason)
            finishEditing(false);
    }
    return QLabel::eventFilter(watched, event);
}

void AngleReading::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_editingEnabled &&
        rect().contains(event->position().toPoint())) {
        beginEditing();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

void AngleReading::keyPressEvent(QKeyEvent *event)
{
    if (m_editingEnabled && (event->key() == Qt::Key_Return ||
                            event->key() == Qt::Key_Enter ||
                            event->key() == Qt::Key_Space ||
                            event->key() == Qt::Key_F2)) {
        beginEditing();
        event->accept();
        return;
    }
    QLabel::keyPressEvent(event);
}

void AngleReading::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    m_editor->setGeometry(contentsRect());
}

void AngleReading::beginEditing()
{
    if (!m_editingEnabled || m_editing)
        return;
    m_editing = true;
    // NOTE: 空白区域通常不接收焦点，因此编辑期间同时监听输入框外的点击。
    qApp->installEventFilter(this);
    m_editor->setText(QString::number(m_angle));
    m_editor->setAccessibleName(accessibleName());
    m_editor->setGeometry(contentsRect());
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
    m_editor->selectAll();
}

void AngleReading::finishEditing(bool submit)
{
    if (!m_editing)
        return;
    bool ok = false;
    const int angle = m_editor->text().toInt(&ok);
    if (submit && (!ok || !m_editor->hasAcceptableInput() ||
                   angle < 0 || angle > m_maximum))
        return;

    // NOTE: 先结束编辑再移交焦点和隐藏输入框，避免失焦事件重复处理。
    m_editing = false;
    qApp->removeEventFilter(this);
    if (m_editor->hasFocus())
        setFocus(Qt::OtherFocusReason);
    m_editor->hide();
    emit editingFinished();
    if (submit && m_editingEnabled)
        emit angleRequested(angle);
}
