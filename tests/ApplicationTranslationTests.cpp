#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTranslator>
#include <QtTest>

class ApplicationTranslationTests : public QObject
{
    Q_OBJECT
private slots:
    void chineseMenusAndStandardButtons();
};

void ApplicationTranslationTests::chineseMenusAndStandardButtons()
{
    QTranslator translator;
    QVERIFY(translator.load(QString::fromUtf8(APP_TRANSLATION_FILE)));
    QVERIFY(qApp->installTranslator(&translator));
    QCOMPARE(QCoreApplication::translate("MainWindow", "Files"), QString::fromUtf8("文件"));
    QDialogButtonBox buttons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    QCOMPARE(buttons.button(QDialogButtonBox::Cancel)->text(), QString::fromUtf8("取消"));
    QCOMPARE(buttons.button(QDialogButtonBox::Ok)->text(), QString::fromUtf8("确定"));
}

QTEST_MAIN(ApplicationTranslationTests)
#include "ApplicationTranslationTests.moc"
