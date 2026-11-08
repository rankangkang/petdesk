#include "controller.h"
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>
#include <stdexcept>

namespace petdesk {
namespace {
class ManagerDialog final : public QDialog {
public:
    std::function<void(QString)> directoryDropped;
    ManagerDialog() { setAcceptDrops(true); }
protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
            QFileInfo(event->mimeData()->urls().first().toLocalFile()).isDir())
            event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        if (directoryDropped && event->mimeData()->urls().size() == 1) {
            const QString directory = event->mimeData()->urls().first().toLocalFile();
            if (QFileInfo(directory).isDir()) {
                event->acceptProposedAction();
                directoryDropped(directory);
            }
        }
    }
};
}

Controller::Controller(const QString &dataRoot)
    : m_library(QDir(dataRoot).filePath("pets")), m_manager(new ManagerDialog),
      m_list(new QListWidget(m_manager)), m_menu(&m_pet), m_tray(this) {
    m_menu.setWindowTitle(QStringLiteral("PetDesk 菜单"));
    m_menu.setAccessibleName(QStringLiteral("PetDesk 菜单"));
    m_manager->setWindowTitle(QStringLiteral("PetDesk · 桌面伙伴"));
    m_manager->resize(480, 470);
    auto *layout = new QVBoxLayout(m_manager);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("把伙伴放到桌面上"));
    auto font = title->font();
    font.setPointSize(font.pointSize() + 6);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    auto *hint = new QLabel(QStringLiteral("选择或拖入一个宠物目录。导入后保存在本机，可离线使用。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_list->setAccessibleName(QStringLiteral("本地宠物"));
    m_list->setIconSize(QSize(48, 52));
    layout->addWidget(m_list, 1);
    auto *buttons = new QHBoxLayout;
    auto *import = new QPushButton(QStringLiteral("导入宠物目录…"));
    auto *use = new QPushButton(QStringLiteral("显示选中的宠物"));
    use->setDefault(true);
    buttons->addWidget(import);
    buttons->addStretch();
    buttons->addWidget(use);
    layout->addLayout(buttons);
    connect(import, &QPushButton::clicked, this, [this] { importFolder(); });
    connect(use, &QPushButton::clicked, this, [this] {
        if (auto *item = m_list->currentItem()) activate(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem *item) { activate(item->data(Qt::UserRole).toString()); });
    static_cast<ManagerDialog *>(m_manager)->directoryDropped = [this](const QString &directory) {
        importDirectory(directory);
    };

    m_menu.addAction(QStringLiteral("管理宠物…"), this, [this] { showManager(); });
    m_menu.addAction(QStringLiteral("导入宠物目录…"), this, [this] { importFolder(); });
    m_menu.addSeparator();
    m_visibility = m_menu.addAction(QStringLiteral("隐藏宠物"), this, [this] {
        if (m_pet.isVisible()) m_pet.hide();
        else if (m_pet.hasPet()) m_pet.show();
        else showManager();
    });
    m_pause = m_menu.addAction(QStringLiteral("暂停动画"));
    m_pause->setCheckable(true);
    m_pause->setChecked(m_settings.value("paused", false).toBool());
    connect(m_pause, &QAction::toggled, this, [this](bool paused) {
        m_pet.setPaused(paused);
        m_settings.setValue("paused", paused);
    });
    m_gaze = m_menu.addAction(QStringLiteral("看向鼠标"));
    m_gaze->setCheckable(true);
    m_gaze->setChecked(m_settings.value("gaze", false).toBool());
    connect(m_gaze, &QAction::toggled, this, [this](bool enabled) {
        m_pet.setGazeEnabled(enabled);
        m_settings.setValue("gaze", enabled);
    });
    auto *sizeMenu = m_menu.addMenu(QStringLiteral("大小"));
    auto *sizeGroup = new QActionGroup(sizeMenu);
    for (int percent : {75, 100, 125, 150, 200}) {
        auto *action = sizeMenu->addAction(QString::number(percent) + '%');
        action->setCheckable(true);
        action->setChecked(percent == m_settings.value("size", 100).toInt());
        sizeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, percent] {
            m_pet.setPetScale(percent / 100.0);
            m_settings.setValue("size", percent);
            m_settings.setValue("position", m_pet.pos());
        });
    }
    auto *motions = m_menu.addMenu(QStringLiteral("动作"));
    const QStringList labels = {QStringLiteral("待机"), QStringLiteral("向右走"), QStringLiteral("向左走"),
        QStringLiteral("挥手"), QStringLiteral("跳跃"), QStringLiteral("沮丧"), QStringLiteral("等待"),
        QStringLiteral("思考"), QStringLiteral("检查")};
    for (int row = 0; row < labels.size(); ++row)
        m_motionActions.append(motions->addAction(labels[row], this, [this, row] { m_pet.play(row); }));
    m_menu.addSeparator();
    m_menu.addAction(QStringLiteral("关于 PetDesk"), this, [this] {
        QMessageBox::about(m_manager, QStringLiteral("关于 PetDesk"),
            QStringLiteral("PetDesk 0.1.0\n\n完全本地的桌面伙伴。\n拖动移动，单击挥手，右键打开管理面板。\n\n素材导入后保存在本机，无需账号或其他应用。"));
    });
    m_menu.addAction(QStringLiteral("退出 PetDesk"), this, [] {
        qInfo("quit requested via menu");
        qApp->quit();
    });
    auto *options = new QHBoxLayout;
    options->addWidget(new QLabel(QStringLiteral("大小")));
    auto *sizeChoice = new QComboBox;
    sizeChoice->setAccessibleName(QStringLiteral("宠物大小"));
    const QList<int> sizes = {75, 100, 125, 150, 200};
    for (int percent : sizes) sizeChoice->addItem(QString::number(percent) + '%', percent);
    const int selectedSize = sizes.indexOf(m_settings.value("size", 100).toInt());
    sizeChoice->setCurrentIndex(selectedSize < 0 ? 1 : selectedSize);
    options->addWidget(sizeChoice);
    connect(sizeChoice, &QComboBox::currentIndexChanged, this, [sizeGroup, sizeChoice](int) {
        for (auto *action : sizeGroup->actions())
            if (action->text() == sizeChoice->currentText()) action->trigger();
    });
    for (auto *action : sizeGroup->actions())
        connect(action, &QAction::triggered, sizeChoice, [action, sizeChoice] {
            const QSignalBlocker blocker(sizeChoice);
            sizeChoice->setCurrentText(action->text());
        });
    auto *pause = new QCheckBox(QStringLiteral("暂停动画"));
    pause->setChecked(m_pause->isChecked());
    connect(pause, &QCheckBox::toggled, m_pause, &QAction::setChecked);
    connect(m_pause, &QAction::toggled, pause, &QCheckBox::setChecked);
    connect(m_pause, &QAction::changed, pause, [this, pause] { pause->setEnabled(m_pause->isEnabled()); });
    options->addWidget(pause);
    auto *gaze = new QCheckBox(QStringLiteral("看向鼠标"));
    gaze->setChecked(m_gaze->isChecked());
    connect(gaze, &QCheckBox::toggled, m_gaze, &QAction::setChecked);
    connect(m_gaze, &QAction::toggled, gaze, &QCheckBox::setChecked);
    connect(m_gaze, &QAction::changed, gaze, [this, gaze] { gaze->setEnabled(m_gaze->isEnabled()); });
    options->addWidget(gaze);
    layout->addLayout(options);
    auto *playback = new QHBoxLayout;
    auto *motion = new QComboBox;
    motion->setAccessibleName(QStringLiteral("宠物动作"));
    motion->addItems(labels);
    motion->setCurrentIndex(3);
    playback->addWidget(motion, 1);
    auto *play = new QPushButton(QStringLiteral("播放动作"));
    connect(play, &QPushButton::clicked, this, [this, motion] { m_pet.play(motion->currentIndex()); });
    connect(m_pause, &QAction::changed, play, [this, play] {
        play->setEnabled(m_pet.hasPet() && !m_pause->isChecked());
    });
    playback->addWidget(play);
    auto *visibility = new QPushButton(QStringLiteral("显示 / 隐藏"));
    connect(visibility, &QPushButton::clicked, m_visibility, &QAction::trigger);
    playback->addWidget(visibility);
    auto *quit = new QPushButton(QStringLiteral("退出"));
    connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
    playback->addWidget(quit);
    layout->addLayout(playback);
    connect(&m_menu, &QMenu::aboutToShow, this, [this] {
        qInfo("pet menu opened");
        m_visibility->setText(m_pet.isVisible() ? QStringLiteral("隐藏宠物") : QStringLiteral("显示宠物"));
        m_pause->setEnabled(m_pet.hasPet());
        m_gaze->setEnabled(m_pet.hasGaze());
        for (auto *action : m_motionActions) action->setEnabled(m_pet.hasPet() && !m_pause->isChecked());
    });
    m_pet.contextRequested = [this](QPoint) { showManager(); };
    m_pet.positionSaved = [this](QPoint position) { m_settings.setValue("position", position); };
    m_tray.setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    m_tray.setToolTip("PetDesk");
    m_tray.setContextMenu(&m_menu);
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
        if (reason == QSystemTrayIcon::DoubleClick) showManager();
    });
    m_tray.show();
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        m_settings.sync();
        m_manager->close();
        m_pet.hide();
        m_tray.hide();
    });
}

