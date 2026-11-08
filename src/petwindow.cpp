#include "petwindow.h"
#include <QApplication>
#include <QDebug>
#include <QCursor>
#include <QGuiApplication>
#include <QHideEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QShowEvent>
#include <algorithm>

namespace petdesk {
namespace { constexpr int Padding = 6; }

PetWindow::PetWindow() : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint) {
    setWindowTitle(QStringLiteral("PetDesk 桌宠"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif
    setCursor(Qt::OpenHandCursor);
    setContextMenuPolicy(Qt::PreventContextMenu);
    m_clock.start();
    m_animation.setSingleShot(true);
    m_animation.setTimerType(Qt::CoarseTimer);
    connect(&m_animation, &QTimer::timeout, this, [this] { advance(); });
    m_gaze.setInterval(100);
    m_gaze.setTimerType(Qt::CoarseTimer);
    connect(&m_gaze, &QTimer::timeout, this, [this] { checkGaze(); });
    m_automatic.setSingleShot(true);
    m_automatic.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_automatic, &QTimer::timeout, this, [this] { runAutomatic(); });
    m_motion.setInterval(33); // Active only during a short walk.
    connect(&m_motion, &QTimer::timeout, this, [this] { moveStep(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this] {
        resetToIdle();
        clampToScreen();
    });
}

void PetWindow::setPet(PetData pet) {
    m_animation.stop();
    m_automatic.stop();
    stopMotion();
    m_pet = std::move(pet);
    m_masks.clear();
    m_policy = ActivityPolicy{};
    m_pressed = m_dragging = m_doubleClicked = false;
    m_playback.stop();
    m_look = -1;
    setAccessibleName(m_pet.name);
    setWindowTitle("PetDesk · " + m_pet.name);
    syncPlayback();
}

void PetWindow::setPetScale(double scale) {
    resetToIdle();
    const QPoint bottom = geometry().bottomLeft();
    m_scale = std::clamp(scale, 0.5, 2.0);
    setFixedSize(qRound(FrameWidth * m_scale) + Padding * 2,
                 qRound(FrameHeight * m_scale) + Padding * 2);
    move(bottom.x(), bottom.y() - height() + 1);
    m_masks.clear();
    refreshFrame();
    clampToScreen();
}

void PetWindow::setPaused(bool paused) {
    m_paused = paused;
    resetToIdle();
}

void PetWindow::setGazeEnabled(bool enabled) {
    m_gazeEnabled = enabled;
    m_look = -1;
    m_cursorClock.invalidate();
    syncPlayback();
}

void PetWindow::setAutomaticEnabled(bool enabled) {
    m_automaticEnabled = enabled;
    m_automatic.stop();
    if (!enabled && m_playback.trigger() == Trigger::Automatic) resetToIdle();
    else scheduleAutomatic(true);
}

void PetWindow::setWalkingEnabled(bool enabled) {
    m_walkingEnabled = enabled;
    if (!enabled && m_playback.trigger() == Trigger::Automatic && m_playback.step().distance != 0)
        resetToIdle();
}

void PetWindow::setAutomaticSuspended(bool suspended) {
    m_automaticSuspended = suspended;
    m_automatic.stop();
    if (suspended && m_playback.trigger() == Trigger::Automatic) resetToIdle();
    else scheduleAutomatic(true);
}

QRect PetWindow::currentFrame() const {
    if (m_look >= 0) return m_pet.frameRect(9 + m_look / 8, m_look % 8);
    return m_pet.frameRect(m_playback.row(), m_playback.frame());
}

void PetWindow::refreshFrame() {
    if (!hasPet()) return;
    const QRect source = currentFrame();
    const int key = source.y() / FrameHeight * Columns + source.x() / FrameWidth;
    if (!m_masks.contains(key)) {
        const QImage frame = m_pet.atlas.copy(source).scaled(width() - Padding * 2,
            height() - Padding * 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QRegion region;
        for (int y = 0; y < frame.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(frame.constScanLine(y));
            int start = -1;
            for (int x = 0; x <= frame.width(); ++x) {
                const bool opaque = x < frame.width() && qAlpha(line[x]) > 1;
                if (opaque && start < 0) start = x;
                if (!opaque && start >= 0) {
                    region += QRect(std::max(0, start - 1) + Padding, y + Padding,
                                    std::min(frame.width(), x + 1) - std::max(0, start - 1), 1);
                    start = -1;
                }
            }
        }
        m_masks.insert(key, region);
    }
    setMask(m_masks.value(key));
    update();
}

void PetWindow::paintEvent(QPaintEvent *) {
    if (!hasPet()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRect(Padding, Padding, width() - Padding * 2, height() - Padding * 2),
                      m_pet.atlas, currentFrame());
}

void PetWindow::scheduleNext() {
    m_animation.stop();
    if (hasPet() && !m_paused && isVisible() && m_look < 0)
        m_animation.start(frameDurationMs(m_playback.action(), m_playback.frame()));
}

void PetWindow::advance() {
    m_playback.advance();
    if (m_displayedSerial != m_playback.serial()) syncPlayback(true);
    else { refreshFrame(); scheduleNext(); }
}

bool PetWindow::startSteps(QVector<AnimationStep> steps, Trigger trigger, const QString &name) {
    if (!hasPet() || m_paused || !isVisible()) return false;
    if (!m_playback.start(std::move(steps), trigger, name)) return false;
    m_automatic.stop();
    stopMotion(); // Interrupt at the current position, never snap to the old destination.
    m_look = -1;
    m_cursorClock.invalidate();
    syncPlayback();
    qInfo().noquote() << "activity:" << name << "trigger:" << static_cast<int>(trigger);
    return true;
}

void PetWindow::play(int row) {
    if (row < 0 || row > 8 || m_paused || !hasPet()) return;
    if (row == 0) { resetToIdle(); return; }
    const auto action = static_cast<Action>(row);
    const int distance = row == 1 ? 112 : row == 2 ? -112 : 0;
    startSteps({{action, row <= 4 ? 2 : 1, distance}}, Trigger::User, actionName(action));
}

void PetWindow::playActivity(Activity activity) {
    startSteps(activitySteps(activity), Trigger::User, activityName(activity));
}

void PetWindow::resetToIdle() {
    m_animation.stop();
    m_automatic.stop();
    stopMotion();
    m_playback.stop();
    m_look = -1;
    m_cursorClock.invalidate();
    syncPlayback();
}

void PetWindow::syncPlayback(bool finishMotion) {
    if (m_displayedSerial != m_playback.serial()) {
        stopMotion(finishMotion);
        m_displayedSerial = m_playback.serial();
        m_look = -1;
        if (m_playback.step().distance != 0 && isVisible() && !m_paused) {
            m_motionOrigin = pos();
            m_motionTarget = boundedWalkTarget(pos(), m_playback.step().distance, m_scale,
                                               movementBounds()).toPoint();
            m_motionDuration = m_playback.stepDurationMs();
            if (m_motionOrigin != m_motionTarget && m_motionDuration > 0) {
                m_motionClock.start();
                m_motion.start();
            }
        }
        if (m_playback.idle()) scheduleAutomatic();
    }
    refreshFrame();
    scheduleNext();
    syncAuxiliaryTimers();
    notifyActivity();
}

void PetWindow::stopMotion(bool finish) {
    if (!m_motionClock.isValid()) return;
    m_motion.stop();
    m_motionClock.invalidate();
    if (finish) move(m_motionTarget);
    clampToScreen();
    if (positionSaved) positionSaved(pos());
}

void PetWindow::moveStep() {
    if (m_paused || !isVisible() || m_pressed || !m_motionClock.isValid()) {
        stopMotion();
        return;
    }
    const double progress = std::min(1.0, double(m_motionClock.elapsed()) / m_motionDuration);
    move(m_motionOrigin + ((m_motionTarget - m_motionOrigin) * progress));
    if (progress >= 1) stopMotion(true);
}

QRectF PetWindow::movementBounds() const {
    auto *screen = QGuiApplication::screenAt(geometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return QRectF(pos(), QSizeF(0, 0));
    const QRect bounds = screen->availableGeometry();
    return QRectF(QPointF(bounds.topLeft()), QPointF(
        std::max(bounds.left(), bounds.right() - width() + 1),
        std::max(bounds.top(), bounds.bottom() - height() + 1)));
}

void PetWindow::scheduleAutomatic(bool initial) {
    m_automatic.stop();
    if (!hasPet() || !isVisible() || m_paused || m_pressed || !m_playback.idle() ||
        !m_automaticEnabled || m_automaticSuspended) return;
    m_automatic.start(initial ? QRandomGenerator::global()->bounded(8'000, 12'001)
                             : QRandomGenerator::global()->bounded(18'000, 34'001));
}

void PetWindow::runAutomatic() {
    if (!hasPet() || !isVisible() || m_paused || m_pressed || !m_playback.idle() ||
        !m_automaticEnabled || m_automaticSuspended) return;
    auto activity = m_policy.choose(m_clock.elapsed(), QRandomGenerator::global()->generateDouble(),
                                    m_walkingEnabled);
    if (!activity) { scheduleAutomatic(); return; }
    // Turn back before walking off the current screen. No per-frame desktop polling.
    const auto bounds = movementBounds();
    if (*activity == Activity::WalkRight && x() + 112 * m_scale > bounds.right())
        activity = Activity::WalkLeft;
    else if (*activity == Activity::WalkLeft && x() - 112 * m_scale < bounds.left())
        activity = Activity::WalkRight;
    if (startSteps(activitySteps(*activity), Trigger::Automatic, activityName(*activity)))
        m_policy.didPlay(*activity, m_clock.elapsed());
}

void PetWindow::syncAuxiliaryTimers() {
    const bool eligible = hasGaze() && isVisible() && !m_paused && !m_pressed &&
        m_playback.idle() && m_gazeEnabled;
    if (eligible && !m_gaze.isActive()) {
        m_lastCursor = QCursor::pos();
        m_cursorClock.invalidate();
        m_gaze.start();
    } else if (!eligible) m_gaze.stop();
}

void PetWindow::checkGaze() {
    const QPoint cursor = QCursor::pos();
    if (cursor != m_lastCursor) {
        m_lastCursor = cursor;
        m_cursorClock.start();
    }
    int look = -1;
    if (m_cursorClock.isValid() && m_cursorClock.elapsed() < 1200) {
        const QPointF anchor = pos() + QPointF(width() / 2.0, Padding + FrameHeight * m_scale * 0.4);
        look = gazeDirection(cursor - anchor, m_scale);
    }
    if (look == m_look) return;
    m_look = look;
    m_animation.stop();
    refreshFrame();
    if (look < 0) scheduleNext();
    notifyActivity();
}

QString PetWindow::currentActivity() const {
    if (!hasPet()) return QStringLiteral("尚未选择宠物");
    if (m_paused) return QStringLiteral("已暂停");
    if (m_pressed && m_dragging) return QStringLiteral("跟着你移动");
    if (m_look >= 0) return QStringLiteral("看向鼠标 · %1°").arg(m_look * 22.5);
    if (m_playback.idle()) return QStringLiteral("待机");
    return m_playback.name() + " · " + actionName(m_playback.action());
}

void PetWindow::notifyActivity() {
    const QString text = currentActivity();
    setAccessibleDescription(text + QStringLiteral("。单击挥手，双击跳跃，拖动移动，右键管理。"));
    if (activityChanged) activityChanged(text);
}

void PetWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    m_pressed = true;
    m_dragging = m_doubleClicked = false;
    m_pressGlobal = m_lastDragGlobal = event->globalPosition().toPoint();
    m_pressOrigin = pos();
    resetToIdle();
    event->accept();
}

void PetWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!m_pressed) return;
    const QPoint global = event->globalPosition().toPoint();
    const QPoint delta = global - m_pressGlobal;
    if (!m_dragging && delta.manhattanLength() < QApplication::startDragDistance()) return;
    const int movement = global.x() - m_lastDragGlobal.x();
    const Action action = movement >= 0 ? Action::Right : Action::Left;
    if (!m_dragging || (movement != 0 && m_playback.action() != action))
        startSteps({{action, 0}}, Trigger::Drag, QStringLiteral("跟着你移动"));
    m_dragging = true;
    m_lastDragGlobal = global;
    setCursor(Qt::ClosedHandCursor);
    move(m_pressOrigin + delta);
    event->accept();
}

void PetWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    m_doubleClicked = true;
    m_pressed = true;
    m_pressGlobal = m_lastDragGlobal = event->globalPosition().toPoint();
    m_pressOrigin = pos();
    playActivity(Activity::Jump);
    event->accept();
}

void PetWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        if (contextRequested) contextRequested(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton || !m_pressed) return;
    m_pressed = false;
    setCursor(Qt::OpenHandCursor);
    if (m_dragging) {
        m_dragging = false;
        resetToIdle(); // Release the exclusive drag priority before the landing reaction.
        clampToScreen();
        if (positionSaved) positionSaved(pos());
        play(4);
    } else if (!m_doubleClicked) playActivity(Activity::Greet);
    m_doubleClicked = false;
    syncAuxiliaryTimers();
    scheduleAutomatic();
    event->accept();
}

void PetWindow::clampToScreen() {
    const auto bounds = movementBounds();
    move(std::clamp(x(), qRound(bounds.left()), qRound(bounds.right())),
         std::clamp(y(), qRound(bounds.top()), qRound(bounds.bottom())));
}

void PetWindow::showEvent(QShowEvent *) {
    scheduleNext();
    syncAuxiliaryTimers();
    scheduleAutomatic(true);
}

void PetWindow::hideEvent(QHideEvent *) {
    m_pressed = m_dragging = false;
    resetToIdle();
    m_animation.stop();
    m_gaze.stop();
    m_automatic.stop();
}
}
