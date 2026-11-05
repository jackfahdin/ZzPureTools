#include <cstring>
#include <functional>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPointer>
#include <QtCore/QTranslator>
#include <QtGui/QAccessible>
#include <QtGui/QMouseEvent>
#include <QtGui/QScreen>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzTeachingTip.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

/** @brief 为教学提示关闭按钮提供确定的 LanguageChange 翻译。 */
class ZzTeachingTipTranslator final : public QTranslator
{
public:
    /** @brief 只翻译关闭文本。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (sourceText != nullptr
            && std::strcmp(sourceText, "关闭") == 0) {
            return QStringLiteral("Translated close");
        }
        return {};
    }
};

/** @brief 在内容重挂父对象期间模拟应用同步回调。 */
class ZzTeachingTipReparentContent final : public QWidget
{
public:
    std::function<void()> onParentChange;
    std::function<void()> onShow;

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Show && onShow) {
            const auto callback = onShow;
            callback();
            return true;
        }
        if (event->type() == QEvent::ParentChange && onParentChange) {
            const auto callback = onParentChange;
            callback();
            return true;
        }
        return QWidget::event(event);
    }
};

/** @brief 验证教学提示定位、生命周期、输入、主题和对象上限契约。 */
class ZzTeachingTipTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 把宿主放到当前屏幕中央并显示固定目标。 */
    static QPushButton *showCenteredTarget(QWidget *host)
    {
        QScreen *screen = QApplication::primaryScreen();
        Q_ASSERT(screen != nullptr);
        const QRect available = screen->availableGeometry();
        host->setGeometry(
            available.center().x() - 260,
            available.center().y() - 210,
            520,
            420);
        auto *target = new QPushButton(QStringLiteral("Target"), host);
        target->setGeometry(220, 190, 80, 36);
        host->show();
        QCoreApplication::processEvents();
        return target;
    }

