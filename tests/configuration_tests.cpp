#include "activation.h"
#include "controller.h"
#include "managerwindow.h"
#include "preferences.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCloseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QtTest>

// Reopen delivery is checked through the real packaged macOS app, not the headless test.
#ifdef Q_OS_MACOS
namespace petdesk { void setReopenHandler(std::function<void()>) {} }
#endif
using namespace petdesk;
namespace {
PetEntry entry(QString key, int version = 2) {
    return {key, key, "description", {}, {}, version};
}
ManagerWindow *console() {
    for (auto *widget : QApplication::topLevelWidgets())
        if (auto *window = qobject_cast<ManagerWindow *>(widget)) return window;
    return nullptr;
}
void makeSource(const QString &path, const QString &name = "Example") {
    QDir().mkpath(path);
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
}
class ConfigurationTests : public QObject {
    Q_OBJECT
private slots:
    void firstRunHasVisibleImportAndNoUnusableRuntimeControls() {
        ManagerWindow window;
        window.show();
        QVERIFY(window.findChild<QPushButton *>("firstImport")->isVisible());
        QVERIFY(window.findChild<QPushButton *>("importPet")->isVisible());
        QVERIFY(!window.findChild<QPushButton *>("toggleVisibility")->isEnabled());
        window.findChild<QPushButton *>("navigation1")->setChecked(true);
        QVERIFY(window.findChild<QPushButton *>("goToLibrary")->isVisible());
        QVERIFY(!window.findChild<QComboBox *>("petSize")->isVisible());
    }
    void browsingDoesNotActivateWithoutExplicitUse() {
        ManagerWindow window;
        window.setLibrary({entry("alpha"), entry("beta")}, {}, {}, false);
        QSignalSpy use(&window, &ManagerWindow::useRequested);
        auto *list = window.findChild<PetList *>("petLibrary");
        list->selectKey("beta");
        QCOMPARE(use.size(), 0);
        window.findChild<QPushButton *>("usePet")->click();
        QCOMPARE(use.size(), 1);
        QCOMPARE(use.at(0).at(0).toString(), QString("beta"));
    }
    void v1DisablesGazeAndPauseDisablesPlayback() {
        ManagerWindow window;
        window.setCurrentPet(entry("legacy", 1), {}, true, true);
        auto *gaze = window.findChild<QCheckBox *>("gaze");
        QVERIFY(!gaze->isEnabled()); QVERIFY(!gaze->isChecked());
        QVERIFY(!window.findChild<QPushButton *>("playAction")->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("playActivity")->isEnabled());
        window.setCurrentPet(entry("modern"), {}, false, true);
        QVERIFY(gaze->isEnabled()); QVERIFY(gaze->isChecked());
        QVERIFY(window.findChild<QPushButton *>("playAction")->isEnabled());
    }
    void loadingOptionsDoesNotWriteBackOrOverwriteAnotherPet() {
        ManagerWindow window;
        QSignalSpy changed(&window, &ManagerWindow::petOptionsChanged);
        window.setCurrentPet(entry("alpha"), {150, false, false, true, 0}, false, true);
        QCOMPARE(changed.size(), 0);
        QVERIFY(!window.findChild<QCheckBox *>("walking")->isEnabled());
        auto *size = window.findChild<QComboBox *>("petSize");
        size->setCurrentIndex(size->findData(125));
        QCOMPARE(changed.size(), 1);
        const auto options = qvariant_cast<PetOptions>(changed.at(0).at(0));
        QCOMPARE(options.size, 125); QVERIFY(!options.gaze); QVERIFY(!options.automatic);
        QVERIFY(options.walking); QCOMPARE(options.pace, 0);
    }
    void movedEntriesHaveARecoverableLibraryRoute() {
        ManagerWindow window;
        window.setLibrary({}, {entry("removed")}, {}, false);
        QSignalSpy restore(&window, &ManagerWindow::restoreRequested);
        window.findChild<QPushButton *>("restorePet")->click();
        QCOMPARE(restore.size(), 1); QCOMPARE(restore.at(0).at(0).toString(), QString("removed"));
        window.setLibrary({entry("removed")}, {}, {}, false);
        window.selectPet("removed");
        QVERIFY(window.findChild<QPushButton *>("usePet")->isEnabled());
    }
    void closeCanHideOrRequestQuit() {
        ManagerWindow window;
        QSignalSpy quit(&window, &ManagerWindow::quitRequested);
        window.shouldQuitOnClose = [] { return false; };
        window.show(); window.close();
        QVERIFY(!window.isVisible()); QCOMPARE(quit.size(), 0);
        QCloseEvent explicitQuit;
        QApplication::sendEvent(&window, &explicitQuit);
        QVERIFY(explicitQuit.isAccepted());
        window.shouldQuitOnClose = [] { return true; };
        window.show(); window.close(); QCOMPARE(quit.size(), 1);
    }
    void perPetSettingsPersistIndependently() {
        QTemporaryDir root;
        const PetOptions alpha{150, false, false, true, 0}, beta{75, true, true, false, 2};
        {
            Preferences preferences(root.path(), false);
            preferences.savePet("alpha", alpha); preferences.savePet("beta", beta);
            preferences.saveApp({false, true, true}); preferences.state().sync();
        }
        Preferences reloaded(root.path(), false);
        QVERIFY(reloaded.pet("alpha") == alpha); QVERIFY(reloaded.pet("beta") == beta);
        QVERIFY(reloaded.pet("new-pet") == PetOptions{});
        QVERIFY((reloaded.app() == AppOptions{false, true, true}));
    }
    void malformedSettingsFallBackToSupportedValues() {
        QTemporaryDir root;
        Preferences preferences(root.path(), false);
        preferences.state().setValue("pets/test/size", 999);
        preferences.state().setValue("pets/test/pace", -5);
        QCOMPARE(preferences.pet("test").size, 100);
        QCOMPARE(preferences.pet("test").pace, 0);
    }
    void migrateOnlyThePreviousPetAndOnlyOnce() {
        QTemporaryDir root, legacyRoot;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, legacyRoot.path());
        QCoreApplication::setOrganizationName("petdesk-configuration-tests");
        QCoreApplication::setApplicationName("legacy-migration");
        {
            QSettings legacy;
            legacy.setValue("currentPet", "previous"); legacy.setValue("size", 150);
            legacy.setValue("gaze", false); legacy.setValue("position", QPoint(300, 200)); legacy.sync();
        }
        {
            Preferences preferences(root.path(), true);
            QCOMPARE(preferences.pet("previous").size, 150); QVERIFY(!preferences.pet("previous").gaze);
            QCOMPARE(preferences.pet("another").size, 100);
            QCOMPARE(preferences.state().value("position").toPoint(), QPoint(300, 200));
            preferences.savePet("previous", {125, true, true, false, 1}); preferences.state().sync();
        }
        Preferences reloaded(root.path(), true);
        QCOMPARE(reloaded.pet("previous").size, 125);
    }
    void archiveAndRestoreKeepOriginalSourceAndExactImage() {
        QTemporaryDir root, source;
        makeSource(source.path());
        PetLibrary library(root.path());
        const auto imported = library.importDirectory(source.path());
        const auto digest = library.load(imported.key).sha256;
        library.archive(imported.key);
        QCOMPARE(library.entries().size(), 0); QCOMPARE(library.archivedEntries().size(), 1);
        QVERIFY(QFile::exists(source.filePath("spritesheet.png")));
        const auto restored = library.restore(imported.key);
        QCOMPARE(restored.version, 2); QCOMPARE(library.archivedEntries().size(), 0);
        QCOMPARE(library.load(restored.key).sha256, digest);
    }
    void reimportingRemovedPetRestoresWithoutDuplicateCopies() {
        QTemporaryDir root, source;
        makeSource(source.path()); PetLibrary library(root.path());
        const auto imported = library.importDirectory(source.path()); library.archive(imported.key);
        QCOMPARE(library.importDirectory(source.path()).key, imported.key);
        QCOMPARE(library.entries().size(), 1); QCOMPARE(library.archivedEntries().size(), 0);
    }
    void invalidArchivePathsAndDamagedRestoreAreRejected() {
        QTemporaryDir root, source;
        makeSource(source.path()); PetLibrary library(root.path());
        QVERIFY_THROWS_EXCEPTION(std::exception, library.archive("../outside"));
        const auto imported = library.importDirectory(source.path()); library.archive(imported.key);
        QVERIFY(QFile::remove(QDir(library.archiveRoot()).filePath(imported.key + "/spritesheet.png")));
        QVERIFY_THROWS_EXCEPTION(std::exception, library.restore(imported.key));
        QCOMPARE(library.entries().size(), 0);
    }
    void runningHostOpensConsoleWhenItReceivesAnotherLaunch() {
        QTemporaryDir root;
        Controller controller(root.path(), false); controller.start();
        auto *window = console(); QVERIFY(window); QVERIFY(window->isVisible());
        window->hide(); QVERIFY(!window->isVisible());
        QVERIFY(ActivationMailbox::request(root.path()));
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 2000);
        QVERIFY(!QFile::exists(root.filePath("open-console.request")));
    }
    void explicitQuietStartupWithExistingPetStaysQuietUntilOpened() {
        QTemporaryDir root, source;
        makeSource(source.path()); PetLibrary library(root.filePath("pets"));
        const auto pet = library.importDirectory(source.path());
        {
            Preferences preferences(root.path(), false);
            preferences.saveApp({false, true, false});
            preferences.state().setValue("currentPet", pet.key); preferences.state().sync();
        }
        Controller controller(root.path(), false); controller.start();
        auto *window = console(); QVERIFY(window); QVERIFY(!window->isVisible());
        controller.showManager(); QVERIFY(window->isVisible());
        QCOMPARE(window->findChild<PetList *>("petLibrary")->count(), 1);
    }
};

QTEST_MAIN(ConfigurationTests)
#include "configuration_tests.moc"
