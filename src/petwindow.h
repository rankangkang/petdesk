#pragma once
#include "behavior.h"
#include "petformat.h"
#include <QElapsedTimer>
#include <QHash>
#include <QRegion>
#include <QTimer>
#include <QWidget>
#include <functional>

namespace petdesk {
class PetWindow final : public QWidget {
public:
    PetWindow();
    void setPet(PetData pet);
    void setPetScale(double scale);
    void setPaused(bool paused);
    void setGazeEnabled(bool enabled);
    void setAutomaticEnabled(bool enabled);
    void setWalkingEnabled(bool enabled);
    void setAutomaticSuspended(bool suspended);
    void play(int row);
    void playActivity(Activity activity);
    QString currentActivity() const;
    void clampToScreen();
    bool hasPet() const { return !m_pet.atlas.isNull(); }
    bool hasGaze() const { return m_pet.version == 2; }
    std::function<void(QPoint)> positionSaved;
    std::function<void(QPoint)> contextRequested;
    std::function<void(QString)> activityChanged;
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QRect currentFrame() const;
    void refreshFrame();
    void advance();
    void scheduleNext();
    void checkGaze();
    bool startSteps(QVector<AnimationStep> steps, Trigger trigger, const QString &name);
    void syncPlayback(bool finishMotion = false);
    void stopMotion(bool finish = false);
    void moveStep();
    void resetToIdle();
    void syncAuxiliaryTimers();
    void scheduleAutomatic(bool initial = false);
    void runAutomatic();
    void notifyActivity();
    QRectF movementBounds() const;
    PetData m_pet;
    Playback m_playback;
    ActivityPolicy m_policy;
    QTimer m_animation, m_gaze, m_automatic, m_motion;
    QElapsedTimer m_clock, m_cursorClock, m_motionClock;
    QHash<int, QRegion> m_masks;
    QPoint m_pressGlobal, m_pressOrigin, m_lastDragGlobal, m_lastCursor;
    QPoint m_motionOrigin, m_motionTarget;
    double m_scale = 1.0;
    int m_look = -1, m_motionDuration = 0;
    quint64 m_displayedSerial = 0;
    bool m_paused = false, m_gazeEnabled = false;
    bool m_automaticEnabled = true, m_walkingEnabled = false, m_automaticSuspended = false;
    bool m_pressed = false, m_dragging = false, m_doubleClicked = false;
};
}
