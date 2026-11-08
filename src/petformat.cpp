#include "petformat.h"
#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <algorithm>
#include <stdexcept>

namespace petdesk {
namespace {
constexpr qint64 MaxImageBytes = 20 * 1024 * 1024;
constexpr qint64 MaxManifestBytes = 128 * 1024;

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toUtf8().constData());
}

QByteArray readBounded(const QString &path, qint64 limit) {
    QFile file(path);
    const QFileInfo info(file);
    if (!info.isFile() || info.size() < 1 || info.size() > limit || !file.open(QIODevice::ReadOnly))
        fail(QStringLiteral("无法读取文件，或文件大小超出限制：%1").arg(info.fileName()));
    const auto bytes = file.read(limit + 1);
    if (bytes.size() > limit || file.error() != QFile::NoError)
        fail(QStringLiteral("读取文件失败：%1").arg(info.fileName()));
    return bytes;
}

QJsonObject readObject(const QString &path) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(readBounded(path, MaxManifestBytes), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(QStringLiteral("宠物说明文件不是有效的 JSON 对象：%1").arg(QFileInfo(path).fileName()));
    return document.object();
}

QString firstString(const QJsonObject &object, const QStringList &keys) {
    for (const auto &key : keys) {
        const auto text = object.value(key).toString().trimmed();
        if (!text.isEmpty()) return text;
    }
    return {};
}

QString safeAssetPath(const QString &root, const QString &relative) {
    if (relative.isEmpty() || QDir::isAbsolutePath(relative))
        fail(QStringLiteral("动画图路径必须位于所选宠物目录中。"));
    const QString path = QFileInfo(QDir(root).filePath(relative)).canonicalFilePath();
    if (path.isEmpty() || !path.startsWith(root.endsWith('/') ? root : root + '/'))
        fail(QStringLiteral("动画图不存在，或路径指向了宠物目录之外。"));
    return path;
}

bool cellHasPixels(const QImage &image, const QRect &rect) {
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = rect.left(); x <= rect.right(); ++x)
            if (qAlpha(line[x]) != 0) return true;
    }
    return false;
}

void saveBytes(const QString &path, const QByteArray &bytes) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        fail(QStringLiteral("无法保存宠物到本机。请检查可用空间和目录权限。"));
}

PetEntry entryFromFolder(const QString &directory) {
    const auto metadata = readObject(QDir(directory).filePath("pet.json"));
    return {QFileInfo(directory).fileName(), metadata.value("displayName").toString(),
            metadata.value("description").toString(), directory,
            QDir(directory).filePath("thumbnail.png"), metadata.value("spriteVersionNumber").toInt()};
}

bool validKey(const QString &key) {
    return QRegularExpression("^[a-f0-9]{64}$").match(key).hasMatch();
}
QString ownedFolder(const QString &root, const QString &key) {
    if (!validKey(key)) fail(QStringLiteral("本地宠物标识无效。"));
    const auto folder = QFileInfo(QDir(root).filePath(key));
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    if (!folder.isDir() || folder.isSymLink() || canonicalRoot.isEmpty() ||
        !folder.canonicalFilePath().startsWith(canonicalRoot + '/'))
        fail(QStringLiteral("本地宠物文件不存在，或已被移动。"));
    return folder.absoluteFilePath();
}
}

QRect PetData::frameRect(int row, int column) const {
    return {column * FrameWidth, row * FrameHeight, FrameWidth, FrameHeight};
}

