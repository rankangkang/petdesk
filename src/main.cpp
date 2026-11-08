#include "controller.h"
#include "petformat.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <cstdio>

int main(int argc, char *argv[]) {
    if (argc >= 2 && QByteArray(argv[1]) == "--validate") {
        QCoreApplication app(argc, argv);
        if (argc != 3) { std::fprintf(stderr, "Usage: PetDesk --validate DIRECTORY\n"); return 2; }
        try {
            const auto pet = petdesk::readPetDirectory(QString::fromLocal8Bit(argv[2]));
            const QJsonObject result{{"ok", true}, {"name", pet.name}, {"version", pet.version},
                {"width", pet.atlas.width()}, {"height", pet.atlas.height()}, {"sha256", pet.sha256}};
            std::fputs(QJsonDocument(result).toJson().constData(), stdout);
            return 0;
        } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
    }
    QApplication app(argc, argv);
    app.setOrganizationName("cmkk");
    app.setOrganizationDomain("cmkk.dev");
    app.setApplicationName("PetDesk");
    app.setApplicationVersion("0.1.0");
    app.setQuitOnLastWindowClosed(false);
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(root)) {
        QMessageBox::critical(nullptr, QStringLiteral("PetDesk"), QStringLiteral("无法创建应用数据目录。"));
        return 1;
    }
    static QString logPath = QDir(root).filePath("petdesk.log");
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
        QFile file(logPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            file.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + " " + message + '\n').toUtf8());
        }
    });
    qInfo("PetDesk starting");
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { qInfo("PetDesk exiting normally"); });
    QLockFile lock(QDir(root).filePath("instance.lock"));
    if (!lock.tryLock(0)) {
        QMessageBox::information(nullptr, QStringLiteral("PetDesk"),
            QStringLiteral("PetDesk 已在运行，请使用菜单栏中的宠物图标打开菜单。"));
        return 0;
    }
    petdesk::Controller controller(root);
    controller.start();
    return app.exec();
}
