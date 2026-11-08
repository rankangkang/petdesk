#include "bundledpets.h"
#include "petformat.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <stdexcept>

namespace petdesk {
void seedBundledPets(const QString &directory, PetLibrary &library) {
    const QFileInfo rootInfo(directory);
    if (!rootInfo.exists()) { library.setProtectedKeys({}); return; }
    if (!rootInfo.isDir() || rootInfo.isSymLink()) throw std::runtime_error("Invalid bundled-pets directory");
    QFile manifest(QDir(directory).filePath("manifest.json"));
    if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 64 * 1024)
        throw std::runtime_error("Cannot read the bundled-pets manifest");
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(manifest.readAll(), &error);
    const QJsonObject root = document.object();
    if (error.error != QJsonParseError::NoError || root.value("version").toInt() != 1 || !root.value("pets").isArray())
        throw std::runtime_error("Invalid bundled-pets manifest");
    const QRegularExpression safeName(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    const QRegularExpression safeKey(QStringLiteral("^[a-f0-9]{64}$"));
    const QJsonArray entries = root.value("pets").toArray();
    if (entries.size() > 100) throw std::runtime_error("Too many bundled pets");
    QSet<QString> seen, keys;
    // Validate the entire manifest before making any library changes.
    for (const auto &value : entries) {
        const QJsonObject item = value.toObject();
        const QString id = item.value("id").toString(), folder = item.value("directory").toString();
        const QString key = item.value("key").toString();
        if (!safeName.match(id).hasMatch() || !safeName.match(folder).hasMatch() ||
            !safeKey.match(key).hasMatch() || seen.contains(id) || keys.contains(key))
            throw std::runtime_error("Invalid bundled-pet entry");
        seen.insert(id); keys.insert(key);
    }
    // Protection is specific to this distribution and lasts only while its manifest is present.
    library.setProtectedKeys(keys);
    QSet<QString> active, removed;
    for (const auto &entry : library.entries()) active.insert(entry.key);
    for (const auto &entry : library.archivedEntries()) removed.insert(entry.key);
    for (const auto &value : entries) {
        const QJsonObject item = value.toObject();
        const QString key = item.value("key").toString();
        const QFileInfo source(QDir(directory).filePath(item.value("directory").toString()));
        if (!source.isDir() || source.isSymLink()) throw std::runtime_error("Missing bundled-pet directory");
        if (active.contains(key)) continue;
        if (removed.contains(key)) {
            library.restore(key);
        } else {
            const auto pet = readPetDirectory(source.absoluteFilePath());
            if (petLibraryKey(pet) != key) throw std::runtime_error("Bundled-pet identity mismatch");
            library.importDirectory(source.absoluteFilePath());
        }
    }
}
}
