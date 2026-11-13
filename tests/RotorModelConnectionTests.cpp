#include "CommunicationProtocol.h"
#include "ConnectionPanel.h"
#include "MainWindow.h"
#include "RotorSettingsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>

#include <utility>

#ifdef Q_OS_UNIX
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

QString rotorResponse(const QJsonValue &modelId, const QString &command = QStringLiteral("ROTOR GET"))
{
    const QJsonObject data{{"model_id", modelId}, {"active_model_id", 1},
                           {"restart_required", true}};
    const QJsonObject response{{"cmd", command}, {"ok", true}, {"data", data}};
    return QLatin1Char('!') + QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact))
           + QStringLiteral("\r\n");
}

} // namespace

class RotorModelConnectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void readsConfiguredModel_data();
    void readsConfiguredModel();
    void invalidResponsesPreserveSelection();
    void reconnectClearsAndReadsAgain();
#ifdef Q_OS_UNIX
    void virtualComDoesNotQuery();
    void serialConnectionReadsModel();
#endif

private:
    bool connectDevice();

    MainWindow *m_window = nullptr;
    ConnectionPanel *m_connection = nullptr;
    RotorSettingsPanel *m_settings = nullptr;
    Communication::Transport *m_transport = nullptr;
    QWebSocketServer *m_server = nullptr;
    QPointer<QWebSocket> m_peer;
    QStringList m_commands;
};

void RotorModelConnectionTests::init()
{
    m_server = new QWebSocketServer(QStringLiteral("rotor-test"), QWebSocketServer::NonSecureMode);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    connect(m_server, &QWebSocketServer::newConnection, this, [this] {
        m_peer = m_server->nextPendingConnection();
        m_peer->setParent(m_server);
        connect(m_peer, &QWebSocket::textMessageReceived, this,
                [this](const QString &message) { m_commands.append(message); });
    });

    m_window = new MainWindow;
    m_connection = m_window->findChild<ConnectionPanel *>();
    m_settings = m_window->findChild<RotorSettingsPanel *>();
    m_transport = m_window->findChild<Communication::Transport *>();
    QVERIFY(m_connection);
    QVERIFY(m_settings);
    QVERIFY(m_transport);
    m_window->findChild<QRadioButton *>(QStringLiteral("networkMode"))->setChecked(true);
    m_window->findChild<QLineEdit *>(QStringLiteral("websocketAddress"))
        ->setText(QStringLiteral("ws://127.0.0.1:%1").arg(m_server->serverPort()));
}

void RotorModelConnectionTests::cleanup()
{
    delete m_window;
    m_window = nullptr;
    delete m_server;
    m_server = nullptr;
    m_commands.clear();
}

bool RotorModelConnectionTests::connectDevice()
{
    m_connection->connectRequested();
    return QTest::qWaitFor([this] { return m_peer && m_connection->isConnected(); }, 3000);
}

void RotorModelConnectionTests::readsConfiguredModel_data()
{
    QTest::addColumn<int>("modelId");
    for (int id = 1; id <= 16; ++id)
        QTest::newRow(qPrintable(QString::number(id))) << id;
}

void RotorModelConnectionTests::readsConfiguredModel()
{
    QFETCH(int, modelId);
    QCOMPARE(m_settings->selectedModelId(), 0);
    QVERIFY(connectDevice());
    QTRY_VERIFY(m_commands.contains(QStringLiteral("!ROTOR GET\r")));
    QCOMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), 1);
    QCOMPARE(m_commands.first(), QStringLiteral("!ROTOR GET\r"));
    for (const QString &command : std::as_const(m_commands)) {
        QVERIFY(!command.startsWith(QStringLiteral("!SESSION AUTH")));
        QVERIFY(!command.startsWith(QStringLiteral("!ROTOR SET")));
    }
    auto *apply = m_settings->findChild<QPushButton *>(QStringLiteral("applyModel"));
    QVERIFY(!apply->isEnabled());
    QCOMPARE(m_settings->selectedModelId(), 0);

    const QString response = rotorResponse(modelId);
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    m_peer->sendTextMessage(response.left(response.size() / 2));
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(m_settings->selectedModelId(), 0);
    m_peer->sendTextMessage(response.mid(response.size() / 2));
    QTRY_COMPARE(m_settings->selectedModelId(), modelId);
    QVERIFY(apply->isEnabled());

    m_peer->sendTextMessage(rotorResponse(16, QStringLiteral("ROTOR SET")));
    QTRY_COMPARE(m_settings->selectedModelId(), 16);
}

