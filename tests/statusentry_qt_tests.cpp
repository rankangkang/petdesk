#include "statusentry.h"
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPixmap>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QtTest>

using namespace petdesk;
class StatusEntryQtTests : public QObject {
    Q_OBJECT
private slots:
    void clickingOpensTheManagerAndContextDoesNotQuit() {
        QAction open(QStringLiteral("打开控制台"), this), importPet(QStringLiteral("导入桌宠"), this);
        QAction visibility(QStringLiteral("显示桌宠"), this), pause(QStringLiteral("暂停动画"), this);
        QAction quit(QStringLiteral("退出 PetDesk"), this); pause.setCheckable(true);
        QSignalSpy opened(&open, &QAction::triggered), exited(&quit, &QAction::triggered);
        StatusEntry entry(&open, &importPet, &visibility, &pause, &quit);
        QMenu *menu = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "petdeskStatusMenu") menu = qobject_cast<QMenu *>(widget);
        QVERIFY(menu); QCOMPARE(menu->defaultAction(), &open);
        auto *tray = menu->findChild<QSystemTrayIcon *>("petdeskStatusEntry"); QVERIFY(tray);
        QVERIFY(!tray->icon().pixmap(16, 16).isNull());
        emit tray->activated(QSystemTrayIcon::Trigger);
        emit tray->activated(QSystemTrayIcon::DoubleClick);
        emit tray->activated(QSystemTrayIcon::Context);
        QCOMPARE(opened.size(), 2); QCOMPARE(exited.size(), 0);
        QVERIFY(menu->actions().contains(&visibility));
        QVERIFY(menu->actions().contains(&pause));
        QVERIFY(menu->actions().contains(&quit));
    }
};
QTEST_MAIN(StatusEntryQtTests)
#include "statusentry_qt_tests.moc"
