#include "terminal/CommandLine.h"

#include <QtTest>

class CommandLineTests : public QObject
{
    Q_OBJECT

private slots:
    void removesOtherControlCharacters();
};

void CommandLineTests::removesOtherControlCharacters()
{
    QString input;
    input.append(QLatin1Char('A'));
    input.append(QChar(0x0001));
    input.append(QLatin1Char('B'));
    input.append(QChar(0x007f));
    input.append(QLatin1Char('C'));

    CommandLine command;
    command.insert(input);

    QCOMPARE(command.text(), QStringLiteral("ABC"));
}

QTEST_GUILESS_MAIN(CommandLineTests)

#include "CommandLineTests.moc"