PetData readPetDirectory(const QString &directory) {
    const auto directoryInfo = QFileInfo(directory);
    const QString root = directoryInfo.canonicalFilePath();
    if (!directoryInfo.isDir() || root.isEmpty())
        fail(QStringLiteral("请选择一个本地宠物目录。"));
    QJsonObject metadata;
    const QString packagePath = QDir(root).filePath("pet.json");
    const QString requestPath = QDir(root).filePath("pet_request.json");
    const bool skillRun = !QFile::exists(packagePath) && QFile::exists(requestPath);
    if (QFile::exists(packagePath)) metadata = readObject(packagePath);
    else if (skillRun) metadata = readObject(requestPath);

    // Some skill deliveries save a cloud verification record under pet.json.
    if (metadata.value("pet").isObject()) {
        const auto record = metadata.value("pet").toObject();
        if (!metadata.contains("name")) metadata.insert("name", record.value("name"));
        if (!metadata.contains("description")) metadata.insert("description", record.value("description"));
    }
    PetData pet;
    pet.id = firstString(metadata, {"id", "slug", "pet_id"});
    pet.name = firstString(metadata, {"displayName", "display_name", "name"});
    if (pet.name.isEmpty()) pet.name = QFileInfo(root).fileName();
    pet.name = pet.name.left(100);
    pet.description = metadata.value("description").toString().left(1000);
    auto version = metadata.value("spriteVersionNumber");
    if (version.isUndefined()) version = metadata.value("sprite_version_number");
    if (!version.isUndefined() && (!version.isDouble() || (version.toDouble() != 1 && version.toDouble() != 2)))
        fail(QStringLiteral("当前只支持 Codex v1 和 v2 宠物图集。"));

    const QString explicitAsset = firstString(metadata, {"spritesheetPath", "spritesheet"});
    if (!explicitAsset.isEmpty()) {
        pet.atlasPath = safeAssetPath(root, explicitAsset);
    } else {
        QStringList candidates;
        if (skillRun) candidates << "final/spritesheet-extended.png" << "final/spritesheet-extended.webp";
        candidates << "spritesheet.webp" << "spritesheet.png" << "assets/spritesheet.png"
                   << "assets/spritesheet.webp" << "final/spritesheet-extended.png"
                   << "final/spritesheet-extended.webp" << "final/spritesheet.png" << "final/spritesheet.webp";
        for (const auto &candidate : candidates) {
            if (QFile::exists(QDir(root).filePath(candidate))) {
                pet.atlasPath = safeAssetPath(root, candidate);
                break;
            }
        }
    }
    if (pet.atlasPath.isEmpty())
        fail(QStringLiteral("没有找到最终动画图。请选择包含 pet.json 和动画图，或 pet_request.json 与 final 目录的宠物目录。"));

    pet.encoded = readBounded(pet.atlasPath, MaxImageBytes);
    QBuffer imageBuffer(&pet.encoded);
    imageBuffer.open(QIODevice::ReadOnly);
    if (pet.encoded.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) pet.format = "png";
    else if (pet.encoded.startsWith("RIFF") && pet.encoded.mid(8, 4) == "WEBP") pet.format = "webp";
    else fail(QStringLiteral("动画图需要是 PNG 或 WebP 文件。"));
    QImageReader reader(&imageBuffer, pet.format.toLatin1());
    const QSize dimensions = reader.size();
    if (dimensions == QSize(1536, 1872)) pet.version = 1;
    else if (dimensions == QSize(1536, 2288)) pet.version = 2;
    else fail(QStringLiteral("动画图尺寸应为 1536×1872（v1）或 1536×2288（v2）。"));
    if (!version.isUndefined() && version.toInt() != pet.version)
        fail(QStringLiteral("宠物说明中的版本与动画图尺寸不一致。"));
    if (reader.imageCount() > 1)
        fail(QStringLiteral("请使用单张精灵图，而不是 GIF 或动态 WebP。"));

    // Decode the same bytes that are copied into the library, not a second file read.
    const QImage decoded = QImage::fromData(pet.encoded, pet.format.toLatin1().constData());
    if (decoded.isNull() || decoded.size() != dimensions || !decoded.hasAlphaChannel())
        fail(QStringLiteral("动画图解码失败，或没有透明通道。"));
    pet.atlas = decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int rows = pet.version == 2 ? 11 : 9;
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column < FrameCounts[row]; ++column)
            if (!cellHasPixels(pet.atlas, pet.frameRect(row, column)))
                fail(QStringLiteral("动画图缺少内容：第 %1 行、第 %2 帧（从 1 计数）。").arg(row + 1).arg(column + 1));
    pet.sha256 = QString::fromLatin1(QCryptographicHash::hash(pet.encoded, QCryptographicHash::Sha256).toHex());
    const auto expectedHash = metadata.value("sha256").toString();
    if (!expectedHash.isEmpty() && expectedHash.compare(pet.sha256, Qt::CaseInsensitive) != 0)
        fail(QStringLiteral("动画图与说明文件的校验值不一致，请重新导出素材。"));
    return pet;
}

