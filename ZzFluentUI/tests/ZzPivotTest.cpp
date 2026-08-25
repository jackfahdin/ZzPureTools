#include <algorithm>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QtMath>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QRegion>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyleOptionTab>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 记录 Pivot 实际交给平台样式的标签 option。 */
class ZzTabLabelRecordingStyle final : public QProxyStyle
{
public:
    explicit ZzTabLabelRecordingStyle(QStyle *baseStyle)
        : QProxyStyle(baseStyle)
    {
    }

    void drawControl(
        ControlElement element,
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget = nullptr) const override
    {
        if (element == CE_TabBarTabLabel) {
            if (const auto *tab = qstyleoption_cast<
                    const QStyleOptionTab *>(option)) {
                labelOptions.append(*tab);
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    mutable QList<QStyleOptionTab> labelOptions;
};

/** @brief 返回图像中精确颜色像素组成的物理像素区域。 */
QRegion zzPhysicalColorRegion(const QImage &image, const QColor &color)
{
    QRegion region;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != color) {
                continue;
            }
            region += QRect(QPoint(x, y), QSize(1, 1));
        }
    }
    return region;
}

/** @brief 把逻辑矩形外包到所有可能覆盖的物理像素。 */
QRect zzPhysicalCoverageRect(const QRect &logical, qreal devicePixelRatio)
{
    const int left = qFloor(logical.left() * devicePixelRatio);
    const int top = qFloor(logical.top() * devicePixelRatio);
    const int right = qCeil(
        (logical.right() + 1) * devicePixelRatio) - 1;
    const int bottom = qCeil(
        (logical.bottom() + 1) * devicePixelRatio) - 1;
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

/** @brief 检查胶囊指示条中央顶部直边没有物理混合像素。 */
bool zzHasCrispCentralEdges(
    const QImage &image,
    const QRect &indicator,
    const QColor &accent)
{
    if (indicator.width() < 3 || indicator.height() < 1) {
        return false;
    }
    const int centerX = indicator.center().x();
    const auto hasColor = [&image](const QPoint &point, const QColor &color) {
        return image.rect().contains(point)
            && image.pixelColor(point) == color;
    };
    const QPoint backgroundPoint(centerX, indicator.top() - 2);
    if (!image.rect().contains(backgroundPoint)) {
        return false;
    }
    const QColor background = image.pixelColor(backgroundPoint);
    return hasColor(QPoint(centerX, indicator.top()), accent)
        && hasColor(QPoint(centerX, indicator.top() - 1), background);
}

/** @brief 创建颜色唯一且无透明边缘的图标供像素安全区断言。 */
QIcon zzTestIcon(const QColor &color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(color);
    return QIcon(pixmap);
}

} // namespace

/** @brief 验证 Pivot 的项 API、原生输入语义、绘制和固定动画预算。 */
class ZzPivotTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 显示固定尺寸 Pivot 并完成布局。 */
    static void showPivot(
        ZzFluentUI::ZzPivot *pivot,
        int width = 520,
        int height = -1)
    {
        pivot->resize(
            width,
            height >= 0 ? height : pivot->sizeHint().height());
        pivot->show();
        QCoreApplication::processEvents();
    }

