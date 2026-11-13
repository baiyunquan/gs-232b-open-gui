#include "CommandLog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

class CommandLogTests final : public QObject
{
    Q_OBJECT

private slots:
    void preservesNonSensitiveBytes_data();
    void preservesNonSensitiveBytes();
    void redactsPasswords_data();
    void redactsPasswords();
    void hidesUnparseableArguments_data();
    void hidesUnparseableArguments();
};

void CommandLogTests::preservesNonSensitiveBytes_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("position") << QByteArrayLiteral("C2\r");
    QTest::newRow("network-poll") << QByteArrayLiteral("!NET GET\r");
    QTest::newRow("rotor-query") << QByteArrayLiteral("!ROTOR GET\r");
    QTest::newRow("fault-query") << QByteArrayLiteral("!MOTION STATUS\r");
    QTest::newRow("trial-parameters")
        << QByteArrayLiteral("!CFG APPLY { \"candidate_id\": 7, \"timeout_s\": 300 }\r");
    QTest::newRow("ethernet-parameters")
        << QByteArrayLiteral("!NET SET {\"eth\":{\"mode\":\"dhcp\"}}\r");
    QTest::newRow("password-metadata")
        << QByteArrayLiteral("!NET SET {\"wifi\":{\"password_set\":true}}\r");
    QTest::newRow("raw-binary") << QByteArray::fromHex("00ff0d");
}

void CommandLogTests::preservesNonSensitiveBytes()
{
    QFETCH(QByteArray, bytes);
    QCOMPARE(Communication::commandForLog(bytes), bytes);
}

void CommandLogTests::redactsPasswords_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QByteArray>("expectedJson");
    QTest::newRow("authentication")
        << QByteArrayLiteral("!SESSION AUTH {\"password\":\"admin-secret\"}\r")
        << QByteArrayLiteral("{\"password\":\"***\"}");
    QTest::newRow("password-change")
        << QByteArrayLiteral("!ADMIN SET {\"password\":\"new-secret\"}\r")
        << QByteArrayLiteral("{\"password\":\"***\"}");
    QTest::newRow("wifi-password")
        << QStringLiteral("!NET SET {\"wifi\":{\"ssid\":\"测试网络\",\"password\":\"wifi-secret\",\"enabled\":true}}\r").toUtf8()
        << QStringLiteral("{\"wifi\":{\"ssid\":\"测试网络\",\"password\":\"***\",\"enabled\":true}}").toUtf8();
    QTest::newRow("escaped-key-and-value")
        << QByteArrayLiteral("!SESSION AUTH {\"pass\\u0077ord\":\"quote\\\"and\\\\slash\"}\r")
        << QByteArrayLiteral("{\"password\":\"***\"}");
    QTest::newRow("nested-array-and-case")
        << QByteArrayLiteral("!NET SET {\"items\":[{\"Password\":\"secret\"},1]}\r")
        << QByteArrayLiteral("{\"items\":[{\"Password\":\"***\"},1]}");
}

void CommandLogTests::redactsPasswords()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QByteArray, expectedJson);
    const QByteArray original = bytes;
    const QByteArray logged = Communication::commandForLog(bytes);
    const qsizetype jsonStart = logged.indexOf('{');
    QVERIFY(jsonStart >= 0);
    QCOMPARE(logged.left(jsonStart), bytes.left(bytes.indexOf('{')));
    QVERIFY(logged.endsWith('\r'));
    QCOMPARE(QJsonDocument::fromJson(logged.mid(jsonStart)),
             QJsonDocument::fromJson(expectedJson));
    QCOMPARE(bytes, original);
}

void CommandLogTests::hidesUnparseableArguments_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QByteArray>("expected");
    QTest::newRow("incomplete-json")
        << QByteArrayLiteral("!NET SET {\"wifi\":{\"password\":\"secret\"}\r")
        << QByteArrayLiteral("!NET SET ***\r");
    QTest::newRow("positional-secret")
        << QByteArrayLiteral("!SESSION AUTH plain-secret\r")
        << QByteArrayLiteral("!SESSION AUTH ***\r");
    QTest::newRow("unicode-secret")
        << QStringLiteral("!SESSION AUTH 密码\r").toUtf8()
        << QByteArrayLiteral("!SESSION AUTH ***\r");
    QTest::newRow("trailing-garbage")
        << QByteArrayLiteral("!SESSION AUTH {\"password\":\"secret\"} extra\r")
        << QByteArrayLiteral("!SESSION AUTH ***\r");
}

void CommandLogTests::hidesUnparseableArguments()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QByteArray, expected);
    QCOMPARE(Communication::commandForLog(bytes), expected);
}

QTEST_GUILESS_MAIN(CommandLogTests)
#include "CommandLogTests.moc"
