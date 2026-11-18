#include "ManagementJson.h"

#include <QtTest>
#include <limits>

class ManagementJsonTests : public QObject
{
    Q_OBJECT
private slots:
    void readsProtocolIntegers_data();
    void readsProtocolIntegers();
    void ignoresNumbersInOtherFields();
};

void ManagementJsonTests::readsProtocolIntegers_data()
{
    QTest::addColumn<QByteArray>("value");
    QTest::addColumn<qint64>("expected");
    QTest::newRow("ordinary") << QByteArray("7") << qint64(7);
    QTest::newRow("beyond-double-precision") << QByteArray("9007199254740993") << qint64(9007199254740993LL);
    QTest::newRow("max") << QByteArray("9223372036854775807") << std::numeric_limits<qint64>::max();
    QTest::newRow("min") << QByteArray("-9223372036854775808") << std::numeric_limits<qint64>::min();
    QTest::newRow("exponent") << QByteArray("9.007199254740993e15") << qint64(9007199254740993LL);
    QTest::newRow("integral-decimal") << QByteArray("70.0e-1") << qint64(7);
    QTest::newRow("zero") << QByteArray("-0.000") << qint64(0);
    QTest::newRow("overflow") << QByteArray("9223372036854775808") << qint64(-1);
    QTest::newRow("underflow") << QByteArray("-9223372036854775809") << qint64(-1);
    QTest::newRow("fraction") << QByteArray("7.1") << qint64(-1);
    QTest::newRow("string") << QByteArray("\"7\"") << qint64(-1);
    QTest::newRow("boolean") << QByteArray("true") << qint64(-1);
    QTest::newRow("null") << QByteArray("null") << qint64(-1);
}

void ManagementJsonTests::readsProtocolIntegers()
{
    QFETCH(QByteArray, value);
    QFETCH(qint64, expected);
    QCOMPARE(jsonDataInteger("{\"data\":{\"trial_id\":" + value + "}}", QStringLiteral("trial_id")), expected);
}

void ManagementJsonTests::ignoresNumbersInOtherFields()
{
    const QByteArray json = R"({"trial_id":99,"data":{"trial_id":7,"nested":{"trial_id":11},"text":"\"trial_id\":9007199254740993"}})";
    QCOMPARE(jsonDataInteger(json, QStringLiteral("trial_id")), qint64(7));
    QCOMPARE(jsonDataInteger("{\"data\":{\"trial_id\":7}", QStringLiteral("trial_id")), qint64(-1));
}

QTEST_GUILESS_MAIN(ManagementJsonTests)
#include "ManagementJsonTests.moc"
