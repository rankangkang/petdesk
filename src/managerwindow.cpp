#include "managerwindow.h"
#include "behavior.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHideEvent>
#include <QLabel>
#include <QButtonGroup>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace petdesk {
namespace {
QLabel *text(const QString &value, QWidget *parent = nullptr) {
    auto *label = new QLabel(value, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
void heading(QVBoxLayout *layout, const QString &title, const QString &description) {
    auto *label = text(title);
    auto font = label->font(); font.setPointSize(22); font.setBold(true); label->setFont(font);
    layout->addWidget(label);
    layout->addWidget(text(description));
}
QPushButton *button(const QString &title, const char *name = nullptr) {
    auto *result = new QPushButton(title);
    if (name) result->setObjectName(name);
    result->setMinimumHeight(30);
    return result;
}
QWidget *page(QStackedWidget *pages, QVBoxLayout *&layout) {
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);
    scroll->setWidget(content);
    pages->addWidget(scroll);
    return content;
}
QGroupBox *group(const QString &title, QVBoxLayout *layout) {
    auto *box = new QGroupBox(title);
    box->setLayout(layout);
    return box;
}

}

ManagerWindow::ManagerWindow() {
    setObjectName("console");
    setWindowTitle(QStringLiteral("PetDesk · 控制台"));
    resize(860, 650);
    setMinimumSize(760, 560);
    setAcceptDrops(true);
    auto *central = new QWidget;
    auto *shell = new QVBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0); shell->setSpacing(0);
    auto *body = new QHBoxLayout;
    body->setSpacing(0);
    auto *sidebar = new QFrame;
    sidebar->setFrameShape(QFrame::StyledPanel);
    sidebar->setFixedWidth(166);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(14, 24, 14, 16); side->setSpacing(18);
    auto *brand = text("PetDesk");
    auto brandFont = brand->font(); brandFont.setPointSize(21); brandFont.setBold(true); brand->setFont(brandFont);
    side->addWidget(brand);
    auto *navigation = new QButtonGroup(this);
    navigation->setExclusive(true);
    int navigationIndex = 0;
    for (const QString &name : {QStringLiteral("宠物库"), QStringLiteral("桌面设置"),
            QStringLiteral("应用偏好"), QStringLiteral("关于")}) {
        auto *item = button(name);
        item->setObjectName(QString("navigation%1").arg(navigationIndex));
        item->setCheckable(true); item->setMinimumHeight(42);
        item->setStyleSheet("QPushButton { text-align:left; padding:8px; border:0; border-radius:6px; }"
            "QPushButton:checked { background:palette(highlight); color:palette(highlighted-text); }");
        const int index = navigationIndex++;
        navigation->addButton(item, index);
        connect(item, &QPushButton::toggled, this, [this, index](bool checked) {
            if (checked) m_pages->setCurrentIndex(index);
        });
        m_navigation.append(item); side->addWidget(item);
    }
    side->addStretch();
    connect(navigation, &QButtonGroup::idClicked, this, [this](int id) { showPage(id); });
    side->addWidget(text("v" + QCoreApplication::applicationVersion()));
    body->addWidget(sidebar);
    m_pages = new QStackedWidget;
    m_pages->setObjectName("pages");
    body->addWidget(m_pages, 1);
    shell->addLayout(body, 1);
    m_notice = text({});
    m_notice->setObjectName("notice");
    m_notice->setMargin(12);
    m_notice->hide();
    shell->addWidget(m_notice);
    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(18, 10, 18, 10);
    m_status = text(QStringLiteral("还没有正在运行的桌宠"));
    m_status->setObjectName("status");
    footer->addWidget(m_status, 1);
    m_visibility = button(QStringLiteral("显示桌宠"), "toggleVisibility");
    connect(m_visibility, &QPushButton::clicked, this, &ManagerWindow::visibilityRequested);
    footer->addWidget(m_visibility);
    auto *hide = button(QStringLiteral("收起控制台"), "hideConsole");
    connect(hide, &QPushButton::clicked, this, [this] { QMainWindow::hide(); });
    footer->addWidget(hide);
    auto *quit = button(QStringLiteral("退出"), "quit");
    connect(quit, &QPushButton::clicked, this, &ManagerWindow::quitRequested);
    footer->addWidget(quit);
    shell->addLayout(footer);
    setCentralWidget(central);

