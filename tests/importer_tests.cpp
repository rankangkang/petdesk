#include "petformat.h"
#include <QDir>
#include <QFile>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace petdesk;
namespace {
void writeJson(const QString &path, const QJsonObject &object) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) qFatal("Cannot write fixture");
    file.write(QJsonDocument(object).toJson());
}
void writeAtlas(const QString &path, int version = 2, int missingRow = -1, const char *format = "PNG") {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(1536, version == 2 ? 2288 : 1872, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    const int rows = version == 2 ? 11 : 9;
    for (int row = 0; row < rows; ++row) {
        if (row == missingRow) continue;
        for (int column = 0; column < FrameCounts[row]; ++column)
            image.setPixelColor(column * FrameWidth + 50, row * FrameHeight + 80, QColor(80, 120, 160));
    }
    if (!image.save(path, format)) qFatal("Cannot encode fixture");
}
bool rejected(const QString &directory) {
    try { readPetDirectory(directory); return false; }
    catch (const std::exception &) { return true; }
}
}

class ImporterTests : public QObject {
    Q_OBJECT
private slots:
    void projectDirectoryPreservesNameAndPixels() {
        QTemporaryDir source;
        writeAtlas(source.filePath("assets/spritesheet.png"));
        writeJson(source.filePath("pet.json"), {{"name", "胡桃"}, {"slug", "hutao"},
            {"spriteVersionNumber", 2}, {"spritesheet", "assets/spritesheet.png"}});
        const auto pet = readPetDirectory(source.path());
        QCOMPARE(pet.name, QString("胡桃"));
        QCOMPARE(pet.version, 2);
        QCOMPARE(pet.atlas.pixelColor(50, 80), QColor(80, 120, 160));
        QCOMPARE(pet.atlas.pixelColor(6 * FrameWidth + 50, 80).alpha(), 0);
    }
    void standardWebpPackage() {
        QVERIFY(QImageWriter::supportedImageFormats().contains("webp"));
        QTemporaryDir source;
        writeAtlas(source.filePath("spritesheet.webp"), 2, -1, "WEBP");
        writeJson(source.filePath("pet.json"), {{"id", "test"}, {"displayName", "WebP pet"},
            {"spritesheetPath", "spritesheet.webp"}, {"spriteVersionNumber", 2}});
        QCOMPARE(readPetDirectory(source.path()).version, 2);
    }
    void skillRunPrefersFinalV2OverIntermediateV1() {
        QTemporaryDir source;
        writeAtlas(source.filePath("final/spritesheet.png"), 1);
        writeAtlas(source.filePath("final/spritesheet-extended.png"), 2);
        writeJson(source.filePath("pet_request.json"), {{"pet_id", "test"},
            {"display_name", "Skill pet"}, {"sprite_version_number", 2}});
        const auto pet = readPetDirectory(source.path());
        QCOMPARE(pet.version, 2);
        QCOMPARE(pet.name, QString("Skill pet"));
        QVERIFY(pet.atlasPath.endsWith("spritesheet-extended.png"));
    }
    void legacyV1WithoutVersionMarker() {
        QTemporaryDir source;
        writeAtlas(source.filePath("spritesheet.png"), 1);
        writeJson(source.filePath("pet.json"), {{"id", "legacy"}, {"displayName", "Legacy"},
            {"spritesheetPath", "spritesheet.png"}});
        QCOMPARE(readPetDirectory(source.path()).version, 1);
    }
    void rejectsVersionMismatchAndMissingRequiredFrames() {
        QTemporaryDir source;
        writeAtlas(source.filePath("spritesheet.png"), 1);
        writeJson(source.filePath("pet.json"), {{"spriteVersionNumber", 2}, {"spritesheetPath", "spritesheet.png"}});
        QVERIFY(rejected(source.path()));
        writeAtlas(source.filePath("spritesheet.png"), 2, 10);
        QVERIFY(rejected(source.path()));
    }
    void rejectsPathsOutsideSelectedDirectory() {
        QTemporaryDir parent;
        writeAtlas(parent.filePath("outside.png"));
        QDir().mkpath(parent.filePath("pet"));
        writeJson(parent.filePath("pet/pet.json"), {{"spritesheetPath", "../outside.png"}});
        QVERIFY(rejected(parent.filePath("pet")));
        QVERIFY(QFile::link(parent.filePath("outside.png"), parent.filePath("pet/linked.png")));
        writeJson(parent.filePath("pet/pet.json"), {{"spritesheetPath", "linked.png"}});
        QVERIFY(rejected(parent.filePath("pet")));
    }
    void rejectsMalformedMetadataAndWrongDigest() {
        QTemporaryDir source;
        writeAtlas(source.filePath("spritesheet.png"));
        writeJson(source.filePath("pet.json"), {{"spriteVersionNumber", 2.5}});
        QVERIFY(rejected(source.path()));
        writeJson(source.filePath("pet.json"), {{"sha256", "incorrect"}});
        QVERIFY(rejected(source.path()));
    }
    void copiedLibrarySurvivesSourceRemovalAndDeduplicates() {
        QTemporaryDir source, storage;
        const QString atlas = source.filePath("spritesheet.png");
        writeAtlas(atlas);
        writeJson(source.filePath("pet.json"), {{"displayName", "Offline pet"}, {"spritesheetPath", "spritesheet.png"}});
        const auto original = readPetDirectory(source.path());
        PetLibrary library(storage.path());
        const auto first = library.importDirectory(source.path());
        QCOMPARE(library.importDirectory(source.path()).key, first.key);
        QCOMPARE(library.entries().size(), 1);
        QVERIFY(QFile::remove(atlas));
        QVERIFY(QFile::remove(source.filePath("pet.json")));
        const auto imported = library.load(first.key);
        QCOMPARE(imported.name, QString("Offline pet"));
        QCOMPARE(imported.sha256, original.sha256);
        QCOMPARE(imported.atlas, original.atlas);
        QVERIFY(imported.encoded.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ImporterTests)
#include "importer_tests.moc"
