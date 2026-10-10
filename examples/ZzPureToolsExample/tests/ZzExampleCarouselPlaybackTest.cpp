#include <ZzTestEventLoop.h>
#include "../ZzExampleCarouselPlayback.h"

#include <QApplication>
#include <QEnterEvent>
#include <QProxyStyle>
#include <QStandardItemModel>
#include <QTest>

using ZzExample::ZzExampleCarouselPlayback;
using ZzFluentUI::ZzCarouselView;

namespace {
void preventAutomaticFocus(ZzCarouselView& view)
{
    view.setFocusPolicy(Qt::NoFocus);
    view.setShowNavigationButtons(false);
}

class PlaybackStyle final : public QProxyStyle {
public:
    bool animate = true;
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr, QStyleHintReturn* data = nullptr) const override
    {
        return hint == SH_Widget_Animate ? int(animate)
                                         : QProxyStyle::styleHint(hint, option, widget, data);
    }
};
}

class ZzExampleCarouselPlaybackTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void playsWithHoverNavigationEnabled()
    {
        QStandardItemModel model(3, 1);
        ZzCarouselView view;
        view.setImmersive(true);
        view.setNavigationButtonTrigger(ZzCarouselView::OnHover);
        view.setWrapAroundEnabled(true);
        view.setFocusPolicy(Qt::NoFocus);
        view.setAnimationDuration(0);
        view.setModel(&model);
        view.show();
        ZzExampleCarouselPlayback playback(&view, &view);
        playback.setPauseOnHover(false);
        playback.setInterval(100);
        playback.setEnabled(true);
        QVERIFY(view.showNavigationButtons());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(view.currentRow() != 0, 500);
    }

    void advancesVisibleViewAndStopsAtBoundary()
    {
        QStandardItemModel model(3, 1);
        ZzCarouselView view;
        preventAutomaticFocus(view);
        view.setAnimationDuration(0);
        view.setModel(&model);
        view.show();
        ZzExampleCarouselPlayback playback(&view, &view);
        playback.setPauseOnHover(false);
        playback.setInterval(100);
        playback.setEnabled(true);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(view.currentRow(), 2, 700);
        QTest::qWait(180);
        QCOMPARE(view.currentRow(), 2);
    }

    void hiddenDisabledAndHoveredViewsPause()
    {
        QStandardItemModel model(3, 1);
        ZzCarouselView view;
        preventAutomaticFocus(view);
        view.setAnimationDuration(0);
        view.setWrapAroundEnabled(true);
        view.setModel(&model);
        ZzExampleCarouselPlayback playback(&view, &view);
        playback.setInterval(100);
        playback.setEnabled(true);
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), 0);
        view.show();
        QEnterEvent enter({}, {}, {});
        QApplication::sendEvent(&view, &enter);
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), 0);
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&view, &leave);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(view.currentRow() != 0, 500);
        view.setEnabled(false);
        const int paused = view.currentRow();
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), paused);
        view.setEnabled(true);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(view.currentRow() != paused, 500);
        view.hide();
        const int hidden = view.currentRow();
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), hidden);
    }

    void manualNavigationRestartsInterval()
    {
        QStandardItemModel model(4, 1);
        ZzCarouselView view;
        preventAutomaticFocus(view);
        view.setAnimationDuration(0);
        view.setModel(&model);
        view.show();
        ZzExampleCarouselPlayback playback(&view, &view);
        playback.setPauseOnHover(false);
        playback.setInterval(300);
        playback.setEnabled(true);
        QTest::qWait(200);
        view.setCurrentRow(1);
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), 1);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(view.currentRow(), 2, 300);
    }

    void ignoredHoverDoesNotRestartPlayback()
    {
        QStandardItemModel model(3, 1);
        ZzCarouselView view;
        preventAutomaticFocus(view);
        view.setAnimationDuration(0);
        view.setModel(&model);
        view.show();
        ZzExampleCarouselPlayback playback(&view, &view);
        playback.setPauseOnHover(false);
        playback.setInterval(600);
        playback.setEnabled(true);
        QTest::qWait(300);
        QEnterEvent enter({}, {}, {});
        QApplication::sendEvent(&view, &enter);
        // 悬停被忽略时计时器不重启：前进必须沿原始计划（启用后 600ms）
        // 发生，早于悬停重启计划（悬停后 600ms）的截止点；慢速 runner
        // 上定时器投递可能延迟，用轮询等待替代固定窗口后的精确比较。
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(view.currentRow(), 1, 550);
    }

    void timeoutCallbackMayDeleteViewAndController()
    {
        QStandardItemModel model(3, 1);
        QPointer<ZzCarouselView> view = new ZzCarouselView;
        preventAutomaticFocus(*view);
        view->setAnimationDuration(0);
        view->setModel(&model);
        view->show();
        auto* playback = new ZzExampleCarouselPlayback(view, view);
        playback->setPauseOnHover(false);
        playback->setInterval(100);
        connect(view, &ZzCarouselView::currentRowChanged, view, [view](int row) {
            if (row == 1)
                delete view.data();
        });
        playback->setEnabled(true);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(view.isNull(), 500);
    }

    void focusAndReducedMotionPausePlayback()
    {
        PlaybackStyle style;
        QStandardItemModel model(4, 1);
        ZzCarouselView view;
        preventAutomaticFocus(view);
        view.setStyle(&style);
        view.setAnimationDuration(0);
        view.setModel(&model);
        view.show();
        QTest::qWait(20);
        ZzExampleCarouselPlayback playback(&view, &view);
        playback.setPauseOnHover(false);
        playback.setInterval(100);
        view.setFocus();
        QVERIFY(view.hasFocus());
        playback.setEnabled(true);
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), 0);
        view.clearFocus();
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(view.currentRow(), 1, 500);
        style.animate = false;
        QEvent styleChange(QEvent::StyleChange);
        QApplication::sendEvent(&view, &styleChange);
        QTest::qWait(160);
        QCOMPARE(view.currentRow(), 1);
        style.animate = true;
        QApplication::sendEvent(&view, &styleChange);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(view.currentRow(), 2, 500);
    }
};
QTEST_MAIN(ZzExampleCarouselPlaybackTest)
#include "ZzExampleCarouselPlaybackTest.moc"
