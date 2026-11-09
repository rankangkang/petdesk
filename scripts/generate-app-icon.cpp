// Package the selected artwork at the sizes required by desktop platforms.
#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QVector>

static bool writeIco(const QImage &image, const QString &path) {
    const QVector<int> sizes{16, 24, 32, 48, 64, 128, 256};
    QVector<QByteArray> frames;
    for (int size : sizes) {
        QByteArray data;
        QBuffer buffer(&data);
        if (!buffer.open(QIODevice::WriteOnly) ||
            !image.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG")) return false;
        frames.append(data);
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0) << quint16(1) << quint16(frames.size());
    quint32 offset = 6 + quint32(frames.size()) * 16;
    for (int i = 0; i < frames.size(); ++i) {
        const auto dimension = quint8(sizes[i] == 256 ? 0 : sizes[i]);
        const auto length = quint32(frames[i].size());
        stream << dimension << dimension << quint8(0) << quint8(0)
               << quint16(1) << quint16(32) << length << offset;
        offset += length;
    }
    for (const auto &frame : frames)
        if (stream.writeRawData(frame.constData(), frame.size()) != frame.size()) return false;
    return stream.status() == QDataStream::Ok;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 7 && !(argc == 8 && QByteArray(argv[7]) == "--preserve")) {
        qWarning("Usage: petdesk_icon_generator artwork.png status-artwork.png app.png app.iconset status.png app.ico [--preserve]");
        return 2;
    }
    const QImage artwork(QString::fromLocal8Bit(argv[1]));
    const QImage statusArtwork(QString::fromLocal8Bit(argv[2]));
    if (artwork.isNull() || artwork.width() != artwork.height() || statusArtwork.isNull() ||
        statusArtwork.width() != statusArtwork.height() || !statusArtwork.hasAlphaChannel()) {
        qWarning("The source images must be square, and the status artwork must have an alpha channel");
        return 1;
    }
    QImage master(1024, 1024, QImage::Format_ARGB32_Premultiplied);
    master.fill(Qt::transparent);
    QPainter painter(&master);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const bool preserve = argc == 8;
    if (preserve) painter.drawImage(QRectF(0, 0, 1024, 1024), artwork);
    else {
        QPainterPath tile;
        tile.addRoundedRect(QRectF(64, 64, 896, 896), 200, 200);
        painter.setClipPath(tile);
        painter.drawImage(QRectF(64, 64, 896, 896), artwork);
    }
    painter.end();
    if (!master.save(QString::fromLocal8Bit(argv[3]))) return 1;

    const QDir iconset(QString::fromLocal8Bit(argv[4]));
    if (!QDir().mkpath(iconset.path())) return 1;
    for (int size : {16, 32, 128, 256, 512}) for (int scale : {1, 2}) {
        const QString name = QString("icon_%1x%1%2.png").arg(size).arg(scale == 2 ? "@2x" : "");
        if (!master.scaled(size * scale, size * scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                 .save(iconset.filePath(name))) return 1;
    }

    // Fit the transparent portrait into the small status area without a background tile.
    QImage status(128, 128, QImage::Format_ARGB32_Premultiplied);
    status.fill(Qt::transparent);
    QPainter statusPainter(&status);
    statusPainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (preserve) {
        // Preserve the supplied transparent canvas rather than applying the original artwork's crop.
        statusPainter.drawImage(QRectF(2, 2, 124, 124), statusArtwork);
    } else {
        const qreal unit = statusArtwork.width() / 1254.0;
        statusPainter.drawImage(QRectF(2, 2, 124, 124), statusArtwork,
                                QRectF(125 * unit, 125 * unit, 1000 * unit, 1000 * unit));
    }
    statusPainter.end();
    if (!status.save(QString::fromLocal8Bit(argv[5]))) return 1;
    return writeIco(master, QString::fromLocal8Bit(argv[6])) ? 0 : 1;
}
