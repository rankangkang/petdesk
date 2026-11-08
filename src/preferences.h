#pragma once
#include <QSettings>
#include <QString>

namespace petdesk {
struct PetOptions {
    int size = 100;
    bool gaze = true, automatic = true, walking = false;
    int pace = 1;
    bool operator==(const PetOptions &) const = default;
};
struct AppOptions {
    bool openConsole = true, restorePet = true, quitOnClose = false;
    bool operator==(const AppOptions &) const = default;
};

class Preferences {
public:
    explicit Preferences(const QString &root, bool migrateLegacy = true);
    PetOptions pet(const QString &key) const;
    void savePet(const QString &key, PetOptions options);
    AppOptions app() const;
    void saveApp(AppOptions options);
    QSettings &state() { return m_settings; }
private:
    QSettings m_settings;
};
}
