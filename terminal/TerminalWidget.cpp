#include "TerminalWidget.h"

#include <pyte/unicode.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBoundaryFinder>

#include <algorithm>

namespace {

QByteArray cursorPosition(int row, int column)
{
    return "\x1b[" + QByteArray::number(row) + ';' + QByteArray::number(column) + 'H';
}

int textColumns(const QString &text)
{
    int width = 0;
    for (char32_t ch : text.toUcs4())
        width += pyte::char_width(ch);
    return width;
}

} // namespace

TerminalWidget::TerminalWidget(QWidget *parent) : qtpyte::TerminalWidget(parent)
{
    setObjectName(QStringLiteral("consoleOutput"));
    setAccessibleName(tr("Device terminal"));
    setAttribute(Qt::WA_InputMethodEnabled);
    connect(this, &qtpyte::TerminalWidget::dataReady, this, &TerminalWidget::handleTerminalData);
    feed(QByteArrayLiteral("\x1b[?25l\x1b[?2004h"));
    captureOutputCursor();
}

void TerminalWidget::setCommandEnabled(bool enabled)
{
    if (m_commandEnabled == enabled)
        return;
    const bool follow = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    const int topLine = viewTopLine();
    hidePrompt();
    m_commandEnabled = enabled;
    if (!enabled)
        m_command.reset();
    feed(enabled ? QByteArrayLiteral("\x1b[?25h") : QByteArrayLiteral("\x1b[?25l"));
    showPrompt();
    verticalScrollBar()->setValue(follow ? verticalScrollBar()->maximum() : topLine - firstLine());
}

void TerminalWidget::appendOutput(const QString &text)
{
    const QString complete = m_outputBuffer.append(text);
    if (complete.isEmpty())
        return;
    const bool follow = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    const int topLine = viewTopLine();
    hidePrompt();
    QByteArray bytes = complete.toUtf8();
    bytes.replace("\n", "\r\n");
    if (m_softWrapped && bytes.startsWith("\r\n"))
        bytes.remove(0, 2);
    m_softWrapped = false;
    feed(bytes);
    captureOutputCursor();
    showPrompt();
    verticalScrollBar()->setValue(follow ? verticalScrollBar()->maximum() : topLine - firstLine());
}

void TerminalWidget::rememberCommand(const QString &command)
{
    m_command.remember(command);
}

QPoint TerminalWidget::outputCursor()
{
    // NOTE: 标准 DSR 查询由模拟器同步响应，不发送到设备，也不与本地按键混淆。
    m_queryingCursor = true;
    feed(QByteArrayLiteral("\x1b[6n"));
    m_queryingCursor = false;
    return m_cursorReply;
}

void TerminalWidget::captureOutputCursor()
{
    QPoint cursor = outputCursor();
    if (cursor.x() > columns()) {
        // NOTE: 提前兑现延迟换行；下一片段若恰好是换行，不再生成重复空行。
        feed(QByteArrayLiteral("\r\n"));
        m_softWrapped = true;
        cursor = outputCursor();
    }
    m_outputLine = endLine() - terminalRows() + cursor.y() - 1;
    m_outputColumn = cursor.x();
}

void TerminalWidget::hidePrompt()
{
    if (!m_promptVisible)
        return;
    const int row = std::clamp(m_outputLine - (endLine() - terminalRows()) + 1, 1, terminalRows());
    feed(cursorPosition(row, std::clamp(m_outputColumn, 1, columns())) + "\x1b[J");
    m_promptVisible = false;
}

void TerminalWidget::showPrompt()
{
    if (!m_commandEnabled)
        return;
    if (m_outputColumn != 1)
        feed(QByteArrayLiteral("\r\n"));

    // NOTE: 命令在同一终端行内水平滚动，避免草稿换行挤掉设备输出或产生额外滚动区。
    const int capacity = std::max(0, columns() - 3);
    int start = m_command.cursor();
    int beforeCursor = 0;
    QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme, m_command.text());
    boundary.setPosition(start);
    while (start > 0) {
        const int previous = boundary.toPreviousBoundary();
        const int width = textColumns(m_command.text().mid(previous, start - previous));
        if (beforeCursor + width > capacity)
            break;
        beforeCursor += width;
        start = previous;
    }
    QString visible;
    int width = 0;
    boundary.setPosition(start);
    for (int end = boundary.toNextBoundary(); end >= 0; end = boundary.toNextBoundary()) {
        const QString glyph = m_command.text().mid(start, end - start);
        const int glyphWidth = textColumns(glyph);
        if (width + glyphWidth > capacity)
            break;
        visible += glyph;
        width += glyphWidth;
        start = end;
    }
    feed(QByteArrayLiteral("> ") + visible.toUtf8());
    const QPoint cursor = outputCursor();
    const int column = 3 + beforeCursor;
    feed(cursorPosition(cursor.y(), std::min(column, columns())));
    m_cursorReply.setX(std::min(column, columns()));
    m_promptVisible = true;
}

void TerminalWidget::redrawPrompt()
{
    hidePrompt();
    showPrompt();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalWidget::handleTerminalData(const QByteArray &data)
{
    if (m_queryingCursor) {
        static const QRegularExpression reply(QStringLiteral("^\x1b\\[(\\d+);(\\d+)R$"));
        const auto match = reply.match(QString::fromLatin1(data));
        if (match.hasMatch())
            m_cursorReply = QPoint(match.captured(2).toInt(), match.captured(1).toInt());
        return;
    }
    if (!m_commandEnabled)
        return;
    if (data.startsWith("\x1b[200~") && data.endsWith("\x1b[201~")) {
        m_command.insert(QString::fromUtf8(data.mid(6, data.size() - 12)));
        redrawPrompt();
    }
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() & Qt::ShiftModifier) &&
        ((event->modifiers() & Qt::ControlModifier) || event->key() == Qt::Key_PageUp ||
         event->key() == Qt::Key_PageDown)) {
        qtpyte::TerminalWidget::keyPressEvent(event);
        return;
    }
    if (!m_commandEnabled) {
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QString command = m_command.take();
        redrawPrompt();
        if (!command.trimmed().isEmpty())
            emit commandRequested(command);
    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        m_command.take();
        redrawPrompt();
    } else {
        m_command.edit(*event);
        redrawPrompt();
    }
    event->accept();
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_commandEnabled) {
        m_command.insert(event->commitString());
        redrawPrompt();
    }
    event->accept();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled: return m_commandEnabled;
    case Qt::ImCursorRectangle:
        return QRect((m_cursorReply.x() - 1) * cellWidth(), (m_cursorReply.y() - 1) * cellHeight(),
                     cellWidth(), cellHeight());
    case Qt::ImSurroundingText: return m_command.text();
    case Qt::ImCursorPosition: return m_command.cursor();
    default: return qtpyte::TerminalWidget::inputMethodQuery(query);
    }
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(tr("Copy"), this, &qtpyte::TerminalWidget::copySelection)->setEnabled(hasSelection());
    menu.addAction(tr("Paste"), this, [this] { paste(QApplication::clipboard()->text()); })
        ->setEnabled(m_commandEnabled);
    menu.addAction(tr("Select all"), this, &qtpyte::TerminalWidget::selectAll);
    menu.exec(event->globalPos());
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    hidePrompt();
    qtpyte::TerminalWidget::resizeEvent(event);
    captureOutputCursor();
    showPrompt();
}