    QVBoxLayout *libraryLayout;
    page(m_pages, libraryLayout);
    heading(libraryLayout, QStringLiteral("你的桌面伙伴"), QStringLiteral("导入自己的桌宠，随时切换。把宠物文件夹拖进来，也可以直接导入。"));
    auto *importRow = new QHBoxLayout;
    m_count = text(QStringLiteral("还没有导入桌宠"));
    m_count->setObjectName("libraryCount");
    importRow->addWidget(m_count, 1);
    auto *help = button(QStringLiteral("支持哪些目录？"), "importHelp");
    connect(help, &QPushButton::clicked, this, [this] { importHelp(); });
    importRow->addWidget(help);
    auto *import = button(QStringLiteral("导入宠物目录…"), "importPet");
    import->setDefault(true);
    connect(import, &QPushButton::clicked, this, &ManagerWindow::importRequested);
    importRow->addWidget(import);
    libraryLayout->addLayout(importRow);
    m_libraryState = new QStackedWidget;
    auto *empty = new QFrame;
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(36, 40, 36, 40); emptyLayout->setSpacing(20);
    heading(emptyLayout, QStringLiteral("从第一只桌宠开始"), QStringLiteral("选择一个已经保存好的宠物文件夹。导入后，素材会保存在 PetDesk 中，之后可直接打开。"));
    auto *firstImport = button(QStringLiteral("选择我的桌宠文件夹…"), "firstImport");
    connect(firstImport, &QPushButton::clicked, this, &ManagerWindow::importRequested);
    emptyLayout->addWidget(firstImport);
    emptyLayout->addWidget(text(QStringLiteral("可使用 Codex pet 文件夹，支持 PNG 和 WebP 素材。")));
    emptyLayout->addStretch();
    m_libraryState->addWidget(empty);
    m_libraryTabs = new QTabWidget;
    m_libraryTabs->setObjectName("libraryTabs");
    auto *installed = new QWidget;
    auto *installedLayout = new QVBoxLayout(installed);
    installedLayout->setContentsMargins(12, 12, 12, 12); installedLayout->setSpacing(12);
    m_pets = new PetList;
    m_pets->setObjectName("petLibrary"); m_pets->setAccessibleName(QStringLiteral("已导入的桌宠"));
    m_pets->setMinimumHeight(156);
    installedLayout->addWidget(m_pets, 1);
    auto *detail = new QHBoxLayout;
    m_preview = text({}); m_preview->setFixedSize(96, 104); m_preview->setAlignment(Qt::AlignCenter);
    detail->addWidget(m_preview);
    auto *description = new QVBoxLayout;
    m_name = text(QStringLiteral("选择一只桌宠"));
    auto nameFont = m_name->font(); nameFont.setPointSize(16); nameFont.setBold(true); m_name->setFont(nameFont);
    description->addWidget(m_name);
    m_capabilities = text({}); description->addWidget(m_capabilities);
    m_description = text({}); m_description->setMaximumHeight(58); description->addWidget(m_description);
    m_selectedStatus = text({}); description->addWidget(m_selectedStatus);
    detail->addLayout(description, 1);
    installedLayout->addLayout(detail);
    auto *petButtons = new QHBoxLayout;
    m_reveal = button(QStringLiteral("查看本地文件"), "revealPet");
    connect(m_reveal, &QPushButton::clicked, this, [this] { emit revealRequested(m_pets->currentKey()); });
    petButtons->addWidget(m_reveal);
    m_archive = button(QStringLiteral("移出宠物库"), "archivePet");
    connect(m_archive, &QPushButton::clicked, this, [this] { emit archiveRequested(m_pets->currentKey()); });
    petButtons->addWidget(m_archive);
    petButtons->addStretch();
    m_use = button(QStringLiteral("使用这只桌宠"), "usePet");
    connect(m_use, &QPushButton::clicked, this, [this] { emit useRequested(m_pets->currentKey()); });
    petButtons->addWidget(m_use);
    installedLayout->addLayout(petButtons);
    connect(m_pets, &PetList::selectionChanged, this, [this] { updateSelection(); });
    m_libraryTabs->addTab(installed, QStringLiteral("我的桌宠"));
    auto *removedPage = new QWidget;
    auto *removedLayout = new QVBoxLayout(removedPage);
    removedLayout->addWidget(text(QStringLiteral("移出的桌宠仍保留在本机，可以恢复；原始素材文件夹不会被改动。")));
    m_removed = new PetList;
    m_removed->setObjectName("removedPets"); m_removed->setAccessibleName(QStringLiteral("已移出的桌宠"));
    removedLayout->addWidget(m_removed, 1);
    m_restore = button(QStringLiteral("恢复到宠物库"), "restorePet");
    connect(m_restore, &QPushButton::clicked, this, [this] { emit restoreRequested(m_removed->currentKey()); });
    connect(m_removed, &PetList::selectionChanged, this, [this] {
        m_restore->setEnabled(!m_removed->currentKey().isEmpty());
    });
    removedLayout->addWidget(m_restore);
    m_libraryTabs->addTab(removedPage, QStringLiteral("已移出"));
    m_libraryState->addWidget(m_libraryTabs);
    libraryLayout->addWidget(m_libraryState, 1);

