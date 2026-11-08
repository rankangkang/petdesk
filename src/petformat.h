#pragma once
#include <QByteArray>
#include <QImage>
#include <QString>
#include <QSet>
#include <QVector>
#include <array>
#include <utility>

namespace petdesk {
inline constexpr int FrameWidth = 192;
inline constexpr int FrameHeight = 208;
inline constexpr int Columns = 8;
inline constexpr std::array<int, 11> FrameCounts = {6, 8, 8, 4, 5, 8, 6, 6, 6, 8, 8};

struct PetData {
    QString id, name, description, atlasPath, format, sha256;
    int version = 0;
    QImage atlas;
    QByteArray encoded;
    QRect frameRect(int row, int column) const;
};

struct PetEntry {
    QString key, name, description, directory, thumbnail;
    int version = 0;
    bool builtIn = false;
};

// Accepts an individual Codex package, the Hu Tao project, or a create-pet run.
// JSON is data only. No scripts or agent services are executed.
PetData readPetDirectory(const QString &directory);
QString petLibraryKey(const PetData &pet);

class PetLibrary {
public:
    explicit PetLibrary(QString root);
    QVector<PetEntry> entries() const;
    PetEntry importDirectory(const QString &directory) const;
    PetData load(const QString &key) const;
    QVector<PetEntry> archivedEntries() const;
    void archive(const QString &key) const;
    PetEntry restore(const QString &key) const;
    void setProtectedKeys(QSet<QString> keys) { m_protectedKeys = std::move(keys); }
    bool isProtected(const QString &key) const { return m_protectedKeys.contains(key); }
    QString archiveRoot() const;
    QString root() const { return m_root; }
private:
    QString m_root;
    QSet<QString> m_protectedKeys;
};
}
