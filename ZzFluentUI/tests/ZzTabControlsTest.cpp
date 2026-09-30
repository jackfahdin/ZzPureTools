#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QAccessible>
#include <QtGui/QDragEnterEvent>
#include <QtCore/QMimeData>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtCore/QAbstractAnimation>
#include <QtGui/QContextMenuEvent>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionTab>
#include <QtCore/QDir>
#include <memory>

#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzControlAppearance.h>
#include <QtCore/QVariantAnimation>

#include "../widgets/src/private/ZzTabBarPrivate.h"

namespace {

/** @brief 仅供测试读取 Qt 实际生成的样式选项。 */
class ZzInspectableTabBar final : public QTabBar
{
public:
    using QTabBar::initStyleOption;
};

/** @brief 创建带稳定对象名的轻量测试页面。 */
QWidget *zzCreatePage(const QString &name, QWidget *parent = nullptr)
{
    auto *page = new QLabel(name, parent);
    page->setObjectName(name);
    return page;
}

/** @brief 为标签设置全部需要跨容器保留的公开元数据。 */
void zzSetTabMetadata(
    ZzFluentUI::ZzTabWidget *tabs,
    int index)
{
    tabs->setTabToolTip(index, QStringLiteral("工具提示"));
    tabs->setTabWhatsThis(index, QStringLiteral("上下文帮助"));
    tabs->setTabEnabled(index, false);
    tabs->fluentTabBar()->setTabData(
        index,
        QStringLiteral("stable-data"));
    tabs->fluentTabBar()->setTabTextColor(index, QColor(21, 84, 156));
}

} // namespace

class ZzTabControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /** @brief 新外观不能吞掉 QTabBar 原有的逐标签文字颜色覆盖。 */
    void appearancePreservesExplicitTabTextColor()
    {
        using namespace ZzFluentUI;
        ZzThemeController controller;
        ZzFluentStyle style(&controller);
        ZzTabBar bar;
        bar.setStyle(&style);
        bar.setAppearance(ZzTabBarAppearance::Pill);
        bar.addTab(QStringLiteral("Current"));
        bar.addTab(QStringLiteral("Colored text"));
        bar.setTabTextColor(1, QColor("#ff0000"));
        bar.resize(300, 40); bar.show();
        const QImage image = bar.grab().toImage();
        int redPixels = 0;
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() > 200 && pixel.green() < 80 && pixel.blue() < 80) ++redPixels;
        }
        QVERIFY(redPixels > 10);
    }

    /** @brief WinUI3 按下会缩短指示条，释放恢复；减少动态效果仍保留按下反馈。 */
    void winuiPressAndStyleLifetime()
    {
        using namespace ZzFluentUI;
        ZzThemeController controller;
        controller.setMode(ZzThemeMode::Light);
        ZzFluentStyle style(&controller);
        QTabBar bar;
        bar.setStyle(&style);
        ZzControlAppearance::setTabBarAppearance(&bar, ZzTabBarAppearance::SegmentedWinUI3);
        ZzControlAppearance::setAccentColor(&bar, QColor("#ff00ff"));
        bar.addTab(QString()); bar.addTab(QString());
        bar.resize(200, 32); bar.show();
        QCoreApplication::processEvents();
        const auto indicatorPixels = [&] {
            const auto image = bar.grab().toImage();
            int pixels = 0;
            const QRect strip = bar.tabRect(0).adjusted(0, 25, 0, 0);
            for (int y = strip.top(); y <= strip.bottom(); ++y)
                for (int x = strip.left(); x <= strip.right(); ++x)
                    if (image.pixelColor(x, y) == QColor("#ff00ff")) ++pixels;
            return pixels;
        };
        const int releasedPixels = indicatorPixels();
        QVERIFY(releasedPixels > 0);
        for (bool reduced : {false, true}) {
            controller.setReducedMotion(reduced);
            QTest::mousePress(&bar, Qt::LeftButton, Qt::NoModifier, bar.tabRect(0).center());
            for (auto *animation : style.findChildren<QVariantAnimation *>()) {
                if (reduced) QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
                else if (animation->state() == QAbstractAnimation::Running) animation->setCurrentTime(animation->duration());
            }
            const int pressedPixels = indicatorPixels();
            QVERIFY(pressedPixels > 0 && pressedPixels < releasedPixels);
            QTest::mouseRelease(&bar, Qt::LeftButton, Qt::NoModifier, bar.tabRect(0).center());
            for (auto *animation : style.findChildren<QVariantAnimation *>())
                if (animation->state() == QAbstractAnimation::Running) animation->setCurrentTime(animation->duration());
            QCOMPARE(indicatorPixels(), releasedPixels);
        }
        QVERIFY(!style.findChildren<QVariantAnimation *>().isEmpty());
        bar.setStyle(nullptr);
        QVERIFY(style.findChildren<QVariantAnimation *>().isEmpty());
    }

    /** @brief Qt 拖动快照使用局部矩形；第 N 个标签仍必须绘制自己的选中背景。 */
    void movingTabUsesLocalCoordinates()
    {
        using namespace ZzFluentUI;
        ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentStyle style(&controller);
        ZzInspectableTabBar bar;
        bar.setStyle(&style);
        for (const auto appearance : {ZzTabBarAppearance::Capsule, ZzTabBarAppearance::SegmentedSlide}) {
            ZzControlAppearance::setTabBarAppearance(&bar, appearance);
            if (bar.count() == 0) for (int i = 0; i < 4; ++i) bar.addTab(QStringLiteral("Long document %1").arg(i));
            bar.resize(180, 42);
            bar.setCurrentIndex(3);
            bar.show();
            QCoreApplication::processEvents();
            QStyleOptionTab option;
            bar.initStyleOption(&option, 3);
            option.position = QStyleOptionTab::Moving;
            option.rect.moveTopLeft(QPoint(0, 0));
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawControl(QStyle::CE_TabBarTab, &option, &painter, &bar);
            painter.end();
            QVERIFY(image.pixelColor(12, 10).alpha() > 0);
            QCOMPARE(image.pixelColor(12, 10), QColor(206, 206, 206));
        }
    }

    /** @brief 非方形按钮跨轴必须容纳，RTL 纵向图标不能侵入 Qt 按钮区域。 */
    void verticalButtonsFitAndDoNotOverlap()
    {
        using namespace ZzFluentUI;
        ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentStyle style(&controller);
        for (const auto shape : {QTabBar::RoundedWest, QTabBar::RoundedEast}) {
            for (const QSize buttonSize : {QSize(80, 20), QSize(20, 80)}) {
                ZzInspectableTabBar bar;
                bar.setStyle(&style);
                ZzControlAppearance::setTabBarAppearance(&bar, ZzTabBarAppearance::Pill);
                bar.setShape(shape);
                bar.setLayoutDirection(Qt::RightToLeft);
                bar.setExpanding(false);
                QPixmap marker(16, 16); marker.fill(QColor("#ff00ff"));
                bar.addTab(QIcon(marker), QStringLiteral("Agjp"));
                auto *button = new QWidget(&bar);
                button->setFixedSize(buttonSize);
                bar.setTabButton(0, QTabBar::RightSide, button);
                bar.resize(200, 300); bar.show();
                QCoreApplication::processEvents();
                QVERIFY(bar.tabRect(0).width() >= buttonSize.width());
                QStyleOptionTab option;
                bar.initStyleOption(&option, 0);
                const QRect occupied = style.subElementRect(QStyle::SE_TabBarTabRightButton, &option, &bar);
                QImage image(bar.size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                style.drawControl(QStyle::CE_TabBarTab, &option, &painter, &bar);
                painter.end();
                int markerPixels = 0;
                for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
                    if (image.pixelColor(x, y) == QColor("#ff00ff")) {
                        ++markerPixels;
                        QVERIFY2(!occupied.contains(x, y), "Icon overlaps vertical tab button");
                    }
                }
                QVERIFY(markerPixels > 0);
            }
        }
    }

    /** @brief 外观切换必须刷新 QTabBar 缓存尺寸，同时保留选择、数据和信号语义。 */
    void appearanceChangesRelayoutWithoutChangingTabs()
    {
        using namespace ZzFluentUI;
        ZzThemeController controller;
        ZzFluentStyle style(&controller);
        ZzTabBar bar;
        bar.setStyle(&style);
        bar.setExpanding(false);
        bar.addTab(QStringLiteral("One"));
        bar.addTab(QStringLiteral("Two"));
        bar.setTabData(1, 42);
        bar.setCurrentIndex(1);
        bar.show();
        QSignalSpy selection(&bar, &QTabBar::currentChanged);
        QSignalSpy appearance(&bar, &ZzTabBar::appearanceChanged);
        bar.setAppearance(ZzTabBarAppearance::Pill);
        const auto pillSize = bar.tabRect(0).size();
        ZzControlAppearance::setTabBarAppearance(&bar, ZzTabBarAppearance::SegmentedSlide);
        QVERIFY(bar.tabRect(0).height() > pillSize.height());
        bar.setAppearance(ZzTabBarAppearance::Capsule);
        QVERIFY(bar.tabRect(0).width() > pillSize.width());
        bar.setAppearance(ZzTabBarAppearance::Capsule);
        QCOMPARE(appearance.count(), 3);
        QCOMPARE(selection.count(), 0);
        QCOMPARE(bar.currentIndex(), 1);
        QCOMPARE(bar.tabData(1).toInt(), 42);
        QVERIFY(!bar.newTabButton()->isVisible());
        bar.setAppearance(static_cast<ZzTabBarAppearance>(-1));
        QCOMPARE(bar.appearance(), ZzTabBarAppearance::Standard);
    }

    /** @brief 自定义颜色必须覆盖像素，并在浅深色和清除覆盖时正确切换。 */
    void segmentedColorsRespectThemeAndReset()
    {
        using namespace ZzFluentUI;
        ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentStyle style(&controller);
        QTabBar bar;
        bar.setStyle(&style);
        bar.setExpanding(true);
        ZzControlAppearance::setTabBarAppearance(&bar, ZzTabBarAppearance::SegmentedSlide);
        ZzControlAppearance::setTabBarColors(&bar,
            {.background = QColor("#dddddd"), .selected = QColor("#8f27da"), .hover = {}, .pressed = {}, .text = {}, .selectedText = {}},
            {.background = QColor("#222222"), .selected = QColor("#123456"), .hover = {}, .pressed = {}, .text = {}, .selectedText = {}});
        bar.addTab(QStringLiteral("One"));
        bar.addTab(QStringLiteral("Two"));
        bar.resize(300, 42);
        bar.show();
        QCoreApplication::processEvents();
        const QPoint sample = bar.tabRect(0).topLeft() + QPoint(12, 10);
        controller.setMode(ZzThemeMode::Light);
        QCOMPARE(bar.grab().toImage().pixelColor(sample), QColor("#8f27da"));
        controller.setMode(ZzThemeMode::Dark);
        QCOMPARE(bar.grab().toImage().pixelColor(sample), QColor("#123456"));
        controller.setMode(ZzThemeMode::HighContrast);
        QVERIFY(bar.grab().toImage().pixelColor(sample) != QColor("#123456"));
        controller.setMode(ZzThemeMode::Light);
        ZzControlAppearance::setTabBarColors(&bar, {});
        QVERIFY(bar.grab().toImage().pixelColor(sample) != QColor("#8f27da"));
    }

    /** @brief 滑动和淡出重定向不跳帧，隐藏、删除和减少动态效果不会留下旧选择。 */
    void appearanceAnimationsRedirectAndSettle_data()
    {
        QTest::addColumn<int>("appearance");
        for (int value : {3, 4, 6, 7}) QTest::newRow(qPrintable(QString::number(value))) << value;
    }

    void appearanceAnimationsRedirectAndSettle()
    {
        using namespace ZzFluentUI;
        QFETCH(int, appearance);
        ZzThemeController controller;
        controller.setMode(ZzThemeMode::Light);
        ZzFluentStyle style(&controller);
        ZzTabBar bar;
        bar.setStyle(&style);
        bar.setAppearance(static_cast<ZzTabBarAppearance>(appearance));
        bar.addTab(QString()); bar.addTab(QString()); bar.addTab(QString());
        bar.resize(300, 50);
        bar.show();
        QCoreApplication::processEvents();
        (void)bar.grab();
        const auto animations = style.findChildren<QVariantAnimation *>();
        bar.setCurrentIndex(1);
        QVariantAnimation *running = nullptr;
        for (auto *animation : animations) if (animation->state() == QAbstractAnimation::Running) running = animation;
        QVERIFY(running);
        running->setCurrentTime(running->duration() / 3);
        const auto before = bar.grab().toImage();
        bar.setCurrentIndex(2);
        QCOMPARE(bar.grab().toImage(), before);
        running->setCurrentTime(running->duration());
        QVERIFY(bar.grab().toImage() != before);
        bar.setCurrentIndex(0);
        controller.setReducedMotion(true);
        for (auto *animation : animations) QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        const auto still = bar.grab().toImage();
        controller.setReducedMotion(false);
        bar.setCurrentIndex(2);
        bar.setTabVisible(2, false);
        bar.removeTab(1);
        (void)bar.grab();
        for (auto *animation : animations) QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QVERIFY(!still.isNull());
        QCOMPARE(style.findChildren<QVariantAnimation *>(), animations);
    }

    /** @brief 各种外观必须为大字体、图标、关闭按钮留足空间，兼容四边与 RTL。 */
    void appearanceGeometry_data()
    {
        QTest::addColumn<int>("appearance");
        QTest::addColumn<int>("shape");
        QTest::addColumn<bool>("rtl");
        for (int value = 1; value <= 9; ++value)
            for (int shape = 0; shape <= 3; ++shape)
                for (bool rtl : {false, true})
                    QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(value).arg(shape).arg(rtl))) << value << shape << rtl;
    }

    void appearanceGeometry()
    {
        using namespace ZzFluentUI;
        QFETCH(int, appearance); QFETCH(int, shape); QFETCH(bool, rtl);
        ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentStyle style(&controller);
        QTabBar bar;
        bar.setStyle(&style);
        ZzControlAppearance::setTabBarAppearance(&bar, static_cast<ZzTabBarAppearance>(appearance));
        bar.setShape(static_cast<QTabBar::Shape>(shape));
        bar.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        bar.setTabsClosable(true);
        bar.setExpanding(false);
        auto font = bar.font(); font.setPointSize(24); bar.setFont(font);
        bar.addTab(style.standardIcon(QStyle::SP_DirIcon), QStringLiteral("Agjp"));
        bar.addTab(QStringLiteral("Hidden"));
        bar.setTabVisible(1, false);
        bar.resize(500, 500);
        bar.show();
        QCoreApplication::processEvents();
        const auto rect = bar.tabRect(0);
        const bool vertical = shape >= 2;
        const int cross = vertical && appearance != 9 ? rect.width() : rect.height();
        QVERIFY(cross >= bar.fontMetrics().height() + 8);
        const auto side = static_cast<QTabBar::ButtonPosition>(style.styleHint(QStyle::SH_TabBar_CloseButtonPosition));
        auto *close = bar.tabButton(0, side);
        QVERIFY(close);
        QVERIFY(rect.contains(close->geometry().center()));
        QVERIFY(!bar.grab().isNull());
    }

    /** @brief 捕获纵向导航仍沿用旋转标签尺寸、导致长标题变成高条的回归。 */
    void navigationTabsKeepHorizontalLabels()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTabBar bar;
        bar.setStyle(&style);
        bar.setAppearance(ZzFluentUI::ZzTabBarAppearance::Navigation);
        bar.setShape(QTabBar::RoundedWest);
        bar.setExpanding(false);
        bar.addTab(QStringLiteral("Overview"));
        bar.addTab(QStringLiteral("A much longer settings page"));
        bar.resize(300, 200);
        bar.show();
        QCoreApplication::processEvents();
        QVERIFY(bar.tabRect(1).width() >= bar.fontMetrics().horizontalAdvance(bar.tabText(1)));
        QVERIFY(bar.tabRect(1).height() < bar.tabRect(1).width());
        QCOMPARE(bar.tabRect(0).width(), bar.tabRect(1).width());
    }

    /** @brief 捕获只缩减文字区域、却未为指示条增加标签尺寸的回归。 */
    void indicatorGutterPreservesMeasuredLabel_data()
    {
        QTest::addColumn<int>("shape");
        QTest::addColumn<int>("fontSize");
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("selected");
        for (int shape = QTabBar::RoundedNorth; shape <= QTabBar::TriangularEast; ++shape) {
            for (int fontSize : {12, 24}) {
                for (bool rtl : {false, true}) {
                    for (bool selected : {false, true}) {
                        QTest::newRow(qPrintable(QStringLiteral("shape-%1-font-%2-rtl-%3-selected-%4")
                            .arg(shape).arg(fontSize).arg(rtl).arg(selected)))
                            << shape << fontSize << rtl << selected;
                    }
                }
            }
        }
    }

    void indicatorGutterPreservesMeasuredLabel()
    {
        QFETCH(int, shape);
        QFETCH(int, fontSize);
        QFETCH(bool, rtl);
        QFETCH(bool, selected);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QFont font = QApplication::font();
        font.setPointSize(fontSize);
        QStyleOptionTab option;
        option.shape = static_cast<QTabBar::Shape>(shape);
        option.fontMetrics = QFontMetrics(font);
        option.text = QStringLiteral("项目设置 Agjp");
        option.direction = rtl ? Qt::RightToLeft : Qt::LeftToRight;
        option.state = QStyle::State_Enabled;
        option.state.setFlag(QStyle::State_Selected, selected);
        const bool vertical = ZzFluentUI::zzIsVerticalTabShape(option.shape);
        // QTabBar includes style padding before calling CT_TabBarTab.
        QSize contents(option.fontMetrics.horizontalAdvance(option.text)
                + style.pixelMetric(QStyle::PM_TabBarTabHSpace, &option),
            option.fontMetrics.height()
                + style.pixelMetric(QStyle::PM_TabBarTabVSpace, &option));
        if (vertical) {
            contents.transpose();
        }
        const QSize baseSize = style.QProxyStyle::sizeFromContents(
            QStyle::CT_TabBarTab, &option, contents, nullptr);
        option.rect = QRect(QPoint(), baseSize);
        const QRect originalLabel = style.QProxyStyle::subElementRect(
            QStyle::SE_TabBarTabText, &option, nullptr);
        option.rect.setSize(style.sizeFromContents(QStyle::CT_TabBarTab, &option, contents));
        const QRect label = style.subElementRect(QStyle::SE_TabBarTabText, &option);
        QVERIFY2(label.width() >= originalLabel.width(), "Indicator reduced measured label width");
        QVERIFY2(label.height() >= originalLabel.height(), "Indicator reduced measured label height");
        QVERIFY2(label.height() >= option.fontMetrics.height(), "Font height exceeds label area");
    }

    /** @brief 在真实标签及页面宿主中检查图标、关闭按钮和大字体的布局。 */
    void actualTabLabelsFit_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("fontSize");
        QTest::addColumn<bool>("rtl");
        for (int kind = 0; kind < 3; ++kind) {
            for (int fontSize : {12, 24}) {
                for (bool rtl : {false, true}) {
                    QTest::newRow(qPrintable(QStringLiteral("kind-%1-font-%2-rtl-%3")
                        .arg(kind).arg(fontSize).arg(rtl))) << kind << fontSize << rtl;
                }
            }
        }
    }

    void actualTabLabelsFit()
    {
        QFETCH(int, kind);
        QFETCH(int, fontSize);
        QFETCH(bool, rtl);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        std::unique_ptr<QWidget> owner;
        QTabBar *bar = nullptr;
        if (kind == 2) {
            auto *tabs = new ZzFluentUI::ZzTabWidget;
            owner.reset(tabs);
            tabs->addTab(new QWidget, QStringLiteral("项目设置 Agjp"));
            tabs->addTab(new QWidget, QStringLiteral("概览 Overview"));
            bar = tabs->fluentTabBar();
        } else {
            bar = kind == 0 ? new QTabBar : new ZzFluentUI::ZzTabBar;
            owner.reset(bar);
            bar->addTab(QStringLiteral("项目设置 Agjp"));
            bar->addTab(QStringLiteral("概览 Overview"));
        }
        owner->setStyle(&style);
        bar->setStyle(&style);
        owner->setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        QFont font = bar->font();
        font.setPointSize(fontSize);
        bar->setFont(font);
        bar->setExpanding(false);
        bar->setTabsClosable(true);
        QPixmap icon(16, 16);
        icon.fill(Qt::green);
        bar->setTabIcon(0, QIcon(icon));
        owner->resize(kind == 2 ? QSize(900, 160) : bar->sizeHint());
        owner->show();
        QVERIFY(QTest::qWaitForWindowExposed(owner.get()));
        for (int selected = 0; selected < bar->count(); ++selected) {
            bar->setCurrentIndex(selected);
            for (int index = 0; index < bar->count(); ++index) {
                QStyleOptionTab option;
                option.initFrom(bar);
                option.rect = bar->tabRect(index);
                option.shape = bar->shape();
                option.text = bar->tabText(index);
                option.icon = bar->tabIcon(index);
                option.iconSize = bar->iconSize();
                option.fontMetrics = QFontMetrics(bar->font());
                option.state.setFlag(QStyle::State_Selected, selected == index);
                if (auto *button = bar->tabButton(index, QTabBar::LeftSide)) {
                    option.leftButtonSize = button->size();
                }
                if (auto *button = bar->tabButton(index, QTabBar::RightSide)) {
                    option.rightButtonSize = button->size();
                }
                const QRect label = style.subElementRect(QStyle::SE_TabBarTabText, &option, bar);
                QVERIFY2(label.height() >= option.fontMetrics.height(),
                    qPrintable(QStringLiteral("tabHeight=%1 labelHeight=%2 fontHeight=%3 selected=%4")
                        .arg(option.rect.height()).arg(label.height())
                        .arg(option.fontMetrics.height()).arg(selected == index)));
                QVERIFY(label.width() >= option.fontMetrics.horizontalAdvance(option.text));
                QVERIFY(bar->rect().contains(option.rect));
            }
        }
        const QString directory = qEnvironmentVariable("ZZ_INDICATOR_REPORT_DIR");
        if (!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(owner->grab().save(QDir(directory).filePath(
                QStringLiteral("tab-label-%1.png").arg(QString::fromLatin1(QTest::currentDataTag())))));
        }
    }

    void contextMenuProviderIsInvoked()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *page = zzCreatePage(QStringLiteral("page"));
        tabs.addTab(page, QStringLiteral("page"));
        bool invoked = false;
        tabs.setTabContextMenuProvider(
            [&](QMenu &menu, int index, QWidget *receivedPage) {
                menu.addAction(QStringLiteral("业务动作"));
                invoked = index == 0 && receivedPage == page;
            });
        tabs.invokeTabContextMenu(QPoint(1, 1), QPoint(1, 1));
        QVERIFY(invoked);
    }

    void clearedBuiltInContextActionsCannotDispatchCommands()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(zzCreatePage(QStringLiteral("first")),
                    QStringLiteral("first"));
        tabs.addTab(zzCreatePage(QStringLiteral("second")),
                    QStringLiteral("second"));
        QPointer<QAction> businessAction;
        tabs.setTabContextMenuProvider(
            [&](QMenu &menu, int, QWidget *) {
                menu.clear();
                businessAction = menu.addAction(QStringLiteral("业务动作"));
            });
        QSignalSpy newTabRequested(
            &tabs, &ZzFluentUI::ZzTabWidget::newTabRequested);
        QSignalSpy tabsCloseRequested(
            &tabs, &ZzFluentUI::ZzTabWidget::tabsCloseRequested);

        tabs.invokeTabContextMenu(QPoint(1, 1), QPoint(1, 1));
        QVERIFY(businessAction);
        businessAction->trigger();

        QCOMPARE(newTabRequested.size(), 0);
        QCOMPARE(tabsCloseRequested.size(), 0);
    }

    void keepsPinnedPartitionAcrossAllMoves()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        auto *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("a"));
        tabs.addTab(b, QStringLiteral("b"));
        tabs.addTab(c, QStringLiteral("c"));

        tabs.setTabPinned(2, true);
        tabs.fluentTabBar()->moveTab(0, 2);
        QVERIFY(tabs.transferTabTo(&tabs, 2, 0));
        int ordinary = tabs.count();
        for (int index = 0; index < tabs.count(); ++index) {
            if (!tabs.isTabPinned(index)) {
                ordinary = index;
                break;
            }
        }
        for (int index = 0; index < tabs.count(); ++index) {
            if (tabs.isTabPinned(index)) {
                QVERIFY(index < ordinary);
            }
        }

        tabs.setTabPinned(tabs.indexOf(c), false);
        ordinary = tabs.count();
        for (int index = 0; index < tabs.count(); ++index) {
            if (!tabs.isTabPinned(index)) {
                ordinary = index;
                break;
            }
        }
        for (int index = 0; index < tabs.count(); ++index) {
            if (tabs.isTabPinned(index)) {
                QVERIFY(index < ordinary);
            }
        }
    }

    void normalizesPublicInsertTabAgainstPinnedPartition()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *pinned = zzCreatePage(QStringLiteral("pinned"));
        auto *ordinary = zzCreatePage(QStringLiteral("ordinary"));
        tabs.addTab(pinned, QStringLiteral("Pinned"));
        tabs.setTabPinned(0, true);

        tabs.insertTab(0, ordinary, QStringLiteral("Ordinary"));

        QCOMPARE(tabs.indexOf(pinned), 0);
        QCOMPARE(tabs.indexOf(ordinary), 1);
        QVERIFY(tabs.isTabPinned(0));
        QVERIFY(!tabs.isTabPinned(1));
    }

    void preservesWorkspaceMetadataAcrossTransfersAndFailures()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        auto *page = zzCreatePage(QStringLiteral("page"));
        source.addTab(page, QStringLiteral("page"));
        source.setTabPinned(0, true);
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        QVERIFY(source.transferTabTo(&target, 0));
        QCOMPARE(target.widget(0), page);
        QVERIFY(target.isTabPinned(0));
        QVERIFY(target.isTabModified(0));
        QVERIFY(target.hasTabAttention(0));
        QVERIFY(!target.isTabCloseEnabled(0));

        auto *ordinaryPage = zzCreatePage(QStringLiteral("ordinary"));
        source.addTab(ordinaryPage, QStringLiteral("ordinary"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        QVERIFY(source.transferTabTo(&target, 0, 0));
        const int ordinaryIndex = target.indexOf(ordinaryPage);
        QCOMPARE(ordinaryIndex, 1);
        QVERIFY(!target.isTabPinned(ordinaryIndex));
        QVERIFY(target.isTabModified(ordinaryIndex));
        QVERIFY(target.hasTabAttention(ordinaryIndex));

        auto *failedPage = zzCreatePage(QStringLiteral("fail"));
        source.addTab(failedPage, QStringLiteral("fail"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);
        target.fluentTabBar()->setTabTransferEnabled(false);
        QVERIFY(!source.transferTabTo(&target, 0));
        QCOMPARE(source.widget(0), failedPage);
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));

        QSignalSpy tearOffSpy(
            &source,
            &ZzFluentUI::ZzTabWidget::tearOffRequested);
        QVERIFY(QMetaObject::invokeMethod(
            source.fluentTabBar(),
            "tearOffRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(QPoint, QPoint(20, 20))));
        QCOMPARE(tearOffSpy.count(), 1);
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void filtersCloseIntentByPageState()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        tabs.addTab(a, QStringLiteral("a"));
        tabs.addTab(b, QStringLiteral("b"));
        tabs.setTabsClosable(true);
        QSignalSpy spy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);

        tabs.setTabCloseEnabled(0, false);
        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));
        QCOMPARE(spy.count(), 0);

        tabs.setTabCloseEnabled(0, true);
        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(
            spy.at(0).at(0).value<QList<QWidget *>>().first(),
            a);

        tabs.setTabPinned(0, true);
        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));
        QCOMPARE(spy.count(), 1);
    }

    void laysOutAndInvokesNewTabAndContextActions()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(zzCreatePage(QStringLiteral("first")),
                    QStringLiteral("First"));
        tabs.addTab(zzCreatePage(QStringLiteral("second")),
                    QStringLiteral("Second"));
        tabs.resize(500, 180);
        tabs.show();
        QCoreApplication::processEvents();

        QWidget *const button = tabs.fluentTabBar()->newTabButton();
        QVERIFY(button->isVisible());
        QVERIFY(!button->geometry().isEmpty());
        const QRect buttonRect(
            button->mapToGlobal(button->rect().topLeft()),
            button->size());
        for (int index = 0; index < tabs.count(); ++index) {
            const QRect tab = tabs.fluentTabBar()->tabRect(index);
            const QRect globalTab(
                tabs.fluentTabBar()->mapToGlobal(tab.topLeft()),
                tab.size());
            QVERIFY(!globalTab.intersects(buttonRect));
        }

        tabs.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        QVERIFY(button->isVisible());

        QSignalSpy newSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::newTabRequested);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(newSpy.count(), 1);

        const QPoint tabPosition = tabs.fluentTabBar()->tabRect(0).center();
        auto triggerContextAction = [&](const QString &text) {
            bool triggered = false;
            QTimer::singleShot(0, [&] {
                auto *menu = qobject_cast<QMenu *>(
                    QApplication::activePopupWidget());
                if (menu == nullptr) {
                    return;
                }
                for (QAction *action : menu->actions()) {
                    if (action->text() == text) {
                        action->trigger();
                        menu->close();
                        triggered = true;
                        return;
                    }
                }
            });
            QContextMenuEvent event(
                QContextMenuEvent::Mouse,
                tabPosition,
                tabs.fluentTabBar()->mapToGlobal(tabPosition));
            QApplication::sendEvent(tabs.fluentTabBar(), &event);
            QCoreApplication::processEvents();
            QVERIFY(triggered);
        };

        QSignalSpy closeSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        triggerContextAction(QStringLiteral("关闭其他标签页"));
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(
            closeSpy.at(0).at(0).value<QList<QWidget *>>().size(),
            1);
        closeSpy.clear();
        triggerContextAction(QStringLiteral("关闭右侧标签页"));
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(
            closeSpy.at(0).at(0).value<QList<QWidget *>>().size(),
            1);

        triggerContextAction(QStringLiteral("新建标签页"));
        QCOMPARE(newSpy.count(), 2);
    }

    void keepsContextMenuTargetPageAfterReorder()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *first = zzCreatePage(QStringLiteral("first"));
        auto *second = zzCreatePage(QStringLiteral("second"));
        tabs.addTab(first, QStringLiteral("First"));
        tabs.addTab(second, QStringLiteral("Second"));
        tabs.resize(500, 180);
        tabs.show();
        QCoreApplication::processEvents();

        QSignalSpy closeSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        const QPoint tabPosition = tabs.fluentTabBar()->tabRect(0).center();
        bool actionTriggered = false;
        QTimer::singleShot(0, [&] {
            auto *menu = qobject_cast<QMenu *>(
                QApplication::activePopupWidget());
            if (menu == nullptr) {
                return;
            }
            tabs.fluentTabBar()->moveTab(0, 1);
            for (QAction *action : menu->actions()) {
                if (action->text() == QStringLiteral("关闭其他标签页")) {
                    action->trigger();
                    menu->close();
                    actionTriggered = true;
                    return;
                }
            }
        });
        QContextMenuEvent event(
            QContextMenuEvent::Mouse,
            tabPosition,
            tabs.fluentTabBar()->mapToGlobal(tabPosition));
        QApplication::sendEvent(tabs.fluentTabBar(), &event);
        QCoreApplication::processEvents();

        QVERIFY(actionTriggered);
        QCOMPARE(closeSpy.count(), 1);
        const QList<QWidget *> pages =
            closeSpy.at(0).at(0).value<QList<QWidget *>>();
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages.front(), second);
    }

    void keepsObserversAndObjectBudgetsStable()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        for (int index = 0; index < 200; ++index) {
            source.addTab(
                zzCreatePage(QString::number(index)),
                QString::number(index));
        }
        const qsizetype objectCount =
            source.findChildren<QObject *>().size()
            + target.findChildren<QObject *>().size();
        const qsizetype timerCount =
            source.findChildren<QTimer *>().size()
            + target.findChildren<QTimer *>().size();
        const qsizetype animationCount =
            source.findChildren<QAbstractAnimation *>().size()
            + target.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            const int index = iteration % source.count();
            source.setCurrentIndex(index);
            source.setTabModified(index, (iteration & 1) != 0);
            source.setTabAttention(index, (iteration & 1) != 0);
        }
        QCOMPARE(
            source.findChildren<QObject *>().size()
                + target.findChildren<QObject *>().size(),
            objectCount);
        QCOMPARE(
            source.findChildren<QTimer *>().size()
                + target.findChildren<QTimer *>().size(),
            timerCount);
        QCOMPARE(
            source.findChildren<QAbstractAnimation *>().size()
                + target.findChildren<QAbstractAnimation *>().size(),
            animationCount);

        int businessSignals = 0;
        auto *next = zzCreatePage(QStringLiteral("next"));
        source.addTab(next, QStringLiteral("next"));
        QObject::connect(
            next,
            &QWidget::windowTitleChanged,
            &source,
            [&businessSignals] { ++businessSignals; });
        QVERIFY(source.transferTabTo(&target, source.indexOf(next)));
        next->setWindowTitle(QStringLiteral("transferred"));
        QCOMPARE(businessSignals, 1);

        QWidget *const removed = source.widget(0);
        source.setTabModified(0, true);
        delete removed;
        QCoreApplication::processEvents();
        auto *replacement = zzCreatePage(QStringLiteral("replacement"));
        source.addTab(replacement, QStringLiteral("replacement"));
        QVERIFY(source.indexOf(replacement) >= 0);
    }

    void restoresTransferAfterTargetIsDestroyedDuringCommit()
    {
        ZzFluentUI::ZzTabWidget source;
        auto *page = zzCreatePage(QStringLiteral("rollback"));
        source.addTab(page, QStringLiteral("Rollback"));
        source.setTabPinned(0, true);
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        auto *target = new ZzFluentUI::ZzTabWidget;
        QPointer<ZzFluentUI::ZzTabWidget> targetGuard(target);
        QObject::connect(
            &source,
            &QTabWidget::currentChanged,
            &source,
            [&targetGuard](int) {
                delete targetGuard.data();
            });

        QVERIFY(!source.transferTabTo(target, 0));
        QVERIFY(targetGuard.isNull());
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QVERIFY(source.isTabPinned(0));
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void rollsBackWhenTargetIsDestroyedFromTabMovedCallback()
    {
        ZzFluentUI::ZzTabWidget source;
        auto *page = zzCreatePage(QStringLiteral("destroyed-target"));
        source.addTab(page, QStringLiteral("Destroyed target"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        auto *target = new ZzFluentUI::ZzTabWidget;
        auto *existing = zzCreatePage(QStringLiteral("existing-pinned"));
        target->addTab(existing, QStringLiteral("Existing"));
        target->setTabPinned(0, true);
        QPointer<ZzFluentUI::ZzTabWidget> targetGuard(target);
        bool destroyingTarget = false;
        QObject::connect(
            target->fluentTabBar(),
            &QTabBar::tabMoved,
            &source,
            [&, page](int, int) {
                if (destroyingTarget || targetGuard.isNull()) {
                    return;
                }
                destroyingTarget = true;
                const int pageIndex = targetGuard->indexOf(page);
                if (pageIndex >= 0) {
                    targetGuard->removeTab(pageIndex);
                }
                page->setParent(nullptr);
                delete targetGuard.data();
            });

        QVERIFY(!source.transferTabTo(target, 0, 0));
        QVERIFY(targetGuard.isNull());
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QVERIFY(!source.isTabPinned(0));
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void rollsBackWhenTargetRemovesPageDuringNormalize()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        auto *page = zzCreatePage(QStringLiteral("removed-during-normalize"));
        source.addTab(page, QStringLiteral("Removed during normalize"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        auto *pinned = zzCreatePage(QStringLiteral("target-pinned"));
        target.addTab(pinned, QStringLiteral("Pinned"));
        target.setTabPinned(0, true);
        bool removedDuringNormalize = false;
        QObject::connect(
            target.fluentTabBar(),
            &QTabBar::tabMoved,
            &target,
            [&](int, int) {
                const int pageIndex = target.indexOf(page);
                if (pageIndex < 0) {
                    return;
                }
                target.removeTab(pageIndex);
                removedDuringNormalize = true;
            });

        QVERIFY(!source.transferTabTo(&target, 0, 0));
        QVERIFY(removedDuringNormalize);
        QCOMPARE(target.indexOf(page), -1);
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QVERIFY(!source.isTabPinned(0));
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void reportsNormalizedTargetIndexAfterPinnedPartition()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        auto *page = zzCreatePage(QStringLiteral("ordinary-transfer"));
        auto *pinned = zzCreatePage(QStringLiteral("existing-pinned"));
        source.addTab(page, QStringLiteral("Ordinary"));
        target.addTab(pinned, QStringLiteral("Pinned"));
        target.setTabPinned(0, true);
        QSignalSpy transferredSpy(
            &target,
            &ZzFluentUI::ZzTabWidget::tabTransferred);

        QVERIFY(source.transferTabTo(&target, 0, 0));

        QCOMPARE(target.indexOf(page), 1);
        QCOMPARE(transferredSpy.count(), 1);
        QCOMPARE(
            transferredSpy.at(0).at(2).toInt(),
            target.indexOf(page));
    }

    void doesNotStealPageTakenByThirdPartyDuringTransfer()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        ZzFluentUI::ZzTabWidget thirdParty;
        auto *page = zzCreatePage(QStringLiteral("third-party-page"));
        auto *pinned = zzCreatePage(QStringLiteral("target-pinned"));
        source.addTab(page, QStringLiteral("Third party"));
        target.addTab(pinned, QStringLiteral("Pinned"));
        target.setTabPinned(0, true);
        bool taken = false;
        QObject::connect(
            target.fluentTabBar(),
            &QTabBar::tabMoved,
            &target,
            [&](int, int) {
                if (taken) {
                    return;
                }
                const int pageIndex = target.indexOf(page);
                if (pageIndex >= 0) {
                    taken = target.transferTabTo(
                        &thirdParty,
                        pageIndex);
                }
            });

        QVERIFY(!source.transferTabTo(&target, 0, 0));
        QVERIFY(taken);
        QCOMPARE(source.indexOf(page), -1);
        QCOMPARE(target.indexOf(page), -1);
        QCOMPARE(thirdParty.count(), 1);
        QCOMPARE(thirdParty.widget(0), page);
    }

    void workspaceStateAndCloseIntentContract()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        auto *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("A"));
        tabs.addTab(b, QStringLiteral("B"));
        tabs.addTab(c, QStringLiteral("C"));

        QSignalSpy modified(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabModifiedChanged);
        tabs.setTabModified(1, true);
        tabs.setTabModified(1, true);
        QCOMPARE(modified.count(), 1);

        QSignalSpy pinned(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabPinnedChanged);
        tabs.setTabPinned(2, true);
        QCOMPARE(tabs.widget(0), c);
        tabs.setTabPinned(0, true);
        QCOMPARE(pinned.count(), 1);

        QSignalSpy attention(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabAttentionChanged);
        QSignalSpy closeEnabled(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabCloseEnabledChanged);
        tabs.setTabAttention(1, true);
        tabs.setTabAttention(1, true);
        QCOMPARE(attention.count(), 1);
        tabs.setTabCloseEnabled(1, false);
        tabs.setTabCloseEnabled(1, false);
        QCOMPARE(closeEnabled.count(), 1);

        tabs.setPageTitle(1, QStringLiteral("Renamed"));
        QCOMPARE(tabs.tabText(1), QStringLiteral("Renamed"));
        QCOMPARE(
            tabs.widget(1)->windowTitle(),
            QStringLiteral("Renamed"));

        QSignalSpy batch(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        tabs.closeOtherTabs(0);
        QCOMPARE(batch.count(), 1);
        QCOMPARE(
            batch.at(0).at(0).value<QList<QWidget *>>().size(),
            1);
        batch.clear();
        tabs.setTabCloseEnabled(1, true);
        tabs.closeTabsToRight(0);
        QCOMPARE(batch.count(), 1);
        QCOMPARE(
            batch.at(0).at(0).value<QList<QWidget *>>().size(),
            2);

        QVERIFY(tabs.fluentTabBar()->newTabButton() != nullptr);
        QSignalSpy newSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::newTabRequested);
        QTest::mouseClick(tabs.fluentTabBar()->newTabButton(), Qt::LeftButton);
        QCOMPARE(newSpy.count(), 1);
    }
    void exposesStableDefaults()
    {
        ZzFluentUI::ZzTabWidget tabs;

        QVERIFY(tabs.fluentTabBar() != nullptr);
        QCOMPARE(tabs.fluentTabBar()->parentWidget(), &tabs);
        QVERIFY(tabs.isMovable());
        QVERIFY(tabs.fluentTabBar()->isTearOffEnabled());
        QVERIFY(tabs.fluentTabBar()->isTabTransferEnabled());
        QVERIFY(tabs.fluentTabBar()->acceptDrops());
        QVERIFY(tabs.fluentTabBar()->usesScrollButtons());
        QCOMPARE(tabs.fluentTabBar()->elideMode(), Qt::ElideRight);
    }

    void emitsCapabilityChangesOnlyForRealUpdates()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QSignalSpy tearOffSpy(
            tabs.fluentTabBar(),
            &ZzFluentUI::ZzTabBar::tearOffEnabledChanged);
        QSignalSpy transferSpy(
            tabs.fluentTabBar(),
            &ZzFluentUI::ZzTabBar::tabTransferEnabledChanged);

        tabs.fluentTabBar()->setTearOffEnabled(true);
        tabs.fluentTabBar()->setTabTransferEnabled(true);
        QCOMPARE(tearOffSpy.count(), 0);
        QCOMPARE(transferSpy.count(), 0);

        tabs.fluentTabBar()->setTearOffEnabled(false);
        tabs.fluentTabBar()->setTabTransferEnabled(false);
        QCOMPARE(tearOffSpy.count(), 1);
        QCOMPARE(transferSpy.count(), 1);
        QCOMPARE(tearOffSpy.at(0).at(0).toBool(), false);
        QCOMPARE(transferSpy.at(0).at(0).toBool(), false);
    }

    void transfersPageAndCompleteMetadata()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        QWidget *first = zzCreatePage(QStringLiteral("first"));
        QWidget *moved = zzCreatePage(QStringLiteral("moved"));
        QWidget *last = zzCreatePage(QStringLiteral("last"));
        const QIcon icon = source.style()->standardIcon(
            QStyle::SP_FileIcon);

        source.addTab(first, QStringLiteral("First"));
        source.addTab(moved, icon, QStringLiteral("Moved"));
        target.addTab(last, QStringLiteral("Last"));
        zzSetTabMetadata(&source, 1);
        QSignalSpy transferredSpy(
            &target,
            &ZzFluentUI::ZzTabWidget::tabTransferred);

        QVERIFY(source.transferTabTo(&target, 1, 0));
        QCOMPARE(source.count(), 1);
        QCOMPARE(target.count(), 2);
        QCOMPARE(target.widget(0), moved);
        QCOMPARE(target.tabText(0), QStringLiteral("Moved"));
        QCOMPARE(target.tabIcon(0).cacheKey(), icon.cacheKey());
        QCOMPARE(target.tabToolTip(0), QStringLiteral("工具提示"));
        QCOMPARE(target.tabWhatsThis(0), QStringLiteral("上下文帮助"));
        QVERIFY(!target.isTabEnabled(0));
        QCOMPARE(
            target.fluentTabBar()->tabData(0).toString(),
            QStringLiteral("stable-data"));
        QCOMPARE(
            target.fluentTabBar()->tabTextColor(0),
            QColor(21, 84, 156));
        QCOMPARE(target.currentWidget(), moved);
        QCOMPARE(transferredSpy.count(), 1);
        QCOMPARE(
            qvariant_cast<ZzFluentUI::ZzTabWidget *>(
                transferredSpy.at(0).at(0)),
            &source);
        QCOMPARE(transferredSpy.at(0).at(1).toInt(), 1);
        QCOMPARE(transferredSpy.at(0).at(2).toInt(), 0);
        QCOMPARE(
            qvariant_cast<QWidget *>(transferredSpy.at(0).at(3)),
            moved);
    }

    void reordersWithinSameContainerByInsertionSlot()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QWidget *a = zzCreatePage(QStringLiteral("a"));
        QWidget *b = zzCreatePage(QStringLiteral("b"));
        QWidget *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("A"));
        tabs.addTab(b, QStringLiteral("B"));
        tabs.addTab(c, QStringLiteral("C"));

        QVERIFY(tabs.transferTabTo(&tabs, 0, 3));
        QCOMPARE(tabs.widget(0), b);
        QCOMPARE(tabs.widget(1), c);
        QCOMPARE(tabs.widget(2), a);

        QVERIFY(tabs.transferTabTo(&tabs, 2, 0));
        QCOMPARE(tabs.widget(0), a);
        QCOMPARE(tabs.widget(1), b);
        QCOMPARE(tabs.widget(2), c);

        QVERIFY(tabs.transferTabTo(&tabs, 1, 2));
        QCOMPARE(tabs.widget(1), b);
    }

    void rejectsInvalidTransfersWithoutChangingSource()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        QWidget *page = zzCreatePage(QStringLiteral("guarded"));
        source.addTab(page, QStringLiteral("Guarded"));

        QVERIFY(!source.transferTabTo(nullptr, 0));
        QVERIFY(!source.transferTabTo(&target, -1));
        QVERIFY(!source.transferTabTo(&target, 1));
        target.fluentTabBar()->setTabTransferEnabled(false);
        QVERIFY(!source.transferTabTo(&target, 0));
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QCOMPARE(target.count(), 0);
    }

    void forwardsTearOffIntentWithoutRemovingPage()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QWidget *page = zzCreatePage(QStringLiteral("tear-off"));
        tabs.addTab(page, QStringLiteral("Tear off"));
        QSignalSpy spy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tearOffRequested);
        const QPoint globalPosition(240, 160);

        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tearOffRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(QPoint, globalPosition)));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(qvariant_cast<QWidget *>(spy.at(0).at(1)), page);
        QCOMPARE(spy.at(0).at(2).toPoint(), globalPosition);
        QCOMPARE(tabs.count(), 1);
        QCOMPARE(tabs.widget(0), page);
    }

    void closeRequestIsIntentOnly()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QPointer<QWidget> page = zzCreatePage(QStringLiteral("closable"));
        tabs.addTab(page, QStringLiteral("Closable"));
        tabs.setTabsClosable(true);
        QSignalSpy spy(&tabs, &QTabWidget::tabCloseRequested);

        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(tabs.count(), 1);
        QCOMPARE(tabs.widget(0), page);
        QVERIFY(!page.isNull());
    }

    void rejectsForgedMimePayload()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(
            zzCreatePage(QStringLiteral("mime")),
            QStringLiteral("MIME"));
        QMimeData mimeData;
        mimeData.setData(
            QStringLiteral("application/x-zz-fluent-tab-v1"),
            QByteArrayLiteral("1"));
        QDragEnterEvent event(
            QPoint(4, 4),
            Qt::MoveAction,
            &mimeData,
            Qt::LeftButton,
            Qt::NoModifier);

        QApplication::sendEvent(tabs.fluentTabBar(), &event);

        QVERIFY(!event.isAccepted());
        QCOMPARE(tabs.count(), 1);
    }

    void computesInsertionSlotsForLtrRtlAndVerticalBars()
    {
        ZzFluentUI::ZzTabBar bar;
        bar.addTab(QStringLiteral("One"));
        bar.addTab(QStringLiteral("Two"));
        bar.addTab(QStringLiteral("Three"));
        bar.resize(360, 40);
        bar.show();
        QCoreApplication::processEvents();

        bar.setLayoutDirection(Qt::LeftToRight);
        QCoreApplication::processEvents();
        const QRect firstLtr = bar.tabRect(0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstLtr.center().x() - 1,
                    firstLtr.center().y())),
            0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstLtr.center().x() + 1,
                    firstLtr.center().y())),
            1);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(bar.width() + 10, 10)),
            bar.count());

        bar.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        const QRect firstRtl = bar.tabRect(0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstRtl.center().x() + 1,
                    firstRtl.center().y())),
            0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstRtl.center().x() - 1,
                    firstRtl.center().y())),
            1);

        bar.setShape(QTabBar::RoundedWest);
        bar.resize(80, 300);
        QCoreApplication::processEvents();
        const QRect firstVertical = bar.tabRect(0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstVertical.center().x(),
                    firstVertical.center().y() - 1)),
            0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstVertical.center().x(),
                    firstVertical.center().y() + 1)),
            1);
    }

    void keepsPagesAndQObjectBudgetsStableAcrossTransfers()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        QSet<QWidget *> expectedPages;
        for (int index = 0; index < 20; ++index) {
            QWidget *page = zzCreatePage(
                QStringLiteral("page-%1").arg(index));
            source.addTab(page, QString::number(index));
            expectedPages.insert(page);
        }

        const qsizetype sourceObjectCount =
            source.findChildren<QObject *>().size();
        const qsizetype targetObjectCount =
            target.findChildren<QObject *>().size();
        const qsizetype timerCount =
            source.findChildren<QTimer *>().size()
            + target.findChildren<QTimer *>().size();
        const qsizetype animationCount =
            source.findChildren<QAbstractAnimation *>().size()
            + target.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            QWidget *page = source.widget(0);
            QVERIFY(page != nullptr);
            QVERIFY(source.transferTabTo(&target, 0));
            const int targetIndex = target.indexOf(page);
            QVERIFY(targetIndex >= 0);
            QVERIFY(target.transferTabTo(&source, targetIndex));
        }

        QSet<QWidget *> actualPages;
        for (int index = 0; index < source.count(); ++index) {
            actualPages.insert(source.widget(index));
        }
        QCOMPARE(actualPages, expectedPages);
        QCOMPARE(source.count(), 20);
        QCOMPARE(target.count(), 0);
        QCOMPARE(
            source.findChildren<QObject *>().size(),
            sourceObjectCount);
        QCOMPARE(
            target.findChildren<QObject *>().size(),
            targetObjectCount);
        QCOMPARE(
            source.findChildren<QTimer *>().size()
                + target.findChildren<QTimer *>().size(),
            timerCount);
        QCOMPARE(
            source.findChildren<QAbstractAnimation *>().size()
                + target.findChildren<QAbstractAnimation *>().size(),
            animationCount);
    }

    void keepsNativeCtrlTabNavigation()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(
            zzCreatePage(QStringLiteral("keyboard-a")),
            QStringLiteral("A"));
        tabs.addTab(
            zzCreatePage(QStringLiteral("keyboard-b")),
            QStringLiteral("B"));
        tabs.setCurrentIndex(0);
        tabs.show();
        tabs.setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        QTest::keyClick(&tabs, Qt::Key_Tab, Qt::ControlModifier);

        QCOMPARE(tabs.currentIndex(), 1);
    }

    void keepsNativeAccessibilityRoles()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(
            zzCreatePage(QStringLiteral("accessible")),
            QStringLiteral("Overview"));
        tabs.setAccessibleName(QStringLiteral("Workspace tabs"));

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(tabs.fluentTabBar());
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::PageTabList);
        QVERIFY(interface->childCount() >= 1);
        QAccessibleInterface *tabInterface = interface->child(0);
        QVERIFY(tabInterface != nullptr);
        QCOMPARE(tabInterface->role(), QAccessible::PageTab);
        QCOMPARE(
            tabInterface->text(QAccessible::Name),
            QStringLiteral("Overview"));
    }

    void doesNotExposeNativeOuterFrame()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzTabWidget tabs;
        tabs.setStyle(&style);
        tabs.fluentTabBar()->setStyle(&style);
        tabs.setPalette(style.standardPalette());
        tabs.addTab(new QWidget, QStringLiteral("Overview"));
        tabs.resize(320, 120);
        tabs.show();
        QCoreApplication::processEvents();

        QVERIFY(tabs.documentMode());
        QImage image(tabs.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(style.standardPalette().color(QPalette::Window));
        QPainter painter(&image);
        tabs.render(&painter);
        painter.end();
        for (int y = 0; y < image.height(); ++y) {
            // 页框关闭后，左边缘不应再与相邻表面产生竖向边线。
            QCOMPARE(image.pixelColor(0, y), image.pixelColor(1, y));
        }
    }
};

QTEST_MAIN(ZzTabControlsTest)

#include "ZzTabControlsTest.moc"
