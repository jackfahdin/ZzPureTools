#include <QtCore/QEvent>
#include <QtCore/QtGlobal>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleOptionFrame>
#include <QtWidgets/QStyleOptionGroupBox>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/** @brief 统计主题传播产生的 StyleChange 事件。 */
class ZzStyleChangeProbe final : public QPushButton
{
public:
    using QPushButton::QPushButton;

    /** @brief 返回已接收的 StyleChange 数量。 */
    [[nodiscard]] int styleChangeCount() const noexcept
    {
        return styleChangeCount_;
    }

protected:
    /** @brief 统计事件后保留 QPushButton 默认行为。 */
    bool event(QEvent *event) override
    {
        if (event != nullptr && event->type() == QEvent::StyleChange) {
            ++styleChangeCount_;
        }
        return QPushButton::event(event);
    }

private:
    int styleChangeCount_ = 0;
};

/** @brief 模拟会自行覆盖整个分组框的基础样式，验证 Fluent 边框契约不依赖其分支。 */
class ZzOpaqueGroupBoxBaseStyle final : public QProxyStyle
{
public:
    ZzOpaqueGroupBoxBaseStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

    /** @brief 在完整分组框入口填色，其余几何与绘制继续使用 Qt。 */
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
        QPainter *painter, const QWidget *widget = nullptr) const override
    {
        if (control == CC_GroupBox) {
            painter->fillRect(option->rect, Qt::magenta);
            return;
        }
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }
};

