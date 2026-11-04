#include "MainWindow.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"),
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    QTranslator translator;
    if (QLocale::system().language() == QLocale::Chinese &&
        translator.load(QStringLiteral(":/i18n/gs_232b_open_gui_zh_CN.qm")))
        app.installTranslator(&translator);

    MainWindow window;
    window.show();
    return app.exec();
}
