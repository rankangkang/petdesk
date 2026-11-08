#include "petwindow.h"
#include <QApplication>
#include <QDebug>
#include <QCursor>
#include <QGuiApplication>
#include <QHideEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

namespace petdesk {
namespace {
constexpr int Padding = 6;
const std::array<QVector<int>, 9> Durations = {{
    {280, 110, 110, 140, 140, 320},
    {120, 120, 120, 120, 120, 120, 120, 220},
    {120, 120, 120, 120, 120, 120, 120, 220},
    {140, 140, 140, 280}, {140, 140, 140, 140, 280},
    {140, 140, 140, 140, 140, 140, 140, 240},
    {150, 150, 150, 150, 150, 260}, {120, 120, 120, 120, 120, 220},
    {150, 150, 150, 150, 150, 280}
}};
}

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
    m_animation.setSingleShot(true);
    m_animation.setTimerType(Qt::CoarseTimer);
    connect(&m_animation, &QTimer::timeout, this, [this] { advance(); });
    m_gaze.setInterval(100);
    m_gaze.setTimerType(Qt::CoarseTimer);
    connect(&m_gaze, &QTimer::timeout, this, [this] { checkGaze(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this] { clampToScreen(); });
}

void PetWindow::setPet(PetData pet) {
    m_animation.stop();
    m_pet = std::move(pet);
    m_masks.clear();
    m_row = m_frame = 0;
    m_look = -1;
    setAccessibleName(m_pet.name);
    setAccessibleDescription(QStringLiteral("拖动移动，单击挥手，右键打开管理面板。"));
    setWindowTitle("PetDesk · " + m_pet.name);
    refreshFrame();
    if (isVisible()) scheduleNext();
}

void PetWindow::setPetScale(double scale) {
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
    m_look = -1;
    m_row = m_frame = 0;
    m_animation.stop();
    m_gaze.stop();
    refreshFrame();
    if (!paused && isVisible()) {
        scheduleNext();
        if (m_gazeEnabled) m_gaze.start();
    }
}

void PetWindow::setGazeEnabled(bool enabled) {
    m_gazeEnabled = enabled;
    m_gaze.stop();
    if (enabled && !m_paused && isVisible()) m_gaze.start();
    else if (m_look >= 0) { m_look = -1; refreshFrame(); scheduleNext(); }
}

QRect PetWindow::currentFrame() const {
    if (m_look >= 0) return m_pet.frameRect(9 + m_look / 8, m_look % 8);
    return m_pet.frameRect(m_row, m_frame);
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
    if (hasPet() && !m_paused && isVisible() && m_look < 0)
        m_animation.start(Durations[m_row][m_frame]);
}

void PetWindow::advance() {
    if (++m_frame >= FrameCounts[m_row]) {
        m_frame = 0;
        if (m_returnToIdle) { m_row = 0; m_returnToIdle = false; }
    }
    refreshFrame();
    scheduleNext();
}

void PetWindow::play(int row, bool returnToIdle) {
    if (!hasPet() || row < 0 || row > 8 || m_paused) return;
    m_animation.stop();
    m_row = row;
    m_frame = 0;
    m_look = -1;
    m_returnToIdle = returnToIdle;
    refreshFrame();
    scheduleNext();
}

void PetWindow::checkGaze() {
    if (!hasGaze() || m_paused || m_pressed || m_row != 0) return;
    const QPoint cursor = QCursor::pos();
    if (cursor == m_lastCursor) return;
    m_lastCursor = cursor;
    const QPointF delta = cursor - (pos() + QPoint(width() / 2, qRound(height() * 0.45)));
    const double distance = std::hypot(delta.x(), delta.y());
    int look = -1;
    if (distance > 24 && distance < 360) {
        const double angle = std::atan2(delta.x(), -delta.y());
        look = (qRound(angle / (2 * std::acos(-1.0)) * 16) + 16) % 16;
    }
    if (look == m_look) return;
    m_look = look;
    m_animation.stop();
    refreshFrame();
    if (look < 0) scheduleNext();
}

void PetWindow::mousePressEvent(QMouseEvent *event) {
    qInfo("pet mouse press: %d", static_cast<int>(event->button()));
    if (event->button() != Qt::LeftButton) return;
    m_pressed = true;
    m_dragging = false;
    m_pressGlobal = event->globalPosition().toPoint();
    m_pressOrigin = pos();
}

void PetWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!m_pressed) return;
    const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
    if (!m_dragging && delta.manhattanLength() < QApplication::startDragDistance()) return;
    m_dragging = true;
    setCursor(Qt::ClosedHandCursor);
    const int row = delta.x() >= 0 ? 1 : 2;
    if (m_row != row) play(row, false);
    move(m_pressOrigin + delta);
}

void PetWindow::mouseReleaseEvent(QMouseEvent *event) {
    qInfo("pet mouse release: %d", static_cast<int>(event->button()));
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
        play(0, false);
        clampToScreen();
        if (positionSaved) positionSaved(pos());
    } else play(3);
}

void PetWindow::clampToScreen() {
    auto *screen = QGuiApplication::screenAt(geometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const auto bounds = screen->availableGeometry();
    move(std::clamp(x(), bounds.left(), std::max(bounds.left(), bounds.right() - width() + 1)),
         std::clamp(y(), bounds.top(), std::max(bounds.top(), bounds.bottom() - height() + 1)));
}

void PetWindow::showEvent(QShowEvent *) {
    scheduleNext();
    if (m_gazeEnabled && !m_paused) m_gaze.start();
}

void PetWindow::hideEvent(QHideEvent *) {
    m_animation.stop();
    m_gaze.stop();
}
}
