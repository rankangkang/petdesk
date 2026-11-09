#pragma once
#include "petformat.h"
#include "petlist.h"
#include "preferences.h"
#include <QHash>
#include <QMainWindow>
#include <QPixmap>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTabWidget;
class QTimer;

namespace petdesk {
namespace ui { class ElidedLabel; }
class ManagerWindow final : public QMainWindow {
    Q_OBJECT
public:
    ManagerWindow();
    void setLibrary(const QVector<PetEntry> &entries, const QVector<PetEntry> &removed,
                    const QString &currentKey, bool petVisible);
    void setCurrentPet(const PetEntry &entry, PetOptions options, bool paused, bool visible);
    void setAppOptions(AppOptions options);
    void setAppIcon(const QImage &image, bool custom);
    void setScreens(const QStringList &names);
    void setActivity(const QString &text);
    void setNotice(const QString &text, bool error = false);
    void selectPet(const QString &key);
    void showPage(int page);
    std::function<void(bool)> visibilityChanged;
    std::function<bool()> shouldQuitOnClose;
    std::function<QImage(const QString &)> previewRequested;
signals:
    void importRequested();
    void directoryDropped(QString directory);
    void useRequested(QString key);
    void archiveRequested(QString key);
    void restoreRequested(QString key);
    void revealRequested(QString key);
    void petOptionsChanged(petdesk::PetOptions options);
    void appOptionsChanged(petdesk::AppOptions options);
    void pauseRequested(bool paused);
    void visibilityRequested();
    void positionRequested(int screenIndex);
    void actionRequested(int row);
    void activityRequested(int activity);
    void dataFolderRequested();
    void repositoryRequested();
    void chooseIconRequested();
    void resetIconRequested();
    void quitRequested();
protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void updateSelection();
    void emitPetOptions();
    void emitAppOptions();
    void importHelp();
    QList<QPushButton *> m_navigation;
    PetList *m_pets, *m_removed;
    QStackedWidget *m_pages, *m_libraryState, *m_runtimeState;
    QTabWidget *m_libraryTabs;
    QLabel *m_preview, *m_description, *m_capabilities, *m_iconPreview;
    ui::ElidedLabel *m_name, *m_currentName;
    QLabel *m_gazeHint, *m_status, *m_notice, *m_sizeValue;
    QLabel *m_footDot;
    QPushButton *m_import, *m_use, *m_archive, *m_reveal, *m_restore, *m_visibility;
    QPushButton *m_playAction, *m_playActivity, *m_resetIcon;
    QSlider *m_size;
    QComboBox *m_pace, *m_screen, *m_action, *m_activity, *m_closeBehavior;
    QCheckBox *m_gaze, *m_automatic, *m_walk, *m_pause, *m_openConsole, *m_restorePet;
    QTimer *m_noticeTimer;
    QHash<QString, PetEntry> m_entries;
    QHash<QString, QPixmap> m_previewCache;
    QString m_currentKey, m_currentNameText;
    bool m_syncing = false, m_petVisible = false, m_hasPet = false;
};
}
Q_DECLARE_METATYPE(petdesk::PetOptions)
Q_DECLARE_METATYPE(petdesk::AppOptions)
