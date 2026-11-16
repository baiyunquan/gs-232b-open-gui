#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>

class AppSettingsTests final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsDoNotWrite();
    void savesImmediatelyAndPreservesOtherKeys();
    void readsExistingPresets();
    void persistsPresetBoundaries();
    void invalidValuesUseDefaults_data();
    void invalidValuesUseDefaults();
    void rejectsInvalidPresetWrites();
    void validatesAndPersistsStationGrid_data();
    void validatesAndPersistsStationGrid();
    void clearsStationGrid();
    void invalidStoredGridDoesNotWrite();
    void reportsStorageFailure();
};

void AppSettingsTests::defaultsDoNotWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(!settings.consoleVisible());
    QVERIFY(settings.stationGrid().isEmpty());
    const std::array<int, 8> azimuth{0, 45, 90, 135, 180, 225, 270, 315};
    const std::array<int, 8> elevation{0, 25, 50, 75, 100, 125, 150, 180};
    for (int index = 0; index < AppSettings::PresetCount; ++index) {
        QCOMPARE(settings.presetAngle(AppSettings::Axis::Azimuth, index), azimuth[index]);
        QCOMPARE(settings.presetAngle(AppSettings::Axis::Elevation, index), elevation[index]);
    }
    QVERIFY(!QFile::exists(fileName));
}

void AppSettingsTests::savesImmediatelyAndPreservesOtherKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    {
        QSettings existing(fileName, QSettings::IniFormat);
        existing.setValue(QStringLiteral("azimuthPreset3"), 123);
        existing.setValue(QStringLiteral("other/value"), QStringLiteral("keep"));
    }

    AppSettings settings(fileName);
    for (bool visible : {true, false}) {
        QVERIFY(settings.setConsoleVisible(visible));
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray content = file.readAll();
        QVERIFY(content.contains(visible ? "consoleVisible=true" : "consoleVisible=false"));
        AppSettings reopened(fileName);
        QCOMPARE(reopened.consoleVisible(), visible);
        QCOMPARE(reopened.presetAngle(AppSettings::Axis::Azimuth, 3), 123);
        QSettings raw(fileName, QSettings::IniFormat);
        QCOMPARE(raw.value(QStringLiteral("other/value")).toString(), QStringLiteral("keep"));
    }
}

void AppSettingsTests::readsExistingPresets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray content("[General]\nazimuthPreset0=450\nelevationPreset7=90\n");
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Azimuth, 0), 450);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Elevation, 7), 90);
    QVERIFY(settings.setPresetAngle(AppSettings::Axis::Elevation, 7, 180));
    QSettings raw(fileName, QSettings::IniFormat);
    QCOMPARE(raw.value(QStringLiteral("azimuthPreset0")).toInt(), 450);
    QCOMPARE(raw.value(QStringLiteral("elevationPreset7")).toInt(), 180);
    QCOMPARE(raw.allKeys().size(), 2);
}

void AppSettingsTests::persistsPresetBoundaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    {
        AppSettings settings(fileName);
        QVERIFY(settings.setConsoleVisible(true));
        for (int index = 0; index < AppSettings::PresetCount; ++index) {
            QVERIFY(settings.setPresetAngle(AppSettings::Axis::Azimuth, index, index % 2 ? 450 : 0));
            QVERIFY(settings.setPresetAngle(AppSettings::Axis::Elevation, index, index % 2 ? 180 : 0));
        }
    }
    AppSettings reopened(fileName);
    QVERIFY(reopened.consoleVisible());
    for (int index = 0; index < AppSettings::PresetCount; ++index) {
        QCOMPARE(reopened.presetAngle(AppSettings::Axis::Azimuth, index), index % 2 ? 450 : 0);
        QCOMPARE(reopened.presetAngle(AppSettings::Axis::Elevation, index), index % 2 ? 180 : 0);
    }
}

void AppSettingsTests::invalidValuesUseDefaults_data()
{
    QTest::addColumn<QByteArray>("azimuth");
    QTest::addColumn<QByteArray>("elevation");
    QTest::addColumn<QByteArray>("console");
    QTest::addColumn<bool>("visible");
    QTest::newRow("empty") << QByteArray() << QByteArray() << QByteArray() << false;
    QTest::newRow("non-numeric") << QByteArray("bad") << QByteArray("true") << QByteArray("bad") << false;
    QTest::newRow("negative") << QByteArray("-1") << QByteArray("-1") << QByteArray("-1") << false;
    QTest::newRow("too-large") << QByteArray("451") << QByteArray("181") << QByteArray("2") << false;
    QTest::newRow("fractional") << QByteArray("90.5") << QByteArray("10.5") << QByteArray("false") << false;
    QTest::newRow("overflow") << QByteArray("4294967296") << QByteArray("4294967296") << QByteArray("0") << false;
    QTest::newRow("bool-string") << QByteArray("bad") << QByteArray("bad") << QByteArray("true") << true;
    QTest::newRow("bool-number") << QByteArray("bad") << QByteArray("bad") << QByteArray("1") << true;
}

