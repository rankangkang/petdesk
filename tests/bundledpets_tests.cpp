#include "bundledpets.h"
#include "petformat.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>

using namespace petdesk;
namespace {
void makePet(const QString &path, const QString &name) {
    QVERIFY(QDir().mkpath(path));
    QImage image(1536, 2288, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int row = 0; row < 11; ++row)
        for (int col = 0; col < FrameCounts[row]; ++col)
            image.setPixelColor(col * FrameWidth + 60, row * FrameHeight + 60, Qt::blue);
    QVERIFY(image.save(QDir(path).filePath("spritesheet.png")));
    QFile file(QDir(path).filePath("pet.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(QJsonObject{{"name", name}, {"spritesheetPath", "spritesheet.png"}}).toJson());
}
void writeManifest(const QString &path, const QJsonArray &entries) {
    QFile file(QDir(path).filePath("manifest.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"pets", entries}}).toJson());
}
QJsonObject manifestItem(const QString &path, const QString &id) {
    return {{"id", id}, {"directory", id}, {"key", petLibraryKey(readPetDirectory(QDir(path).filePath(id)))}};
}
}

class BundledPetsTests : public QObject {
    Q_OBJECT
private slots:
    void missingBundleLeavesOrdinaryPetsRemovable() {
        QTemporaryDir root, source;
        makePet(source.filePath("pet"), "Ordinary");
        PetLibrary library(root.filePath("library"));
        const auto key = library.importDirectory(source.filePath("pet")).key;
        seedBundledPets(root.filePath("missing"), library);
        QVERIFY(!library.entries().first().builtIn);
        library.archive(key);
        QCOMPARE(library.archivedEntries().size(), 1);
    }
    void seedsAndProtectsBuiltInsWithoutDuplicatingExistingPets() {
        QTemporaryDir root, bundle;
        QJsonArray manifest;
        for (const auto &id : {"alpha", "beta", "gamma"}) {
            makePet(bundle.filePath(id), id);
            manifest.append(manifestItem(bundle.path(), id));
        }
        writeManifest(bundle.path(), manifest);
        PetLibrary library(root.filePath("library"));
        const auto existing = library.importDirectory(bundle.filePath("alpha"));
        const auto archived = library.importDirectory(bundle.filePath("beta"));
        library.archive(archived.key);
        seedBundledPets(bundle.path(), library);
        QCOMPARE(library.entries().size(), 3);
        QVERIFY(library.archivedEntries().isEmpty());
        for (const auto &entry : library.entries()) {
            QVERIFY(entry.builtIn);
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, library.archive(entry.key));
        }
        QVERIFY(library.isProtected(existing.key));
        seedBundledPets(bundle.path(), library);
        QCOMPARE(library.entries().size(), 3);
        seedBundledPets(root.filePath("missing"), library);
        QVERIFY(!library.isProtected(existing.key));
        library.archive(existing.key);
        QCOMPARE(library.entries().size(), 2);
    }
    void missingAssetCanBeRetriedWithoutDuplicates() {
        QTemporaryDir root, bundle, holding;
        makePet(bundle.filePath("alpha"), "Alpha");
        makePet(bundle.filePath("beta"), "Beta");
        writeManifest(bundle.path(), {manifestItem(bundle.path(), "alpha"), manifestItem(bundle.path(), "beta")});
        QVERIFY(QDir().rename(bundle.filePath("beta"), holding.filePath("beta")));
        PetLibrary library(root.filePath("library"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, seedBundledPets(bundle.path(), library));
        QCOMPARE(library.entries().size(), 1);
        QVERIFY(library.entries().first().builtIn);
        QVERIFY(QDir().rename(holding.filePath("beta"), bundle.filePath("beta")));
        seedBundledPets(bundle.path(), library);
        QCOMPARE(library.entries().size(), 2);
    }
    void validatesManifestBeforeChangingLibrary() {
        QTemporaryDir root, bundle;
        makePet(bundle.filePath("alpha"), "Alpha");
        const auto item = manifestItem(bundle.path(), "alpha");
        auto unsafe = item; unsafe.insert("id", "other"); unsafe.insert("directory", "../outside");
        writeManifest(bundle.path(), {item, unsafe});
        PetLibrary library(root.filePath("library"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, seedBundledPets(bundle.path(), library));
        QVERIFY(library.entries().isEmpty());
        auto duplicate = item; duplicate.insert("id", "other");
        writeManifest(bundle.path(), {item, duplicate});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, seedBundledPets(bundle.path(), library));
        QVERIFY(library.entries().isEmpty());
    }
};
QTEST_GUILESS_MAIN(BundledPetsTests)
#include "bundledpets_tests.moc"
