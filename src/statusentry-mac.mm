#include "statusentry.h"
#include "statusentry-mac-bridge.h"
#include <QAction>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>

NSImage *PetDeskStatusPortrait(void) {
    QFile iconFile(QStringLiteral(":/assets/PetDesk-status.png"));
    if (!iconFile.open(QIODevice::ReadOnly)) return nil;
    const QByteArray iconData = iconFile.readAll();
    NSData *data = [NSData dataWithBytes:iconData.constData() length:iconData.size()];
    NSImage *image = [[NSImage alloc] initWithData:data];
    [image setSize:NSMakeSize(18, 18)];
    [image setTemplate:NO];
    return image;
}

@implementation PetDeskStatusTarget
- (void)performAction:(NSMenuItem *)item {
    auto *action = static_cast<QAction *>([item.representedObject pointerValue]);
    if (action && action->isEnabled()) action->trigger();
}
- (void)menuWillOpen:(NSMenu *)menu {
    for (NSMenuItem *item in menu.itemArray) {
        if (item.isSeparatorItem) continue;
        auto *action = static_cast<QAction *>([item.representedObject pointerValue]);
        if (!action) continue;
        item.title = action->text().toNSString();
        item.enabled = action->isEnabled();
        item.state = action->isCheckable() && action->isChecked() ? NSControlStateValueOn : NSControlStateValueOff;
    }
}
@end

namespace petdesk {
struct StatusEntry::Impl {
    NSStatusItem *status = nil;
    NSMenu *menu = nil;
    PetDeskStatusTarget *target = nil;
    Impl(QAction *open, QAction *importPet, QAction *visibility, QAction *pause, QAction *quit) {
        // The offscreen Qt test platform has no AppKit status bar.
        if (QGuiApplication::platformName() == QStringLiteral("offscreen")) return;
        target = [[PetDeskStatusTarget alloc] init];
        menu = [[NSMenu alloc] initWithTitle:@"PetDesk"];
        menu.autoenablesItems = NO; menu.delegate = target;
        auto add = [this](QAction *action) {
            NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:action->text().toNSString()
                action:@selector(performAction:) keyEquivalent:@""];
            item.target = target; item.representedObject = [NSValue valueWithPointer:action];
            [menu addItem:item];
        };
        add(open); add(importPet); [menu addItem:NSMenuItem.separatorItem];
        add(visibility); add(pause); [menu addItem:NSMenuItem.separatorItem]; add(quit);
        status = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
        NSImage *image = PetDeskStatusPortrait();
        if (!image) {
            image = [NSImage imageWithSystemSymbolName:@"face.smiling" accessibilityDescription:@"PetDesk"];
            [image setTemplate:YES];
        }
        status.button.image = image; status.button.toolTip = @"PetDesk";
        [status.button setAccessibilityLabel:@"PetDesk"];
        [status.button setAccessibilityIdentifier:@"PetDeskStatusItem"];
        status.menu = menu;
        if (!status || !status.button || !image) {
            if (status) [NSStatusBar.systemStatusBar removeStatusItem:status];
            status = nil;
            // Keep a visible recovery route if the system cannot create the entry.
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            qWarning("Could not create the macOS status entry; using a Dock icon as a fallback");
        } else {
            qInfo("macOS status entry created");
        }
    }
    ~Impl() {
        menu.delegate = nil;
        if (status) [NSStatusBar.systemStatusBar removeStatusItem:status];
    }
};
StatusEntry::StatusEntry(QAction *open, QAction *importPet, QAction *visibility, QAction *pause, QAction *quit)
    : m_impl(std::make_unique<Impl>(open, importPet, visibility, pause, quit)) {}
StatusEntry::~StatusEntry() = default;
void StatusEntry::activateApplication() { if (NSApp) [NSApp activate]; }
}
