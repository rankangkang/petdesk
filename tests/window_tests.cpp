#include "petwindow.h"
#include <QPainter>
#include <QtTest>

using namespace petdesk;
namespace {
PetData fixture() {
    PetData pet;
    pet.name = "test fixture";
    pet.version = 2;
    pet.atlas = QImage(1536, 2288, QImage::Format_ARGB32);
    pet.atlas.fill(Qt::transparent);
    QPainter painter(&pet.atlas);
    for (int row = 0; row < 11; ++row)
        for (int column = 0; column < FrameCounts[row]; ++column)
            painter.fillRect(column * FrameWidth + 60, row * FrameHeight + 60, 60, 100, Qt::blue);
    return pet;
}
void prepare(PetWindow &window) {
    window.setAutomaticEnabled(false);
    window.setGazeEnabled(false);
    window.setPetScale(1);
    window.setPet(fixture());
    window.move(200, 200);
    window.show();
}
}

class WindowTests : public QObject {
    Q_OBJECT
private slots:
    void hidingCancelsTheOldPlanRatherThanResumingIt() {
        PetWindow window;
        prepare(window);
        window.playActivity(Activity::Think);
        QVERIFY(window.currentActivity().startsWith(activityName(Activity::Think)));
        window.hide();
        QCOMPARE(window.currentActivity(), actionName(Action::Idle));
        window.playActivity(Activity::Jump); // Hidden pets do not accumulate work.
        window.show();
        QCOMPARE(window.currentActivity(), actionName(Action::Idle));
    }
    void pausingStopsAnActiveWalkAtItsCurrentPosition() {
        PetWindow window;
        prepare(window);
        window.playActivity(Activity::WalkLeft);
        QTest::qWait(160);
        QVERIFY(window.x() < 200);
        window.setPaused(true);
        const QPoint pausedPosition = window.pos();
        window.playActivity(Activity::Think);
        QTest::qWait(160);
        QCOMPARE(window.pos(), pausedPosition);
        QCOMPARE(window.currentActivity(), QStringLiteral("已暂停"));
        window.setPaused(false);
        QCOMPARE(window.currentActivity(), actionName(Action::Idle));
        QCOMPARE(window.pos(), pausedPosition);
    }
    void manualWalkMovesAndFinishesAtItsBoundedTarget() {
        PetWindow window;
        prepare(window);
        window.playActivity(Activity::WalkLeft);
        QTest::qWait(2400);
        QCOMPARE(window.pos(), QPoint(88, 200));
        QVERIFY(window.currentActivity().contains(actionName(Action::Review)));
        window.playActivity(Activity::Greet);
        const QPoint interruptedPosition = window.pos();
        QTest::qWait(100);
        QCOMPARE(window.pos(), interruptedPosition);
    }
    void managerSuspensionDoesNotBlockAnExplicitActivity() {
        PetWindow window;
        prepare(window);
        window.setAutomaticEnabled(true);
        window.setAutomaticSuspended(true);
        window.playActivity(Activity::Recover);
        QVERIFY(window.currentActivity().startsWith(activityName(Activity::Recover)));
        window.setAutomaticEnabled(false);
        QVERIFY(window.currentActivity().startsWith(activityName(Activity::Recover)));
    }
};

QTEST_MAIN(WindowTests)
#include "window_tests.moc"
