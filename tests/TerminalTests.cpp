#include "ConsolePanel.h"
#include "terminal/TerminalWidget.h"

#include <QInputMethodEvent>
#include <QScrollBar>
#include <QSignalSpy>
#include <QtTest>

class TerminalTests final : public QObject
{
    Q_OBJECT

private slots:
    void submitsOnlyOnEnter();
    void editsAndRecallsCommands();
    void incomingOutputPreservesDraft();
    void selectionDoesNotRedirectOutput();
    void scrollbackPausesAndResumesFollowing();
    void pasteDoesNotSubmitCommands();
    void disconnectClearsDraftAndHistory();
    void sensitiveCommandsAreNotRemembered();
    void unicodeEditingUsesGraphemeBoundaries();
    void fragmentedAnsiOutput();
    void longDraftSurvivesOutputAndResize();
    void outputSurvivesResize();
};

void TerminalTests::submitsOnlyOnEnter()
{
    ConsolePanel panel;
    auto *terminal = panel.findChild<TerminalWidget *>();
    QSignalSpy requested(&panel, &ConsolePanel::commandRequested);
    QTest::keyClicks(terminal, "C2");
    QTest::keyClick(terminal, Qt::Key_Return);
    QVERIFY(requested.isEmpty());
    panel.setCommandEnabled(true);
    QTest::keyClicks(terminal, "C2");
    QVERIFY(requested.isEmpty());
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.size(), 1);
    QCOMPARE(requested.first().first().toString(), QStringLiteral("C2"));
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.size(), 1);
}

void TerminalTests::editsAndRecallsCommands()
{
    ConsolePanel panel;
    panel.setCommandEnabled(true);
    auto *terminal = panel.findChild<TerminalWidget *>();
    QSignalSpy requested(&panel, &ConsolePanel::commandRequested);
    QTest::keyClicks(terminal, "M129");
    QTest::keyClick(terminal, Qt::Key_Backspace);
    QTest::keyClicks(terminal, "3");
    QTest::keyClick(terminal, Qt::Key_Left);
    QTest::keyClick(terminal, Qt::Key_Delete);
    QTest::keyClicks(terminal, "0");
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.last().first().toString(), QStringLiteral("M120"));
    QTest::keyClicks(terminal, "C");
    QTest::keyClick(terminal, Qt::Key_Up);
    QTest::keyClick(terminal, Qt::Key_Down);
    QTest::keyClicks(terminal, "2");
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.last().first().toString(), QStringLiteral("C2"));
    QTest::keyClick(terminal, Qt::Key_Up);
    QTest::keyClick(terminal, Qt::Key_Up);
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.last().first().toString(), QStringLiteral("M120"));
}

void TerminalTests::incomingOutputPreservesDraft()
{
    ConsolePanel panel;
    panel.resize(640, 400);
    panel.show();
    panel.setCommandEnabled(true);
    auto *terminal = panel.findChild<TerminalWidget *>();
    QSignalSpy requested(&panel, &ConsolePanel::commandRequested);
    QTest::keyClicks(terminal, "M120");
    panel.appendReceived("AZ=0");
    panel.appendReceived("45 EL=030\r\n");
    panel.appendSent("C2\r");
    panel.appendNotice("connected");
    panel.appendError("disconnected");
    panel.appendProcessOutput("progress\r\n");
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.last().first().toString(), QStringLiteral("M120"));
    QVERIFY(terminal->sessionText().contains(QStringLiteral("< AZ=045 EL=030")));
    QVERIFY(terminal->sessionText().contains(QStringLiteral("> C2")));
}

void TerminalTests::selectionDoesNotRedirectOutput()
{
    TerminalWidget terminal;
    terminal.resize(640, 400);
    terminal.show();
    terminal.appendOutput(QStringLiteral("first\nsecond\n"));
    terminal.selectAll();
    terminal.appendOutput(QStringLiteral("third\n"));
    const QString session = terminal.sessionText();
    QVERIFY(session.indexOf(QStringLiteral("first")) < session.indexOf(QStringLiteral("second")));
    QVERIFY(session.indexOf(QStringLiteral("second")) < session.indexOf(QStringLiteral("third")));
    QVERIFY(terminal.selectedText().contains(QStringLiteral("first")));
}

void TerminalTests::scrollbackPausesAndResumesFollowing()
{
    TerminalWidget terminal;
    terminal.resize(640, 400);
    terminal.show();
    terminal.setCommandEnabled(true);
    for (int i = 0; i < 150; ++i)
        terminal.appendOutput(QStringLiteral("RX %1\n").arg(i));
    auto *bar = terminal.verticalScrollBar();
    QVERIFY(bar->maximum() > 0);
    QCOMPARE(bar->value(), bar->maximum());
    bar->setValue(bar->maximum() / 2);
    const int topLine = terminal.viewTopLine();
    terminal.appendOutput(QStringLiteral("new while reading\n"));
    QCOMPARE(terminal.viewTopLine(), topLine);
    bool found = false;
    for (int line = terminal.firstLine(); line < terminal.endLine(); ++line)
        found = found || terminal.lineAt(line).contains(QStringLiteral("new while reading"));
    QVERIFY(found);
    bar->setValue(bar->maximum());
    terminal.appendOutput(QStringLiteral("new at bottom\n"));
    QCOMPARE(bar->value(), bar->maximum());
    QVERIFY(terminal.screenText().contains(QStringLiteral("new at bottom")));
}

