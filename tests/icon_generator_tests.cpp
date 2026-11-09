#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class IconGeneratorTests : public QObject {
    Q_OBJECT
private slots:
    void rectangularArtworkKeepsItsProportions() {
        QTemporaryDir root; QVERIFY(root.isValid());
        QImage appSource(80, 40, QImage::Format_ARGB32); appSource.fill(Qt::green);
        QImage statusSource(40, 80, QImage::Format_ARGB32); statusSource.fill(Qt::blue);
        const QString appPath = root.filePath("app-source.png"), statusPath = root.filePath("status-source.png");
        QVERIFY(appSource.save(appPath)); QVERIFY(statusSource.save(statusPath));
        QProcess generator;
        generator.start(QString::fromUtf8(PETDESK_ICON_GENERATOR_PATH),
                        {appPath, statusPath, root.filePath("app.png"), root.filePath("app.iconset"),
                         root.filePath("status.png"), root.filePath("app.ico"), "--preserve"});
        QVERIFY(generator.waitForFinished(10000));
        QCOMPARE(generator.exitStatus(), QProcess::NormalExit);
        QCOMPARE(generator.exitCode(), 0);
        const QImage app(root.filePath("app.png")), status(root.filePath("status.png"));
        QCOMPARE(app.size(), QSize(1024, 1024)); QCOMPARE(status.size(), QSize(128, 128));
        QCOMPARE(app.pixelColor(512, 512), QColor(Qt::green));
        QCOMPARE(app.pixelColor(512, 20).alpha(), 0);
        QCOMPARE(status.pixelColor(64, 64), QColor(Qt::blue));
        QCOMPARE(status.pixelColor(10, 64).alpha(), 0);
        QVERIFY(QFileInfo(root.filePath("app.ico")).size() > 0);
    }
};
QTEST_GUILESS_MAIN(IconGeneratorTests)
#include "icon_generator_tests.moc"