Controller::~Controller() { delete m_manager; }

void Controller::start() {
    m_pet.setPetScale(m_settings.value("size", 100).toInt() / 100.0);
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    const QPoint fallback(screen.right() - m_pet.width() - 40, screen.bottom() - m_pet.height() - 30);
    m_pet.move(m_settings.value("position", fallback).toPoint());
    m_pet.clampToScreen();
    refreshLibrary();
    const QString current = m_settings.value("currentPet").toString();
    if (!current.isEmpty()) activate(current);
    else showManager();
}

void Controller::refreshLibrary() {
    m_list->clear();
    for (const auto &entry : m_library.entries()) {
        auto *item = new QListWidgetItem(QIcon(entry.thumbnail), entry.name, m_list);
        item->setData(Qt::UserRole, entry.key);
        item->setToolTip(entry.description);
        if (entry.key == m_currentKey) m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem() && m_list->count()) m_list->setCurrentRow(0);
}

void Controller::showManager() {
    m_pause->setEnabled(m_pet.hasPet());
    m_gaze->setEnabled(m_pet.hasGaze());
    refreshLibrary();
    m_manager->show();
    m_manager->raise();
    m_manager->activateWindow();
}

void Controller::importFolder() {
    showManager();
    const auto directory = QFileDialog::getExistingDirectory(m_manager, QStringLiteral("选择宠物目录"),
        m_settings.value("lastImportParent", QDir::homePath()).toString(), QFileDialog::ShowDirsOnly);
    if (!directory.isEmpty()) importDirectory(directory);
}