void RotorModelConnectionTests::invalidResponsesPreserveSelection()
{
    QVERIFY(connectDevice());
    m_peer->sendTextMessage(rotorResponse(5));
    QTRY_COMPARE(m_settings->selectedModelId(), 5);
    const QList<QJsonValue> invalidIds = {
        QJsonValue(), QJsonValue(QJsonValue::Undefined), "6", true, 0, -1, 17, 6.5, 4294967296.0
    };
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    for (const QJsonValue &id : invalidIds) {
        const auto count = received.size();
        m_peer->sendTextMessage(rotorResponse(id));
        QTRY_COMPARE(received.size(), count + 1);
        QCOMPARE(m_settings->selectedModelId(), 5);
    }
    const QStringList unrelatedOrFailed = {
        QStringLiteral("!{\"cmd\":\"ROTOR GET\",\"ok\":false,\"error\":\"BUSY\",\"data\":{\"model_id\":6}}\r\n"),
        QStringLiteral("!{\"cmd\":\"ROTOR SET\",\"ok\":false,\"error\":\"BUSY\",\"data\":{\"model_id\":6}}\r\n"),
        QStringLiteral("!{\"cmd\":\"SYS INFO\",\"ok\":true,\"data\":{\"model_id\":6}}\r\n"),
        QStringLiteral("!invalid-json\r\n")
    };
    for (const QString &response : unrelatedOrFailed) {
        const auto count = received.size();
        m_peer->sendTextMessage(response);
        QTRY_COMPARE(received.size(), count + 1);
        QCOMPARE(m_settings->selectedModelId(), 5);
    }
}

void RotorModelConnectionTests::reconnectClearsAndReadsAgain()
{
    QVERIFY(connectDevice());
    QTRY_COMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), 1);
    m_peer->sendTextMessage(rotorResponse(7));
    QTRY_COMPARE(m_settings->selectedModelId(), 7);
    m_peer->close();
    QTRY_VERIFY(!m_connection->isConnected());
    QCOMPARE(m_settings->selectedModelId(), 0);
    m_peer.clear();

    m_transport->bytesReceived(rotorResponse(5).toUtf8());
    QCOMPARE(m_settings->selectedModelId(), 0);
    QVERIFY(connectDevice());
    QTRY_COMPARE(m_commands.count(QStringLiteral("!ROTOR GET\r")), 2);
    QCOMPARE(m_settings->selectedModelId(), 0);
    m_peer->sendTextMessage(rotorResponse(12));
    QTRY_COMPARE(m_settings->selectedModelId(), 12);
}

#ifdef Q_OS_UNIX
void RotorModelConnectionTests::virtualComDoesNotQuery()
{
    m_window->findChild<QCheckBox *>(QStringLiteral("virtualCom"))->setChecked(true);
    QVERIFY(connectDevice());
    QTest::qWait(100);
    QVERIFY(m_commands.isEmpty());
    m_peer->sendTextMessage(rotorResponse(5));
    QSignalSpy received(m_transport, &Communication::Transport::bytesReceived);
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(m_settings->selectedModelId(), 0);
}

void RotorModelConnectionTests::serialConnectionReadsModel()
{
    const int masterFd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    QVERIFY(masterFd >= 0);
    QFile master;
    QVERIFY(master.open(masterFd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle));
    QVERIFY(::grantpt(masterFd) == 0);
    QVERIFY(::unlockpt(masterFd) == 0);
    const char *slave = ::ptsname(masterFd);
    QVERIFY(slave);
    m_window->findChild<QRadioButton *>(QStringLiteral("serialMode"))->setChecked(true);
    m_window->findChild<QComboBox *>(QStringLiteral("serialPort"))
        ->setEditText(QString::fromLocal8Bit(slave));
    m_connection->connectRequested();
    QVERIFY(m_connection->isConnected());

    QByteArray commands;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        char buffer[1024];
        const auto count = ::read(masterFd, buffer, sizeof(buffer));
        if (count > 0)
            commands.append(buffer, count);
        return commands.contains("!ROTOR GET\r");
    }(), 3000);
    QCOMPARE(commands.count("!ROTOR GET\r"), 1);
    QVERIFY(!commands.contains("!ROTOR SET"));
    const QByteArray response = rotorResponse(9).toUtf8();
    QCOMPARE(::write(masterFd, response.constData(), response.size()), response.size());
    QTRY_COMPARE(m_settings->selectedModelId(), 9);
    m_connection->disconnectRequested();
    QCOMPARE(m_settings->selectedModelId(), 0);
}
#endif

QTEST_MAIN(RotorModelConnectionTests)
#include "RotorModelConnectionTests.moc"