    QVBoxLayout *runtimeLayout;
    page(m_pages, runtimeLayout);
    heading(runtimeLayout, QStringLiteral("桌面设置"), QStringLiteral("显示与互动设置按每只桌宠分别保存，换宠物也不用重新调整。"));
    m_runtimeState = new QStackedWidget;
    auto *noCurrent = new QWidget;
    auto *noCurrentLayout = new QVBoxLayout(noCurrent);
    noCurrentLayout->addWidget(text(QStringLiteral("先在宠物库选择一只桌宠，再调整它的显示与互动方式。")));
    auto *toLibrary = button(QStringLiteral("前往宠物库"), "goToLibrary");
    connect(toLibrary, &QPushButton::clicked, this, [this] { showPage(0); });
    noCurrentLayout->addWidget(toLibrary); noCurrentLayout->addStretch();
    m_runtimeState->addWidget(noCurrent);
    auto *runtime = new QWidget;
    auto *runtimeBody = new QVBoxLayout(runtime); runtimeBody->setSpacing(16);
    m_currentName = text({}); m_currentName->setObjectName("currentPetName");
    runtimeBody->addWidget(m_currentName);
    auto *displayLayout = new QVBoxLayout;
    auto *displayForm = new QFormLayout;
    m_size = new QComboBox; m_size->setObjectName("petSize"); m_size->setAccessibleName(QStringLiteral("桌宠大小"));
    for (int size : {75, 100, 125, 150, 200}) m_size->addItem(QString::number(size) + '%', size);
    displayForm->addRow(QStringLiteral("大小"), m_size);
    m_screen = new QComboBox; m_screen->setAccessibleName(QStringLiteral("目标屏幕"));
    displayForm->addRow(QStringLiteral("放到屏幕"), m_screen);
    displayLayout->addLayout(displayForm);
    auto *reset = button(QStringLiteral("移到所选屏幕右下角"), "resetPosition");
    connect(reset, &QPushButton::clicked, this, [this] { emit positionRequested(m_screen->currentIndex()); });
    displayLayout->addWidget(reset);
    runtimeBody->addWidget(group(QStringLiteral("显示与位置"), displayLayout));
    auto *behavior = new QVBoxLayout;
    m_gaze = new QCheckBox(QStringLiteral("看向鼠标")); m_gaze->setObjectName("gaze"); behavior->addWidget(m_gaze);
    m_gazeHint = text({}); behavior->addWidget(m_gazeHint);
    m_automatic = new QCheckBox(QStringLiteral("自由活动")); m_automatic->setObjectName("automatic"); behavior->addWidget(m_automatic);
    m_walk = new QCheckBox(QStringLiteral("允许自动散步")); m_walk->setObjectName("walking"); behavior->addWidget(m_walk);
    auto *paceForm = new QFormLayout;
    m_pace = new QComboBox; m_pace->setObjectName("pace"); m_pace->setAccessibleName(QStringLiteral("活动频率"));
    m_pace->addItems({QStringLiteral("安静 · 偶尔活动"), QStringLiteral("适中 · 默认"), QStringLiteral("活泼 · 更常活动")});
    paceForm->addRow(QStringLiteral("活动频率"), m_pace); behavior->addLayout(paceForm);
    runtimeBody->addWidget(group(QStringLiteral("互动与活动"), behavior));
    auto *previewLayout = new QVBoxLayout;
    auto *actionRow = new QHBoxLayout;
    m_action = new QComboBox; m_action->setObjectName("action"); m_action->setAccessibleName(QStringLiteral("基础动作"));
    for (int row = 0; row < 9; ++row) m_action->addItem(actionName(static_cast<Action>(row)));
    actionRow->addWidget(m_action, 1);
    m_playAction = button(QStringLiteral("播放动作"), "playAction");
    connect(m_playAction, &QPushButton::clicked, this, [this] { emit actionRequested(m_action->currentIndex()); });
    actionRow->addWidget(m_playAction); previewLayout->addLayout(actionRow);
    auto *activityRow = new QHBoxLayout;
    m_activity = new QComboBox; m_activity->setObjectName("activity"); m_activity->setAccessibleName(QStringLiteral("连续小活动"));
    for (int id = 0; id < static_cast<int>(Activity::Count); ++id) m_activity->addItem(activityName(static_cast<Activity>(id)));
    activityRow->addWidget(m_activity, 1);
    m_playActivity = button(QStringLiteral("开始小活动"), "playActivity");
    connect(m_playActivity, &QPushButton::clicked, this, [this] { emit activityRequested(m_activity->currentIndex()); });
    activityRow->addWidget(m_playActivity); previewLayout->addLayout(activityRow);
    m_pause = new QCheckBox(QStringLiteral("暂停所有动画")); m_pause->setObjectName("pause"); previewLayout->addWidget(m_pause);
    connect(m_pause, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncing) emit pauseRequested(checked);
    });
    previewLayout->addWidget(text(QStringLiteral("在桌面单击挥手、双击跳跃；按住拖动移动，右键回到控制台。")));
    runtimeBody->addWidget(group(QStringLiteral("试试动作"), previewLayout));
    runtimeBody->addStretch();
    m_runtimeState->addWidget(runtime);
    runtimeLayout->addWidget(m_runtimeState, 1);
    for (auto *check : {m_gaze, m_automatic, m_walk})
        connect(check, &QCheckBox::toggled, this, [this] { emitPetOptions(); });
    connect(m_size, &QComboBox::currentIndexChanged, this, [this] { emitPetOptions(); });
    connect(m_pace, &QComboBox::currentIndexChanged, this, [this] { emitPetOptions(); });

    QVBoxLayout *preferencesLayout;
    page(m_pages, preferencesLayout);
    heading(preferencesLayout, QStringLiteral("应用偏好"), QStringLiteral("决定再次打开 PetDesk 和关闭控制台时的行为。设置会自动保存。"));
    auto *startup = new QVBoxLayout;
    m_openConsole = new QCheckBox(QStringLiteral("启动时打开控制台")); m_openConsole->setObjectName("openConsole"); startup->addWidget(m_openConsole);
    m_restorePet = new QCheckBox(QStringLiteral("启动时恢复上次的桌宠")); m_restorePet->setObjectName("restoreLastPet"); startup->addWidget(m_restorePet);
    preferencesLayout->addWidget(group(QStringLiteral("启动 PetDesk 时"), startup));
    auto *closing = new QVBoxLayout;
    auto *closeForm = new QFormLayout;
    m_closeBehavior = new QComboBox; m_closeBehavior->setObjectName("closeBehavior");
    m_closeBehavior->setAccessibleName(QStringLiteral("关闭控制台时"));
    m_closeBehavior->addItems({QStringLiteral("桌宠继续运行"), QStringLiteral("退出 PetDesk")});
    closeForm->addRow(QStringLiteral("关闭控制台时"), m_closeBehavior); closing->addLayout(closeForm);
    closing->addWidget(text(QStringLiteral("收起后，可再次打开 PetDesk，或右键桌宠、使用菜单栏图标回到控制台。")));
    preferencesLayout->addWidget(group(QStringLiteral("关闭与常驻"), closing));
    auto *storage = new QVBoxLayout;
    storage->addWidget(text(QStringLiteral("导入的素材和设置保存在本机。原始文件夹移动或删除后，已导入的桌宠仍可使用。")));
    auto *data = button(QStringLiteral("查看本地数据文件夹"), "openDataFolder");
    connect(data, &QPushButton::clicked, this, &ManagerWindow::dataFolderRequested); storage->addWidget(data);
    preferencesLayout->addWidget(group(QStringLiteral("本地数据"), storage)); preferencesLayout->addStretch();
    for (auto *check : {m_openConsole, m_restorePet})
        connect(check, &QCheckBox::toggled, this, [this] { emitAppOptions(); });
    connect(m_closeBehavior, &QComboBox::currentIndexChanged, this, [this] { emitAppOptions(); });

    QVBoxLayout *about;
    page(m_pages, about);
    heading(about, "PetDesk " + QCoreApplication::applicationVersion(), QStringLiteral("为你自己的桌宠准备的轻量底座。"));
    about->addWidget(text(QStringLiteral("从本地目录导入、管理和切换桌宠，按每只宠物保存互动设置。支持 Codex v1/v2 素材；v2 可使用 16 向注视。")));
    about->addWidget(text(QStringLiteral("完全离线运行，无需账号、模型、API key 或 Agent。当前支持单只桌宠在桌面显示，宠物库可保存多只。")));
    about->addWidget(text(QStringLiteral("素材属于各自作者。导入前请确认你有使用权限。")));
    auto *repo = button(QStringLiteral("查看开源仓库"), "repository");
    connect(repo, &QPushButton::clicked, this, &ManagerWindow::repositoryRequested); about->addWidget(repo);
    about->addStretch();
    showPage(0);
    setLibrary({}, {}, {}, false);
    setCurrentPet({}, {}, false, false);
    setAppOptions({});
}

