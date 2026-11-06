#include "virtual_serial/CrDelimitedBuffer.h"
#include "virtual_serial/VirtualSerialBridge.h"

#include <QSignalSpy>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

class VirtualSerialTests final : public QObject
{
    Q_OBJECT

private slots:
    void crDelimiterIgnoresChunkBoundaries();
#ifdef Q_OS_UNIX
    void ptyBridgeForwardsBothDirections();
#endif
};

void VirtualSerialTests::crDelimiterIgnoresChunkBoundaries()
{
    CrDelimitedBuffer buffer;
    QCOMPARE(buffer.append(QByteArrayLiteral("A")), QList<QByteArray>{});
    QCOMPARE(buffer.append(QByteArrayLiteral("B\rC\r")),
             (QList<QByteArray>{QByteArrayLiteral("AB\r"), QByteArrayLiteral("C\r")}));
    QCOMPARE(buffer.append(QByteArrayLiteral("\nD\nE")), QList<QByteArray>{});
    QCOMPARE(buffer.pendingSize(), qsizetype(3));
    QCOMPARE(buffer.append(QByteArrayLiteral("F\r")),
             (QList<QByteArray>{QByteArrayLiteral("D\nEF\r")}));
    QCOMPARE(buffer.pendingSize(), qsizetype(0));
}

#ifdef Q_OS_UNIX
void VirtualSerialTests::ptyBridgeForwardsBothDirections()
{
    VirtualSerialBridge bridge;
    QSignalSpy lineSpy(&bridge, &VirtualSerialBridge::webSocketLineReady);
    QVERIFY(lineSpy.isValid());
    QVERIFY(bridge.start());
    QVERIFY(bridge.clientPortName().startsWith(QLatin1Char('/')));

    const QByteArray devicePath = bridge.clientPortName().toLocal8Bit();
    const int clientFd = ::open(devicePath.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    QVERIFY(clientFd >= 0);

    QVERIFY(::write(clientFd, "A", 1) == 1);
    QVERIFY(::write(clientFd, "B\rC", 3) == 3);
    QVERIFY(::write(clientFd, "\r", 1) == 1);
    QTRY_COMPARE_WITH_TIMEOUT(lineSpy.size(), 2, 1000);
    QCOMPARE(lineSpy.at(0).at(0).toByteArray(), QByteArrayLiteral("AB\r"));
    QCOMPARE(lineSpy.at(1).at(0).toByteArray(), QByteArrayLiteral("C\r"));

    bridge.feedWebSocketBytes(QByteArrayLiteral("reply"));
    bridge.feedWebSocketBytes(QByteArrayLiteral("1\rreply2\r"));
    QByteArray received;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        char buffer[128];
        const ssize_t count = ::read(clientFd, buffer, sizeof(buffer));
        if (count > 0)
            received.append(buffer, static_cast<qsizetype>(count));
        return received == QByteArrayLiteral("reply1\rreply2\r");
    }(), 1000);

    ::close(clientFd);
}
#endif

QTEST_GUILESS_MAIN(VirtualSerialTests)
#include "VirtualSerialTests.moc"