void TerminalTests::pasteDoesNotSubmitCommands()
{
    TerminalWidget terminal;
    terminal.setCommandEnabled(true);
    QSignalSpy requested(&terminal, &TerminalWidget::commandRequested);
    terminal.paste(QStringLiteral("C2\r\nM120\n"));
    QVERIFY(requested.isEmpty());
    QTest::keyClick(&terminal, Qt::Key_Return);
    QCOMPARE(requested.size(), 1);
    QCOMPARE(requested.first().first().toString(), QStringLiteral("C2 M120 "));
}

void TerminalTests::disconnectClearsDraftAndHistory()
{
    ConsolePanel panel;
    panel.setCommandEnabled(true);
    auto *terminal = panel.findChild<TerminalWidget *>();
    QSignalSpy requested(&panel, &ConsolePanel::commandRequested);
    QTest::keyClicks(terminal, "M120");
    QTest::keyClick(terminal, Qt::Key_Return);
    QTest::keyClicks(terminal, "C2");
    panel.setCommandEnabled(false);
    panel.setCommandEnabled(true);
    QTest::keyClick(terminal, Qt::Key_Up);
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.size(), 1);
}

void TerminalTests::sensitiveCommandsAreNotRemembered()
{
    ConsolePanel panel;
    panel.setCommandEnabled(true);
    auto *terminal = panel.findChild<TerminalWidget *>();
    QSignalSpy requested(&panel, &ConsolePanel::commandRequested);
    QTest::keyClicks(terminal, "C2");
    QTest::keyClick(terminal, Qt::Key_Return);
    QTest::keyClicks(terminal, "!SESSION AUTH {\"password\":\"test-secret\"}");
    QTest::keyClick(terminal, Qt::Key_Return);
    QTest::keyClick(terminal, Qt::Key_Up);
    QTest::keyClick(terminal, Qt::Key_Return);
    QCOMPARE(requested.last().first().toString(), QStringLiteral("C2"));
    QVERIFY(!terminal->sessionText().contains(QStringLiteral("test-secret")));
}

void TerminalTests::unicodeEditingUsesGraphemeBoundaries()
{
    TerminalWidget terminal;
    terminal.setCommandEnabled(true);
    QSignalSpy requested(&terminal, &TerminalWidget::commandRequested);
    QInputMethodEvent commit;
    commit.setCommitString(QString::fromUtf8("测试👩‍🚀"));
    QApplication::sendEvent(&terminal, &commit);
    QTest::keyClick(&terminal, Qt::Key_Backspace);
    QTest::keyClick(&terminal, Qt::Key_Return);
    QCOMPARE(requested.first().first().toString(), QString::fromUtf8("测试"));
}

void TerminalTests::fragmentedAnsiOutput()
{
    TerminalWidget terminal;
    terminal.setCommandEnabled(true);
    terminal.appendOutput(QStringLiteral("\x1b[31"));
    terminal.appendOutput(QStringLiteral("mCOLOR\x1b[0m\n"));
    terminal.appendOutput(QStringLiteral("\x1b]0;title"));
    terminal.appendOutput(QStringLiteral("\x1b"));
    terminal.appendOutput(QStringLiteral("\\OSC-END\n"));
    QVERIFY(terminal.sessionText().contains(QStringLiteral("COLOR")));
    QVERIFY(!terminal.sessionText().contains(QStringLiteral("mCOLOR")));
    QVERIFY(terminal.sessionText().contains(QStringLiteral("OSC-END")));
    QVERIFY(!terminal.sessionText().contains(QStringLiteral("title")));
}

void TerminalTests::longDraftSurvivesOutputAndResize()
{
    TerminalWidget terminal;
    terminal.resize(640, 400);
    terminal.show();
    terminal.setCommandEnabled(true);
    QSignalSpy requested(&terminal, &TerminalWidget::commandRequested);
    const QString command = QStringLiteral("M") + QString(500, QLatin1Char('1'));
    terminal.paste(command);
    terminal.appendOutput(QStringLiteral("output during editing\n"));
    terminal.resize(240, 240);
    QApplication::processEvents();
    terminal.resize(800, 600);
    QApplication::processEvents();
    QTest::keyClick(&terminal, Qt::Key_Home);
    QTest::keyClick(&terminal, Qt::Key_Right);
    QTest::keyClick(&terminal, Qt::Key_Delete);
    QTest::keyClick(&terminal, Qt::Key_Return);
    QCOMPARE(requested.first().first().toString(), command.left(command.size() - 1));
}

void TerminalTests::outputSurvivesResize()
{
    TerminalWidget terminal;
    terminal.resize(800, 400);
    terminal.show();
    terminal.setCommandEnabled(true);
    const QString line = QStringLiteral("AZ=045 EL=030 retained device response");
    for (int i = 0; i < 100; ++i)
        terminal.appendOutput(line + QLatin1Char('\n'));
    terminal.resize(160, 240);
    QApplication::processEvents();
    terminal.appendOutput(QStringLiteral("new\n"));
    terminal.resize(800, 400);
    QApplication::processEvents();
    int retained = 0;
    for (int row = terminal.firstLine(); row < terminal.endLine(); ++row)
        retained += terminal.lineAt(row).contains(line);
    QCOMPARE(retained, 100);
}

QTEST_MAIN(TerminalTests)
#include "TerminalTests.moc"