void ManagerWindow::setLibrary(const QVector<PetEntry> &entries, const QVector<PetEntry> &removed,
                              const QString &currentKey, bool petVisible) {
    const QString previous = m_pets->currentKey();
    m_currentKey = currentKey; m_petVisible = petVisible;
    m_entries.clear();
    for (const auto &entry : entries) m_entries.insert(entry.key, entry);
    m_pets->setEntries(entries, currentKey, petVisible);
    m_removed->setEntries(removed);
    m_restore->setEnabled(!m_removed->currentKey().isEmpty());
    m_count->setText(QStringLiteral("%1 只桌宠 · %2 只正在显示").arg(entries.size()).arg(petVisible ? 1 : 0));
    m_libraryState->setCurrentIndex(entries.isEmpty() && removed.isEmpty() ? 0 : 1);
    m_libraryTabs->setTabText(1, QStringLiteral("已移出 (%1)").arg(removed.size()));
    selectPet(m_entries.contains(previous) ? previous : currentKey);
    if (entries.isEmpty() && !removed.isEmpty()) m_libraryTabs->setCurrentIndex(1);
    updateSelection();
}

void ManagerWindow::selectPet(const QString &key) {
    if (m_entries.contains(key)) { m_pets->selectKey(key); m_libraryTabs->setCurrentIndex(0); }
}

