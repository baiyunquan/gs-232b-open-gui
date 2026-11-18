#include "CommandLine.h"

#include <QKeyEvent>
#include <QVector>
#include <utf8proc.h>

#include <algorithm>

namespace {
constexpr int kHistoryLimit = 100;

QVector<int> graphemeBoundaries(const QString &text)
{
    QVector<int> boundaries{0};
    utf8proc_int32_t state = 0;
    utf8proc_int32_t previous = -1;
    int offset = 0;
    for (uint codepoint : text.toUcs4()) {
        const auto current = static_cast<utf8proc_int32_t>(codepoint);
        if (previous >= 0 && utf8proc_grapheme_break_stateful(previous, current, &state))
            boundaries.append(offset);
        offset += codepoint > 0xffff ? 2 : 1;
        previous = current;
    }
    if (boundaries.last() != text.size())
        boundaries.append(text.size());
    return boundaries;
}
}

int CommandLine::previousBoundary() const
{
    const auto boundaries = graphemeBoundaries(m_text);
    const auto boundary = std::lower_bound(boundaries.cbegin(), boundaries.cend(), m_cursor);
    return boundary == boundaries.cbegin() ? 0 : *std::prev(boundary);
}

int CommandLine::nextBoundary() const
{
    const auto boundaries = graphemeBoundaries(m_text);
    const auto boundary = std::upper_bound(boundaries.cbegin(), boundaries.cend(), m_cursor);
    return boundary == boundaries.cend() ? static_cast<int>(m_text.size()) : *boundary;
}

void CommandLine::edit(const QKeyEvent &event)
{
    if (event.modifiers() & Qt::ControlModifier) {
        switch (event.key()) {
        case Qt::Key_A: m_cursor = 0; return;
        case Qt::Key_E: m_cursor = m_text.size(); return;
        case Qt::Key_U: m_text.remove(0, m_cursor); m_cursor = 0; return;
        case Qt::Key_K: m_text.truncate(m_cursor); return;
        default: return;
        }
    }
    switch (event.key()) {
    case Qt::Key_Left: m_cursor = previousBoundary(); break;
    case Qt::Key_Right: m_cursor = nextBoundary(); break;
    case Qt::Key_Home: m_cursor = 0; break;
    case Qt::Key_End: m_cursor = m_text.size(); break;
    case Qt::Key_Up: recall(-1); break;
    case Qt::Key_Down: recall(1); break;
    case Qt::Key_Backspace: {
        const int start = previousBoundary();
        m_text.remove(start, m_cursor - start);
        m_cursor = start;
        break;
    }
    case Qt::Key_Delete: m_text.remove(m_cursor, nextBoundary() - m_cursor); break;
    default:
        if (!(event.modifiers() & (Qt::AltModifier | Qt::MetaModifier)))
            insert(event.text());
        break;
    }
}

void CommandLine::insert(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral(" "));
    for (QChar &ch : text) {
        if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n') || ch == QLatin1Char('\t'))
            ch = QLatin1Char(' ');
    }
    QString filtered;
    filtered.reserve(text.size());
    for (auto it = text.cbegin(); it != text.cend(); ++it) {
        const QChar ch = *it;
        if (ch.category() != QChar::Other_Control)
            filtered.append(ch);
    }
    text.swap(filtered);
    m_text.insert(m_cursor, text);
    m_cursor += text.size();
}

QString CommandLine::take()
{
    const QString command = m_text;
    m_text.clear();
    m_draft.clear();
    m_cursor = 0;
    m_historyIndex = m_history.size();
    return command;
}

void CommandLine::remember(const QString &command)
{
    if (!command.trimmed().isEmpty() && (m_history.isEmpty() || m_history.last() != command)) {
        m_history.append(command);
        if (m_history.size() > kHistoryLimit)
            m_history.removeFirst();
    }
    m_historyIndex = m_history.size();
}

void CommandLine::reset()
{
    take();
    m_history.clear();
    m_historyIndex = 0;
}

void CommandLine::recall(int direction)
{
    if (m_historyIndex == m_history.size())
        m_draft = m_text;
    m_historyIndex = std::clamp(m_historyIndex + direction, 0, static_cast<int>(m_history.size()));
    m_text = m_historyIndex == m_history.size() ? m_draft : m_history.at(m_historyIndex);
    m_cursor = m_text.size();
}
