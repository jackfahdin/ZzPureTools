#include <cstring>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPointer>
#include <QtCore/QTranslator>
#include <QtGui/QAccessible>
#include <QtGui/QScreen>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
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
};

QTEST_MAIN(ZzTeachingTipTest)

#include "ZzTeachingTipTest.moc"