private Q_SLOTS:
    void delegatesIconItemsToTheNativeTabApi()
    {
        std::unique_ptr<QStyle> style(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(style != nullptr);
        ZzFluentUI::ZzPivot pivot;
        const QIcon computer = style->standardIcon(
            QStyle::SP_ComputerIcon);
        const QIcon directory = style->standardIcon(
            QStyle::SP_DirIcon);

        QCOMPARE(
            pivot.addItem(computer, QStringLiteral("Sessions")),
            0);
        QCOMPARE(
            pivot.insertItem(0, directory, QStringLiteral("Files")),
            0);
        QCOMPARE(pivot.itemIcon(0).cacheKey(), directory.cacheKey());
        QCOMPARE(pivot.itemIcon(1).cacheKey(), computer.cacheKey());
        QVERIFY(pivot.itemIcon(-1).isNull());

        pivot.setItemIcon(1, QIcon());
        QVERIFY(pivot.itemIcon(1).isNull());
        pivot.setItemIcon(8, directory);
        QCOMPARE(pivot.count(), 2);
    }

    void indicatorStaysInTheBottomContentGutter_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("overflow");
        QTest::addColumn<qreal>("devicePixelRatio");

        QTest::newRow("ltr-long-dpr-1")
            << Qt::LeftToRight
            << QStringLiteral("A deliberately long session destination")
            << false
            << 1.0;
        QTest::newRow("rtl-long-dpr-1.25")
            << Qt::RightToLeft
            << QStringLiteral("A deliberately long session destination")
            << false
            << 1.25;
        QTest::newRow("ltr-icon-only-dpr-1.5")
            << Qt::LeftToRight
            << QString()
            << false
            << 1.5;
        QTest::newRow("rtl-overflow-dpr-2")
            << Qt::RightToLeft
            << QStringLiteral("Session destination")
            << true
            << 2.0;
    }

    void indicatorStaysInTheBottomContentGutter()
    {
        QFETCH(Qt::LayoutDirection, direction);
        QFETCH(QString, text);
        QFETCH(bool, overflow);
        QFETCH(qreal, devicePixelRatio);
        ZzFluentUI::ZzThemeController controller;
        auto *recordingStyle = new ZzTabLabelRecordingStyle(
            QStyleFactory::create(QStringLiteral("Fusion")));
        ZzFluentUI::ZzFluentStyle style(&controller, recordingStyle);
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.setLayoutDirection(direction);
        const QColor iconColor(17, 231, 109);
        const QIcon icon = zzTestIcon(iconColor);
        pivot.addItem(icon, text);
        if (overflow) {
            for (int index = 1; index < 12; ++index) {
                pivot.addItem(
                    icon,
                    QStringLiteral("Destination %1").arg(index));
            }
        }
        showPivot(&pivot, overflow ? 260 : 300);
        pivot.clearFocus();
        recordingStyle->labelOptions.clear();

        const QSize physicalSize(
            qCeil(pivot.width() * devicePixelRatio),
            qCeil(pivot.height() * devicePixelRatio));
        QImage image(
            physicalSize,
            QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatio);
        image.fill(pivot.palette().color(QPalette::Window));
        QPainter painter(&image);
        pivot.render(&painter);
        painter.end();

        const auto selected = std::find_if(
            recordingStyle->labelOptions.cbegin(),
            recordingStyle->labelOptions.cend(),
            [](const QStyleOptionTab &option) {
                return option.state.testFlag(QStyle::State_Selected);
            });
        QVERIFY(selected != recordingStyle->labelOptions.cend());
        const QRect tab = pivot.tabRect(pivot.currentIndex());
        const QRect textArea = style.subElementRect(
            QStyle::SE_TabBarTabText,
            &(*selected),
            &pivot).intersected(tab);
        const QColor accent = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::Accent);
        const QRect physicalTab = zzPhysicalCoverageRect(
            tab.intersected(pivot.rect()), devicePixelRatio);
        const QRect physicalTextArea = zzPhysicalCoverageRect(
            textArea, devicePixelRatio);
        const QRegion indicatorRegion = zzPhysicalColorRegion(
            image, accent).intersected(physicalTab);
        const QRegion iconRegion = zzPhysicalColorRegion(
            image, iconColor).intersected(physicalTab);

        QVERIFY(!indicatorRegion.isEmpty());
        QVERIFY(!iconRegion.isEmpty());
        QVERIFY(indicatorRegion.intersected(physicalTextArea).isEmpty());
        QVERIFY(indicatorRegion.intersected(iconRegion).isEmpty());
        QVERIFY(zzHasCrispCentralEdges(
            image,
            indicatorRegion.boundingRect(),
            accent));
    }

    void animatedIndicatorFramesAlignToPhysicalPixels_data()
    {
        QTest::addColumn<qreal>("devicePixelRatio");
        QTest::newRow("dpr-1.25") << 1.25;
        QTest::newRow("dpr-1.5") << 1.5;
        QTest::newRow("dpr-2") << 2.0;
    }

    void animatedIndicatorFramesAlignToPhysicalPixels()
    {
        QFETCH(qreal, devicePixelRatio);
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.addItem(QStringLiteral("Overview"));
        pivot.addItem(QStringLiteral("Build output"));
        showPivot(&pivot, 360);
        pivot.clearFocus();
        auto *animation = pivot.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        pivot.setCurrentIndex(1);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        animation->setCurrentTime(animation->duration() / 2);

        const QSize physicalSize(
            qCeil(pivot.width() * devicePixelRatio),
            qCeil(pivot.height() * devicePixelRatio));
        QImage image(
            physicalSize,
            QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatio);
        const QColor background = pivot.palette().color(QPalette::Window);
        image.fill(background);
        QPainter painter(&image);
        pivot.render(&painter);
        painter.end();

        const QColor accent = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::Accent);
        const QRegion indicatorRegion = zzPhysicalColorRegion(image, accent);
        QVERIFY(!indicatorRegion.isEmpty());
        QVERIFY(zzHasCrispCentralEdges(
            image,
            indicatorRegion.boundingRect(),
            accent));
    }

    void smallHeightsKeepStyleOptionsInsideTheVisibleGutter_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::addColumn<bool>("iconOnly");
        QTest::addColumn<int>("height");
        QTest::addColumn<qreal>("devicePixelRatio");

        QTest::newRow("minimal-height")
            << Qt::LeftToRight << false << 2 << 1.25;
        QTest::newRow("below-gutter")
            << Qt::LeftToRight << false << 1 << 1.5;
        QTest::newRow("rtl-icon-only-below-gutter")
            << Qt::RightToLeft << true << 1 << 2.0;
    }

    void smallHeightsKeepStyleOptionsInsideTheVisibleGutter()
    {
        QFETCH(Qt::LayoutDirection, direction);
        QFETCH(bool, iconOnly);
        QFETCH(int, height);
        QFETCH(qreal, devicePixelRatio);
        ZzFluentUI::ZzThemeController controller;
        auto *recordingStyle = new ZzTabLabelRecordingStyle(
            QStyleFactory::create(QStringLiteral("Fusion")));
        ZzFluentUI::ZzFluentStyle style(&controller, recordingStyle);
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.setLayoutDirection(direction);
        const QColor iconColor(17, 231, 109);
        pivot.addItem(
            zzTestIcon(iconColor),
            iconOnly ? QString() : QStringLiteral("Session"));
        showPivot(&pivot, 180, height);
        pivot.clearFocus();
        recordingStyle->labelOptions.clear();

        const QSize physicalSize(
            qCeil(pivot.width() * devicePixelRatio),
            qCeil(pivot.height() * devicePixelRatio));
        QImage image(
            physicalSize,
            QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatio);
        image.fill(pivot.palette().color(QPalette::Window));
        QPainter painter(&image);
        pivot.render(&painter);
        painter.end();

        const auto selected = std::find_if(
            recordingStyle->labelOptions.cbegin(),
            recordingStyle->labelOptions.cend(),
            [](const QStyleOptionTab &option) {
                return option.state.testFlag(QStyle::State_Selected);
            });
        QVERIFY(selected != recordingStyle->labelOptions.cend());
        QVERIFY(selected->rect.height() >= 0);
        QVERIFY(selected->rect.isEmpty()
                || pivot.rect().contains(selected->rect));

        const QColor accent = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::Accent);
        const QRegion indicatorRegion = zzPhysicalColorRegion(image, accent);
        const QRect physicalVisibleTab = zzPhysicalCoverageRect(
            pivot.tabRect(0).intersected(pivot.rect()),
            devicePixelRatio);
        QVERIFY(indicatorRegion.isEmpty()
                || physicalVisibleTab.contains(
                    indicatorRegion.boundingRect()));
    }

    void managesItemsAndEmitsCountOnlyForEffectiveChanges()
    {
        ZzFluentUI::ZzPivot pivot;
        QSignalSpy countSpy(
            &pivot, &ZzFluentUI::ZzPivot::itemCountChanged);

        QCOMPARE(pivot.count(), 0);
        QCOMPARE(pivot.currentIndex(), -1);
        QVERIFY(!pivot.isMovable());
        QVERIFY(!pivot.tabsClosable());
        QVERIFY(!pivot.expanding());
        QVERIFY(pivot.usesScrollButtons());
        QVERIFY(!pivot.drawBase());
        QCOMPARE(pivot.shape(), QTabBar::RoundedNorth);

        QCOMPARE(pivot.addItem(QStringLiteral("Overview")), 0);
        QCOMPARE(pivot.addItem(QStringLiteral("Reports")), 1);
        QCOMPARE(pivot.insertItem(1, QStringLiteral("Settings")), 1);
        QCOMPARE(pivot.count(), 3);
        QCOMPARE(countSpy.count(), 3);
        QCOMPARE(pivot.itemText(0), QStringLiteral("Overview"));
        QCOMPARE(pivot.itemText(1), QStringLiteral("Settings"));
        QCOMPARE(pivot.itemText(2), QStringLiteral("Reports"));
        QCOMPARE(pivot.itemText(-1), QString());

        pivot.setItemText(1, QStringLiteral("Preferences"));
        pivot.setItemText(1, QStringLiteral("Preferences"));
        pivot.setItemText(8, QStringLiteral("Ignored"));
        QCOMPARE(pivot.itemText(1), QStringLiteral("Preferences"));
        QCOMPARE(countSpy.count(), 3);

        pivot.removeItem(-1);
        pivot.removeItem(8);
        QCOMPARE(countSpy.count(), 3);
        pivot.removeItem(1);
        QCOMPARE(pivot.count(), 2);
        QCOMPARE(countSpy.count(), 4);
        QCOMPARE(countSpy.at(3).at(0).toInt(), 2);
    }

    void switchesOnceByMouseKeyboardAndMnemonic()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.addItem(QStringLiteral("&Overview"));
        pivot.addItem(QStringLiteral("&Settings"));
        pivot.addItem(QStringLiteral("&About"));
        showPivot(&pivot);
        QSignalSpy currentSpy(&pivot, &QTabBar::currentChanged);

        QTest::mouseClick(
            &pivot,
            Qt::LeftButton,
            Qt::NoModifier,
            pivot.tabRect(1).center());
        QCOMPARE(pivot.currentIndex(), 1);
        QCOMPARE(currentSpy.count(), 1);

        pivot.setFocus(Qt::OtherFocusReason);
        QTest::keyClick(&pivot, Qt::Key_Right);
        QCOMPARE(pivot.currentIndex(), 2);
        QTest::keyClick(&pivot, Qt::Key_Home);
        QCOMPARE(pivot.currentIndex(), 0);
        QTest::keyClick(&pivot, Qt::Key_End);
        QCOMPARE(pivot.currentIndex(), 2);

        pivot.setTabEnabled(0, false);
        QTest::keyClick(&pivot, Qt::Key_Home);
        QCOMPARE(pivot.currentIndex(), 1);
        pivot.setTabEnabled(0, true);
        pivot.setTabVisible(2, false);
        QTest::keyClick(&pivot, Qt::Key_End);
        QCOMPARE(pivot.currentIndex(), 1);
        pivot.setTabVisible(2, true);

        QTest::keyClick(&pivot, Qt::Key_S, Qt::AltModifier);
        QCOMPARE(pivot.currentIndex(), 1);
    }

    void preservesRtlGeometryOverflowAndAccessibility()
    {
        ZzFluentUI::ZzPivot pivot;
        pivot.setLayoutDirection(Qt::RightToLeft);
        for (int index = 0; index < 12; ++index) {
            pivot.addItem(QStringLiteral("Long destination %1").arg(index));
        }
        showPivot(&pivot, 260);

        QVERIFY(pivot.tabRect(0).center().x() > pivot.tabRect(1).center().x());
        const auto scrollButtons = pivot.findChildren<QToolButton *>();
        QCOMPARE(scrollButtons.size(), 2);
        QVERIFY(scrollButtons.at(0)->isVisible()
                || scrollButtons.at(1)->isVisible());

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&pivot);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::PageTabList);
        int pageTabCount = 0;
        bool foundFirstItem = false;
        for (int index = 0; index < interface->childCount(); ++index) {
            QAccessibleInterface *child = interface->child(index);
            if (child == nullptr || child->role() != QAccessible::PageTab) {
                continue;
            }
            ++pageTabCount;
            foundFirstItem = foundFirstItem
                || child->text(QAccessible::Name).contains(
                    QStringLiteral("Long destination 0"));
        }
        QCOMPARE(pageTabCount, pivot.count());
        QVERIFY(foundFirstItem);
    }

    void animatesContinuouslyAndDrawsOnlyBottomIndicator()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.addItem(QStringLiteral("Overview"));
        pivot.addItem(QStringLiteral("Build output"));
        pivot.addItem(QStringLiteral("History"));
        showPivot(&pivot);
        auto *animation = pivot.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        pivot.setCurrentIndex(1);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QTest::qWait(24);
        pivot.setCurrentIndex(2);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);

        pivot.clearFocus();
        QTest::mouseMove(
            &pivot,
            QPoint(pivot.width() - 1, pivot.height() - 1));
        QCoreApplication::processEvents();
        QImage image(pivot.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(pivot.palette().color(QPalette::Window));
        QPainter painter(&image);
        pivot.render(&painter);
        painter.end();
        const auto snapshot = style.themeSnapshot();
        const QColor accent = snapshot->color(ZzFluentUI::ZzColorToken::Accent);
        const QRect currentTab = pivot.tabRect(pivot.currentIndex());
        const int thickness = qCeil(snapshot->metric(
            ZzFluentUI::ZzMetricToken::SelectionIndicatorThickness));
        int bottomAccentPixels = 0;
        int upperAccentPixels = 0;
        for (int y = currentTab.top(); y <= currentTab.bottom(); ++y) {
            for (int x = currentTab.left(); x <= currentTab.right(); ++x) {
                if (image.pixelColor(x, y) != accent) {
                    continue;
                }
                if (y >= currentTab.bottom() - thickness) {
                    ++bottomAccentPixels;
                } else {
                    ++upperAccentPixels;
                }
            }
        }
        QVERIFY(bottomAccentPixels > 0);
        QCOMPARE(upperAccentPixels, 0);
        QCOMPARE(
            image.pixelColor(
                pivot.tabRect(0).left() + 1,
                pivot.tabRect(0).top() + 1),
            image.pixelColor(
                currentTab.left() + 1,
                currentTab.top() + 1));
    }

    void reducedMotionStopsRunningAnimationAndKeepsObjectBudget()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        for (const QString &text : {
                 QStringLiteral("Overview"),
                 QStringLiteral("Settings"),
                 QStringLiteral("About"),
                 QStringLiteral("History")}) {
            pivot.addItem(text);
        }
        showPivot(&pivot);
        QVariantAnimation *const animation =
            pivot.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        pivot.setCurrentIndex(1);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);

        const qsizetype animationCount =
            pivot.findChildren<QAbstractAnimation *>().size();
        const qsizetype timerCount = pivot.findChildren<QTimer *>().size();
        const qsizetype objectCount = pivot.findChildren<QObject *>().size();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            pivot.setCurrentIndex(iteration % pivot.count());
        }
        QCOMPARE(pivot.findChild<QVariantAnimation *>(), animation);
        QCOMPARE(
            pivot.findChildren<QAbstractAnimation *>().size(),
            animationCount);
        QCOMPARE(pivot.findChildren<QTimer *>().size(), timerCount);
        QCOMPARE(pivot.findChildren<QObject *>().size(), objectCount);
    }
};

QTEST_MAIN(ZzPivotTest)

#include "ZzPivotTest.moc"
