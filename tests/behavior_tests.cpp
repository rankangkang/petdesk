#include "behavior.h"
#include "petformat.h"
#include <QSet>
#include <QtTest>
#include <cmath>

using namespace petdesk;
namespace {
QVector<int> playToIdle(Playback &playback) {
    QVector<int> rows;
    for (int frame = 0; frame < 1000 && !playback.idle(); ++frame) {
        if (rows.isEmpty() || rows.last() != playback.row()) rows.append(playback.row());
        playback.advance();
    }
    if (playback.idle()) rows.append(0);
    return rows;
}
}

class BehaviorTests : public QObject {
    Q_OBJECT
private slots:
    void focusedActivityFinishesInOrder() {
        Playback playback;
        QVERIFY(playback.start(activitySteps(Activity::Think), Trigger::User, "think"));
        QCOMPARE(playToIdle(playback), QVector<int>({6, 7, 8, 4, 0}));
        QCOMPARE(playback.frame(), 0);
        QCOMPARE(playback.trigger(), Trigger::Idle);
    }
    void allAuthoredRowsAndFramesAreUsedWithoutPadding() {
        QSet<int> visited;
        Playback playback;
        for (int i = 0; i < FrameCounts[0]; ++i) {
            visited.insert(playback.row() * 8 + playback.frame());
            playback.advance();
        }
        for (int id = 0; id < static_cast<int>(Activity::Count); ++id) {
            QVERIFY(playback.start(activitySteps(static_cast<Activity>(id)), Trigger::User, "activity"));
            int ticks = 0;
            while (!playback.idle() && ticks++ < 1000) {
                QVERIFY(playback.frame() < FrameCounts[playback.row()]);
                visited.insert(playback.row() * 8 + playback.frame());
                playback.advance();
            }
            QVERIFY(playback.idle());
        }
        QCOMPARE(visited.size(), 57);
        QVERIFY(!visited.contains(6)); // Original row 0, column 6 stays unused.
        QCOMPARE(cycleDurationMs(Action::Idle), 5500);
    }
    void explicitInputPreemptsAutomaticButAutomaticCannotPreemptInput() {
        Playback playback;
        QVERIFY(playback.start(activitySteps(Activity::Think), Trigger::Automatic, "auto"));
        QVERIFY(!playback.start(activitySteps(Activity::Recover), Trigger::Automatic, "second auto"));
        QVERIFY(playback.start(activitySteps(Activity::Greet), Trigger::User, "click"));
        QVERIFY(!playback.start(activitySteps(Activity::Think), Trigger::Automatic, "auto"));
        QCOMPARE(playback.action(), Action::Wave);
        QCOMPARE(playToIdle(playback), QVector<int>({3, 0})); // Cancelled work never resumes.
    }
    void draggingIsExclusiveAndReleaseClearsItsPriority() {
        Playback playback;
        QVERIFY(playback.start({{Action::Left, 0}}, Trigger::Drag, "drag"));
        for (int i = 0; i < 200; ++i) playback.advance();
        QCOMPARE(playback.trigger(), Trigger::Drag);
        QVERIFY(!playback.start(activitySteps(Activity::Jump), Trigger::User, "double click"));
        QVERIFY(!playback.start(activitySteps(Activity::Greet), Trigger::Automatic, "auto"));
        QVERIFY(playback.start({{Action::Right, 0}}, Trigger::Drag, "reverse drag"));
        QCOMPARE(playback.frame(), 0);
        playback.stop();
        QVERIFY(playback.start(activitySteps(Activity::Jump), Trigger::User, "drop"));
        QCOMPARE(playToIdle(playback), QVector<int>({4, 0}));
    }
    void explicitReplacementDoesNotQueueOldActivity() {
        Playback playback;
        QVERIFY(playback.start(activitySteps(Activity::Think), Trigger::User, "think"));
        for (int i = 0; i < 10; ++i) playback.advance();
        QVERIFY(playback.start(activitySteps(Activity::Jump), Trigger::User, "jump"));
        QCOMPARE(playback.frame(), 0);
        QCOMPARE(playToIdle(playback), QVector<int>({4, 0}));
    }
    void invalidTimelineCannotReplaceCurrentState() {
        Playback playback;
        QVERIFY(playback.start(activitySteps(Activity::Greet), Trigger::User, "greet"));
        for (const auto &steps : QVector<QVector<AnimationStep>>{
            {}, {{static_cast<Action>(9), 1}}, {{Action::Wave, 0}}, {{Action::Wave, 9}},
            {{Action::Right, 1, -20}}, {{Action::Left, 1, 20}}, {{Action::Jump, 1, 20}}}) {
            QVERIFY(!playback.start(steps, Trigger::User, "invalid"));
            QCOMPARE(playback.action(), Action::Wave);
        }
        playback.stop();
        QCOMPARE(playback.action(), Action::Idle);
        QCOMPARE(playback.frame(), 0);
    }
    void automaticPolicyHasCooldownNoImmediateRepeatsAndOptionalWalking() {
        ActivityPolicy policy;
        QSet<int> visited;
        for (int i = 0; i < 6; ++i) {
            const auto activity = policy.choose(0, 0.0, false);
            QVERIFY(activity.has_value());
            QVERIFY(*activity != Activity::WalkLeft && *activity != Activity::WalkRight);
            QVERIFY(!visited.contains(static_cast<int>(*activity)));
            visited.insert(static_cast<int>(*activity));
            policy.didPlay(*activity, 0);
        }
        QVERIFY(!policy.choose(59'999, 0.0, false));
        QVERIFY(policy.choose(60'000, 1.0, false));
        const auto walk = policy.choose(0, 0.0, true);
        QVERIFY(walk == Activity::WalkRight || walk == Activity::WalkLeft);
        policy.didPlay(*walk, 0);
        QVERIFY(!policy.choose(0, 1.0, true)); // No immediate back-and-forth walk.
    }
    void allSixteenGazeDirectionsUseClockwiseViewerCoordinates() {
        for (int sector = 0; sector < 16; ++sector) {
            const double angle = sector * 2 * std::acos(-1.0) / 16;
            const QPointF cursor(150 * std::sin(angle), -150 * std::cos(angle));
            QCOMPARE(gazeDirection(cursor), sector);
            QCOMPARE(gazeDirection(cursor * 1.5, 1.5), sector);
            QCOMPARE(9 + sector / 8, sector < 8 ? 9 : 10);
        }
        QCOMPARE(gazeDirection({0, 0}), -1);
        QCOMPARE(gazeDirection({0, -23}), -1);
        QCOMPARE(gazeDirection({0, -600}), -1);
        QCOMPARE(gazeDirection({0, -100}), 0);
        QCOMPARE(gazeDirection({100, 0}), 4);
        QCOMPARE(gazeDirection({0, 100}), 8);
        QCOMPARE(gazeDirection({-100, 0}), 12);
    }
    void walksStayInsideOffsetAndSmallScreens() {
        const QRectF screen(-1920, 40, 1700, 800);
        QCOMPARE(boundedWalkTarget({-250, 100}, 112, 1, screen), QPointF(-220, 100));
        QCOMPARE(boundedWalkTarget({-1890, 100}, -112, 1, screen), QPointF(-1920, 100));
        QCOMPARE(boundedWalkTarget({0, 0}, 112, 2, QRectF(0, 0, 0, 0)), QPointF(0, 0));
        QCOMPARE(boundedWalkTarget({200, 300}, -112, 1.5, QRectF(0, 0, 1000, 700)), QPointF(32, 300));
    }
};

QTEST_GUILESS_MAIN(BehaviorTests)
#include "behavior_tests.moc"