PetLibrary::PetLibrary(QString root) : m_root(std::move(root)) {}

QVector<PetEntry> PetLibrary::entries() const {
    QVector<PetEntry> result;
    for (const auto &folder : QDir(m_root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!validKey(folder.fileName()) || folder.isSymLink()) continue;
        try { result.append(entryFromFolder(folder.absoluteFilePath())); }
        catch (const std::exception &) { /* Incomplete directories are not published imports. */ }
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return result;
}

PetEntry PetLibrary::importDirectory(const QString &directory) const {
    auto pet = readPetDirectory(directory);
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
        pet.sha256.toUtf8() + '\0' + pet.name.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (!QDir().mkpath(m_root)) fail(QStringLiteral("无法创建本地宠物目录。"));
    const QString target = QDir(m_root).filePath(key);
    if (QDir(target).exists()) {
        load(key); // A damaged previous copy must not be reported as a successful import.
        return entryFromFolder(target);
    }
    if (QFileInfo::exists(QDir(archiveRoot()).filePath(key))) return restore(key);
    QTemporaryDir staging(QDir(m_root).filePath(".import-XXXXXX"));
    if (!staging.isValid()) fail(QStringLiteral("无法创建导入暂存目录。"));
    const QString asset = "spritesheet." + pet.format;
    saveBytes(QDir(staging.path()).filePath(asset), pet.encoded);
    QJsonObject metadata{{"id", pet.id.isEmpty() ? key : pet.id}, {"displayName", pet.name},
        {"description", pet.description}, {"spritesheetPath", asset},
        {"spriteVersionNumber", pet.version}, {"sha256", pet.sha256}};
    saveBytes(QDir(staging.path()).filePath("pet.json"), QJsonDocument(metadata).toJson());
    if (!pet.atlas.copy(pet.frameRect(0, 0)).scaled(96, 104, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .save(QDir(staging.path()).filePath("thumbnail.png")))
        fail(QStringLiteral("无法保存宠物缩略图。"));
    readPetDirectory(staging.path());
    if (!QDir().rename(staging.path(), target)) fail(QStringLiteral("无法完成宠物导入。"));
    staging.setAutoRemove(false);
    return entryFromFolder(target);
}

PetData PetLibrary::load(const QString &key) const {
    auto pet = readPetDirectory(ownedFolder(m_root, key));
    pet.encoded.clear();
    pet.encoded.squeeze();
    return pet;
}

QString PetLibrary::archiveRoot() const {
    const QString path = QDir(m_root).filePath(".removed");
    if (QFileInfo(path).isSymLink()) fail(QStringLiteral("已移出目录不能是符号链接。"));
    return path;
}

QVector<PetEntry> PetLibrary::archivedEntries() const {
    return PetLibrary(archiveRoot()).entries();
}

void PetLibrary::archive(const QString &key) const {
    const QString source = ownedFolder(m_root, key);
    const QString target = QDir(archiveRoot()).filePath(key);
    if (QFileInfo::exists(target)) fail(QStringLiteral("已移出列表中存在同一只宠物，请先恢复它。"));
    if (!QDir().mkpath(archiveRoot()) || !QDir().rename(source, target))
        fail(QStringLiteral("无法移出这只宠物，请检查本地目录权限。"));
}

PetEntry PetLibrary::restore(const QString &key) const {
    const QString source = ownedFolder(archiveRoot(), key);
    // Validate before publishing a removed entry back into the library.
    readPetDirectory(source);
    const QString target = QDir(m_root).filePath(key);
    if (QFileInfo::exists(target)) fail(QStringLiteral("宠物库里已经有这只宠物，无需重复恢复。"));
    if (!QDir().rename(source, target)) fail(QStringLiteral("无法恢复这只宠物，请检查本地目录权限。"));
    return entryFromFolder(target);
}
}
