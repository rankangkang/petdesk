#include "controller.h"
#include "petformat.h"
#include "uistyle.h"
#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QIcon>
#include <cstdio>

namespace {
void restrictBundledPlugins() {
    const QString plugins = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../PlugIns");
    if (QDir(plugins).exists()) QCoreApplication::setLibraryPaths({plugins});
}
}
int main(int argc, char *argv[]) {
    if (argc >= 2 && QByteArray(argv[1]) == "--validate") {
        QCoreApplication app(argc, argv);
        restrictBundledPlugins();
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
    restrictBundledPlugins();
    petdesk::ui::applyApplicationStyle();
    app.setOrganizationName("cmkk");
    app.setOrganizationDomain("cmkk.dev");
    app.setApplicationName("PetDesk");
    app.setApplicationVersion("0.4.0");
    app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser;
    parser.setApplicationDescription("PetDesk desktop pet host");
    parser.addHelpOption(); parser.addVersionOption();
    QCommandLineOption dataOption("data-dir", "Use an isolated local data directory.", "directory");
    parser.addOption(dataOption); parser.process(app);
    const bool isolated = parser.isSet(dataOption);
    const QString root = isolated ? QDir(parser.value(dataOption)).absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    app.setWindowIcon(QIcon(":/assets/PetDesk.png"));
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
        if (petdesk::ActivationMailbox::request(root)) return 0;
        QMessageBox::warning(nullptr, QStringLiteral("PetDesk"), QStringLiteral("无法打开已运行的 PetDesk，请使用菜单栏中的控制台入口。"));
        return 1;
    }
    petdesk::Controller controller(root, !isolated);
    controller.start();
    return app.exec();
}
