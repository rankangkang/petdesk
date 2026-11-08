#pragma once
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <array>
#include <optional>

namespace petdesk {
// These are the nine authored Codex rows. Working is focused effort, not locomotion.
enum class Action { Idle, Right, Left, Wave, Jump, Failed, Waiting, Working, Review };
enum class Trigger { Idle, Automatic, User, Drag };
enum class Activity { Greet, Jump, Wait, Think, Inspect, Recover, WalkRight, WalkLeft, Count };

struct AnimationStep {
    Action action = Action::Idle;
    int cycles = 1; // Zero is allowed only for a held drag animation.
    int distance = 0; // Horizontal logical pixels, before pet scaling.
};

QString actionName(Action action);
QString activityName(Activity activity);
QVector<AnimationStep> activitySteps(Activity activity);
int frameDurationMs(Action action, int frame);
int cycleDurationMs(Action action);
int gazeDirection(QPointF delta, double scale = 1.0);
QPointF boundedWalkTarget(QPointF origin, int distance, double scale, const QRectF &bounds);

// Pure, deterministic timeline. No timers, windows, scripts, services or agent state.
class Playback {
public:
    bool start(QVector<AnimationStep> steps, Trigger trigger, QString name);
    void stop();
    void advance();
    Action action() const { return m_step.action; }
    int row() const { return static_cast<int>(m_step.action); }
    int frame() const { return m_frame; }
    Trigger trigger() const { return m_trigger; }
    bool idle() const { return m_trigger == Trigger::Idle; }
    AnimationStep step() const { return m_step; }
    int stepDurationMs() const;
    quint64 serial() const { return m_serial; }
    QString name() const { return m_name; }
private:
    void enterStep();
    QVector<AnimationStep> m_steps;
    AnimationStep m_step{Action::Idle, 0, 0};
    int m_index = 0, m_frame = 0, m_remaining = 0;
    Trigger m_trigger = Trigger::Idle;
    QString m_name;
    quint64 m_serial = 0;
};

class ActivityPolicy {
public:
    std::optional<Activity> choose(qint64 now, double random, bool allowWalk) const;
    void didPlay(Activity activity, qint64 now);
private:
    std::array<std::optional<qint64>, static_cast<int>(Activity::Count)> m_lastPlayed{};
    std::optional<Activity> m_previous;
};
}
