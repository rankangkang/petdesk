#include "controller.h"
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

namespace petdesk {
namespace {
QLabel *label(const QString &text) {
    auto *result = new QLabel(text);
    result->setTextFormat(Qt::PlainText); result->setWordWrap(true);
    return result;
}
}
Controller::Controller(const QString &dataRoot, bool migrateLegacy)
    : m_root(dataRoot), m_library(QDir(dataRoot).filePath("pets")),
      m_preferences(dataRoot, migrateLegacy), m_manager(new ManagerWindow), m_tray(this) {
    auto &settings = m_preferences.state();
    m_paused = settings.value("paused", false).toBool();
    m_manager->setAppOptions(m_preferences.app());
    m_manager->visibilityChanged = [this](bool visible) { m_pet.setAutomaticSuspended(visible); };
    m_manager->shouldQuitOnClose = [this] { return m_preferences.app().quitOnClose; };
    m_pet.contextRequested = [this](QPoint) { showManager(); };
    m_pet.positionSaved = [this](QPoint point) { m_preferences.state().setValue("position", point); };
    m_pet.activityChanged = [this](const QString &activity) { m_manager->setActivity(activity); };
    connect(m_manager, &ManagerWindow::importRequested, this, [this] { importFolder(); });
    connect(m_manager, &ManagerWindow::directoryDropped, this, [this](const QString &path) { importDirectory(path); });
    connect(m_manager, &ManagerWindow::useRequested, this, [this](const QString &key) { activate(key); });
    connect(m_manager, &ManagerWindow::archiveRequested, this, [this](const QString &key) { archivePet(key); });
    connect(m_manager, &ManagerWindow::restoreRequested, this, [this](const QString &key) { restorePet(key); });
    connect(m_manager, &ManagerWindow::petOptionsChanged, this, [this](PetOptions options) { applyOptions(options); });
    connect(m_manager, &ManagerWindow::appOptionsChanged, this, [this](AppOptions options) {
        m_preferences.saveApp(options);
        m_manager->setNotice(QStringLiteral("应用偏好已保存。"));
    });
    connect(m_manager, &ManagerWindow::visibilityRequested, this, [this] { setPetVisible(!m_pet.isVisible()); });
    connect(m_manager, &ManagerWindow::positionRequested, this, [this](int screen) { moveToScreen(screen); });
    connect(m_manager, &ManagerWindow::actionRequested, this, [this](int row) {
        setPetVisible(true); m_pet.play(row);
    });
    connect(m_manager, &ManagerWindow::activityRequested, this, [this](int activity) {
        setPetVisible(true); m_pet.playActivity(static_cast<Activity>(activity));
    });
    connect(m_manager, &ManagerWindow::revealRequested, this, [this](const QString &key) {
        for (const auto &entry : m_library.entries()) if (entry.key == key) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(entry.directory)); break;
        }
    });
    connect(m_manager, &ManagerWindow::dataFolderRequested, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));
    });
    connect(m_manager, &ManagerWindow::repositoryRequested, this, [] {
        QDesktopServices::openUrl(QUrl("https://github.com/rankangkang/petdesk"));
    });
    connect(m_manager, &ManagerWindow::quitRequested, qApp, &QApplication::quit);

    auto *appMenu = m_manager->menuBar()->addMenu(QStringLiteral("文件"));
    auto *open = appMenu->addAction(QStringLiteral("打开控制台"), this, [this] { showManager(); });
    open->setShortcut(QKeySequence("Ctrl+1"));
    auto *import = appMenu->addAction(QStringLiteral("导入桌宠…"), this, [this] { importFolder(); });
    import->setShortcut(QKeySequence::Open);
    auto *preferences = appMenu->addAction(QStringLiteral("应用偏好…"), this, [this] {
        showManager(); m_manager->showPage(2);
    });
    preferences->setMenuRole(QAction::PreferencesRole); preferences->setShortcut(QKeySequence("Ctrl+,"));
    appMenu->addSeparator();
    auto *quit = appMenu->addAction(QStringLiteral("退出 PetDesk"), qApp, &QApplication::quit);
    quit->setMenuRole(QAction::QuitRole); quit->setShortcut(QKeySequence::Quit);
    m_menu.addAction(QStringLiteral("打开控制台"), this, [this] { showManager(); });
    m_menu.addAction(QStringLiteral("导入桌宠…"), this, [this] { importFolder(); });
    m_menu.addSeparator();
    m_visibility = m_menu.addAction(QStringLiteral("显示桌宠"), this, [this] { setPetVisible(!m_pet.isVisible()); });
    m_pause = m_menu.addAction(QStringLiteral("暂停动画")); m_pause->setCheckable(true); m_pause->setChecked(m_paused);
    connect(m_pause, &QAction::toggled, this, [this](bool paused) {
        m_paused = paused; m_preferences.state().setValue("paused", paused); m_pet.setPaused(paused); syncCurrent();
    });
    connect(m_manager, &ManagerWindow::pauseRequested, m_pause, &QAction::setChecked);
    m_menu.addSeparator();
    m_menu.addAction(QStringLiteral("退出 PetDesk"), qApp, &QApplication::quit);
    m_tray.setIcon(QApplication::windowIcon().isNull() ? QApplication::style()->standardIcon(QStyle::SP_DesktopIcon) : QApplication::windowIcon());
    m_tray.setToolTip("PetDesk · 桌宠控制台"); m_tray.setContextMenu(&m_menu);
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showManager();
    });
    m_tray.show();
    connect(qApp, &QGuiApplication::screenAdded, this, [this] { refreshScreens(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this] { refreshScreens(); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        m_preferences.state().sync(); m_pet.hide(); m_tray.hide(); m_manager->hide();
    });
}
Controller::~Controller() { setReopenHandler({}); m_pet.activityChanged = {}; delete m_manager; }