void ManagerWindow::updateSelection() {
    const auto found = m_entries.constFind(m_pets->currentKey());
    const bool valid = found != m_entries.cend();
    for (auto *widget : {m_use, m_archive, m_reveal}) widget->setEnabled(valid);
    m_preview->clear();
    if (!valid) {
        m_name->setText(QStringLiteral("还没有桌宠")); m_capabilities->clear(); m_description->clear();
        m_selectedStatus->setText(QStringLiteral("点击上方「导入宠物目录…」添加。")); return;
    }
    const auto &entry = *found;
    m_preview->setPixmap(QPixmap(entry.thumbnail));
    m_name->setText(entry.name);
    m_capabilities->setText(entry.version == 2 ? QStringLiteral("9 种动作 · 16 向注视") : QStringLiteral("9 种动作"));
    m_description->setText(entry.description);
    m_description->setToolTip(entry.description);
    const bool current = entry.key == m_currentKey;
    m_selectedStatus->setText(current ? (m_petVisible ? QStringLiteral("正在桌面陪伴你") : QStringLiteral("已隐藏，可以重新显示")) : QStringLiteral("已导入，可以随时使用"));
    m_use->setText(current && m_petVisible ? QStringLiteral("桌面设置") : QStringLiteral("使用这只桌宠"));
}

