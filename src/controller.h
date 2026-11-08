#pragma once
#include "petformat.h"
#include "petwindow.h"
#include <QDialog>
#include <QListWidget>
#include <QMenu>
#include <QSettings>
#include <QSystemTrayIcon>

namespace petdesk {
class Controller final : public QObject {
public:
    explicit Controller(const QString &dataRoot);
    ~Controller() override;
    void start();
private:
    void importFolder();
    void importDirectory(const QString &directory);
    void activate(const QString &key);
    void refreshLibrary();
    void showManager();
    void showError(const QString &message);
    PetLibrary m_library;
    QSettings m_settings;
    PetWindow m_pet;
    QDialog *m_manager;
    QListWidget *m_list;
    QMenu m_menu;
    QSystemTrayIcon m_tray;
    QAction *m_pause, *m_gaze, *m_visibility;
    QList<QAction *> m_motionActions;
    QString m_currentKey;
};
}
