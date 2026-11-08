#pragma once
#include "petformat.h"
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
    void play(int row, bool returnToIdle = true);
    void clampToScreen();
    bool hasPet() const { return !m_pet.atlas.isNull(); }
    bool hasGaze() const { return m_pet.version == 2; }
    std::function<void(QPoint)> positionSaved;
    std::function<void(QPoint)> contextRequested;
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QRect currentFrame() const;
    void refreshFrame();
    void advance();
    void scheduleNext();
    void checkGaze();
    PetData m_pet;
    QTimer m_animation, m_gaze;
    QHash<int, QRegion> m_masks;
    QPoint m_pressGlobal, m_pressOrigin, m_lastCursor;
    double m_scale = 1.0;
    int m_row = 0, m_frame = 0, m_look = -1;
    bool m_paused = false, m_gazeEnabled = false, m_returnToIdle = false;
    bool m_pressed = false, m_dragging = false;
};
}
