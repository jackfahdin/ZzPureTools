#include "../widgets/src/private/ZzPageTransitionPrivate.h"
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtTest/QTest>
#include <functional>
#include <type_traits>

namespace {
using ZzPlayer = ZzFluentUI::ZzPageTransitionPrivate;
static_assert(!std::is_copy_constructible_v<ZzPlayer>);
static_assert(!std::is_move_constructible_v<ZzPlayer>);
static_assert(!std::is_copy_assignable_v<ZzPlayer>);

/** @brief 用真实 Paint 回调验证分阶段捕获的像素和取消。 */
class ZzTransitionPage final : public QWidget
{
public:
    explicit ZzTransitionPage(QColor color, QWidget *parent) : QWidget(parent), color_(color) {}
    std::function<void()> painted;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), color_);
        const auto callback = painted;
        if (callback) callback();
    }
private:
    QColor color_;
};

/** @brief 固定 200×100 逻辑像素的水平滑动。 */
ZzPlayer::Settings zzSettings(QWidget &owner)
{
    return {owner.rect(), owner.devicePixelRatioF(), Qt::white,
        ZzPlayer::Mode::SlideHorizontal, false, 200};
}
} // namespace

/** @brief 验证不拥有页面的播放器捕获、取消与生命周期。 */
class ZzPageTransitionPrivateTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void ownerDestructionReleasesPreparedImages()
    {
        auto owner = std::make_unique<QWidget>();
        owner->resize(200, 100);
        QList<bool> observations;
        ZzPlayer player(owner.get());
        auto *page = new ZzTransitionPage(Qt::red, owner.get());
        player.setCaptureObserver([&](bool entering) { observations.append(entering); });
        const auto token = player.beginCapture(zzSettings(*owner));
        QVERIFY(player.captureSource(token, page));
        QVERIFY(player.retainedBytes() > 0);
        owner.reset();
        QCOMPARE(player.retainedBytes(), 0);
        QCOMPARE(observations, QList<bool>({true, false}));
        QVERIFY(!player.play(token));
    }

    void cleanupRejectsReentrantCapture()
    {
        QWidget owner;
        owner.resize(200, 100);
        ZzPlayer player(&owner);
        ZzPlayer::Token nested = 999;
        player.setCaptureObserver([&](bool entering) {
            if (!entering) nested = player.beginCapture(zzSettings(owner));
        });
        QVERIFY(player.beginCapture(zzSettings(owner)) != 0);
        player.cancel();
        QCOMPARE(nested, 0);
        QCOMPARE(player.retainedBytes(), 0);
    }

    void sourceCanDieBeforePlayback()
    {
        QWidget owner;
        owner.resize(200, 100);
        owner.show();
        QCoreApplication::processEvents();
        ZzPlayer player(&owner);
        auto *red = new ZzTransitionPage(Qt::red, &owner);
        auto *blue = new ZzTransitionPage(Qt::blue, &owner);
        int finished = 0;
        player.setFinishedCallback([&] { ++finished; });
        const auto token = player.beginCapture(zzSettings(owner));
        QVERIFY(token != 0);
        QVERIFY(player.captureSource(token, red));
        QVERIFY(player.captureTarget(token, blue));
        delete red;
        QVERIFY(player.play(token));
        QVERIFY(player.running());
        auto *animation = owner.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        animation->pause();
        animation->setCurrentTime(100);
        const auto frame = owner.grab().toImage();
        const auto dpr = frame.devicePixelRatio();
        QCOMPARE(frame.pixelColor(qRound(5 * dpr), qRound(50 * dpr)), QColor(Qt::red));
        QCOMPARE(frame.pixelColor(qRound(195 * dpr), qRound(50 * dpr)), QColor(Qt::blue));
        animation->setCurrentTime(200);
        QCOMPARE(finished, 1);
        QCOMPARE(player.retainedBytes(), 0);
        QVERIFY(!player.running());
    }

    void cancelInvalidatesPreparedFrames()
    {
        QWidget owner;
        owner.resize(200, 100);
        ZzPlayer player(&owner);
        ZzTransitionPage red(Qt::red, &owner), blue(Qt::blue, &owner);
        const auto token = player.beginCapture(zzSettings(owner));
        QVERIFY(player.captureSource(token, &red));
        QVERIFY(player.captureTarget(token, &blue));
        QVERIFY(player.retainedBytes() > 0);
        player.cancel();
        QVERIFY(!player.play(token));
        QCOMPARE(player.retainedBytes(), 0);
        auto oversized = zzSettings(owner);
        oversized.contentRect = QRect(0, 0, 4096, 4096);
        oversized.dpr = 2;
        QCOMPARE(player.beginCapture(oversized), 0);
        QCOMPARE(player.retainedBytes(), 0);
    }

    void renderCancellationDiscardsBothImages()
    {
        QWidget owner;
        owner.resize(200, 100);
        ZzPlayer player(&owner);
        ZzTransitionPage red(Qt::red, &owner), blue(Qt::blue, &owner);
        QList<bool> observations;
        player.setCaptureObserver([&](bool entering) { observations.append(entering); });
        const auto token = player.beginCapture(zzSettings(owner));
        QVERIFY(player.captureSource(token, &red));
        blue.painted = [&] { player.cancel(); };
        QVERIFY(!player.captureTarget(token, &blue));
        QCOMPARE(player.retainedBytes(), 0);
        QVERIFY(!player.play(token));
        QCOMPARE(observations, QList<bool>({true, false}));
    }

    void completionMayDestroyOwnerAndPlayer()
    {
        auto *owner = new QWidget;
        owner->resize(200, 100);
        owner->show();
        QCoreApplication::processEvents();
        auto player = std::make_unique<ZzPlayer>(owner);
        auto *red = new ZzTransitionPage(Qt::red, owner);
        auto *blue = new ZzTransitionPage(Qt::blue, owner);
        const auto token = player->beginCapture(zzSettings(*owner));
        QVERIFY(player->captureSource(token, red));
        QVERIFY(player->captureTarget(token, blue));
        player->setFinishedCallback([&] { player.reset(); delete owner; });
        QVERIFY(player->play(token));
        QPointer<QWidget> guard(owner);
        auto *animation = owner->findChild<QVariantAnimation *>();
        animation->setCurrentTime(animation->duration());
        QVERIFY(!guard);
        QVERIFY(!player);
    }
};

QTEST_MAIN(ZzPageTransitionPrivateTest)
#include "ZzPageTransitionPrivateTest.moc"
