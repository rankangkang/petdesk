// Render a generic application-window symbol. No character artwork is used.
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 3) return 2;
    QImage master(1024, 1024, QImage::Format_ARGB32_Premultiplied);
    master.fill(Qt::transparent);
    QPainter p(&master);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#5164d9")); p.drawRoundedRect(QRectF(64, 64, 896, 896), 200, 200);
    p.setBrush(QColor(255, 255, 255, 110)); p.drawRoundedRect(QRectF(212, 234, 520, 390), 34, 34);
    p.setBrush(Qt::white); p.drawRoundedRect(QRectF(292, 350, 520, 390), 34, 34);
    p.setBrush(QColor("#5164d9")); p.drawRoundedRect(QRectF(330, 470, 444, 226), 16, 16);
    p.drawEllipse(QPointF(338, 410), 10, 10); p.drawEllipse(QPointF(376, 410), 10, 10);
    p.end();
    if (!master.save(QString::fromLocal8Bit(argv[1]))) return 1;
    const QDir iconset(QString::fromLocal8Bit(argv[2]));
    QDir().mkpath(iconset.path());
    for (int size : {16, 32, 128, 256, 512}) for (int scale : {1, 2}) {
        const QString name = QString("icon_%1x%1%2.png").arg(size).arg(scale == 2 ? "@2x" : "");
        if (!master.scaled(size * scale, size * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .save(iconset.filePath(name))) return 1;
    }
    return 0;
}
