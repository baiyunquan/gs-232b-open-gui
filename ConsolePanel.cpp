#include "ConsolePanel.h"
#include "CommandLog.h"
#include "terminal/TerminalWidget.h"

#include <QVBoxLayout>

ConsolePanel::ConsolePanel(QWidget *parent) : QGroupBox(tr("CONSOLE"), parent)
{
    setObjectName(QStringLiteral("consolePanel"));
    auto *layout = new QVBoxLayout(this);
    m_output = new TerminalWidget(this);
    layout->addWidget(m_output, 1);
    setFocusProxy(m_output);
    connect(m_output, &TerminalWidget::commandRequested, this, [this](const QString &command) {
        const QByteArray bytes = command.toUtf8() + '\r';
        if (Communication::commandForLog(bytes) == bytes)
            m_output->rememberCommand(command);
        emit commandRequested(command);
    });

    // NOTE: 控制台只发出用户输入，收发回显由真实传输信号驱动。
}

void ConsolePanel::setCommandEnabled(bool enabled)
{
    m_output->setCommandEnabled(enabled);
}

void ConsolePanel::appendReceived(const QByteArray &bytes)
{
    appendTraffic(bytes, false);
}

void ConsolePanel::appendSent(const QByteArray &bytes)
{
    appendTraffic(bytes, true);
}

void ConsolePanel::appendError(const QString &error)
{
    m_output->appendOutput((m_lineStart ? QString() : QStringLiteral("\n")) +
                           QStringLiteral("! ") + error + QStringLiteral("\n"));
    m_lineStart = true;
}

void ConsolePanel::appendProcessOutput(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;

    QString text = QString::fromLocal8Bit(bytes);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));
    m_output->appendOutput((m_lineStart ? QStringLiteral("# ") : QString()) + text);
    m_lineStart = text.endsWith(QChar('\n'));
}

void ConsolePanel::appendNotice(const QString &message)
{
    m_output->appendOutput((m_lineStart ? QString() : QStringLiteral("\n")) +
                           QStringLiteral("* ") + message + QStringLiteral("\n"));
    m_lineStart = true;
}

void ConsolePanel::appendTraffic(const QByteArray &bytes, bool outgoing)
{
    if (bytes.isEmpty())
        return;

    QString text = QString::fromUtf8(bytes);
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLatin1(bytes.toHex(' '));
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    const QString prefix = m_lineStart ? (outgoing ? QStringLiteral("> ") : QStringLiteral("< "))
                                      : QString();
    m_output->appendOutput(prefix + text);
    m_lineStart = text.endsWith(QChar('\n'));
}
