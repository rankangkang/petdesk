#include "preferences.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace petdesk {
namespace {
int sizeValue(const QVariant &value) {
    const int size = value.toInt();
    return QList<int>{75, 100, 125, 150, 200}.contains(size) ? size : 100;
}
}
Preferences::Preferences(const QString &root, bool migrateLegacy)
    : m_settings(QDir(root).filePath("settings.ini"), QSettings::IniFormat) {
    if (migrateLegacy && !QFileInfo::exists(m_settings.fileName())) {
        QSettings legacy;
        for (const auto &key : legacy.allKeys()) m_settings.setValue(key, legacy.value(key));
        const QString selected = m_settings.value("currentPet").toString();
        if (!selected.isEmpty()) {
            savePet(selected, {sizeValue(m_settings.value("size", 100)),
                m_settings.value("gaze", true).toBool(), m_settings.value("automatic", true).toBool(),
                m_settings.value("walking", false).toBool(), 1});
        }
        m_settings.setValue("settingsVersion", 1);
        m_settings.sync();
    }
}
PetOptions Preferences::pet(const QString &key) const {
    const QString prefix = "pets/" + key + '/';
    return {sizeValue(m_settings.value(prefix + "size", 100)),
        m_settings.value(prefix + "gaze", true).toBool(),
        m_settings.value(prefix + "automatic", true).toBool(),
        m_settings.value(prefix + "walking", false).toBool(),
        std::clamp(m_settings.value(prefix + "pace", 1).toInt(), 0, 2)};
}
void Preferences::savePet(const QString &key, PetOptions options) {
    if (key.isEmpty()) return;
    const QString prefix = "pets/" + key + '/';
    m_settings.setValue(prefix + "size", sizeValue(options.size));
    m_settings.setValue(prefix + "gaze", options.gaze);
    m_settings.setValue(prefix + "automatic", options.automatic);
    m_settings.setValue(prefix + "walking", options.walking);
    m_settings.setValue(prefix + "pace", std::clamp(options.pace, 0, 2));
}
AppOptions Preferences::app() const {
    return {m_settings.value("startup/openConsole", true).toBool(),
        m_settings.value("startup/restorePet", true).toBool(),
        m_settings.value("window/quitOnClose", false).toBool()};
}
void Preferences::saveApp(AppOptions options) {
    m_settings.setValue("startup/openConsole", options.openConsole);
    m_settings.setValue("startup/restorePet", options.restorePet);
    m_settings.setValue("window/quitOnClose", options.quitOnClose);
}
}
