#include "statusentry.h"
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QImage>
#include <QPixmap>
#include <QStyle>
#include <QSystemTrayIcon>

namespace petdesk {
struct StatusEntry::Impl {
    QMenu menu;
    QSystemTrayIcon tray;
    Impl(QAction *open, QAction *importPet, QAction *visibility, QAction *pause, QAction *quit) {
        menu.setObjectName("petdeskStatusMenu");
        tray.setObjectName("petdeskStatusEntry"); tray.setParent(&menu);
        menu.addAction(open); menu.setDefaultAction(open);
        menu.addAction(importPet); menu.addSeparator();
        menu.addAction(visibility); menu.addAction(pause); menu.addSeparator(); menu.addAction(quit);
        const QIcon statusIcon(QStringLiteral(":/assets/PetDesk-status.png"));
        tray.setIcon(!statusIcon.isNull() ? statusIcon : (QApplication::windowIcon().isNull()
            ? QApplication::style()->standardIcon(QStyle::SP_DesktopIcon) : QApplication::windowIcon()));
        tray.setToolTip(QStringLiteral("PetDesk")); tray.setContextMenu(&menu);
        QObject::connect(&tray, &QSystemTrayIcon::activated, &tray, [open](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) open->trigger();
        });
        tray.show();
    }
};
StatusEntry::StatusEntry(QAction *open, QAction *importPet, QAction *visibility, QAction *pause, QAction *quit)
    : m_impl(std::make_unique<Impl>(open, importPet, visibility, pause, quit)) {}
StatusEntry::~StatusEntry() = default;
void StatusEntry::setIcon(const QImage &image) {
    if (!image.isNull()) m_impl->tray.setIcon(QIcon(QPixmap::fromImage(image)));
}
void StatusEntry::activateApplication() {}
}
