#include "AngleReading.h"
#include "ControlPanel.h"

#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TargetControlTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void submitsTargets_data();
    void submitsTargets();
    void rejectedInputCanBeCorrected();
    void stationGridChangesTakeEffect();
    void cancellationDoesNotMove();
    void disablingCancelsPendingInput();
    void elevationRemainsNumeric();

private:
    QTemporaryDir m_settingsDirectory;
};

namespace {

QLineEdit *edit(AngleReading *reading, const QString &text)
{
    QCoreApplication::processEvents();
    reading->beginEditing();
    auto *editor = reading->findChild<QLineEdit *>();
    editor->setText(text);
    return editor;
}

} // namespace

void TargetControlTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
}

void TargetControlTests::submitsTargets_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("command");
    QTest::newRow("grid") << QStringLiteral("FN31") << QStringLiteral("M023");
    QTest::newRow("eight-character-grid") << QStringLiteral("PM95AA12") << QStringLiteral("M246");
    QTest::newRow("callsign") << QStringLiteral("ja1abc") << QStringLiteral("M011");
    QTest::newRow("prefix") << QStringLiteral("JA") << QStringLiteral("M011");
    QTest::newRow("long-call") << QStringLiteral("EA8/K1ABC/P") << QStringLiteral("M335");
    QTest::newRow("numeric") << QStringLiteral("405") << QStringLiteral("M405");
    QTest::newRow("upper-limit") << QStringLiteral("450") << QStringLiteral("M450");
    QTest::newRow("north") << QStringLiteral("0") << QStringLiteral("M000");
}

void TargetControlTests::submitsTargets()
{
    QFETCH(QString, input);
    QFETCH(QString, command);
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *reading = panel.findChild<AngleReading *>(QStringLiteral("azimuthReading"));
    auto *editor = edit(reading, input);
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), command);
}

void TargetControlTests::rejectedInputCanBeCorrected()
{
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    QSignalSpy errors(&panel, &ControlPanel::inputError);
    auto *reading = panel.findChild<AngleReading *>(QStringLiteral("azimuthReading"));
    for (const QString input : {QString(), QStringLiteral("Q1ABC"), QStringLiteral("451"),
                               QStringLiteral("PM95AA"), QStringLiteral("JA1ABC/MM")}) {
        auto *editor = edit(reading, input);
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(commands.isEmpty());
        QCOMPARE(errors.size(), 1);
        QVERIFY(!errors.takeFirst().first().toString().isEmpty());
        QVERIFY(editor->hasFocus());
        editor->setText(QStringLiteral("JA"));
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M011"));
    }
}

void TargetControlTests::stationGridChangesTakeEffect()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *reading = panel.findChild<AngleReading *>(QStringLiteral("azimuthReading"));
    auto *editor = edit(reading, QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    panel.setStationGrid(QStringLiteral("PM95AA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M011"));
    editor = edit(reading, QStringLiteral("PM95AA"));
    panel.setStationGrid(QStringLiteral("FN31"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M335"));
    panel.setStationGrid({});
    editor = edit(reading, QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    editor->setText(QStringLiteral("360"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.takeFirst().first().toString(), QStringLiteral("M360"));
}

void TargetControlTests::cancellationDoesNotMove()
{
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *reading = panel.findChild<AngleReading *>(QStringLiteral("azimuthReading"));
    QTest::keyClick(edit(reading, QStringLiteral("JA")), Qt::Key_Escape);
    QVERIFY(commands.isEmpty());
    edit(reading, QStringLiteral("JA"));
    auto *elevation = panel.findChild<AngleReading *>(QStringLiteral("elevationReading"));
    elevation->setFocus();
    QCoreApplication::processEvents();
    QVERIFY(commands.isEmpty());
}

void TargetControlTests::disablingCancelsPendingInput()
{
    ControlPanel panel;
    panel.setStationGrid(QStringLiteral("PM95AA"));
    panel.setCommandEnabled(true);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *reading = panel.findChild<AngleReading *>(QStringLiteral("azimuthReading"));
    auto *editor = edit(reading, QStringLiteral("JA"));
    panel.setCommandEnabled(false);
    QTest::keyClick(editor, Qt::Key_Return);
    reading->beginEditing();
    QVERIFY(commands.isEmpty());
}

void TargetControlTests::elevationRemainsNumeric()
{
    ControlPanel panel;
    panel.setCommandEnabled(true);
    panel.setPosition(405, 20);
    panel.show();
    QSignalSpy commands(&panel, &ControlPanel::commandRequested);
    auto *reading = panel.findChild<AngleReading *>(QStringLiteral("elevationReading"));
    auto *editor = edit(reading, QStringLiteral("JA"));
    QTest::keyClick(editor, Qt::Key_Return);
    QVERIFY(commands.isEmpty());
    editor->setText(QStringLiteral("90"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().first().toString(), QStringLiteral("W405 090"));
}

QTEST_MAIN(TargetControlTests)
#include "TargetControlTests.moc"
