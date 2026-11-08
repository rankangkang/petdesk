#include "statusentry-mac-bridge.h"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QSignalSpy>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    @autoreleasepool {
        QAction open(QStringLiteral("打开控制台")), quit(QStringLiteral("退出 PetDesk"));
        QAction pause(QStringLiteral("暂停动画")); pause.setCheckable(true); pause.setChecked(true);
        QSignalSpy opened(&open, &QAction::triggered), exited(&quit, &QAction::triggered);
        PetDeskStatusTarget *target = [[PetDeskStatusTarget alloc] init];
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@"PetDesk"];
        NSMenuItem *openItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        NSMenuItem *pauseItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        openItem.representedObject = [NSValue valueWithPointer:&open];
        pauseItem.representedObject = [NSValue valueWithPointer:&pause];
        [menu addItem:openItem]; [menu addItem:pauseItem];
        [target menuWillOpen:menu];
        if (![openItem.title isEqualToString:@"打开控制台"] || pauseItem.state != NSControlStateValueOn) {
            qCritical("Native status menu did not reflect action text or state"); return 1;
        }
        [target performAction:openItem];
        if (opened.size() != 1 || !exited.isEmpty()) {
            qCritical("Native status action dispatched incorrectly"); return 1;
        }
        open.setEnabled(false); [target menuWillOpen:menu]; [target performAction:openItem];
        if (openItem.enabled || opened.size() != 1) {
            qCritical("Disabled native menu action remained active"); return 1;
        }
    }
    return 0;
}
