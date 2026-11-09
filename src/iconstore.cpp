#include "iconstore.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <stdexcept>

namespace petdesk {
namespace {
QImage readIcon(const QString &path) {
    const QFileInfo file(path);
    if (!file.isFile() || file.size() <= 0 || file.size() > 10 * 1024 * 1024)
        throw std::runtime_error("图标文件无法读取，或超过 10 MB。");
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QByteArray format = reader.format().toLower();
    const QSize size = reader.size();
    if ((format != "png" && format != "jpeg" && format != "jpg" && format != "webp") || !size.isValid() ||
        size.width() > 8192 || size.height() > 8192 || qint64(size.width()) * size.height() > 16 * 1024 * 1024)
        throw std::runtime_error("请选择不超过 1600 万像素的 PNG、JPEG 或 WebP 图片。");
    const QImage source = reader.read();
    if (source.isNull()) throw std::runtime_error("无法读取图标图片。");
    const QImage scaled = source.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    bool visible = false;
    for (int y = 0; y < scaled.height() && !visible; ++y)
        for (int x = 0; x < scaled.width(); ++x)
            if (scaled.pixelColor(x, y).alpha() != 0) { visible = true; break; }
    if (!visible) throw std::runtime_error("图标不能是完全透明的图片。");
    QImage result(256, 256, QImage::Format_ARGB32_Premultiplied); result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawImage((256 - scaled.width()) / 2, (256 - scaled.height()) / 2, scaled);
    return result;
}
}
IconStore::IconStore(const QString &dataRoot) : m_path(QDir(dataRoot).filePath("custom-icon.png")) {}
QImage IconStore::importFile(const QString &path) const {
    const QImage icon = readIcon(path);
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) throw std::runtime_error("无法创建图标保存目录。");
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || !icon.save(&file, "PNG") || !file.commit())
        throw std::runtime_error("无法保存图标，请检查可用空间和目录权限。");
    return icon;
}
QImage IconStore::load() const {
    try { return readIcon(m_path); }
    catch (const std::exception &) { return {}; }
}
}
