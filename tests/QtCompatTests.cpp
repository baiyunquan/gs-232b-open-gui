#include "QtCompat.h"

#include <QJsonValue>
#include <QtTest>

class QtCompatTests : public QObject
{
    Q_OBJECT

private slots:
    void convertsLargeJsonIntegers();
    void rejectsNonIntegerJsonValues();
};

void QtCompatTests::convertsLargeJsonIntegers()
{
    const QJsonValue value = QJsonValue::fromVariant(QVariant::fromValue<qlonglong>(3000000000LL));
    QCOMPARE(jsonIntegerValue(value, -1), qint64(3000000000LL));
}

void QtCompatTests::rejectsNonIntegerJsonValues()
{
    QCOMPARE(jsonIntegerValue(QJsonValue(QStringLiteral("7")), -1), qint64(-1));
    QCOMPARE(jsonIntegerValue(QJsonValue(1.5), -1), qint64(-1));
}

QTEST_GUILESS_MAIN(QtCompatTests)

#include "QtCompatTests.moc"
