#pragma once
#include "activation.h"
#include "iconstore.h"
#include "managerwindow.h"
#include "petformat.h"
#include "petwindow.h"
#include "preferences.h"
#include <memory>

namespace petdesk {
class StatusEntry;
class Controller final : public QObject {
public:
    explicit Controller(const QString &dataRoot, bool migrateLegacy = true);
    ~Controller() override;
    void start();
    void showManager();
private:
    void importFolder();
    void importDirectory(const QString &directory);
    void activate(const QString &key, bool visible = true, bool interactive = true);
    void refreshLibrary();
    void syncCurrent();
    void refreshScreens();
    void applyOptions(PetOptions options, bool force = false);
    void setPetVisible(bool visible);
    void moveToScreen(int index);
    void archivePet(const QString &key);
    void restorePet(const QString &key);
    void showError(const QString &message);
    void chooseIcon();
    void applyAppIcon(const QImage &custom = {});
    QString m_root;
    PetLibrary m_library;
    IconStore m_iconStore;
    Preferences m_preferences;
    PetWindow m_pet;
    ManagerWindow *m_manager;
    std::unique_ptr<StatusEntry> m_statusEntry;
    QAction *m_pause, *m_visibility;
    QString m_currentKey;
    PetEntry m_currentEntry;
    PetOptions m_options;
    bool m_paused = false;
    std::unique_ptr<ActivationMailbox> m_activation;
};
}