void AppSettingsTests::invalidValuesUseDefaults()
{
    QFETCH(QByteArray, azimuth);
    QFETCH(QByteArray, elevation);
    QFETCH(QByteArray, console);
    QFETCH(bool, visible);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content = "[General]\nazimuthPreset1=" + azimuth +
                               "\nelevationPreset1=" + elevation +
                               "\n[view]\nconsoleVisible=" + console + '\n';
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Azimuth, 1), 45);
    QCOMPARE(settings.presetAngle(AppSettings::Axis::Elevation, 1), 25);
    QCOMPARE(settings.consoleVisible(), visible);
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::rejectsInvalidPresetWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    for (auto axis : {AppSettings::Axis::Azimuth, AppSettings::Axis::Elevation}) {
        const int beyondMaximum = axis == AppSettings::Axis::Azimuth ? 451 : 181;
        QVERIFY(!settings.setPresetAngle(axis, -1, 90));
        QVERIFY(!settings.setPresetAngle(axis, AppSettings::PresetCount, 90));
        QVERIFY(!settings.setPresetAngle(axis, 0, -1));
        QVERIFY(!settings.setPresetAngle(axis, 0, beyondMaximum));
        QCOMPARE(settings.presetAngle(axis, -1), 0);
        QCOMPARE(settings.presetAngle(axis, AppSettings::PresetCount), 0);
    }
    QVERIFY(!QFile::exists(fileName));
}

void AppSettingsTests::validatesAndPersistsStationGrid_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<bool>("valid");
    QTest::newRow("four-characters") << QStringLiteral("PM95") << true;
    QTest::newRow("six-characters") << QStringLiteral("PM95AA") << true;
    QTest::newRow("eight-characters") << QStringLiteral("PM95AA12") << true;
    QTest::newRow("minimum") << QStringLiteral("AA00AA00") << true;
    QTest::newRow("maximum") << QStringLiteral("RR99XX99") << true;
    QTest::newRow("lowercase") << QStringLiteral("pm95aa") << true;
    QTest::newRow("trimmed") << QStringLiteral("  pM95aA12\n") << true;
    QTest::newRow("too-short") << QStringLiteral("PM") << false;
    QTest::newRow("odd-length") << QStringLiteral("PM95A") << false;
    QTest::newRow("field-out-of-range") << QStringLiteral("SS00") << false;
    QTest::newRow("subsquare-out-of-range") << QStringLiteral("PM95YA") << false;
    QTest::newRow("non-numeric-square") << QStringLiteral("PM9A") << false;
    QTest::newRow("non-numeric-extension") << QStringLiteral("PM95AAAA") << false;
    QTest::newRow("too-long") << QStringLiteral("PM95AA12AA") << false;
    QTest::newRow("internal-space") << QStringLiteral("PM 95AA") << false;
    QTest::newRow("internal-newline") << QStringLiteral("PM95\nAA") << false;
    QTest::newRow("fullwidth-digits") << QStringLiteral("PM９５") << false;
    QTest::newRow("unicode-case-expansion") << QStringLiteral("PM95ß") << false;
}

void AppSettingsTests::validatesAndPersistsStationGrid()
{
    QFETCH(QString, grid);
    QFETCH(bool, valid);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    QVERIFY(settings.setConsoleVisible(true));
    QVERIFY(settings.setStationGrid(QStringLiteral("AA00")));
    QFile before(fileName);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const QByteArray original = before.readAll();
    before.close();

    QCOMPARE(AppSettings::isValidStationGrid(grid), valid);
    QCOMPARE(settings.setStationGrid(grid), valid);
    const QString expected = valid ? grid.trimmed().toUpper() : QStringLiteral("AA00");
    QCOMPARE(settings.stationGrid(), expected);
    AppSettings reopened(fileName);
    QCOMPARE(reopened.stationGrid(), expected);
    QVERIFY(reopened.consoleVisible());
    QFile after(fileName);
    QVERIFY(after.open(QIODevice::ReadOnly));
    const QByteArray content = after.readAll();
    if (valid)
        QVERIFY(content.contains("grid=" + expected.toUtf8()));
    else
        QCOMPARE(content, original);
}

void AppSettingsTests::clearsStationGrid()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    AppSettings settings(fileName);
    for (const QString &empty : {QString(), QStringLiteral("  ")}) {
        QVERIFY(settings.setStationGrid(QStringLiteral("PM95AA")));
        QVERIFY(!AppSettings::isValidStationGrid(empty));
        QVERIFY(settings.setStationGrid(empty));
        AppSettings reopened(fileName);
        QVERIFY(reopened.stationGrid().isEmpty());
    }
}

void AppSettingsTests::invalidStoredGridDoesNotWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    const QByteArray content("[station]\ngrid=ZZ99AA\n");
    {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }
    AppSettings settings(fileName);
    QVERIFY(settings.stationGrid().isEmpty());
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), content);
}

void AppSettingsTests::reportsStorageFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("settings.ini"));
    QVERIFY(QDir().mkdir(fileName));
    AppSettings settings(fileName);
    QVERIFY(!settings.setConsoleVisible(true));
    QVERIFY(!settings.setStationGrid(QStringLiteral("PM95AA")));
    QVERIFY(!settings.setPresetAngle(AppSettings::Axis::Azimuth, 0, 90));
}

QTEST_GUILESS_MAIN(AppSettingsTests)
#include "AppSettingsTests.moc"
