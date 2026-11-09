#include "managerwindow.h"
#include "behavior.h"
#include "uistyle.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

namespace petdesk {
namespace {
QLabel *text(const QString &value, const char *style = nullptr) {
    auto *label = new QLabel(value);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    if (style) label->setObjectName(style);
    return label;
}
void heading(QVBoxLayout *layout, const QString &title) {
    layout->addWidget(text(title, "pageTitle"));
}
QPushButton *button(const QString &title, const char *name = nullptr, const char *role = nullptr) {
    auto *result = new QPushButton(title);
    if (name) result->setObjectName(name);
    if (role) result->setProperty("role", role);
    result->setMinimumHeight(36);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}
QWidget *page(QStackedWidget *pages, QVBoxLayout *&layout) {
    auto *scroll = new QScrollArea;
    scroll->setObjectName("pageScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName("pageContent");
    auto *centering = new QHBoxLayout(content);
    centering->setContentsMargins(32, 28, 32, 28);
    auto *column = new QWidget;
    column->setObjectName("pageColumn");
    column->setMaximumWidth(760);
    centering->addStretch(); centering->addWidget(column, 1); centering->addStretch();
    layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    scroll->setWidget(content);
    pages->addWidget(scroll);
    return content;
}
QFrame *card(const char *name, QVBoxLayout *&layout, int margin = 18) {
    auto *frame = new QFrame;
    frame->setObjectName(name);
    layout = new QVBoxLayout(frame);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(12);
    return frame;
}
QWidget *group(const QString &title, QVBoxLayout *contents) {
    auto *section = new QWidget;
    auto *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(8);
    sectionLayout->addWidget(text(title, "sectionTitle"));
    QVBoxLayout *layout;
    auto *frame = card("sectionCard", layout, 16);
    layout->addLayout(contents);
    sectionLayout->addWidget(frame);
    return section;
}
QWidget *settingRow(const QString &title, QWidget *control, QLabel **hintLabel = nullptr) {
    auto *container = new QWidget;
    container->setMinimumHeight(42);
    auto *row = new QHBoxLayout(container);
    row->setContentsMargins(0, 2, 0, 2); row->setSpacing(16);
    auto *labels = new QVBoxLayout; labels->setSpacing(3);
    labels->addWidget(text(title, "settingTitle"));
    if (hintLabel) {
        auto *detail = text({}, "helper"); detail->hide(); labels->addWidget(detail);
        *hintLabel = detail;
    }
    row->addLayout(labels, 1); row->addWidget(control, 0, Qt::AlignVCenter);
    return container;
}
QLabel *divider() {
    auto *line = new QLabel; line->setObjectName("divider"); line->setFixedHeight(1); return line;
}
}

ManagerWindow::ManagerWindow() {
    setObjectName("console");
    setWindowTitle(QStringLiteral("PetDesk · 控制台"));
    resize(1040, 740);
    setMinimumSize(800, 600);
    setAcceptDrops(true);
    setStyleSheet(ui::styleSheet());
    auto *central = new QWidget; central->setObjectName("consoleRoot");
    auto *shell = new QHBoxLayout(central); shell->setContentsMargins(0, 0, 0, 0); shell->setSpacing(0);

    auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(190);
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(12, 24, 12, 18); side->setSpacing(3);
    auto *brand = new QLabel(QStringLiteral("PetDesk")); brand->setObjectName("wordmark");
    brand->setContentsMargins(9, 0, 0, 0); side->addWidget(brand);
    side->addSpacing(24);
    auto *navigation = new QButtonGroup(this); navigation->setExclusive(true);
    const QStringList navNames = {QStringLiteral("宠物库"), QStringLiteral("桌面设置"),
        QStringLiteral("应用偏好"), QStringLiteral("关于")};
    const std::array<ui::Icon, 4> navIcons = {ui::Icon::Library, ui::Icon::Sliders, ui::Icon::Settings, ui::Icon::Info};
    for (int index = 0; index < navNames.size(); ++index) {
        auto *item = button(navNames[index]);
        item->setObjectName(QString("navigation%1").arg(index));
        item->setProperty("nav", true); item->setCheckable(true); item->setMinimumHeight(36);
        item->setIcon(ui::icon(navIcons[index], QColor("#4d4d4d"), QColor("#202123")));
        item->setIconSize(QSize(17, 17));
        navigation->addButton(item, index);
        connect(item, &QPushButton::toggled, this, [this, index](bool checked) {
            if (checked) m_pages->setCurrentIndex(index);
        });
        m_navigation.append(item); side->addWidget(item);
    }
    side->addStretch();
    auto *version = text("v" + QCoreApplication::applicationVersion(), "sideFoot");
    version->setContentsMargins(9, 0, 0, 0); side->addWidget(version);
    shell->addWidget(sidebar);

    auto *workspace = new QWidget; workspace->setObjectName("workspace");
    auto *workspaceLayout = new QVBoxLayout(workspace); workspaceLayout->setContentsMargins(0, 0, 0, 0); workspaceLayout->setSpacing(0);
    m_pages = new QStackedWidget; m_pages->setObjectName("pages"); workspaceLayout->addWidget(m_pages, 1);
    m_notice = text({}, "notice"); m_notice->hide();
    m_noticeTimer = new QTimer(this); m_noticeTimer->setObjectName("noticeTimer");
    m_noticeTimer->setSingleShot(true); m_noticeTimer->setInterval(5000);
    connect(m_noticeTimer, &QTimer::timeout, m_notice, &QLabel::hide);
    auto *noticeArea = new QHBoxLayout; noticeArea->setContentsMargins(26, 0, 26, 9); noticeArea->addWidget(m_notice);
    workspaceLayout->addLayout(noticeArea);
    auto *bottom = new QFrame; bottom->setObjectName("bottomBar");
    auto *footer = new QHBoxLayout(bottom); footer->setContentsMargins(32, 8, 24, 8); footer->setSpacing(8);
    m_footDot = text(QStringLiteral("●"), "footDot"); footer->addWidget(m_footDot);
    m_status = text(QStringLiteral("还没有正在运行的桌宠"), "status"); footer->addWidget(m_status, 1);
    m_visibility = button(QStringLiteral("显示桌宠"), "toggleVisibility");
    m_visibility->setIcon(ui::icon(ui::Icon::Eye, QColor("#555555")));
    connect(m_visibility, &QPushButton::clicked, this, &ManagerWindow::visibilityRequested); footer->addWidget(m_visibility);
    auto *hide = button(QStringLiteral("收起窗口"), "hideConsole", "quiet");
    hide->setIcon(ui::icon(ui::Icon::Hide, QColor("#666666")));
    connect(hide, &QPushButton::clicked, this, [this] { QMainWindow::hide(); }); footer->addWidget(hide);
    auto *quit = button(QStringLiteral("退出应用"), "quit", "quiet");
    quit->setIcon(ui::icon(ui::Icon::Exit, QColor("#666666")));
    connect(quit, &QPushButton::clicked, this, &ManagerWindow::quitRequested); footer->addWidget(quit);
    workspaceLayout->addWidget(bottom); shell->addWidget(workspace, 1); setCentralWidget(central);
    connect(navigation, &QButtonGroup::idClicked, this, [this](int id) { showPage(id); });

    QVBoxLayout *libraryLayout; page(m_pages, libraryLayout);
    auto *libraryHeader = new QHBoxLayout; libraryHeader->setSpacing(8);
    libraryHeader->addWidget(text(QStringLiteral("宠物库"), "pageTitle"), 1);
    auto *help = button(QStringLiteral("支持格式"), "importHelp", "quiet");
    connect(help, &QPushButton::clicked, this, [this] { importHelp(); }); libraryHeader->addWidget(help);
    m_import = button(QStringLiteral("导入桌宠"), "importPet", "primary");
    m_import->setIcon(ui::icon(ui::Icon::Plus, Qt::white)); m_import->setDefault(true);
    connect(m_import, &QPushButton::clicked, this, &ManagerWindow::importRequested); libraryHeader->addWidget(m_import);
    libraryLayout->addLayout(libraryHeader);
    m_libraryState = new QStackedWidget;
    QVBoxLayout *emptyLayout; auto *empty = card("emptyCard", emptyLayout, 30);
    emptyLayout->addStretch();
    auto *emptyMark = new ui::Mark; emptyLayout->addWidget(emptyMark, 0, Qt::AlignHCenter);
    auto *emptyTitle = text(QStringLiteral("还没有桌宠"), "emptyTitle"); emptyTitle->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyTitle);
    auto *firstImport = button(QStringLiteral("导入桌宠"), "firstImport", "primary");
    firstImport->setIcon(ui::icon(ui::Icon::Plus, Qt::white));
    connect(firstImport, &QPushButton::clicked, this, &ManagerWindow::importRequested); emptyLayout->addWidget(firstImport, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(); m_libraryState->addWidget(empty);

    m_libraryTabs = new QTabWidget; m_libraryTabs->setObjectName("libraryTabs");
    m_libraryTabs->setDocumentMode(true); m_libraryTabs->tabBar()->setExpanding(false);
    auto *installed = new QWidget;
    auto *installedLayout = new QHBoxLayout(installed); installedLayout->setContentsMargins(0, 8, 0, 0); installedLayout->setSpacing(15);
    QVBoxLayout *listLayout; auto *listCard = card("listCard", listLayout, 16); listCard->setMinimumWidth(200);
    m_pets = new PetList; m_pets->setObjectName("petLibrary"); m_pets->setAccessibleName(QStringLiteral("已导入的桌宠"));
    listLayout->addWidget(m_pets, 1);
    installedLayout->addWidget(listCard, 5);

    QVBoxLayout *detailLayout; auto *detailCard = card("detailCard", detailLayout, 17); detailCard->setMinimumWidth(240); detailLayout->setSpacing(8);
    auto *stage = new ui::PreviewStage;
    auto *stageLayout = new QVBoxLayout(stage); stageLayout->setContentsMargins(6, 6, 6, 6);
    m_preview = text({}); m_preview->setFixedSize(164, 178); m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setAccessibleName(QStringLiteral("桌宠形象预览")); stageLayout->addWidget(m_preview, 0, Qt::AlignCenter);
    detailLayout->addWidget(stage, 1);
    m_name = new ui::ElidedLabel; m_name->setObjectName("petName");
    m_name->setFullText(QStringLiteral("选择一只桌宠")); detailLayout->addWidget(m_name);
    m_capabilities = text({}, "capabilities"); detailLayout->addWidget(m_capabilities);
    m_description = text({}, "description"); m_description->setMaximumHeight(70); detailLayout->addWidget(m_description);
    m_use = button(QStringLiteral("使用桌宠"), "usePet", "primary");
    m_use->setIcon(ui::icon(ui::Icon::Arrow, Qt::white));
    connect(m_use, &QPushButton::clicked, this, [this] { emit useRequested(m_pets->currentKey()); }); detailLayout->addWidget(m_use);
    auto *petButtons = new QHBoxLayout; petButtons->setSpacing(3);
    m_reveal = button(QStringLiteral("查看文件"), "revealPet", "quiet");
    m_reveal->setIcon(ui::icon(ui::Icon::Folder, QColor("#666666")));
    connect(m_reveal, &QPushButton::clicked, this, [this] { emit revealRequested(m_pets->currentKey()); }); petButtons->addWidget(m_reveal);
    petButtons->addStretch();
    m_archive = button(QStringLiteral("移出宠物库"), "archivePet", "danger");
    connect(m_archive, &QPushButton::clicked, this, [this] { emit archiveRequested(m_pets->currentKey()); }); petButtons->addWidget(m_archive);
    detailLayout->addLayout(petButtons); installedLayout->addWidget(detailCard, 6);
    connect(m_pets, &PetList::selectionChanged, this, [this] { updateSelection(); });
    m_libraryTabs->addTab(installed, QStringLiteral("我的桌宠"));
    QVBoxLayout *removedLayout; auto *removedPage = card("removedCard", removedLayout, 20);
    removedLayout->addWidget(text(QStringLiteral("已移出的桌宠"), "sectionTitle"));
    removedLayout->addWidget(text(QStringLiteral("移出的桌宠仍保留在本机，可以随时恢复。"), "helper"));
    m_removed = new PetList; m_removed->setObjectName("removedPets"); m_removed->setAccessibleName(QStringLiteral("已移出的桌宠"));
    removedLayout->addWidget(m_removed, 1);
    m_restore = button(QStringLiteral("恢复到宠物库"), "restorePet");
    m_restore->setIcon(ui::icon(ui::Icon::Restore, QColor("#555555")));
    connect(m_restore, &QPushButton::clicked, this, [this] { emit restoreRequested(m_removed->currentKey()); });
    connect(m_removed, &PetList::selectionChanged, this, [this] { m_restore->setEnabled(!m_removed->currentKey().isEmpty()); });
    removedLayout->addWidget(m_restore, 0, Qt::AlignLeft);
    m_libraryTabs->addTab(removedPage, QStringLiteral("已移出")); m_libraryState->addWidget(m_libraryTabs);
    libraryLayout->addWidget(m_libraryState, 1);

    QVBoxLayout *runtimeLayout; page(m_pages, runtimeLayout);
    heading(runtimeLayout, QStringLiteral("桌面设置"));
    m_runtimeState = new QStackedWidget;
    QVBoxLayout *noCurrentLayout; auto *noCurrent = card("emptyCard", noCurrentLayout, 30);
    noCurrentLayout->addStretch(); noCurrentLayout->addWidget(text(QStringLiteral("尚未使用桌宠"), "emptyTitle"));
    auto *toLibrary = button(QStringLiteral("前往宠物库"), "goToLibrary", "primary");
    connect(toLibrary, &QPushButton::clicked, this, [this] { showPage(0); }); noCurrentLayout->addWidget(toLibrary, 0, Qt::AlignLeft); noCurrentLayout->addStretch();
    m_runtimeState->addWidget(noCurrent);
    auto *runtime = new QWidget; auto *runtimeBody = new QVBoxLayout(runtime); runtimeBody->setContentsMargins(0, 0, 0, 0); runtimeBody->setSpacing(14);
    m_currentName = new ui::ElidedLabel; m_currentName->setObjectName("currentPetName");
    runtimeBody->addWidget(m_currentName);
    auto *displayLayout = new QVBoxLayout; displayLayout->setSpacing(6);
    auto *sizeControls = new QWidget;
    auto *sizeRow = new QHBoxLayout(sizeControls); sizeRow->setContentsMargins(0, 0, 0, 0); sizeRow->setSpacing(10);
    m_size = new QSlider(Qt::Horizontal); m_size->setObjectName("petSize");
    m_size->setAccessibleName(QStringLiteral("桌宠大小"));
    m_size->setAccessibleDescription(QStringLiteral("范围 50% 到 200%"));
    m_size->setRange(50, 200); m_size->setSingleStep(5); m_size->setPageStep(25);
    m_size->setValue(100); m_size->setFixedWidth(150);
    m_sizeValue = text(QStringLiteral("100%"), "settingValue"); m_sizeValue->setObjectName("petSizeValue");
    m_sizeValue->setMinimumWidth(42); m_sizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeRow->addWidget(m_size); sizeRow->addWidget(m_sizeValue);
    displayLayout->addWidget(settingRow(QStringLiteral("大小"), sizeControls));
    displayLayout->addWidget(divider());
    m_screen = new ui::ComboBox; m_screen->setAccessibleName(QStringLiteral("目标屏幕"));
    m_screen->setMinimumWidth(140); m_screen->setMaximumWidth(200);
    auto *positionControls = new QWidget; auto *positionRow = new QHBoxLayout(positionControls);
    positionRow->setContentsMargins(0, 0, 0, 0); positionRow->setSpacing(8);
    positionRow->addWidget(m_screen);
    auto *reset = button(QStringLiteral("移至右下角"), "resetPosition");
    positionRow->addWidget(reset);
    connect(reset, &QPushButton::clicked, this, [this] { emit positionRequested(m_screen->currentIndex()); });
    displayLayout->addWidget(settingRow(QStringLiteral("移动到屏幕"), positionControls));
    runtimeBody->addWidget(group(QStringLiteral("显示与位置"), displayLayout));
    auto *behavior = new QVBoxLayout; behavior->setSpacing(6);
    m_gaze = new ui::Toggle(QStringLiteral("看向鼠标")); m_gaze->setObjectName("gaze");
    behavior->addWidget(settingRow(QStringLiteral("看向鼠标"), m_gaze, &m_gazeHint));
    m_gazeHint->setObjectName("gazeHint");
    behavior->addWidget(divider());
    m_automatic = new ui::Toggle(QStringLiteral("自动播放动作")); m_automatic->setObjectName("automatic");
    behavior->addWidget(settingRow(QStringLiteral("自动播放动作"), m_automatic));
    behavior->addWidget(divider());
    m_walk = new ui::Toggle(QStringLiteral("自动散步")); m_walk->setObjectName("walking");
    behavior->addWidget(settingRow(QStringLiteral("自动散步"), m_walk));
    behavior->addWidget(divider());
    m_pace = new ui::ComboBox; m_pace->setObjectName("pace"); m_pace->setAccessibleName(QStringLiteral("活动频率"));
    m_pace->addItems({QStringLiteral("安静 · 偶尔活动"), QStringLiteral("适中 · 默认"), QStringLiteral("活泼 · 更常活动")});
    behavior->addWidget(settingRow(QStringLiteral("活动频率"), m_pace));
    runtimeBody->addWidget(group(QStringLiteral("互动与活动"), behavior));
    auto *previewLayout = new QVBoxLayout; previewLayout->setSpacing(6);
    auto *actionControls = new QWidget; auto *actionRow = new QHBoxLayout(actionControls);
    actionRow->setContentsMargins(0, 0, 0, 0); actionRow->setSpacing(8);
    m_action = new ui::ComboBox; m_action->setObjectName("action"); m_action->setAccessibleName(QStringLiteral("基础动作"));
    for (int row = 0; row < 9; ++row) m_action->addItem(actionName(static_cast<Action>(row)));
    m_action->setMinimumWidth(125); actionRow->addWidget(m_action); m_playAction = button(QStringLiteral("播放"), "playAction");
    connect(m_playAction, &QPushButton::clicked, this, [this] { emit actionRequested(m_action->currentIndex()); }); actionRow->addWidget(m_playAction);
    previewLayout->addWidget(settingRow(QStringLiteral("基础动作"), actionControls));
    previewLayout->addWidget(divider());
    auto *activityControls = new QWidget; auto *activityRow = new QHBoxLayout(activityControls);
    activityRow->setContentsMargins(0, 0, 0, 0); activityRow->setSpacing(8);
    m_activity = new ui::ComboBox; m_activity->setObjectName("activity"); m_activity->setAccessibleName(QStringLiteral("连续小活动"));
    for (int id = 0; id < static_cast<int>(Activity::Count); ++id) m_activity->addItem(activityName(static_cast<Activity>(id)));
    m_activity->setMinimumWidth(125); activityRow->addWidget(m_activity); m_playActivity = button(QStringLiteral("播放"), "playActivity");
    connect(m_playActivity, &QPushButton::clicked, this, [this] { emit activityRequested(m_activity->currentIndex()); }); activityRow->addWidget(m_playActivity);
    previewLayout->addWidget(settingRow(QStringLiteral("连续活动"), activityControls));
    previewLayout->addWidget(divider());
    m_pause = new ui::Toggle(QStringLiteral("暂停所有动画")); m_pause->setObjectName("pause");
    previewLayout->addWidget(settingRow(QStringLiteral("暂停所有动画"), m_pause));
    connect(m_pause, &QCheckBox::toggled, this, [this](bool checked) { if (!m_syncing) emit pauseRequested(checked); });
    runtimeBody->addWidget(group(QStringLiteral("动作与动画"), previewLayout)); runtimeBody->addStretch();
    m_runtimeState->addWidget(runtime); runtimeLayout->addWidget(m_runtimeState, 1);
    for (auto *check : {m_gaze, m_automatic, m_walk}) connect(check, &QCheckBox::toggled, this, [this] { emitPetOptions(); });
    connect(m_size, &QSlider::valueChanged, this, [this](int size) {
        m_sizeValue->setText(QString::number(size) + QLatin1Char('%')); emitPetOptions();
    });
    connect(m_pace, &QComboBox::currentIndexChanged, this, [this] { emitPetOptions(); });

    QVBoxLayout *preferencesLayout; page(m_pages, preferencesLayout);
    heading(preferencesLayout, QStringLiteral("应用偏好"));
    auto *startup = new QVBoxLayout; startup->setSpacing(6);
    m_openConsole = new ui::Toggle(QStringLiteral("启动时打开控制台")); m_openConsole->setObjectName("openConsole");
    startup->addWidget(settingRow(QStringLiteral("打开控制台"), m_openConsole));
    startup->addWidget(divider());
    m_restorePet = new ui::Toggle(QStringLiteral("启动时恢复上次的桌宠")); m_restorePet->setObjectName("restoreLastPet");
    startup->addWidget(settingRow(QStringLiteral("恢复上次的桌宠"), m_restorePet));
    preferencesLayout->addWidget(group(QStringLiteral("启动时"), startup));
    auto *closing = new QVBoxLayout; closing->setSpacing(6);
    m_closeBehavior = new ui::ComboBox; m_closeBehavior->setObjectName("closeBehavior"); m_closeBehavior->setAccessibleName(QStringLiteral("关闭控制台时"));
    m_closeBehavior->addItems({QStringLiteral("桌宠继续运行"), QStringLiteral("退出 PetDesk")});
    closing->addWidget(settingRow(QStringLiteral("关闭控制台时"), m_closeBehavior));
    preferencesLayout->addWidget(group(QStringLiteral("关闭与常驻"), closing));
    auto *appearance = new QVBoxLayout;
    auto *iconControls = new QWidget; auto *iconLayout = new QHBoxLayout(iconControls);
    iconLayout->setContentsMargins(0, 0, 0, 0); iconLayout->setSpacing(8);
    m_iconPreview = new QLabel; m_iconPreview->setObjectName("appIconPreview");
    m_iconPreview->setFixedSize(32, 32); m_iconPreview->setAccessibleName(QStringLiteral("当前图标"));
    auto *chooseIcon = button(QStringLiteral("更换"), "chooseAppIcon");
    chooseIcon->setToolTip(QStringLiteral("更换窗口及状态栏图标；应用文件图标由发行包决定。"));
    m_resetIcon = button(QStringLiteral("恢复默认"), "resetAppIcon");
    connect(chooseIcon, &QPushButton::clicked, this, &ManagerWindow::chooseIconRequested);
    connect(m_resetIcon, &QPushButton::clicked, this, &ManagerWindow::resetIconRequested);
    iconLayout->addWidget(m_iconPreview); iconLayout->addWidget(chooseIcon); iconLayout->addWidget(m_resetIcon);
    appearance->addWidget(settingRow(QStringLiteral("窗口与状态栏图标"), iconControls));
    preferencesLayout->addWidget(group(QStringLiteral("外观"), appearance));
    auto *storage = new QVBoxLayout; storage->setSpacing(6);
    auto *data = button(QStringLiteral("打开文件夹"), "openDataFolder");
    data->setIcon(ui::icon(ui::Icon::Folder, QColor("#555555")));
    connect(data, &QPushButton::clicked, this, &ManagerWindow::dataFolderRequested);
    storage->addWidget(settingRow(QStringLiteral("桌宠和设置"), data));
    preferencesLayout->addWidget(group(QStringLiteral("本地数据"), storage)); preferencesLayout->addStretch();
    for (auto *check : {m_openConsole, m_restorePet}) connect(check, &QCheckBox::toggled, this, [this] { emitAppOptions(); });
    connect(m_closeBehavior, &QComboBox::currentIndexChanged, this, [this] { emitAppOptions(); });

    QVBoxLayout *about; page(m_pages, about);
    heading(about, QStringLiteral("关于 PetDesk"));
    auto *aboutContents = new QVBoxLayout; aboutContents->setSpacing(6);
    aboutContents->addWidget(settingRow(QStringLiteral("版本"), text(QCoreApplication::applicationVersion(), "settingValue")));
    aboutContents->addWidget(divider());
    aboutContents->addWidget(settingRow(QStringLiteral("支持的素材"), text(QStringLiteral("Codex pet v1 / v2"), "settingValue")));
    aboutContents->addWidget(divider());
    auto *repo = button(QStringLiteral("打开仓库"), "repository"); repo->setIcon(ui::icon(ui::Icon::Arrow, QColor("#555555")));
    connect(repo, &QPushButton::clicked, this, &ManagerWindow::repositoryRequested);
    aboutContents->addWidget(settingRow(QStringLiteral("开源仓库"), repo));
    about->addWidget(group(QStringLiteral("应用信息"), aboutContents));
    auto *gestures = new QVBoxLayout; gestures->setSpacing(6);
    gestures->addWidget(settingRow(QStringLiteral("单击"), text(QStringLiteral("挥手"), "settingValue")));
    gestures->addWidget(divider());
    gestures->addWidget(settingRow(QStringLiteral("双击"), text(QStringLiteral("跳跃"), "settingValue")));
    gestures->addWidget(divider());
    gestures->addWidget(settingRow(QStringLiteral("拖动"), text(QStringLiteral("移动"), "settingValue")));
    gestures->addWidget(divider());
    gestures->addWidget(settingRow(QStringLiteral("右键"), text(QStringLiteral("打开控制台"), "settingValue")));
    about->addWidget(group(QStringLiteral("桌面交互"), gestures)); about->addStretch();
    showPage(0);
    setLibrary({}, {}, {}, false); setCurrentPet({}, {}, false, false); setAppOptions({});
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
    const bool empty = entries.isEmpty() && removed.isEmpty();
    m_libraryState->setCurrentIndex(empty ? 0 : 1); m_import->setVisible(!empty);
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
        m_archive->setText(QStringLiteral("移出宠物库")); m_archive->setToolTip({});
        m_name->setFullText(QStringLiteral("还没有桌宠")); m_capabilities->clear(); m_description->clear();
        m_description->hide(); return;
    }
    const auto &entry = *found;
    m_archive->setEnabled(!entry.builtIn);
    m_archive->setText(entry.builtIn ? QStringLiteral("内置桌宠") : QStringLiteral("移出宠物库"));
    m_archive->setToolTip(entry.builtIn ? QStringLiteral("内置桌宠不可移除") : QString());
    if (!m_previewCache.contains(entry.key)) {
        QImage image;
        if (previewRequested) image = previewRequested(entry.key);
        if (image.isNull() && !entry.thumbnail.isEmpty()) image.load(entry.thumbnail);
        if (!image.isNull()) {
            if (m_previewCache.size() >= 16) m_previewCache.clear();
            m_previewCache.insert(entry.key, QPixmap::fromImage(image));
        }
    }
    const QPixmap original = m_previewCache.value(entry.key);
    if (!original.isNull()) {
        const qreal dpr = m_preview->devicePixelRatioF();
        QPixmap scaled = original.scaled(QSize(qRound(160 * dpr), qRound(174 * dpr)),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        m_preview->setPixmap(scaled);
    }
    m_name->setFullText(entry.name);
    m_capabilities->setText((entry.builtIn ? QStringLiteral("内置 · ") : QString()) +
        (entry.version == 2 ? QStringLiteral("9 种动作 · 16 向注视") : QStringLiteral("9 种动作")));
    m_description->setText(entry.description);
    m_description->setToolTip(entry.description);
    m_description->setVisible(!entry.description.isEmpty());
    const bool current = entry.key == m_currentKey;
    m_use->setText(current && m_petVisible ? QStringLiteral("桌面设置") : QStringLiteral("使用桌宠"));
}

void ManagerWindow::setCurrentPet(const PetEntry &entry, PetOptions options, bool paused, bool visible) {
    const QScopedValueRollback<bool> blocker(m_syncing, true);
    m_hasPet = !entry.key.isEmpty(); m_currentKey = entry.key; m_currentNameText = entry.name; m_petVisible = visible;
    m_runtimeState->setCurrentIndex(m_hasPet ? 1 : 0);
    m_currentName->setFullText(QStringLiteral("当前桌宠：") + entry.name);
    m_size->setValue(options.size);
    m_gaze->setChecked(options.gaze && entry.version == 2); m_gaze->setEnabled(entry.version == 2);
    m_gazeHint->setText(QStringLiteral("该桌宠不支持注视"));
    m_gazeHint->setVisible(m_hasPet && entry.version != 2);
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
void ManagerWindow::setAppIcon(const QImage &image, bool custom) {
    const qreal dpr = m_iconPreview->devicePixelRatioF();
    QPixmap preview = QPixmap::fromImage(image).scaled(QSize(qRound(32 * dpr), qRound(32 * dpr)),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    preview.setDevicePixelRatio(dpr);
    m_iconPreview->setPixmap(preview); m_resetIcon->setEnabled(custom);
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
    m_footDot->setProperty("running", m_hasPet && m_petVisible);
    m_footDot->style()->unpolish(m_footDot); m_footDot->style()->polish(m_footDot);
}
void ManagerWindow::setNotice(const QString &value, bool error) {
    m_noticeTimer->stop();
    m_notice->setText(value);
    m_notice->setProperty("severity", error ? "error" : "success");
    m_notice->style()->unpolish(m_notice); m_notice->style()->polish(m_notice);
    m_notice->setVisible(!value.isEmpty());
    if (!error && !value.isEmpty()) m_noticeTimer->start();
}
void ManagerWindow::emitPetOptions() {
    if (m_syncing || !m_hasPet) return;
    m_walk->setEnabled(m_automatic->isChecked()); m_pace->setEnabled(m_automatic->isChecked());
    emit petOptionsChanged({m_size->value(), m_gaze->isChecked(), m_automatic->isChecked(),
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
        QStringLiteral("选择 Codex pet 文件夹，也可以将文件夹拖入窗口。\n\n支持包含 PNG/WebP 动画图的宠物目录、create-pet 工作目录或其 final 目录。请先解压下载的压缩包。\n\nv1 素材有 9 种动作；v2 还包含 16 向注视。"));
}
}