private Q_SLOTS:
    void parentShowDoesNotOpenTip()
    {
        QWidget host;
        ZzFluentUI::ZzTeachingTip tip(&host);
        tip.setTitle(QStringLiteral("Hidden until requested"));
        showCenteredTarget(&host);
        QVERIFY(!tip.isVisible());
    }

    void followsAncestorsAndDismissesClippedTarget()
    {
        QWidget host;
        showCenteredTarget(&host);
        QWidget viewport(&host);
        viewport.setGeometry(100, 80, 260, 220);
        QWidget content(&viewport);
        content.setGeometry(0, 0, 260, 600);
        QPushButton target(QStringLiteral("Nested"), &content);
        target.setGeometry(60, 80, 90, 32);
        viewport.show(); content.show(); target.show();
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setLightDismissEnabled(false);
        tip.setTitle(QStringLiteral("Nested target"));
        tip.setTargetWidget(&target);
        tip.showForTarget();
        QVERIFY(tip.isVisible());
        const QPoint before = tip.pos();
        content.move(0, -20);
        QCoreApplication::processEvents();
        QCOMPARE(tip.pos(), before + QPoint(0, -20));
        content.move(0, -180);
        QCoreApplication::processEvents();
        QVERIFY(!tip.isVisible());
        tip.showForTarget();
        QVERIFY(!tip.isVisible());
    }

    void restoresFocusAfterKeyboardDismiss()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        host.activateWindow();
        target->setFocus();
        QCoreApplication::processEvents();
        QVERIFY(target->hasFocus());
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTeachingTip tip(&host);
        tip.setStyle(&style);
        tip.setTargetWidget(target);
        auto *editor = new QLineEdit;
        tip.setContentWidget(editor);
        tip.showForTarget();
        tip.activateWindow();
        editor->setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor->hasFocus());
        QTest::keyClick(editor, Qt::Key_Escape);
        QCoreApplication::processEvents();
        QVERIFY(!tip.isVisible());
        QVERIFY(target->hasFocus());
    }

    void targetNotificationCanDeleteTip()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        auto *tip = new ZzFluentUI::ZzTeachingTip;
        const QPointer<ZzFluentUI::ZzTeachingTip> guard(tip);
        connect(tip, &ZzFluentUI::ZzTeachingTip::targetWidgetChanged, this,
            [tip] { delete tip; });
        tip->setTargetWidget(target);
        QVERIFY(guard.isNull());
    }

    void placementNotificationCanDeleteTip()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        auto *tip = new ZzFluentUI::ZzTeachingTip;
        const QPointer<ZzFluentUI::ZzTeachingTip> guard(tip);
        tip->setTargetWidget(target);
        connect(tip, &ZzFluentUI::ZzTeachingTip::effectivePlacementChanged, this,
            [tip] { delete tip; });
        tip->showForTarget();
        QVERIFY(guard.isNull());
    }

    void placementNotificationCanCancelOpening()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTargetWidget(target);
        connect(&tip, &ZzFluentUI::ZzTeachingTip::effectivePlacementChanged,
            &tip, &ZzFluentUI::ZzTeachingTip::dismiss);
        tip.showForTarget();
        QVERIFY(!tip.isVisible());
    }

    void replacingContentCanDeleteTip()
    {
        auto *tip = new ZzFluentUI::ZzTeachingTip;
        const QPointer<ZzFluentUI::ZzTeachingTip> guard(tip);
        auto *oldContent = new QWidget;
        tip->setContentWidget(oldContent);
        connect(oldContent, &QObject::destroyed, this, [tip] { delete tip; });
        auto replacement = std::make_unique<QWidget>();
        tip->setContentWidget(replacement.get());
        QVERIFY(guard.isNull());
        QVERIFY(!replacement->parent());
    }

    void contentReparentCanScheduleTipDeletion()
    {
        auto *tip = new ZzFluentUI::ZzTeachingTip;
        const QPointer<ZzFluentUI::ZzTeachingTip> guard(tip);
        auto *content = new ZzTeachingTipReparentContent;
        const QPointer<QWidget> contentGuard(content);
        // QWidget::setParent itself still accesses the receiver after ParentChange.
        content->onParentChange = [tip] { tip->deleteLater(); };
        tip->setContentWidget(content);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!guard);
        QVERIFY(!contentGuard);
    }

    void contentReparentPreservesNestedOwnershipChange()
    {
        ZzFluentUI::ZzTeachingTip tip;
        auto *content = new ZzTeachingTipReparentContent;
        QWidget *taken = nullptr;
        content->onParentChange = [&] {
            content->onParentChange = {};
            taken = tip.takeContentWidget();
        };
        tip.setContentWidget(content);
        QCOMPARE(taken, content);
        QVERIFY(!tip.contentWidget());
        QVERIFY(!taken->parent());
        delete taken;

        auto *oldContent = new ZzTeachingTipReparentContent;
        tip.setContentWidget(oldContent);
        auto *replacement = new QLabel(QStringLiteral("Replacement"));
        oldContent->onParentChange = [&] { tip.setContentWidget(replacement); };
        QCOMPARE(tip.takeContentWidget(), oldContent);
        QCOMPARE(tip.contentWidget(), replacement);
        QVERIFY(!replacement->parentWidget()->isHidden());
        delete oldContent;
    }

    void targetDestructionNotificationCanDeleteTip()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        auto *tip = new ZzFluentUI::ZzTeachingTip;
        const QPointer<ZzFluentUI::ZzTeachingTip> guard(tip);
        tip->setTargetWidget(target);
        tip->showForTarget();
        connect(tip, &ZzFluentUI::ZzTeachingTip::targetWidgetChanged, this,
            [tip] { delete tip; });
        delete target;
        QVERIFY(guard.isNull());
    }

    void outsideDismissCanDeleteEventReceiver()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        auto *outside = new QPushButton(QStringLiteral("Outside"), &host);
        outside->setGeometry(10, 10, 80, 32);
        outside->show();
        const QPointer<QPushButton> receiver(outside);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTargetWidget(target);
        tip.showForTarget();
        connect(&tip, &ZzFluentUI::ZzTeachingTip::dismissed, this,
            [outside] { delete outside; });
        const QPoint local(5, 5);
        QMouseEvent press(QEvent::MouseButtonPress, local,
            outside->mapToGlobal(local), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(outside, &press);
        QVERIFY(receiver.isNull());
        QVERIFY(!tip.isVisible());
    }

    void outsideDismissDoesNotRestorePreviousFocus()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        host.activateWindow();
        target->setFocus();
        QCoreApplication::processEvents();
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTargetWidget(target);
        auto *editor = new QLineEdit;
        tip.setContentWidget(editor);
        tip.showForTarget();
        tip.activateWindow();
        editor->setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor->hasFocus());
        int targetFocusChanges = 0;
        connect(qApp, &QApplication::focusChanged, &tip,
            [&](QWidget *, QWidget *now) { targetFocusChanges += now == target ? 1 : 0; });
        const QPoint local(10, 10);
        QMouseEvent press(QEvent::MouseButtonPress, local, host.mapToGlobal(local),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &press);
        QCOMPARE(targetFocusChanges, 0);
        QVERIFY(!tip.isVisible());
    }

    void outsideClickDuringClosingCancelsFocusRestoration()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        QLineEdit outside(&host);
        outside.setGeometry(10, 10, 100, 32);
        outside.show();
        host.activateWindow();
        target->setFocus();
        QCoreApplication::processEvents();
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTargetWidget(target);
        auto *editor = new QLineEdit;
        tip.setContentWidget(editor);
        tip.showForTarget();
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group);
        group->setCurrentTime(group->duration());
        tip.activateWindow();
        editor->setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor->hasFocus());
        tip.dismiss();
        const QPoint local(5, 5);
        QMouseEvent press(QEvent::MouseButtonPress, local, outside.mapToGlobal(local),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&outside, &press);
        host.activateWindow();
        outside.setFocus();
        QCoreApplication::processEvents();
        QVERIFY(outside.hasFocus());
        group->setCurrentTime(group->duration());
        QCoreApplication::processEvents();
        QVERIFY(!tip.isVisible());
        QVERIFY(outside.hasFocus());
    }

    void emitsOnlyForEffectivePropertyChanges()
    {
        ZzFluentUI::ZzTeachingTip tip;
        QSignalSpy titleSpy(&tip, &ZzFluentUI::ZzTeachingTip::titleChanged);
        QSignalSpy textSpy(&tip, &ZzFluentUI::ZzTeachingTip::textChanged);
        QSignalSpy preferredSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::preferredPlacementChanged);
        QSignalSpy lightDismissSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::lightDismissEnabledChanged);
        QSignalSpy actionTextSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::actionTextChanged);
        QSignalSpy actionEnabledSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::actionEnabledChanged);
        QSignalSpy actionVisibleSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::actionVisibleChanged);
        QSignalSpy closeVisibleSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::closeButtonVisibleChanged);

        tip.setTitle(QStringLiteral("New command"));
        tip.setTitle(QStringLiteral("New command"));
        tip.setText(QStringLiteral("Run the command from this toolbar."));
        tip.setText(QStringLiteral("Run the command from this toolbar."));
        tip.setPreferredPlacement(
            ZzFluentUI::ZzTeachingTipPlacement::Right);
        tip.setPreferredPlacement(
            ZzFluentUI::ZzTeachingTipPlacement::Right);
        tip.setLightDismissEnabled(false);
        tip.setLightDismissEnabled(false);
        tip.setActionText(QStringLiteral("Try it"));
        tip.setActionText(QStringLiteral("Try it"));
        tip.setActionEnabled(false);
        tip.setActionEnabled(false);
        tip.setActionVisible(true);
        tip.setActionVisible(true);
        tip.setCloseButtonVisible(false);
        tip.setCloseButtonVisible(false);

        QCOMPARE(titleSpy.count(), 1);
        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(preferredSpy.count(), 1);
        QCOMPARE(lightDismissSpy.count(), 1);
        QCOMPARE(actionTextSpy.count(), 1);
        QCOMPARE(actionEnabledSpy.count(), 1);
        QCOMPARE(actionVisibleSpy.count(), 1);
        QCOMPARE(closeVisibleSpy.count(), 1);
        QCOMPARE(
            tip.windowFlags() & Qt::WindowType_Mask,
            Qt::WindowFlags(Qt::Tool));
        QVERIFY(tip.windowFlags().testFlag(Qt::FramelessWindowHint));
    }

    void transfersContentOwnershipAndDisconnectsDuringDestruction()
    {
        QPointer<QWidget> ownedGuard;
        {
            ZzFluentUI::ZzTeachingTip tip;
            QSignalSpy contentSpy(
                &tip, &ZzFluentUI::ZzTeachingTip::contentWidgetChanged);
            auto *first = new QLabel(QStringLiteral("First"));
            QPointer<QWidget> firstGuard(first);
            tip.setContentWidget(first);
            tip.setContentWidget(first);
            QCOMPARE(contentSpy.count(), 1);
            auto secondOwner = std::make_unique<QLabel>(
                QStringLiteral("Second"));
            QLabel *const second = secondOwner.get();
            tip.setContentWidget(secondOwner.release());
            QVERIFY(firstGuard.isNull());
            QCOMPARE(contentSpy.count(), 2);
            std::unique_ptr<QWidget> taken(tip.takeContentWidget());
            QCOMPARE(taken.get(), second);
            QCOMPARE(taken->parentWidget(), nullptr);
            QCOMPARE(contentSpy.count(), 3);

            auto *owned = new QLabel(QStringLiteral("Owned"));
            ownedGuard = owned;
            tip.setContentWidget(owned);
        }
        QVERIFY(ownedGuard.isNull());
    }

    void positionsEveryDirectionAndTracksTargetMovement()
    {
        QWidget host;
        QPushButton *target = showCenteredTarget(&host);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTitle(QStringLiteral("Placement"));
        tip.setText(QStringLiteral("A compact deterministic teaching tip."));
        tip.setTargetWidget(target);

        for (const auto placement : {
                 ZzFluentUI::ZzTeachingTipPlacement::Bottom,
                 ZzFluentUI::ZzTeachingTipPlacement::Top,
                 ZzFluentUI::ZzTeachingTipPlacement::Right,
                 ZzFluentUI::ZzTeachingTipPlacement::Left}) {
            tip.setPreferredPlacement(placement);
            tip.showForTarget();
            QVERIFY(tip.isVisible());
            QCOMPARE(tip.effectivePlacement(), placement);
            QVERIFY(target->screen()->availableGeometry().contains(
                tip.frameGeometry()));
            tip.dismiss();
            QVERIFY(!tip.isVisible());
        }

        tip.setPreferredPlacement(
            ZzFluentUI::ZzTeachingTipPlacement::Auto);
        tip.showForTarget();
        QCOMPARE(
            tip.effectivePlacement(),
            ZzFluentUI::ZzTeachingTipPlacement::Bottom);
        const QPoint beforeMove = tip.pos();
        target->move(target->x() + 48, target->y() + 24);
        QCoreApplication::processEvents();
        QVERIFY(tip.pos() != beforeMove);
        QVERIFY(target->screen()->availableGeometry().contains(
            tip.frameGeometry()));
    }

    void fallsBackFromScreenEdgeAndDismissesWithTarget()
    {
        QScreen *screen = QApplication::primaryScreen();
        QVERIFY(screen != nullptr);
        const QRect available = screen->availableGeometry();
        QWidget host;
        host.setGeometry(
            available.left() + 100,
            available.bottom() - 150,
            420,
            140);
        auto *target = new QPushButton(QStringLiteral("Edge"), &host);
        target->setGeometry(170, 96, 80, 32);
        host.show();
        QCoreApplication::processEvents();

        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTitle(QStringLiteral("Boundary fallback"));
        tip.setText(QStringLiteral("Bottom does not fit, so top is used."));
        tip.setPreferredPlacement(
            ZzFluentUI::ZzTeachingTipPlacement::Bottom);
        tip.setTargetWidget(target);
        QSignalSpy dismissedSpy(&tip, &ZzFluentUI::ZzTeachingTip::dismissed);
        QSignalSpy targetSpy(
            &tip, &ZzFluentUI::ZzTeachingTip::targetWidgetChanged);

        tip.showForTarget();
        QVERIFY(tip.isVisible());
        QCOMPARE(
            tip.effectivePlacement(),
            ZzFluentUI::ZzTeachingTipPlacement::Top);
        QVERIFY(available.contains(tip.frameGeometry()));
        target->hide();
        QCoreApplication::processEvents();
        QVERIFY(!tip.isVisible());
        QCOMPARE(dismissedSpy.count(), 1);

        target->show();
        tip.showForTarget();
        QVERIFY(tip.isVisible());
        delete target;
        QCoreApplication::processEvents();
        QCOMPARE(tip.targetWidget(), nullptr);
        QVERIFY(!tip.isVisible());
        QCOMPARE(dismissedSpy.count(), 2);
        QCOMPARE(targetSpy.count(), 1);
    }

    void appliesLightDismissWithoutClosingForActionOrDeactivation()
    {
        QWidget host;
        QPushButton *target = showCenteredTarget(&host);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTitle(QStringLiteral("Try this command"));
        tip.setText(QStringLiteral("Action sends intent and stays open."));
        tip.setActionText(QStringLiteral("Try it"));
        tip.setActionVisible(true);
        tip.setTargetWidget(target);
        tip.showForTarget();
        QVERIFY(tip.isVisible());
        QSignalSpy actionSpy(&tip, &ZzFluentUI::ZzTeachingTip::actionTriggered);
        QSignalSpy dismissedSpy(&tip, &ZzFluentUI::ZzTeachingTip::dismissed);

        QTest::mouseClick(target, Qt::LeftButton);
        QVERIFY(tip.isVisible());
        auto *action = tip.findChild<ZzFluentUI::ZzPushButton *>(
            QStringLiteral("zzTeachingTipActionButton"));
        QVERIFY(action != nullptr);
        QTest::mouseClick(action, Qt::LeftButton);
        QCOMPARE(actionSpy.count(), 1);
        QVERIFY(tip.isVisible());
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(&tip, &deactivate);
        QVERIFY(tip.isVisible());

        QTest::mouseClick(&host, Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
        QCoreApplication::processEvents();
        QVERIFY(!tip.isVisible());
        QCOMPARE(dismissedSpy.count(), 1);

        tip.setLightDismissEnabled(false);
        tip.showForTarget();
        QTest::mouseClick(&host, Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
        QCoreApplication::processEvents();
        QVERIFY(tip.isVisible());
        QTest::keyClick(&tip, Qt::Key_Escape);
        QVERIFY(!tip.isVisible());
        QCOMPARE(dismissedSpy.count(), 2);
    }

    void appliesThemeAccessibilityRtlAndBoundedAnimations()
    {
        QWidget host;
        QPushButton *target = showCenteredTarget(&host);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTitle(QStringLiteral("Inspect"));
        tip.setText(QStringLiteral("Inspect the generated artifact."));
        tip.setActionText(QStringLiteral("Open"));
        tip.setActionVisible(true);
        tip.setLayoutDirection(Qt::RightToLeft);
        tip.setTargetWidget(target);

        auto *title = tip.findChild<QLabel *>(
            QStringLiteral("zzTeachingTipTitle"));
        auto *body = tip.findChild<QLabel *>(
            QStringLiteral("zzTeachingTipText"));
        auto *action = tip.findChild<ZzFluentUI::ZzPushButton *>(
            QStringLiteral("zzTeachingTipActionButton"));
        auto *close = tip.findChild<QToolButton *>(
            QStringLiteral("zzTeachingTipCloseButton"));
        QVERIFY(title != nullptr);
        QVERIFY(body != nullptr);
        QVERIFY(action != nullptr);
        QVERIFY(close != nullptr);
        QCOMPARE(
            title->font(),
            style.themeSnapshot()->font(
                ZzFluentUI::ZzTypographyToken::Subtitle));
        QCOMPARE(
            body->font(),
            style.themeSnapshot()->font(
                ZzFluentUI::ZzTypographyToken::Body));
        QCOMPARE(action->font(), body->font());
        QCOMPARE(tip.layoutDirection(), Qt::RightToLeft);

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&tip);
        QVERIFY(interface != nullptr);
        QVERIFY(interface->role() != QAccessible::NoRole);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Inspect"));
        QCOMPARE(
            interface->text(QAccessible::Description),
            QStringLiteral("Inspect the generated artifact."));

        ZzTeachingTipTranslator translator;
        QCoreApplication::installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&tip, &languageChange);
        QCOMPARE(close->accessibleName(), QStringLiteral("Translated close"));
        QCOMPARE(close->toolTip(), QStringLiteral("Translated close"));
        QCoreApplication::removeTranslator(&translator);

        const qsizetype animationCount =
            tip.findChildren<QAbstractAnimation *>().size();
        QCOMPARE(animationCount, 3);
        for (int iteration = 0; iteration < 12; ++iteration) {
            tip.showForTarget();
            QVERIFY(tip.isVisible());
            tip.dismiss();
            QVERIFY(!tip.isVisible());
        }
        QCOMPARE(
            tip.findChildren<QAbstractAnimation *>().size(),
            animationCount);
    }

    void reversesRunningAnimationWithoutObjectGrowth()
    {
        QWidget host;
        QPushButton *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTitle(QStringLiteral("Animated"));
        tip.setText(QStringLiteral("Reuse the same transition objects."));
        tip.setTargetWidget(target);
        const qsizetype animationCount =
            tip.findChildren<QAbstractAnimation *>().size();
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group != nullptr);
        QSignalSpy dismissedSpy(&tip, &ZzFluentUI::ZzTeachingTip::dismissed);

        tip.showForTarget();
        QVERIFY(tip.isVisible());
        QCOMPARE(group->state(), QAbstractAnimation::Running);
        tip.dismiss();
        QCOMPARE(group->state(), QAbstractAnimation::Running);
        ZZ_VERIFY_EVENTUALLY(!tip.isVisible());
        QCOMPARE(dismissedSpy.count(), 1);

        tip.showForTarget();
        QVERIFY(tip.isVisible());
        ZZ_COMPARE_EVENTUALLY(group->state(), QAbstractAnimation::Stopped);
        QCOMPARE(tip.windowOpacity(), 1.0);
        QCOMPARE(
            tip.findChildren<QAbstractAnimation *>().size(),
            animationCount);
        tip.dismiss();
        ZZ_VERIFY_EVENTUALLY(!tip.isVisible());
        QCOMPARE(dismissedSpy.count(), 2);
    }

    void opensFromTargetEdge_data()
    {
        QTest::addColumn<ZzFluentUI::ZzTeachingTipPlacement>("placement");
        QTest::newRow("top") << ZzFluentUI::ZzTeachingTipPlacement::Top;
        QTest::newRow("bottom") << ZzFluentUI::ZzTeachingTipPlacement::Bottom;
        QTest::newRow("left") << ZzFluentUI::ZzTeachingTipPlacement::Left;
        QTest::newRow("right") << ZzFluentUI::ZzTeachingTipPlacement::Right;
    }

    void opensFromTargetEdge()
    {
        QFETCH(ZzFluentUI::ZzTeachingTipPlacement, placement);
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTitle(QStringLiteral("Scale"));
        tip.setTargetWidget(target);
        tip.setPreferredPlacement(placement);
        auto *content = new QLabel(QStringLiteral("Live content"));
        tip.setContentWidget(content);
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group);
        tip.showForTarget();
        QCOMPARE(tip.effectivePlacement(), placement);
        const QSize finalSize = tip.size();
        const auto opaqueBounds = [](const QImage &image) {
            QRect result;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (qAlpha(image.pixel(x, y)) > 32) {
                        result |= QRect(x, y, 1, 1);
                    }
                }
            }
            return result;
        };
        group->setCurrentTime(group->duration() / 4);
        const QImage intermediate = tip.grab().toImage();
        const QRect during = opaqueBounds(intermediate);
        group->setCurrentTime(group->duration());
        const QImage completed = tip.grab().toImage();
        const QRect after = opaqueBounds(completed);
        QVERIFY(!during.isEmpty());
        QVERIFY(during.width() < after.width());
        QVERIFY(during.height() < after.height());
        QCOMPARE(tip.size(), finalSize);
        QVERIFY(content->isVisible());
        QVERIFY(!content->parentWidget()->sizePolicy().retainSizeWhenHidden());
        switch (placement) {
        case ZzFluentUI::ZzTeachingTipPlacement::Top:
            QVERIFY(during.center().y() > after.center().y());
            break;
        case ZzFluentUI::ZzTeachingTipPlacement::Bottom:
            QVERIFY(during.center().y() < after.center().y());
            break;
        case ZzFluentUI::ZzTeachingTipPlacement::Left:
            QVERIFY(during.center().x() > after.center().x());
            break;
        case ZzFluentUI::ZzTeachingTipPlacement::Right:
            QVERIFY(during.center().x() < after.center().x());
            break;
        case ZzFluentUI::ZzTeachingTipPlacement::Auto:
            QFAIL("Expected an explicit placement");
        }
    }

    void interruptedOpeningRestoresLiveContent()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTargetWidget(target);
        auto *content = new QLineEdit;
        tip.setContentWidget(content);
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group);
        tip.showForTarget();
        group->setCurrentTime(group->duration() / 4);
        QVERIFY(!content->isVisible());
        tip.setText(QStringLiteral("Content changed while opening"));
        QVERIFY(content->isVisible());
        QCOMPARE(group->state(), QAbstractAnimation::Stopped);
        QCOMPARE(tip.windowOpacity(), 1.0);
        tip.hide();
        tip.showForTarget();
        tip.dismiss();
        tip.showForTarget();
        group->setCurrentTime(group->duration());
        QVERIFY(content->isVisible());
        tip.hide();
        QVERIFY(!content->parentWidget()->isHidden());
        controller.setReducedMotion(true);
        tip.showForTarget();
        QVERIFY(content->isVisible());
        QCOMPARE(group->state(), QAbstractAnimation::Stopped);
        QCOMPARE(tip.findChildren<QAbstractAnimation *>().size(), 3);
    }

    void restoringContentCanDeleteIncomingWidget()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTargetWidget(target);
        auto *content = new ZzTeachingTipReparentContent;
        tip.setContentWidget(content);
        tip.showForTarget();
        auto *incoming = new QLabel(QStringLiteral("Incoming"));
        const QPointer<QWidget> incomingGuard(incoming);
        content->onShow = [content, incoming] {
            content->onShow = {};
            delete incoming;
        };
        tip.setContentWidget(incoming);
        QVERIFY(!incomingGuard);
        QCOMPARE(tip.contentWidget(), content);
        QVERIFY(content->isVisible());
    }

    void restoringContentCanReopenAndChangeVisibility()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTargetWidget(target);
        tip.setTitle(QStringLiteral("Title"));
        tip.setText(QStringLiteral("Text"));
        tip.setActionVisible(true);
        auto *content = new ZzTeachingTipReparentContent;
        tip.setContentWidget(content);
        tip.showForTarget();
        content->onShow = [&tip, content] {
            content->onShow = {};
            tip.showForTarget();
            tip.setActionVisible(false);
        };
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group);
        group->setCurrentTime(group->duration());
        QCOMPARE(group->state(), QAbstractAnimation::Stopped);
        QVERIFY(tip.isVisible());
        QVERIFY(content->isVisible());
        QVERIFY(tip.findChild<QWidget *>(QStringLiteral("zzTeachingTipHeader"))->isVisible());
        QVERIFY(tip.findChild<QWidget *>(QStringLiteral("zzTeachingTipText"))->isVisible());
        QVERIFY(!tip.findChild<QWidget *>(QStringLiteral("zzTeachingTipActionButton"))->isVisible());
        QVERIFY(!content->parentWidget()->sizePolicy().retainSizeWhenHidden());
    }

    void contentDestructionDuringOpeningClearsSnapshot()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTargetWidget(target);
        auto *content = new QLabel(QStringLiteral("Old content"));
        tip.setContentWidget(content);
        tip.showForTarget();
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group);
        QCOMPARE(group->state(), QAbstractAnimation::Running);
        delete content;
        QVERIFY(!tip.contentWidget());
        QCOMPARE(group->state(), QAbstractAnimation::Stopped);
        QVERIFY(!tip.findChild<QWidget *>(QStringLiteral("zzTeachingTipContentHost"))->isVisible());
        QVERIFY(tip.findChild<QWidget *>(QStringLiteral("zzTeachingTipHeader"))->isVisible());
    }

    void snapshotRestorationPreservesNestedTake()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setTargetWidget(target);
        auto *content = new ZzTeachingTipReparentContent;
        tip.setContentWidget(content);
        tip.showForTarget();
        QWidget *taken = nullptr;
        content->onShow = [&] {
            content->onShow = {};
            taken = tip.takeContentWidget();
        };
        auto incoming = std::make_unique<QWidget>();
        tip.setContentWidget(incoming.get());
        QCOMPARE(taken, content);
        QVERIFY(!tip.contentWidget());
        QVERIFY(!incoming->parent());
        QVERIFY(!taken->parent());
        delete taken;
    }

    void changingThemeOrTargetSettlesOpening()
    {
        QWidget host;
        auto *target = showCenteredTarget(&host);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTeachingTip tip;
        tip.setStyle(&style);
        tip.setTargetWidget(target);
        auto *content = new QLabel(QStringLiteral("Content"));
        tip.setContentWidget(content);
        tip.showForTarget();
        controller.setReducedMotion(true);
        auto *group = tip.findChild<QParallelAnimationGroup *>();
        QVERIFY(group);
        QCOMPARE(group->state(), QAbstractAnimation::Stopped);
        QVERIFY(content->isVisible());
        tip.hide();
        controller.setReducedMotion(false);
        tip.showForTarget();
        target->move(target->pos() + QPoint(0, 12));
        QCOMPARE(group->state(), QAbstractAnimation::Stopped);
        QVERIFY(content->isVisible());
    }
};

QTEST_MAIN(ZzTeachingTipTest)

#include "ZzTeachingTipTest.moc"
