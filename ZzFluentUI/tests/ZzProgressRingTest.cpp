#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QProxyStyle>
#include <limits>
#include <functional>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzThemeController.h>

namespace {

/** @brief Real widget callback used to exercise synchronous QWidget lifecycle reentry. */
class ZzCenterLifecycleWidget final : public QWidget
{
public:
    std::function<void()> onHide;
protected:
    void hideEvent(QHideEvent *event) override
    {
        QWidget::hideEvent(event);
        if (onHide) onHide();
    }
};

/** @brief 为动画生命周期测试提供确定启用动效的基础样式。 */
class ZzProgressAnimationStyle final : public QProxyStyle
{
public:
    /** @brief 对 Widget 动效返回启用，其余提示委托平台样式。 */
    [[nodiscard]] int styleHint(
        StyleHint hint,
        const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr,
        QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == SH_Widget_Animate) {
            return 1;
        }
        return QProxyStyle::styleHint(
            hint,
            option,
            widget,
            returnData);
    }
};

/** @brief 使用高区分度 palette 渲染单个环形进度。 */
QImage zzRenderRing(
    ZzFluentUI::ZzProgressRing *ring,
    int minimum,
    int maximum,
    int value,
    bool inverted = false)
{
    Q_ASSERT(ring != nullptr);
    QPalette palette = ring->palette();
    palette.setColor(QPalette::Active, QPalette::Mid, Qt::black);
    palette.setColor(QPalette::Inactive, QPalette::Mid, Qt::black);
    palette.setColor(QPalette::Disabled, QPalette::Mid, Qt::darkGray);
    palette.setColor(QPalette::Active, QPalette::Highlight, Qt::red);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, Qt::red);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, Qt::gray);
    palette.setColor(QPalette::Active, QPalette::Accent, Qt::red);
    palette.setColor(QPalette::Inactive, QPalette::Accent, Qt::red);
    palette.setColor(QPalette::Disabled, QPalette::Accent, Qt::gray);
    palette.setColor(QPalette::Active, QPalette::Text, Qt::green);
    palette.setColor(QPalette::Inactive, QPalette::Text, Qt::green);
    ring->setPalette(palette);
    ring->setRange(minimum, maximum);
    ring->setValue(value);
    ring->setInvertedAppearance(inverted);
    ring->resize(80, 80);

    QImage image(ring->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    ring->render(&painter);
    painter.end();
    return image;
}

/** @brief 统计由高区分度 palette 绘制的红色进度像素。 */
int zzRedPixelCount(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > color.green() + 48
                && color.red() > color.blue() + 48
                && color.alpha() > 64) {
                ++count;
            }
        }
    }
    return count;
}

/** @brief 分别统计图像左右半区的红色进度像素。 */
QPair<int, int> zzRedHalfCounts(const QImage &image)
{
    int left = 0;
    int right = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() <= color.green() + 48
                || color.red() <= color.blue() + 48
                || color.alpha() <= 64) {
                continue;
            }
            if (x < image.width() / 2) {
                ++left;
            } else {
                ++right;
            }
        }
    }
    return {left, right};
}

} // namespace

