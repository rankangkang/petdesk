#include "behavior.h"
#include "petformat.h"
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace petdesk {
namespace {
const std::array<QVector<int>, 9> Durations = {{
    {3200, 130, 160, 200, 180, 1630}, // A calm blink/breath loop, 5.5 seconds.
    {120, 120, 120, 120, 120, 120, 120, 220},
    {120, 120, 120, 120, 120, 120, 120, 220},
    {140, 140, 140, 280}, {140, 140, 140, 140, 280},
    {140, 140, 140, 140, 140, 140, 140, 240},
    {150, 150, 150, 150, 150, 260}, {120, 120, 120, 120, 120, 220},
    {150, 150, 150, 150, 150, 280}
}};
bool walking(Activity activity) {
    return activity == Activity::WalkRight || activity == Activity::WalkLeft;
}
}

QString actionName(Action action) {
    static const QStringList names = {QStringLiteral("待机"), QStringLiteral("向右走"),
        QStringLiteral("向左走"), QStringLiteral("挥手"), QStringLiteral("跳跃"),
        QStringLiteral("沮丧"), QStringLiteral("等待"), QStringLiteral("专注"), QStringLiteral("观察")};
    return names.value(static_cast<int>(action));
}

QString activityName(Activity activity) {
    static const QStringList names = {QStringLiteral("打招呼"), QStringLiteral("蹦一蹦"),
        QStringLiteral("等一等"), QStringLiteral("认真琢磨"), QStringLiteral("东看看"),
        QStringLiteral("再试一次"), QStringLiteral("向右散步"), QStringLiteral("向左散步")};
    return names.value(static_cast<int>(activity));
}

QVector<AnimationStep> activitySteps(Activity activity) {
    using A = Action;
    switch (activity) {
    case Activity::Greet: return {{A::Wave, 2}};
    case Activity::Jump: return {{A::Jump, 2}};
    case Activity::Wait: return {{A::Waiting, 2}, {A::Wave, 1}};
    case Activity::Think: return {{A::Waiting, 1}, {A::Working, 3}, {A::Review, 2}, {A::Jump, 1}};
    case Activity::Inspect: return {{A::Review, 2}, {A::Waiting, 1}, {A::Wave, 1}};
    case Activity::Recover: return {{A::Working, 1}, {A::Failed, 1}, {A::Review, 1}, {A::Jump, 1}};
    case Activity::WalkRight: return {{A::Right, 2, 112}, {A::Review, 1}};
    case Activity::WalkLeft: return {{A::Left, 2, -112}, {A::Review, 1}};
    default: return {};
    }
}

int frameDurationMs(Action action, int frame) {
    const int row = static_cast<int>(action);
    if (row < 0 || row >= int(Durations.size())) return 1000;
    return Durations[row].value(frame, 1000);
}

int cycleDurationMs(Action action) {
    const int row = static_cast<int>(action);
    if (row < 0 || row >= int(Durations.size())) return 0;
    int duration = 0;
    for (int value : Durations[row]) duration += value;
    return duration;
}

int gazeDirection(QPointF delta, double scale) {
    const double distance = std::hypot(delta.x(), delta.y());
    if (distance < 24 * scale || distance > 480 * scale) return -1;
    const double angle = std::atan2(delta.x(), -delta.y());
    return (qRound(angle / (2 * std::acos(-1.0)) * 16) + 16) % 16;
}

QPointF boundedWalkTarget(QPointF origin, int distance, double scale, const QRectF &bounds) {
    return {std::clamp(origin.x() + distance * scale, bounds.left(), bounds.right()),
            std::clamp(origin.y(), bounds.top(), bounds.bottom())};
}

bool Playback::start(QVector<AnimationStep> steps, Trigger trigger, QString name) {
    if (steps.isEmpty() || trigger == Trigger::Idle ||
        (trigger == Trigger::Automatic && !idle()) ||
        static_cast<int>(trigger) < static_cast<int>(m_trigger))
        return false;
    for (const auto &step : steps) {
        const int row = static_cast<int>(step.action);
        if (row < 0 || row > 8 || step.cycles < 0 || step.cycles > 8 ||
            (step.cycles == 0 && trigger != Trigger::Drag) || std::abs(step.distance) > 256 ||
            (step.distance != 0 && step.action != Action::Right && step.action != Action::Left) ||
            (step.action == Action::Right && step.distance < 0) ||
            (step.action == Action::Left && step.distance > 0) ||
            (trigger == Trigger::Drag && (steps.size() != 1 || step.cycles != 0 ||
                step.distance != 0 || (step.action != Action::Left && step.action != Action::Right)))) return false;
    }
    m_steps = std::move(steps);
    m_index = 0;
    m_trigger = trigger;
    m_name = std::move(name);
    enterStep();
    return true;
}

void Playback::enterStep() {
    m_step = m_steps[m_index];
    m_frame = 0;
    m_remaining = m_step.cycles;
    ++m_serial;
}

void Playback::stop() {
    m_steps.clear();
    m_step = {Action::Idle, 0, 0};
    m_index = m_frame = m_remaining = 0;
    m_trigger = Trigger::Idle;
    m_name.clear();
    ++m_serial;
}

void Playback::advance() {
    if (++m_frame < FrameCounts[row()]) return;
    m_frame = 0;
    if (idle() || m_step.cycles == 0 || --m_remaining > 0) return;
    if (++m_index < m_steps.size()) enterStep();
    else stop();
}

int Playback::stepDurationMs() const {
    return cycleDurationMs(m_step.action) * m_step.cycles;
}

std::optional<Activity> ActivityPolicy::choose(qint64 now, double random, bool allowWalk) const {
    QVector<Activity> candidates;
    for (int i = 0; i < static_cast<int>(Activity::Count); ++i) {
        const auto activity = static_cast<Activity>(i);
        if ((!allowWalk && walking(activity)) || m_previous == activity ||
            (m_previous && walking(*m_previous) && walking(activity))) continue;
        if (m_lastPlayed[i] && now - *m_lastPlayed[i] < 60'000) continue;
        candidates.append(activity);
    }
    if (candidates.isEmpty()) return std::nullopt;
    const int index = std::min(int(candidates.size()) - 1,
        int(std::clamp(std::isfinite(random) ? random : 0.0, 0.0, 1.0) * candidates.size()));
    return candidates[index];
}

void ActivityPolicy::didPlay(Activity activity, qint64 now) {
    const int index = static_cast<int>(activity);
    if (index < 0 || index >= int(m_lastPlayed.size())) return;
    m_previous = activity;
    m_lastPlayed[index] = now;
    if (walking(activity)) {
        // Treat both directions as one cooldown, including a turn at a screen edge.
        m_lastPlayed[static_cast<int>(Activity::WalkLeft)] = now;
        m_lastPlayed[static_cast<int>(Activity::WalkRight)] = now;
    }
}
}