void Controller::start() {
    auto &state = m_preferences.state();
    m_pet.setPetScale(1);
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const QRect bounds = screen->availableGeometry();
        m_pet.move(state.value("position", QPoint(bounds.right() - m_pet.width() - 40,
            bounds.bottom() - m_pet.height() - 30)).toPoint());
    }
    m_pet.clampToScreen(); refreshScreens(); refreshLibrary();
    const QString current = state.value("currentPet").toString();
    if (!current.isEmpty() && m_preferences.app().restorePet)
        activate(current, state.value("petVisible", true).toBool(), false);
    if (!m_currentKey.isEmpty()) m_manager->selectPet(m_currentKey);
    if (m_preferences.app().openConsole || !m_pet.hasPet()) showManager();
    // AppKit installs its initial event handlers while starting the native loop.
    QTimer::singleShot(0, this, [this] {
        setReopenHandler([this] { qInfo("application reopen received"); showManager(); });
    });
    m_activation = std::make_unique<ActivationMailbox>(m_root, [this] { showManager(); }, this);
    m_activation->consume();
}
void Controller::showManager() {
    refreshLibrary(); refreshScreens(); syncCurrent();
    m_manager->show(); m_manager->raise(); m_manager->activateWindow();
}
void Controller::refreshLibrary() {
    try { m_manager->setLibrary(m_library.entries(), m_library.archivedEntries(), m_currentKey, m_pet.isVisible()); }
    catch (const std::exception &error) { m_manager->setNotice(QString::fromUtf8(error.what()), true); }
}
void Controller::syncCurrent() {
    m_manager->setCurrentPet(m_currentEntry, m_options, m_paused, m_pet.isVisible());
    m_manager->setActivity(m_pet.currentActivity());
    m_visibility->setText(m_pet.isVisible() ? QStringLiteral("隐藏桌宠") : QStringLiteral("显示桌宠"));
    m_visibility->setEnabled(m_pet.hasPet()); m_pause->setEnabled(m_pet.hasPet());
}
void Controller::refreshScreens() {
    QStringList names;
    for (auto *screen : QGuiApplication::screens()) names.append(screen->name() +
        (screen == QGuiApplication::primaryScreen() ? QStringLiteral(" · 主屏幕") : QString{}));
    m_manager->setScreens(names);
}
void Controller::applyOptions(PetOptions options, bool force) {
    if (m_currentKey.isEmpty()) return;
    const auto old = m_options; m_options = options;
    m_preferences.savePet(m_currentKey, options);
    if (force || old.size != options.size) m_pet.setPetScale(options.size / 100.0);
    if (force || old.gaze != options.gaze) m_pet.setGazeEnabled(options.gaze && m_pet.hasGaze());
    if (force || old.automatic != options.automatic) m_pet.setAutomaticEnabled(options.automatic);
    if (force || old.walking != options.walking) m_pet.setWalkingEnabled(options.walking);
    if (force || old.pace != options.pace) m_pet.setActivityPace(options.pace);
    m_preferences.state().setValue("position", m_pet.pos());
    syncCurrent();
    if (!force) m_manager->setNotice(QStringLiteral("这只桌宠的设置已保存。"));
}
void Controller::activate(const QString &key, bool visible, bool interactive) {
    if (key.isEmpty()) return;
    try {
        if (key != m_currentKey || !m_pet.hasPet()) {
            auto pet = m_library.load(key);
            const QString folder = QDir(m_library.root()).filePath(key);
            m_currentEntry = {key, pet.name, pet.description, folder, QDir(folder).filePath("thumbnail.png"), pet.version};
            m_currentKey = key; m_pet.setPet(std::move(pet));
            applyOptions(m_preferences.pet(key), true);
            m_pet.setPaused(m_paused);
        }
        m_preferences.state().setValue("currentPet", key);
        setPetVisible(visible);
        m_pet.clampToScreen();
        if (interactive) {
            refreshLibrary(); m_manager->selectPet(key); m_manager->showPage(1);
            m_manager->setNotice(QStringLiteral("%1 已准备好。可以调整设置，或收起控制台回到桌面。").arg(m_currentEntry.name));
        }
        qInfo("pet selected");
    } catch (const std::exception &error) { showError(QString::fromUtf8(error.what())); }
}
void Controller::setPetVisible(bool visible) {
    if (!m_pet.hasPet()) { showManager(); m_manager->showPage(0); return; }
    if (visible) m_pet.show(); else m_pet.hide();
    m_preferences.state().setValue("petVisible", visible);
    refreshLibrary(); syncCurrent();
}
void Controller::moveToScreen(int index) {
    if (!m_pet.hasPet()) return;
    const auto screens = QGuiApplication::screens();
    if (index < 0 || index >= screens.size()) return;
    const QRect bounds = screens[index]->availableGeometry();
    m_pet.play(0);
    m_pet.move(std::max(bounds.left(), bounds.right() - m_pet.width() - 32),
        std::max(bounds.top(), bounds.bottom() - m_pet.height() - 20));
    m_pet.clampToScreen(); m_preferences.state().setValue("position", m_pet.pos()); setPetVisible(true);
    m_manager->setNotice(QStringLiteral("桌宠已移到所选屏幕右下角。"));
}
void Controller::archivePet(const QString &key) {
    if (key.isEmpty()) return;
    try {
        m_library.archive(key);
        if (key == m_currentKey) {
            m_pet.hide(); m_pet.setPet({}); m_currentKey.clear(); m_currentEntry = {};
            m_preferences.state().remove("currentPet"); syncCurrent();
        }
        refreshLibrary();
        m_manager->setNotice(QStringLiteral("已移出宠物库。可在「已移出」中恢复，原始素材未改动。"));
    } catch (const std::exception &error) { showError(QString::fromUtf8(error.what())); }
}
void Controller::restorePet(const QString &key) {
    if (key.isEmpty()) return;
    try {
        const auto entry = m_library.restore(key);
        refreshLibrary(); m_manager->selectPet(entry.key); m_manager->showPage(0);
        m_manager->setNotice(QStringLiteral("已恢复 %1，点击「使用这只桌宠」即可显示。").arg(entry.name));
    } catch (const std::exception &error) { showError(QString::fromUtf8(error.what())); }
}
void Controller::importFolder() {
    showManager(); m_manager->showPage(0);
    const QString directory = QFileDialog::getExistingDirectory(m_manager, QStringLiteral("选择自己的桌宠文件夹"),
        m_preferences.state().value("lastImportParent", QDir::homePath()).toString(), QFileDialog::ShowDirsOnly);
    if (!directory.isEmpty()) importDirectory(directory);
}
void Controller::importDirectory(const QString &directory) {
    showManager(); m_manager->showPage(0);
    try {
        const auto pet = readPetDirectory(directory);
        QDialog preview(m_manager); preview.setWindowTitle(QStringLiteral("预览并导入桌宠")); preview.setMinimumWidth(440);
        auto *layout = new QVBoxLayout(&preview); layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(16);
        auto *row = new QHBoxLayout;
        auto *image = new QLabel;
        image->setPixmap(QPixmap::fromImage(pet.atlas.copy(pet.frameRect(0, 0))).scaled(96, 104, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        row->addWidget(image);
        auto *info = new QVBoxLayout;
        info->addWidget(label(pet.name));
        info->addWidget(label(pet.version == 2 ? QStringLiteral("9 种动作 · 16 向注视") : QStringLiteral("9 种动作")));
        if (!pet.description.isEmpty()) info->addWidget(label(pet.description));
        row->addLayout(info, 1); layout->addLayout(row);
        layout->addWidget(label(QStringLiteral("素材检查通过。导入后保存在本机，之后可在宠物库中随时使用。")));
        auto *buttons = new QDialogButtonBox;
        auto *cancel = buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
        auto *onlyImport = buttons->addButton(QStringLiteral("仅导入"), QDialogButtonBox::ActionRole);
        auto *use = buttons->addButton(QStringLiteral("导入并使用"), QDialogButtonBox::AcceptRole); use->setDefault(true);
        bool activateAfter = true;
        connect(cancel, &QPushButton::clicked, &preview, &QDialog::reject);
        connect(onlyImport, &QPushButton::clicked, &preview, [&preview, &activateAfter] { activateAfter = false; preview.accept(); });
        connect(use, &QPushButton::clicked, &preview, &QDialog::accept);
        layout->addWidget(buttons);
        if (preview.exec() != QDialog::Accepted) return;
        const auto entry = m_library.importDirectory(directory);
        m_preferences.state().setValue("lastImportParent", QFileInfo(directory).absolutePath());
        refreshLibrary(); m_manager->selectPet(entry.key);
        if (activateAfter) activate(entry.key);
        else m_manager->setNotice(QStringLiteral("%1 已导入。选择它并点击「使用这只桌宠」即可显示。").arg(entry.name));
    } catch (const std::exception &error) { showError(QString::fromUtf8(error.what())); }
}
void Controller::showError(const QString &message) {
    showManager(); m_manager->setNotice(message, true);
}
}