/** @brief 验证环形进度复用 Qt 语义并维持单动画与稳定绘制。 */
class ZzProgressRingTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Missing fractional thickness would lose half-pixel input and notify an incoherent legacy width.
    void fractionalThicknessNormalizesAndNotifiesCoherentState()
    {
        ZzFluentUI::ZzProgressRing ring;
        QVERIFY(ring.setProperty("thickness", 6.5));
        QCOMPARE(ring.property("thickness").toReal(), 6.5);
        QCOMPARE(ring.ringWidth(), 7);
        ring.setRingWidth(9);
        QCOMPARE(ring.property("thickness").toReal(), 9.0);
        ring.setProperty("thickness", std::numeric_limits<qreal>::infinity());
        QCOMPARE(ring.property("thickness").toReal(), 9.0);
        ring.setProperty("thickness", -2.0);
        QCOMPARE(ring.property("thickness").toReal(), 1.0);
        ring.setProperty("thickness", 90.0);
        QCOMPARE(ring.property("thickness").toReal(), 64.0);
        ring.setTextVisible(false);
        ring.setThickness(6);
        const QImage integral = zzRenderRing(&ring, 0, 100, 100);
        ring.setThickness(6.5);
        QVERIFY(integral != zzRenderRing(&ring, 0, 100, 100));
    }

    // Ignoring custom colors, titles, fonts or text hiding must change real rendered pixels.
    void rendersTitleValueAndCustomRingColors()
    {
        ZzFluentUI::ZzProgressRing ring;
        QVERIFY(ring.setProperty("title", QStringLiteral("Build")));
        ring.setProperty("titleColor", QColor(Qt::blue));
        ring.setProperty("valueColor", QColor(Qt::green));
        const QImage titled = zzRenderRing(&ring, 0, 100, 50);
        int blueTop = 0;
        int greenBottom = 0;
        for (int y = 15; y < 65; ++y) {
            for (int x = 15; x < 65; ++x) {
                const QColor color = titled.pixelColor(x, y);
                blueTop += y < 40 && color.blue() > color.red() + 48 && color.blue() > color.green() + 48;
                greenBottom += y >= 40 && color.green() > color.red() + 48 && color.green() > color.blue() + 48;
            }
        }
        QVERIFY(blueTop > 10);
        QVERIFY(greenBottom > 10);
        ring.setTextVisible(false);
        ring.setProperty("ringColor", QColor(Qt::blue));
        ring.setProperty("trackColor", QColor(Qt::green));
        const QImage custom = zzRenderRing(&ring, 0, 100, 50);
        QCOMPARE(zzRedPixelCount(custom), 0);
        QVERIFY(custom.pixelColor(75, 40).blue() > 200);
        QVERIFY(custom.pixelColor(4, 40).green() > 200);
        ring.setProperty("ringColor", QColor());
        QVERIFY(zzRedPixelCount(zzRenderRing(&ring, 0, 100, 50)) > 100);
        ring.setProperty("trackColor", QColor());
        QCOMPARE(zzRenderRing(&ring, 0, 100, 0).pixelColor(4, 40), QColor(Qt::black));
    }

    // Separate fonts and spacing must affect actual glyph bounds; oversized text must stay off the ring.
    void customFontsSpacingAndTextVisibilityAffectPixels()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setTitle(QStringLiteral("I"));
        ring.setTitleColor(Qt::blue);
        ring.setValueColor(Qt::green);
        QFont titleFont;
        titleFont.setPixelSize(11);
        QFont valueFont;
        valueFont.setPixelSize(14);
        ring.setTitleFont(titleFont);
        ring.setValueFont(valueFont);
        const auto bounds = [](const QImage &image, bool blue) {
            QRect result;
            for (int y = 12; y < 68; ++y) {
                for (int x = 12; x < 68; ++x) {
                    const QColor color = image.pixelColor(x, y);
                    const bool matches = blue ? color.blue() > color.red() + 48 && color.blue() > color.green() + 48
                        : color.green() > color.red() + 48 && color.green() > color.blue() + 48;
                    if (matches) result = result.united(QRect(x, y, 1, 1));
                }
            }
            return result;
        };
        ring.setTextSpacing(-5);
        const QImage close = zzRenderRing(&ring, 0, 100, 50);
        const int closeGap = bounds(close, false).top() - bounds(close, true).bottom();
        QCOMPARE(ring.textSpacing(), 0);
        ring.setTextSpacing(10);
        const QImage spaced = zzRenderRing(&ring, 0, 100, 50);
        QVERIFY(bounds(spaced, false).top() - bounds(spaced, true).bottom() >= closeGap + 9);
        titleFont.setPixelSize(18);
        ring.setTitleFont(titleFont);
        QVERIFY(bounds(zzRenderRing(&ring, 0, 100, 50), true).height() > bounds(close, true).height());
        valueFont.setPixelSize(200);
        ring.setValueFont(valueFont);
        const QImage oversized = zzRenderRing(&ring, 0, 100, 50);
        QVERIFY(bounds(oversized, true).isEmpty());
        QVERIFY(bounds(oversized, false).isEmpty());
        ring.setTitleFont(QFont());
        ring.setValueFont(QFont());
        ring.setTextVisible(false);
        const QImage hidden = zzRenderRing(&ring, 0, 100, 50);
        QVERIFY(bounds(hidden, true).isEmpty());
        QVERIFY(bounds(hidden, false).isEmpty());
        ring.setTextVisible(true);
        ring.setTextSpacing(999);
        QCOMPARE(ring.textSpacing(), 100);
        QVERIFY(bounds(zzRenderRing(&ring, 0, 100, 50), true).isEmpty());
    }

    // A raw dangling pointer, delayed layout or failure to own replacement widgets is observable here.
    void ownsAndSynchronizesCenterWidget()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.resize(120, 120);
        QPointer<QWidget> first = new QWidget;
        QVERIFY(ring.setProperty("centerWidget", QVariant::fromValue(first.data())));
        QCOMPARE(first->parentWidget(), &ring);
        QCOMPARE(first->geometry(), QRect(12, 12, 96, 96));
        ring.setProperty("thickness", 10.5);
        QCOMPARE(first->geometry(), QRect(17, 17, 86, 86));
        ring.setTextVisible(false);
        QVERIFY(first->isHidden());
        ring.setTextVisible(true);
        QVERIFY(!first->isHidden());
        ring.setProperty("centerWidget", QVariant::fromValue(&ring));
        QCOMPARE(ring.property("centerWidget").value<QWidget *>(), first.data());
        auto *descendant = new QWidget(first);
        ring.setProperty("centerWidget", QVariant::fromValue(descendant));
        QCOMPARE(ring.property("centerWidget").value<QWidget *>(), first.data());
        auto *second = new QWidget;
        ring.setProperty("centerWidget", QVariant::fromValue(second));
        QVERIFY(first.isNull());
        QWidget other;
        second->setParent(&other);
        QCOMPARE(ring.property("centerWidget").value<QWidget *>(), nullptr);
        auto *third = new QWidget;
        ring.setProperty("centerWidget", QVariant::fromValue(third));
        delete third;
        QCOMPARE(ring.property("centerWidget").value<QWidget *>(), nullptr);
    }

    // take must not detach a center re-adopted by its own synchronous hide handler.
    void takingCenterPreservesReentrantAdoption()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.show();
        auto *center = new ZzCenterLifecycleWidget;
        ring.setCenterWidget(center);
        center->onHide = [&] { ring.setCenterWidget(center); };
        QWidget *taken = ring.takeCenterWidget();
        center->onHide = {};
        QCOMPARE(taken, nullptr);
        QCOMPARE(ring.centerWidget(), center);
        QCOMPARE(center->parentWidget(), &ring);
    }

    // External destruction clears once; a null notification can synchronously install a new center.
    void externalCenterDeletionCanInstallReplacement()
    {
        ZzFluentUI::ZzProgressRing ring;
        auto *center = new QWidget;
        auto *replacement = new QWidget;
        ring.setCenterWidget(center);
        connect(&ring, &ZzFluentUI::ZzProgressRing::centerWidgetChanged, &ring,
            [&](QWidget *current) { if (!current) ring.setCenterWidget(replacement); });
        delete center;
        QCOMPARE(ring.centerWidget(), nullptr);
        QCoreApplication::processEvents();
        QCOMPARE(ring.centerWidget(), replacement);
        QCOMPARE(replacement->parentWidget(), &ring);
    }

    // A destroyed child is still in QObject's parent list until its destructor finishes.
    // Null notifications must not let a slot delete that parent midway through child destruction.
    void externalCenterDeletionCanDeleteRingFromNotification()
    {
        QPointer<ZzFluentUI::ZzProgressRing> ring = new ZzFluentUI::ZzProgressRing;
        auto *center = new QWidget;
        ring->setCenterWidget(center);
        bool childDestructorActive = false;
        bool unsafeNotification = false;
        connect(ring, &ZzFluentUI::ZzProgressRing::centerWidgetChanged,
            this, [&](QWidget *current) {
                if (!current) {
                    unsafeNotification |= childDestructorActive;
                    delete ring.data();
                }
            });
        childDestructorActive = true;
        delete center;
        childDestructorActive = false;
        QVERIFY(!unsafeNotification);
        QCoreApplication::processEvents();
        QVERIFY(ring.isNull());
    }

    // take returns no dangling pointer if the ownership-transfer notification deletes the released widget.
    void takeCenterHandlesDeletionDuringNotification()
    {
        ZzFluentUI::ZzProgressRing ring;
        QPointer<QWidget> center = new QWidget;
        ring.setCenterWidget(center);
        connect(&ring, &ZzFluentUI::ZzProgressRing::centerWidgetChanged, &ring,
            [&](QWidget *current) { if (!current) delete center.data(); });
        QCOMPARE(ring.takeCenterWidget(), nullptr);
        QVERIFY(center.isNull());
    }

    // Slots can replace centers during destruction or notification, and can delete the ring itself.
    void centerReplacementAndNotificationAreReentrant()
    {
        ZzFluentUI::ZzProgressRing ring;
        auto *old = new QWidget;
        auto *winner = new QWidget;
        auto *unused = new QWidget;
        ring.setCenterWidget(old);
        connect(old, &QObject::destroyed, &ring, [&] { ring.setCenterWidget(winner); });
        ring.setCenterWidget(unused);
        QCOMPARE(ring.centerWidget(), winner);
        QCOMPARE(unused->parentWidget(), nullptr);
        delete unused;
        auto *taken = ring.takeCenterWidget();
        QCOMPARE(taken, winner);
        QCOMPARE(taken->parentWidget(), nullptr);
        QVERIFY(taken->isHidden());
        delete taken;

        QPointer<ZzFluentUI::ZzProgressRing> victim = new ZzFluentUI::ZzProgressRing;
        connect(victim, &ZzFluentUI::ZzProgressRing::centerWidgetChanged, victim,
            [&] { delete victim.data(); });
        victim->setCenterWidget(new QWidget);
        QVERIFY(victim.isNull());
    }

    // External setParent must finish before notification slots can re-adopt that widget.
    void externalReparentNotifiesAfterQtCompletesAndSuppressesStaleSignals()
    {
        ZzFluentUI::ZzProgressRing ring;
        QWidget other;
        auto *center = new QWidget;
        ring.setCenterWidget(center);
        bool inSetParent = false;
        bool sawUnsafeNotification = false;
        const auto connection = connect(&ring, &ZzFluentUI::ZzProgressRing::centerWidgetChanged,
            &ring, [&](QWidget *current) {
                if (!current) {
                    sawUnsafeNotification |= inSetParent;
                    ring.setCenterWidget(center);
                }
            });
        inSetParent = true;
        center->setParent(&other);
        inSetParent = false;
        QCOMPARE(ring.centerWidget(), nullptr);
        QCoreApplication::processEvents();
        QVERIFY(!sawUnsafeNotification);
        QCOMPARE(ring.centerWidget(), center);
        QCOMPARE(center->parentWidget(), &ring);
        disconnect(connection);
        QSignalSpy spy(&ring, &ZzFluentUI::ZzProgressRing::centerWidgetChanged);
        center->setParent(&other);
        auto *replacement = new QWidget;
        ring.setCenterWidget(replacement);
        QCoreApplication::processEvents();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ring.centerWidget(), replacement);
    }

    // A deleted sender and recursive thickness changes must not yield stale legacy notifications.
    void thicknessSignalsTolerateDeletionAndRecursiveChanges()
    {
        ZzFluentUI::ZzProgressRing ring;
        QSignalSpy widthSpy(&ring, &ZzFluentUI::ZzProgressRing::ringWidthChanged);
        connect(&ring, &ZzFluentUI::ZzProgressRing::thicknessChanged, &ring, [&](qreal thickness) {
            QCOMPARE(ring.thickness(), thickness);
            QCOMPARE(ring.ringWidth(), qRound(thickness));
            if (thickness == 6.5) ring.setThickness(9);
        });
        ring.setThickness(6.5);
        QCOMPARE(ring.thickness(), 9.0);
        QCOMPARE(widthSpy.count(), 1);
        QCOMPARE(widthSpy.at(0).at(0).toInt(), 9);
        QPointer<ZzFluentUI::ZzProgressRing> victim = new ZzFluentUI::ZzProgressRing;
        connect(victim, &ZzFluentUI::ZzProgressRing::thicknessChanged, victim,
            [&] { delete victim.data(); });
        victim->setThickness(8.5);
        QVERIFY(victim.isNull());
    }

    // A recursive fractional change with the same rounded result must still publish 6 -> 7.
    void recursiveFractionalThicknessPublishesEffectiveIntegerChange()
    {
        ZzFluentUI::ZzProgressRing ring;
        QSignalSpy widthSpy(&ring, &ZzFluentUI::ZzProgressRing::ringWidthChanged);
        connect(&ring, &ZzFluentUI::ZzProgressRing::thicknessChanged, &ring,
            [&](qreal thickness) {
                if (thickness == 6.5) ring.setThickness(7.2);
            });
        ring.setThickness(6.5);
        QCOMPARE(ring.thickness(), 7.2);
        QCOMPARE(ring.ringWidth(), 7);
        QCOMPARE(widthSpy.count(), 1);
        QCOMPARE(widthSpy.at(0).at(0).toInt(), 7);
    }

    /** @brief 放大圆环时中央数值随之放大，显式字体则保持调用方的选择。 */
    void scalesValueTextAndHonorsExplicitFont()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setValue(68);
        QPalette palette = ring.palette();
        palette.setColor(QPalette::All, QPalette::Text, Qt::green);
        palette.setColor(QPalette::All, QPalette::Window, Qt::black);
        ring.setPalette(palette);
        QRect textBounds;
        const auto textPixels = [&ring, &textBounds](int extent) {
            textBounds = {};
            ring.resize(extent, extent);
            QImage image(ring.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            ring.render(&painter);
            painter.end();
            int count = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if (color.green() > color.red() + 48
                        && color.green() > color.blue() + 48) {
                        ++count;
                        textBounds = textBounds.united(QRect(x, y, 1, 1));
                    }
                }
            }
            return count;
        };
        const int small = textPixels(80);
        const int large = textPixels(160);
        QVERIFY(small > 0);
        QVERIFY(large > small * 2);
        QFont custom = ring.font();
        custom.setPixelSize(14);
        ring.setFont(custom);
        QVERIFY(textPixels(80) > 0);
        const QRect customSmallBounds = textBounds;
        const int customLarge = textPixels(160);
        // 相同字号允许亚像素起点产生覆盖率差异，但字形尺寸不能随环放大。
        QVERIFY(qAbs(customSmallBounds.width() - textBounds.width()) <= 1);
        QVERIFY(qAbs(customSmallBounds.height() - textBounds.height()) <= 1);
        ring.setFont(QFont());
        QVERIFY(textPixels(160) > customLarge * 2);
        ring.setFormat(QString(100, QLatin1Char('W')));
        QVERIFY(textPixels(80) > 0);
        QVERIFY(QRect(19, 19, 42, 42).contains(textBounds));
        QCOMPARE(textPixels(24), 0);
    }

    /** @brief 改速保持当前相位、动画实例和业务值，非法周期不能造成零除。 */
    void changesDurationWithoutJumpingOrAllocating()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, new ZzProgressAnimationStyle);
        ZzFluentUI::ZzProgressRing ring;
        ring.setStyle(&style);
        ring.setRange(0, 0);
        ring.show();
        auto *animation = ring.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        QSignalSpy durations(&ring, &ZzFluentUI::ZzProgressRing::indeterminateDurationChanged);
        const qsizetype objects = ring.findChildren<QObject *>().size();
        // 静态元属性缺失时 setProperty 返回 false，不能把动态属性误当实现。
        QVERIFY(ring.setProperty("indeterminateDuration", 1600));
        QCOMPARE(animation->duration(), 1600);
        QCOMPARE(ring.indeterminateDuration(), 1600);
        QCOMPARE(durations.count(), 1);
        ring.setIndeterminateDuration(1600);
        QCOMPARE(durations.count(), 1);
        animation->setCurrentTime(400);
        const qreal before = animation->currentValue().toReal();
        const int value = ring.value();
        QVERIFY(ring.setProperty("indeterminateDuration", 800));
        QCOMPARE(animation->duration(), 800);
        QVERIFY(qAbs(animation->currentValue().toReal() - before) < 0.002);
        QCOMPARE(ring.value(), value);
        QCOMPARE(ring.findChildren<QObject *>().size(), objects);
        QCOMPARE(ring.findChild<QVariantAnimation *>(), animation);
        QVERIFY(ring.setProperty("indeterminateDuration", 0));
        QCOMPARE(animation->duration(), 200);
        QVERIFY(ring.setProperty("indeterminateDuration", 100000));
        QCOMPARE(animation->duration(), 60000);
        ring.hide();
        QVERIFY(ring.setProperty("indeterminateDuration", 800));
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    }

    void exposesStableDefaults()
    {
        ZzFluentUI::ZzProgressRing ring;

        QCOMPARE(ring.minimum(), 0);
        QCOMPARE(ring.maximum(), 100);
        QCOMPARE(ring.value(), 0);
        QCOMPARE(ring.ringWidth(), 6);
        QCOMPARE(ring.sizeHint(), QSize(120, 120));
        QCOMPARE(ring.minimumSizeHint(), QSize(48, 48));
        QCOMPARE(ring.indeterminateDuration(), 800);
        QCOMPARE(ring.findChildren<QVariantAnimation *>().size(), 1);
        QVERIFY(ring.findChildren<QTimer *>().isEmpty());
    }

    void emitsRingWidthOnlyForEffectiveChanges()
    {
        ZzFluentUI::ZzProgressRing ring;
        QSignalSpy spy(
            &ring,
            &ZzFluentUI::ZzProgressRing::ringWidthChanged);

        ring.setRingWidth(6);
        QCOMPARE(spy.count(), 0);
        ring.setRingWidth(0);
        QCOMPARE(ring.ringWidth(), 1);
        QCOMPARE(spy.count(), 1);
        ring.setRingWidth(-10);
        QCOMPARE(spy.count(), 1);
        ring.setRingWidth(64);
        QCOMPARE(ring.ringWidth(), 64);
        QCOMPARE(spy.count(), 2);
        ring.setRingWidth(100);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(ring.minimumSizeHint(), QSize(136, 136));
        QCOMPARE(ring.sizeHint(), QSize(136, 136));
    }

    void preservesNativeRangeValueAndSignalSemantics()
    {
        ZzFluentUI::ZzProgressRing ring;
        QSignalSpy valueSpy(&ring, &QProgressBar::valueChanged);

        ring.setRange(20, 120);
        QCOMPARE(ring.minimum(), 20);
        QCOMPARE(ring.maximum(), 120);
        valueSpy.clear();
        ring.setValue(70);
        QCOMPARE(valueSpy.count(), 1);
        QCOMPARE(ring.value(), 70);
        ring.setValue(70);
        QCOMPARE(valueSpy.count(), 1);
        ring.setMinimum(80);
        QCOMPARE(ring.minimum(), 80);
        QCOMPARE(ring.maximum(), 120);
        QVERIFY(ring.value() < ring.minimum());
        ring.setMaximum(60);
        QCOMPARE(ring.minimum(), 60);
        QCOMPARE(ring.maximum(), 60);
    }

    void rendersDeterminateIndeterminateAndEqualRanges()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setTextVisible(false);

        const int emptyCount = zzRedPixelCount(
            zzRenderRing(&ring, 20, 120, 20));
        const int halfCount = zzRedPixelCount(
            zzRenderRing(&ring, 20, 120, 70));
        const int fullCount = zzRedPixelCount(
            zzRenderRing(&ring, 20, 120, 120));
        const int busyCount = zzRedPixelCount(
            zzRenderRing(&ring, 0, 0, 0));
        const QImage equalImage = zzRenderRing(&ring, 5, 5, 5);

        QCOMPARE(emptyCount, 0);
        QVERIFY(halfCount > emptyCount);
        QVERIFY(fullCount > halfCount);
        QVERIFY(busyCount > emptyCount);
        QVERIFY(busyCount < fullCount);
        QVERIFY(!equalImage.isNull());

        ring.setTextVisible(true);
        ring.setFormat(QString(400, QLatin1Char('W')));
        QVERIFY(!zzRenderRing(&ring, 0, 100, 72).isNull());
    }

    void invertedAppearanceReversesArcDirection()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setTextVisible(false);
        const QPair<int, int> normal = zzRedHalfCounts(
            zzRenderRing(&ring, 0, 100, 25, false));
        const QPair<int, int> inverted = zzRedHalfCounts(
            zzRenderRing(&ring, 0, 100, 25, true));

        QVERIFY(normal.second > normal.first);
        QVERIFY(inverted.first > inverted.second);
    }

    void visuallyDistinguishesDisabledState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzProgressRing ring;
        ring.setStyle(&style);
        ring.setPalette(style.standardPalette());
        ring.setTextVisible(false);
        ring.setRange(0, 0);
        ring.resize(80, 80);
        const auto render = [&ring] {
            QImage image(
                ring.size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            ring.render(&painter);
            painter.end();
            return image;
        };

        const QImage enabled = render();
        ring.setEnabled(false);
        const QImage disabled = render();
        qsizetype differentPixels = 0;
        for (int y = 0; y < enabled.height(); ++y) {
            for (int x = 0; x < enabled.width(); ++x) {
                if (enabled.pixel(x, y) != disabled.pixel(x, y)) {
                    ++differentPixels;
                }
            }
        }
        QVERIFY(differentPixels > 100);
    }

    void startsAndStopsOneAnimationFromLifecycle()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzProgressAnimationStyle);
        ZzFluentUI::ZzProgressRing ring;
        ring.setStyle(&style);
        ring.setRange(0, 0);
        auto *animation = ring.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);

        ring.show();
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        ring.hide();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        ring.show();
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        ring.setEnabled(false);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        ring.setEnabled(true);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        ring.setRange(0, 100);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        ring.setRange(0, 0);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        controller.setReducedMotion(true);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Stopped;
        }));
    }

    void repeatedStateChangesDoNotGrowObjects()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzProgressAnimationStyle);
        ZzFluentUI::ZzProgressRing ring;
        ring.setStyle(&style);
        ring.show();
        const qsizetype descendants = ring.findChildren<QObject *>().size();
        const qsizetype animations =
            ring.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = ring.findChildren<QTimer *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            if ((iteration % 2) == 0) {
                ring.setRange(0, 0);
            } else {
                ring.setRange(20, 120);
                ring.setValue(20 + (iteration % 101));
            }
        }

        QCOMPARE(ring.findChildren<QObject *>().size(), descendants);
        QCOMPARE(
            ring.findChildren<QAbstractAnimation *>().size(),
            animations);
        QCOMPARE(ring.findChildren<QTimer *>().size(), timers);
        QCOMPARE(animations, 1);
        QCOMPARE(timers, 0);
        ring.hide();
        QCOMPARE(
            ring.findChild<QVariantAnimation *>()->state(),
            QAbstractAnimation::Stopped);
    }

    void exposesProgressBarAccessibility()
    {
        ZzFluentUI::ZzProgressRing ring;
        ring.setAccessibleName(QStringLiteral("Build progress"));
        ring.setRange(10, 90);
        ring.setValue(40);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&ring);

        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::ProgressBar);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Build progress"));
        QAccessibleValueInterface *valueInterface =
            interface->valueInterface();
        QVERIFY(valueInterface != nullptr);
        QCOMPARE(valueInterface->minimumValue().toInt(), 10);
        QCOMPARE(valueInterface->maximumValue().toInt(), 90);
        QCOMPARE(valueInterface->currentValue().toInt(), 40);
    }

    void destroysRunningAnimationWithoutDeferredCallbacks()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzProgressAnimationStyle);
        auto *ring = new ZzFluentUI::ZzProgressRing;
        ring->setStyle(&style);
        ring->setRange(0, 0);
        ring->show();
        auto *animation = ring->findChild<QVariantAnimation *>();
        if (animation == nullptr) {
            delete ring;
            QFAIL("未找到环形进度的不确定状态动画");
        }
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));

        ring->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(ZzProgressRingTest)

#include "ZzProgressRingTest.moc"