void Controller::importDirectory(const QString &directory) {
    try {
        const auto preview = readPetDirectory(directory);
        QMessageBox confirmation(m_manager);
        confirmation.setWindowTitle(QStringLiteral("导入宠物"));
        confirmation.setText(preview.name);
        confirmation.setInformativeText(preview.description.isEmpty()
            ? QStringLiteral("导入后保存在本机，随时可以使用。") : preview.description);
        confirmation.setTextFormat(Qt::PlainText);
        confirmation.setIconPixmap(QPixmap::fromImage(preview.atlas.copy(preview.frameRect(0, 0)))
            .scaled(96, 104, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        auto *accept = confirmation.addButton(QStringLiteral("使用这只宠物"), QMessageBox::AcceptRole);
        confirmation.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        confirmation.exec();
        if (confirmation.clickedButton() != accept) return;
        const auto entry = m_library.importDirectory(directory);
        m_settings.setValue("lastImportParent", QFileInfo(directory).absolutePath());
        refreshLibrary();
        activate(entry.key);
    } catch (const std::exception &error) { showError(QString::fromUtf8(error.what())); }
}

void Controller::activate(const QString &key) {
    try {
        auto pet = m_library.load(key);
        const auto icon = QIcon(QPixmap::fromImage(pet.atlas.copy(pet.frameRect(0, 0))));
        m_tray.setIcon(icon);
        m_tray.setToolTip("PetDesk · " + pet.name);
        m_pet.setPet(std::move(pet));
        m_pet.setPaused(m_pause->isChecked());
        m_pet.setGazeEnabled(m_gaze->isChecked() && m_pet.hasGaze());
        m_currentKey = key;
        m_settings.setValue("currentPet", key);
        m_pet.show();
        qInfo("pet displayed");
        m_pet.clampToScreen();
        m_manager->hide();
    } catch (const std::exception &error) { showError(QString::fromUtf8(error.what())); }
}

void Controller::showError(const QString &message) {
    showManager();
    QMessageBox::warning(m_manager, QStringLiteral("无法使用这个宠物"), message);
}
}
