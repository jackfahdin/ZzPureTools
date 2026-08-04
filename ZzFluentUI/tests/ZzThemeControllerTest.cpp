#include <QtCore/QtGlobal>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/**
 * @brief 验证主题控制器完整快照交换和变更分类语义。
 */
class ZzThemeControllerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void swapsCompleteSnapshots()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(
            &controller,
            &ZzFluentUI::ZzThemeController::snapshotChanged);
        const auto beforeSnapshot = controller.snapshot();
        const quint64 before = beforeSnapshot->revision();

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);

        QCOMPARE(controller.mode(), ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(
            controller.resolvedMode(),
            ZzFluentUI::ZzThemeMode::Dark);
        QVERIFY(controller.snapshot() != beforeSnapshot);
        QCOMPARE(beforeSnapshot->revision(), before);
        QCOMPARE(controller.snapshot()->revision(), before + 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toULongLong(), before + 1);
        const auto changes = spy.at(0)
                                 .at(1)
                                 .value<ZzFluentUI::ZzThemeChangeKinds>();
        QVERIFY(changes.testFlag(
            ZzFluentUI::ZzThemeChangeKind::Colors));
    }

    void reportsAccessibilityChanges()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(
            &controller,
            &ZzFluentUI::ZzThemeController::snapshotChanged);

        controller.setReducedMotion(true);

        QVERIFY(controller.reducedMotion());
        QCOMPARE(
            controller.snapshot()->duration(
                ZzFluentUI::ZzMotionToken::Normal),
            0);
        const auto changes = spy.at(0)
                                 .at(1)
                                 .value<ZzFluentUI::ZzThemeChangeKinds>();
        QVERIFY(changes.testFlag(
            ZzFluentUI::ZzThemeChangeKind::Motion));
        QVERIFY(changes.testFlag(
            ZzFluentUI::ZzThemeChangeKind::Accessibility));
    }

    void ignoresEquivalentAssignments()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(
            &controller,
            &ZzFluentUI::ZzThemeController::snapshotChanged);

        controller.setAccentColor(controller.accentColor());
        controller.setMode(controller.mode());
        controller.setReducedMotion(controller.reducedMotion());

        QCOMPARE(spy.count(), 0);
    }

    void reportsApplicationFontGeometryChanges()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(
            &controller,
            &ZzFluentUI::ZzThemeController::snapshotChanged);
        const QFont bodyBefore = controller.snapshot()->font(
            ZzFluentUI::ZzTypographyToken::Body);
        const QFont original = QGuiApplication::font();
        QFont changed = original;
        if (changed.pointSizeF() > 0.0) {
            changed.setPointSizeF(changed.pointSizeF() + 1.0);
        } else {
            changed.setPixelSize(qMax(1, changed.pixelSize()) + 1);
        }

        QGuiApplication::setFont(changed);
        QTRY_COMPARE(spy.count(), 1);
        const auto changes = spy.at(0)
                                 .at(1)
                                 .value<ZzFluentUI::ZzThemeChangeKinds>();
        QVERIFY(changes.testFlag(
            ZzFluentUI::ZzThemeChangeKind::Geometry));
        const QFont bodyAfter = controller.snapshot()->font(
            ZzFluentUI::ZzTypographyToken::Body);
        QVERIFY(bodyAfter != bodyBefore);

        QGuiApplication::setFont(original);
    }
};

QTEST_MAIN(ZzThemeControllerTest)

#include "ZzThemeControllerTest.moc"