/** @brief 验证 Fluent Widgets 样式、绘制和缓存传播行为。 */
class ZzFluentStyleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsGroupBoxBorderContinuousBelowTitle()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        for (bool checkable : {false, true}) {
            QStyleOptionGroupBox option;
            option.rect = QRect(0, 0, 260, 120);
            option.state = QStyle::State_Enabled | QStyle::State_On;
            option.palette = style.standardPalette();
            option.text = QStringLiteral("GroupBox");
            option.fontMetrics = QFontMetrics(QFont(QStringLiteral("DejaVu Sans"), 10));
            option.lineWidth = 1;
            option.textAlignment = Qt::AlignLeft;
            option.subControls = QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxLabel;
            if (checkable) option.subControls |= QStyle::SC_GroupBoxCheckBox;
            const QRect label = style.subControlRect(QStyle::CC_GroupBox, &option,
                QStyle::SC_GroupBoxLabel);
            QStyleOptionFrame frame;
            frame.QStyleOption::operator=(option);
            frame.lineWidth = 1;
            frame.rect = style.subControlRect(QStyle::CC_GroupBox, &option,
                QStyle::SC_GroupBoxFrame);
            QImage expected(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            expected.fill(Qt::transparent);
            QPainter framePainter(&expected);
            style.drawPrimitive(QStyle::PE_FrameGroupBox, &frame, &framePainter);
            framePainter.end();
            QImage actual(expected.size(), expected.format());
            actual.fill(Qt::transparent);
            QPainter painter(&actual);
            style.drawComplexControl(QStyle::CC_GroupBox, &option, &painter);
            painter.end();
            // 标题下沿覆盖边框几何时，也不能把整段上边框裁成缺口。
            for (int y = frame.rect.top(); y < frame.rect.top() + 3; ++y) {
                if (expected.pixelColor(label.center().x(), y).alpha() != 0) {
                    QCOMPARE(actual.pixelColor(label.center().x(), y),
                        expected.pixelColor(label.center().x(), y));
                }
            }
        }
    }

    void keepsFlatGroupBoxIndependentOfBaseComplexPainting()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&controller);
        style.setBaseStyle(new ZzOpaqueGroupBoxBaseStyle);
        QStyleOptionGroupBox option;
        option.rect = QRect(0, 0, 160, 80);
        option.state = QStyle::State_Enabled;
        option.palette = style.standardPalette();
        option.features = QStyleOptionFrame::Flat;
        option.subControls = QStyle::SC_GroupBoxFrame;
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawComplexControl(QStyle::CC_GroupBox, &option, &painter);
        painter.end();
        QCOMPARE(image.pixelColor(80, 40).alpha(), 0);
        QCOMPARE(image.pixelColor(0, 40).alpha(), 0);
        QCOMPARE(image.pixelColor(80, 79).alpha(), 0);
        QCOMPARE(image.pixelColor(80, 1), QColor(Qt::white));
    }

    void paintsGroupBoxFrameWithoutFillingContents()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        for (const auto mode : {ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark, ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            for (bool flat : {false, true}) {
                QStyleOptionFrame option;
                option.rect = QRect(0, 0, 160, 80);
                option.state = QStyle::State_Enabled;
                option.palette = style.standardPalette();
                if (flat) option.features |= QStyleOptionFrame::Flat;
                QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                style.drawPrimitive(QStyle::PE_FrameGroupBox, &option, &painter);
                painter.end();
                const QColor stroke = image.pixelColor(80, 1);
                QVERIFY(stroke.alpha() >= 150);
                QCOMPARE(stroke.red(), mode == ZzFluentUI::ZzThemeMode::Light ? 0 : 255);
                QCOMPARE(image.pixelColor(80, 0).alpha(), 0);
                QCOMPARE(image.pixelColor(80, 40).alpha(), 0);
                if (flat) {
                    QCOMPARE(image.pixelColor(0, 40).alpha(), 0);
                    QCOMPARE(image.pixelColor(80, 79).alpha(), 0);
                } else {
                    QCOMPARE(image.pixelColor(1, 40), stroke);
                    QCOMPARE(image.pixelColor(80, 78), stroke);
                    QCOMPARE(image.pixelColor(0, 40).alpha(), 0);
                    QCOMPARE(image.pixelColor(80, 79).alpha(), 0);
                }
            }
        }
    }

    void mapsMetricsAndPalette()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        QCOMPARE(style.pixelMetric(QStyle::PM_ButtonMargin), 12);
        QCOMPARE(
            style.standardPalette().color(QPalette::Window),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::Surface));
    }

    void mapsStandardTextRolesForDarkAndHighContrast()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            const auto snapshot = controller.snapshot();
            const QPalette palette = style.standardPalette();
            const QColor primary = snapshot->color(
                ZzFluentUI::ZzColorToken::TextPrimary);
            const QColor secondary = snapshot->color(
                ZzFluentUI::ZzColorToken::TextSecondary);
            const QColor accent = snapshot->color(
                ZzFluentUI::ZzColorToken::Accent);

            QCOMPARE(palette.color(QPalette::WindowText), primary);
            QCOMPARE(palette.color(QPalette::Text), primary);
            QCOMPARE(palette.color(QPalette::ButtonText), primary);
            QCOMPARE(palette.color(QPalette::ToolTipText), primary);
            QCOMPARE(palette.color(QPalette::PlaceholderText), secondary);
            QCOMPARE(
                palette.color(QPalette::Disabled, QPalette::WindowText),
                secondary);
            QCOMPARE(palette.color(QPalette::Link), accent);
        }
    }

    void providesHighContrastMenuStateColor()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        const auto snapshot = controller.snapshot();

        // 高对比度普通控件沿用黑色底色；菜单由样式专门使用强调色，
        // 因而这里验证菜单可用的强调色与菜单面板确实可区分。
        QVERIFY(snapshot->color(ZzFluentUI::ZzColorToken::Accent)
                != snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
    }

    void appliesPaletteOnConstruction()
    {
        // 模拟 Windows 深色模式下残留的深色应用 palette：构造样式后
        // 必须立即被令牌 palette 覆盖，不能等首次主题切换。
        QPalette foreignPalette;
        foreignPalette.setColor(QPalette::ButtonText, QColor("#ffffff"));
        foreignPalette.setColor(QPalette::Text, QColor("#ffffff"));
        foreignPalette.setColor(QPalette::WindowText, QColor("#ffffff"));
        QApplication::setPalette(foreignPalette);

        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::Light);
        ZzFluentUI::ZzFluentStyle style(&controller);

        const auto snapshot = controller.snapshot();
        const QColor primary = snapshot->color(
            ZzFluentUI::ZzColorToken::TextPrimary);
        QCOMPARE(QApplication::palette().color(QPalette::ButtonText), primary);
        QCOMPARE(QApplication::palette().color(QPalette::Text), primary);
        QCOMPARE(QApplication::palette().color(QPalette::WindowText), primary);

        // 主题切换后继续跟随令牌 palette。
        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(
            QApplication::palette().color(QPalette::ButtonText),
            controller.snapshot()->color(ZzFluentUI::ZzColorToken::TextPrimary));
    }

    void defaultsToFusionBaseStyle()
    {
        // 平台基样式（WindowsVista）会按系统主题为按钮文字取色，
        // 深色系统下与本样式的浅色令牌表面冲突；空基样式必须落到 Fusion。
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QVERIFY(style.baseStyle() != nullptr);
        QCOMPARE(
            QString::fromLatin1(style.baseStyle()->metaObject()->className()),
            QStringLiteral("QFusionStyle"));
    }

    void invalidatesColorCacheWithoutChangingMetric()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const int metric = style.pixelMetric(QStyle::PM_ButtonMargin);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);

        QCOMPARE(
            style.themeRevision(),
            controller.snapshot()->revision());
        QCOMPARE(style.pixelMetric(QStyle::PM_ButtonMargin), metric);
        QCOMPARE(style.iconCacheBytes(), 0);
    }

    void exposesAccessibleFocusPolicy()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        QVERIFY(style.styleHint(QStyle::SH_UnderlineShortcut) >= 0);
        QVERIFY(style.pixelMetric(QStyle::PM_FocusFrameHMargin) >= 2);
    }

    void drawsVisibleHighContrastFocusRing()
    {
        QImage image(
            QSize(64, 32),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QPainter painter(&image);
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::HighContrast,
            QColor(Qt::yellow),
            1,
            true);

        ZzFluentUI::ZzFluentPainter::drawFocusRing(
            &painter,
            QRectF(2, 2, 60, 28),
            snapshot,
            1.0);

        painter.end();
        QVERIFY(image.pixelColor(2, 16) != QColor(Qt::black));
    }

    void drawsSharedSurfacesWithoutLeakingPainterState()
    {
        QImage image(QSize(80, 48), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setPen(QPen(Qt::green, 3.0));
        painter.setBrush(Qt::yellow);
        painter.setRenderHint(QPainter::Antialiasing, false);
        const QPen originalPen = painter.pen();
        const QBrush originalBrush = painter.brush();
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::Light,
            QColor(QStringLiteral("#0067c0")),
            1,
            false);

        ZzFluentUI::ZzFluentPainter::drawPopupSurface(
            &painter,
            QRectF(4, 4, 36, 28),
            snapshot);
        ZzFluentUI::ZzFluentPainter::drawBadgeSurface(
            &painter,
            QRectF(48, 8, 20, 20),
            snapshot,
            ZzFluentUI::ZzColorToken::Success);
        ZzFluentUI::ZzFluentPainter::drawOverlayScrim(
            &painter,
            QRectF(0, 36, 80, 12),
            snapshot);

        QCOMPARE(painter.pen(), originalPen);
        QCOMPARE(painter.brush(), originalBrush);
        QVERIFY(!painter.testRenderHint(QPainter::Antialiasing));
        painter.end();
        QCOMPARE(image.pixelColor(20, 16),
                 snapshot.color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
        QCOMPARE(image.pixelColor(58, 18),
                 snapshot.color(ZzFluentUI::ZzColorToken::Success));
        QVERIFY(image.pixelColor(40, 42).alpha() > 0);
    }

    void cachesTintedResourceIcons()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const ZzFluentUI::ZzIconDescriptor descriptor{
            QStringLiteral(
                ":/zzfluent/tests/ZzFluentTestSquare.svg"),
            true};

        const QPixmap first = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::LeftToRight);
        QVERIFY(!first.isNull());
        const int firstCost = style.iconCacheBytes();
        QVERIFY(firstCost > 0);

        const QPixmap second = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::LeftToRight);
        QCOMPARE(second.cacheKey(), first.cacheKey());
        QCOMPARE(style.iconCacheBytes(), firstCost);

        const QPixmap secondColor = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::red),
            Qt::LeftToRight);
        QVERIFY(!secondColor.isNull());
        const int secondColorCost = style.iconCacheBytes();
        QVERIFY(secondColorCost > firstCost);
        QVERIFY(secondColorCost < firstCost * 2);

        const QPixmap mirrored = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::RightToLeft);
        QVERIFY(!mirrored.isNull());
        QVERIFY(first.toImage() != mirrored.toImage());
        const int mirroredCost = style.iconCacheBytes();
        QVERIFY(mirroredCost > secondColorCost);

        controller.setReducedMotion(true);
        QCOMPARE(
            style.themeRevision(),
            controller.snapshot()->revision());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);
        const QPixmap afterMotionChange = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::LeftToRight);
        QCOMPARE(afterMotionChange.cacheKey(), first.cacheKey());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);

        const QPixmap oversized = style.iconPixmap(
            descriptor,
            QSize(4096, 4096),
            1.0,
            QColor(Qt::green),
            Qt::LeftToRight);
        QVERIFY(oversized.isNull());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QVERIFY(style.iconCacheBytes() > 0);
        QVERIFY(style.iconCacheBytes() < mirroredCost);
    }

    void rendersOriginalSvgAndCustomDescriptorColors()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const auto originalDescriptor =
            ZzFluentUI::ZzIconDescriptor::fromSvgResource(
                QStringLiteral(
                    ":/zzfluent/tests/ZzFluentTestSquare.svg"),
                false,
                ZzFluentUI::ZzIconColorMode::Original);
        const QPixmap original = style.iconPixmap(
            originalDescriptor,
            QSize(16, 16),
            1.0,
            {},
            Qt::LeftToRight);
        QVERIFY(!original.isNull());
        QCOMPARE(
            original.toImage().pixelColor(8, 8),
            QColor(Qt::black));

        const auto customDescriptor =
            ZzFluentUI::ZzIconDescriptor::fromSvgResource(
                QStringLiteral(
                    ":/zzfluent/tests/ZzFluentTestSquare.svg"),
                false,
                ZzFluentUI::ZzIconColorMode::Custom,
                QColor(Qt::magenta));
        const QPixmap custom = style.iconPixmap(
            customDescriptor,
            QSize(16, 16),
            1.0,
            QColor(Qt::green),
            Qt::LeftToRight);
        QVERIFY(!custom.isNull());
        QCOMPARE(
            custom.toImage().pixelColor(8, 8),
            QColor(Qt::magenta));
    }

    void rendersBundledSvgAndFontGlyphs()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const auto bundled =
            ZzFluentUI::ZzIconDescriptor::fromBundledSvg(
                ZzFluentUI::ZzBundledSvgIcon::Sun);
        const QPixmap svg = style.iconPixmap(
            bundled,
            QSize(24, 24),
            1.5,
            QColor(Qt::yellow),
            Qt::LeftToRight);
        QVERIFY(!svg.isNull());

        const auto font =
            ZzFluentUI::ZzIconDescriptor::fromFontIcon(
                ZzFluentUI::ZzFontIcon::House,
                false,
                ZzFluentUI::ZzIconColorMode::Custom,
                QColor(Qt::cyan));
        const QPixmap glyph = style.iconPixmap(
            font,
            QSize(24, 24),
            1.5,
            QColor(Qt::red),
            Qt::LeftToRight);
        QVERIFY(!glyph.isNull());

        const QImage glyphImage = glyph.toImage();
        bool foundCyanPixel = false;
        for (int y = 0; y < glyphImage.height() && !foundCyanPixel; ++y) {
            for (int x = 0; x < glyphImage.width(); ++x) {
                const QColor pixel = glyphImage.pixelColor(x, y);
                if (pixel.alpha() > 200
                    && pixel.red() == 0
                    && pixel.green() == 255
                    && pixel.blue() == 255) {
                    foundCyanPixel = true;
                    break;
                }
            }
        }
        QVERIFY(foundCyanPixel);
    }

    void sendsStyleChangesForGeometryAndMotionUpdates()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzStyleChangeProbe topLevel;
        ZzStyleChangeProbe child(&topLevel);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(topLevel.styleChangeCount(), 0);
        QCOMPARE(child.styleChangeCount(), 0);

        controller.setReducedMotion(true);
        QCOMPARE(topLevel.styleChangeCount(), 1);
        QCOMPARE(child.styleChangeCount(), 1);
        controller.setReducedMotion(true);
        QCOMPARE(topLevel.styleChangeCount(), 1);
        QCOMPARE(child.styleChangeCount(), 1);

        const QFont original = QGuiApplication::font();
        QFont changed = original;
        if (changed.pointSizeF() > 0.0) {
            changed.setPointSizeF(changed.pointSizeF() + 1.0);
        } else {
            changed.setPixelSize(qMax(1, changed.pixelSize()) + 1);
        }
        QGuiApplication::setFont(changed);

        QVERIFY(QTest::qWaitFor([&topLevel, &child] {
            return topLevel.styleChangeCount() == 2 &&
                   child.styleChangeCount() == 2;
        }));
        QCOMPARE(topLevel.styleChangeCount(), 2);
        QCOMPARE(child.styleChangeCount(), 2);
        QGuiApplication::setFont(original);
    }
};

QTEST_MAIN(ZzFluentStyleTest)

#include "ZzFluentStyleTest.moc"