void ManagerWindow::setCurrentPet(const PetEntry &entry, PetOptions options, bool paused, bool visible) {
    const QScopedValueRollback<bool> blocker(m_syncing, true);
    m_hasPet = !entry.key.isEmpty(); m_currentKey = entry.key; m_currentNameText = entry.name; m_petVisible = visible;
    m_runtimeState->setCurrentIndex(m_hasPet ? 1 : 0);
    m_currentName->setText(QStringLiteral("正在设置：") + entry.name);
    m_size->setCurrentIndex(std::max(0, m_size->findData(options.size)));
    m_gaze->setChecked(options.gaze && entry.version == 2); m_gaze->setEnabled(entry.version == 2);
    m_gazeHint->setText(entry.version == 2 ? QStringLiteral("鼠标停下后会回到正常待机。") : QStringLiteral("这只桌宠的素材没有注视姿势，其他动作可以正常使用。"));
    m_automatic->setChecked(options.automatic); m_walk->setChecked(options.walking);
    m_walk->setEnabled(options.automatic); m_pace->setEnabled(options.automatic); m_pace->setCurrentIndex(options.pace);
    m_pause->setChecked(paused); m_playAction->setEnabled(!paused); m_playActivity->setEnabled(!paused);
    m_visibility->setEnabled(m_hasPet); m_visibility->setText(visible ? QStringLiteral("隐藏桌宠") : QStringLiteral("显示桌宠"));
    if (!m_hasPet) setActivity({});
    updateSelection();
}
void ManagerWindow::setAppOptions(AppOptions options) {
    const QScopedValueRollback<bool> blocker(m_syncing, true);
    m_openConsole->setChecked(options.openConsole); m_restorePet->setChecked(options.restorePet);
    m_closeBehavior->setCurrentIndex(options.quitOnClose ? 1 : 0);
}
void ManagerWindow::setScreens(const QStringList &names) {
    const int previous = m_screen->currentIndex();
    m_screen->clear(); m_screen->addItems(names);
    if (previous >= 0 && previous < names.size()) m_screen->setCurrentIndex(previous);
}
void ManagerWindow::setActivity(const QString &activity) {
    const QString value = !m_hasPet ? QStringLiteral("还没有正在运行的桌宠") :
        m_currentNameText + " · " + (m_petVisible ? activity : QStringLiteral("已隐藏"));
    m_status->setText(value); m_status->setAccessibleName(value);
}
void ManagerWindow::setNotice(const QString &value, bool error) {
    m_notice->setText(value);
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    m_notice->setStyleSheet(QString("QLabel { background: %1; color: %2; }")
        .arg(error ? (dark ? "#492c2b" : "#fff0ed") : (dark ? "#243d36" : "#edf7f1"))
        .arg(dark ? "#f5f5f5" : "#24332b"));
    m_notice->setVisible(!value.isEmpty());
}
void ManagerWindow::emitPetOptions() {
    if (m_syncing || !m_hasPet) return;
    m_walk->setEnabled(m_automatic->isChecked()); m_pace->setEnabled(m_automatic->isChecked());
    emit petOptionsChanged({m_size->currentData().toInt(), m_gaze->isChecked(), m_automatic->isChecked(),
        m_walk->isChecked(), m_pace->currentIndex()});
}
void ManagerWindow::emitAppOptions() {
    if (!m_syncing) emit appOptionsChanged({m_openConsole->isChecked(), m_restorePet->isChecked(), m_closeBehavior->currentIndex() == 1});
}
void ManagerWindow::showPage(int page) {
    const int index = std::clamp(page, 0, 3);
    m_navigation[index]->setChecked(true); m_pages->setCurrentIndex(index);
}
void ManagerWindow::closeEvent(QCloseEvent *event) {
    event->accept(); // Do not veto an explicit application quit. Last-window auto-quit is disabled.
    if (shouldQuitOnClose && shouldQuitOnClose()) emit quitRequested();
}
void ManagerWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event); if (visibilityChanged) visibilityChanged(true);
}
void ManagerWindow::hideEvent(QHideEvent *event) {
    QMainWindow::hideEvent(event); if (visibilityChanged) visibilityChanged(false);
}
void ManagerWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
        QFileInfo(event->mimeData()->urls().first().toLocalFile()).isDir()) event->acceptProposedAction();
}
void ManagerWindow::dropEvent(QDropEvent *event) {
    if (event->mimeData()->urls().size() != 1) return;
    const QString directory = event->mimeData()->urls().first().toLocalFile();
    if (QFileInfo(directory).isDir()) { event->acceptProposedAction(); emit directoryDropped(directory); }
}
void ManagerWindow::importHelp() {
    QMessageBox::information(this, QStringLiteral("选择哪个文件夹？"),
        QStringLiteral("选择已经保存好的 Codex pet 文件夹。\n\n支持包含宠物说明和 PNG/WebP 动画图的目录，也支持 create-pet 工作目录或其 final 目录。\n\n请先解压下载的压缩包，再选择里面的宠物文件夹。导入前会显示预览，并检查素材是否完整。\n\nv1 素材有 9 种动作；v2 还包含 16 向注视。"));
}
}
