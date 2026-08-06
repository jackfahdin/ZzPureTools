#include "ZzExampleSmokeControllerPrivate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QFontInfo>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTextEdit>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

constexpr int zzScreenshotLogicalWidth = 1280;
constexpr int zzScreenshotLogicalHeight = 800;
constexpr int zzScreenshotTextPadding = 3;
constexpr int zzScreenshotChannelTolerance = 3;
constexpr qreal zzScreenshotReferenceDifferenceRatio = 0.005;
constexpr qreal zzScreenshotCompatibilityDifferenceRatio = 0.02;
constexpr int zzScreenshotReferenceQtMajor = 6;
constexpr int zzScreenshotReferenceQtMinor = 11;

/** @brief 保存一次综合示例截图比较的统计和差异图。 */
struct ZzExampleScreenshotComparison final
{
    qsizetype comparedPixels = 0;
    qsizetype differentPixels = 0;
    QImage difference;
};

/** @brief 将路由表中的 UTF-8 常量转换为 Qt 字符串。 */
[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

/** @brief 返回关闭场景对应的 QMessageBox 按钮角色。 */
[[nodiscard]] QMessageBox::ButtonRole zzCloseButtonRole(
    ZzExampleSmokeScenario scenario)
{
    switch (scenario) {
    case ZzExampleSmokeScenario::CloseCancel:
        return QMessageBox::RejectRole;
    case ZzExampleSmokeScenario::CloseMinimize:
        return QMessageBox::ActionRole;
    case ZzExampleSmokeScenario::CloseConfirm:
        return QMessageBox::AcceptRole;
    default:
        return QMessageBox::InvalidRole;
    }
}

/** @brief 返回当前 Qt minor 对应的综合示例非文字差异上限。 */
[[nodiscard]] constexpr qreal zzScreenshotMaximumDifferenceRatio()
{
    if constexpr (
        QT_VERSION_MAJOR == zzScreenshotReferenceQtMajor
        && QT_VERSION_MINOR == zzScreenshotReferenceQtMinor) {
        return zzScreenshotReferenceDifferenceRatio;
    }
    return zzScreenshotCompatibilityDifferenceRatio;
}

/** @brief 把子控件逻辑矩形映射到综合示例窗口。 */
[[nodiscard]] QRect zzMapToWindow(
    const QWidget *widget,
    const QRect &rect,
    const QWidget *window)
{
    return QRect(widget->mapTo(window, rect.topLeft()), rect.size());
}

/** @brief 将外扩后的逻辑文字矩形加入物理像素遮罩。 */
void zzPaintTextMask(
    QPainter *painter,
    const QWidget *widget,
    QRect rect,
    const QWidget *window)
{
    if (rect.isEmpty()) {
        return;
    }
    painter->fillRect(
        zzMapToWindow(widget, rect, window).adjusted(
            -zzScreenshotTextPadding,
            -zzScreenshotTextPadding,
            zzScreenshotTextPadding,
            zzScreenshotTextPadding),
        Qt::white);
}

/** @brief 为单行或自动换行文字计算实际字体边界。 */
[[nodiscard]] QRect zzTextBounds(
    const QWidget *widget,
    const QRect &bounds,
    int alignment,
    const QString &text)
{
    if (bounds.isEmpty() || text.isEmpty()) {
        return {};
    }
    return widget->fontMetrics().boundingRect(
        bounds,
        alignment,
        text);
}

/** @brief 递归遮罩 item view 中当前可见索引的展示文字。 */
void zzMaskVisibleIndexes(
    QPainter *painter,
    QAbstractItemView *view,
    const QModelIndex &parent,
    const QWidget *window)
{
    QAbstractItemModel *model = view->model();
    if (model == nullptr) {
        return;
    }
    for (int row = 0; row < model->rowCount(parent); ++row) {
        for (int column = 0; column < model->columnCount(parent); ++column) {
            const QModelIndex index = model->index(row, column, parent);
            const QRect itemRect = view->visualRect(index);
            const QString text = index.data(Qt::DisplayRole).toString();
            if (!text.isEmpty() && itemRect.isValid()
                && itemRect.intersects(view->viewport()->rect())) {
                const QRect textRect = zzTextBounds(
                    view->viewport(),
                    itemRect.adjusted(4, 1, -4, -1),
                    Qt::AlignLeading | Qt::AlignVCenter,
                    text).adjusted(-36, 0, 36, 0);
                zzPaintTextMask(
                    painter,
                    view->viewport(),
                    textRect,
                    window);
            }
            if (column == 0 && model->hasChildren(index)) {
                zzMaskVisibleIndexes(painter, view, index, window);
            }
        }
    }
}

/** @brief 为综合窗口内可见 Qt 控件建立字体差异遮罩。 */
[[nodiscard]] QImage zzBuildExampleTextMask(
    ZzPureTools::ZzApplicationWindow &window,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzScreenshotLogicalWidth * dpr),
        qRound(zzScreenshotLogicalHeight * dpr));
    QImage mask(physicalSize, QImage::Format_Grayscale8);
    mask.setDevicePixelRatio(dpr);
    mask.fill(0);
    QPainter painter(&mask);

    const auto widgets = window.findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (!widget->isVisible()) {
            continue;
        }
        if (auto *label = qobject_cast<QLabel *>(widget)) {
            int flags = static_cast<int>(label->alignment());
            if (label->wordWrap()) {
                flags |= Qt::TextWordWrap;
            }
            zzPaintTextMask(
                &painter,
                label,
                zzTextBounds(
                    label,
                    label->contentsRect(),
                    flags,
                    label->text()),
                &window);
            continue;
        }
        if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
            zzPaintTextMask(
                &painter,
                button,
                zzTextBounds(
                    button,
                    button->contentsRect().adjusted(6, 2, -6, -2),
                    Qt::AlignCenter,
                    button->text()),
                &window);
            continue;
        }
        if (auto *editor = qobject_cast<QLineEdit *>(widget)) {
            const QString text = editor->displayText().isEmpty()
                ? editor->placeholderText() : editor->displayText();
            zzPaintTextMask(
                &painter,
                editor,
                zzTextBounds(
                    editor,
                    editor->contentsRect().adjusted(4, 1, -4, -1),
                    static_cast<int>(editor->alignment() | Qt::AlignVCenter),
                    text),
                &window);
            continue;
        }
        if (auto *combo = qobject_cast<QComboBox *>(widget)) {
            zzPaintTextMask(
                &painter,
                combo,
                zzTextBounds(
                    combo,
                    combo->contentsRect().adjusted(8, 1, -28, -1),
                    Qt::AlignLeading | Qt::AlignVCenter,
                    combo->currentText()),
                &window);
            continue;
        }
        if (auto *view = qobject_cast<QAbstractItemView *>(widget)) {
            zzMaskVisibleIndexes(&painter, view, {}, &window);
            continue;
        }
        if (auto *tabs = qobject_cast<QTabBar *>(widget)) {
            for (int index = 0; index < tabs->count(); ++index) {
                zzPaintTextMask(
                    &painter,
                    tabs,
                    zzTextBounds(
                        tabs,
                        tabs->tabRect(index),
                        Qt::AlignCenter,
                        tabs->tabText(index)),
                    &window);
            }
            continue;
        }
        if (auto *progress = qobject_cast<QProgressBar *>(widget)) {
            if (progress->isTextVisible()) {
                zzPaintTextMask(
                    &painter,
                    progress,
                    zzTextBounds(
                        progress,
                        progress->contentsRect(),
                        Qt::AlignCenter,
                        progress->text()),
                    &window);
            }
            continue;
        }
        if (auto *group = qobject_cast<QGroupBox *>(widget)) {
            zzPaintTextMask(
                &painter,
                group,
                zzTextBounds(
                    group,
                    group->contentsRect().adjusted(8, 0, -8, 0),
                    Qt::AlignLeading | Qt::AlignTop,
                    group->title()),
                &window);
            continue;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(widget)) {
            if (!textEdit->toPlainText().isEmpty()) {
                zzPaintTextMask(
                    &painter,
                    textEdit->viewport(),
                    textEdit->viewport()->rect(),
                    &window);
            }
        }
    }
    painter.end();
    return mask;
}

/** @brief 将完整综合窗口渲染到指定 DPR 的固定物理画布。 */
[[nodiscard]] QImage zzRenderExampleWindow(
    ZzPureTools::ZzApplicationWindow &window,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzScreenshotLogicalWidth * dpr),
        qRound(zzScreenshotLogicalHeight * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    window.render(&painter);
    painter.end();
    return image;
}

/** @brief 验证截图不是透明或单色空画布。 */
[[nodiscard]] bool zzHasVisualContent(const QImage &image)
{
    int minimumLuma = 255;
    int maximumLuma = 0;
    bool hasOpaquePixel = false;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(
            image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            if (qAlpha(pixel) == 0) {
                continue;
            }
            hasOpaquePixel = true;
            const int luma = (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3;
            minimumLuma = std::min(minimumLuma, luma);
            maximumLuma = std::max(maximumLuma, luma);
        }
    }
    return hasOpaquePixel && maximumLuma - minimumLuma >= 8;
}

/** @brief 返回截图遮罩覆盖的物理像素数量。 */
[[nodiscard]] qsizetype zzMaskedPixelCount(const QImage &mask)
{
    qsizetype result = 0;
    for (int y = 0; y < mask.height(); ++y) {
        const uchar *line = mask.constScanLine(y);
        for (int x = 0; x < mask.width(); ++x) {
            if (line[x] != 0) {
                ++result;
            }
        }
    }
    return result;
}

/** @brief 比较未被文字遮罩覆盖的截图像素并生成洋红差异证据。 */
[[nodiscard]] ZzExampleScreenshotComparison zzCompareExampleImages(
    const QImage &expected,
    const QImage &actual,
    const QImage &mask)
{
    ZzExampleScreenshotComparison result;
    result.difference = QImage(
        actual.size(), QImage::Format_ARGB32_Premultiplied);
    result.difference.fill(Qt::transparent);
    const QImage expectedArgb = expected.convertToFormat(
        QImage::Format_ARGB32_Premultiplied);
    const QImage actualArgb = actual.convertToFormat(
        QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < actual.height(); ++y) {
        const auto *expectedLine = reinterpret_cast<const QRgb *>(
            expectedArgb.constScanLine(y));
        const auto *actualLine = reinterpret_cast<const QRgb *>(
            actualArgb.constScanLine(y));
        const uchar *maskLine = mask.constScanLine(y);
        auto *differenceLine = reinterpret_cast<QRgb *>(
            result.difference.scanLine(y));
        for (int x = 0; x < actual.width(); ++x) {
            if (maskLine[x] != 0) {
                continue;
            }
            ++result.comparedPixels;
            const QRgb expectedPixel = expectedLine[x];
            const QRgb actualPixel = actualLine[x];
            const bool different =
                std::abs(qRed(expectedPixel) - qRed(actualPixel))
                    > zzScreenshotChannelTolerance
                || std::abs(qGreen(expectedPixel) - qGreen(actualPixel))
                    > zzScreenshotChannelTolerance
                || std::abs(qBlue(expectedPixel) - qBlue(actualPixel))
                    > zzScreenshotChannelTolerance
                || std::abs(qAlpha(expectedPixel) - qAlpha(actualPixel))
                    > zzScreenshotChannelTolerance;
            if (different) {
                ++result.differentPixels;
                differenceLine[x] = qRgba(255, 0, 255, 255);
            }
        }
    }
    return result;
}

/** @brief 返回三种显式主题及其稳定基线文件名。 */
[[nodiscard]] constexpr auto zzScreenshotThemes()
{
    using ZzTheme = std::pair<ZzFluentUI::ZzThemeMode, const char *>;
    return std::array<ZzTheme, 3>{
        ZzTheme{ZzFluentUI::ZzThemeMode::Light, "light"},
        ZzTheme{ZzFluentUI::ZzThemeMode::Dark, "dark"},
        ZzTheme{ZzFluentUI::ZzThemeMode::HighContrast, "high-contrast"}};
}

} // namespace

ZzExampleSmokeControllerPrivate::ZzExampleSmokeControllerPrivate(
    bool enabled,
    ZzPureTools::ZzPureApplication *pureApplication,
    std::shared_ptr<ZzExampleApplicationContext> applicationContext)
    : application(pureApplication)
    , context(std::move(applicationContext))
    , scenario(readScenario(enabled))
{
    Q_ASSERT(application != nullptr);
    Q_ASSERT(context != nullptr);
    if (scenario == ZzExampleSmokeScenario::MultiWindow
        || scenario == ZzExampleSmokeScenario::CloseConfirm) {
        application->setQuitOnLastWindowClosed(false);
    }
}

bool ZzExampleSmokeControllerPrivate::closeGuardEnabled() const noexcept
{
    return scenario == ZzExampleSmokeScenario::Disabled
        || scenario == ZzExampleSmokeScenario::CloseCancel
        || scenario == ZzExampleSmokeScenario::CloseMinimize
        || scenario == ZzExampleSmokeScenario::CloseConfirm;
}

void ZzExampleSmokeControllerPrivate::windowAttached(
    ZzPureTools::ZzApplicationWindow &window)
{
    if (scheduled || scenario == ZzExampleSmokeScenario::Disabled) {
        return;
    }
    scheduled = true;
    switch (scenario) {
    case ZzExampleSmokeScenario::Routes:
        scheduleRouteSmoke(window);
        break;
    case ZzExampleSmokeScenario::MultiWindow:
        scheduleMultiWindowSmoke(window);
        break;
    case ZzExampleSmokeScenario::CloseCancel:
    case ZzExampleSmokeScenario::CloseMinimize:
    case ZzExampleSmokeScenario::CloseConfirm:
        scheduleCloseGuardSmoke(window);
        break;
    case ZzExampleSmokeScenario::Screenshot:
        scheduleScreenshotSmoke(window);
        break;
    case ZzExampleSmokeScenario::Invalid:
        QTimer::singleShot(0, application, [this] {
            fail("unsupported smoke scenario");
        });
        break;
    case ZzExampleSmokeScenario::Disabled:
        break;
    }
}

ZzExampleSmokeScenario ZzExampleSmokeControllerPrivate::readScenario(
    bool enabled)
{
    if (!enabled) {
        return ZzExampleSmokeScenario::Disabled;
    }
    const QString value = qEnvironmentVariable(
        "ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO").trimmed();
    if (value.isEmpty() || value == QStringLiteral("routes")) {
        return ZzExampleSmokeScenario::Routes;
    }
    if (value == QStringLiteral("multi-window")) {
        return ZzExampleSmokeScenario::MultiWindow;
    }
    if (value == QStringLiteral("close-cancel")) {
        return ZzExampleSmokeScenario::CloseCancel;
    }
    if (value == QStringLiteral("close-minimize")) {
        return ZzExampleSmokeScenario::CloseMinimize;
    }
    if (value == QStringLiteral("close-confirm")) {
        return ZzExampleSmokeScenario::CloseConfirm;
    }
    if (value == QStringLiteral("screenshot")) {
        return ZzExampleSmokeScenario::Screenshot;
    }
    return ZzExampleSmokeScenario::Invalid;
}

void ZzExampleSmokeControllerPrivate::scheduleRouteSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        auto *controller = window.navigationController();
        if (controller == nullptr) {
            fail("route smoke has no navigation controller");
            return;
        }
        for (const auto &route : ZzExampleRouteCatalog::routes()) {
            auto result = controller->navigate(
                ZzPureTools::ZzRouteId(zzFromUtf8(route.routeId)));
            if (!result) {
                fail("route smoke navigation failed");
                return;
            }
        }
    });
}

void ZzExampleSmokeControllerPrivate::scheduleMultiWindowSmoke(
    ZzPureTools::ZzApplicationWindow &firstWindow)
{
    QTimer::singleShot(0, &firstWindow, [this, &firstWindow] {
        if (application->windowCount() != 1) {
            fail("multi-window smoke did not start with one window");
            return;
        }
        auto secondResult = application->createWindow();
        if (!secondResult) {
            fail("multi-window smoke could not create a second window");
            return;
        }
        auto *secondWindow = secondResult.value();
        auto *firstNavigation = firstWindow.navigationController();
        auto *secondNavigation = secondWindow->navigationController();
        auto *firstShell = ZzExampleWindowShell::attachedTo(firstWindow);
        auto *secondShell = ZzExampleWindowShell::attachedTo(*secondWindow);
        if (application->windowCount() != 2
            || firstNavigation == nullptr || secondNavigation == nullptr
            || firstNavigation == secondNavigation
            || firstWindow.windowAgent() == nullptr
            || secondWindow->windowAgent() == nullptr
            || firstWindow.windowAgent() == secondWindow->windowAgent()
            || firstShell == nullptr || secondShell == nullptr
            || firstShell == secondShell) {
            fail("multi-window smoke found shared window-owned state");
            return;
        }

        auto *firstActivity = firstWindow.findChild<QListView *>(
            QStringLiteral("zzExampleActivityList"));
        auto *secondActivity = secondWindow->findChild<QListView *>(
            QStringLiteral("zzExampleActivityList"));
        if (firstActivity == nullptr || secondActivity == nullptr
            || firstActivity->model() != &context->activityModel()
            || secondActivity->model() != &context->activityModel()) {
            fail("multi-window smoke did not share the activity model");
            return;
        }

        const int activityRows = context->activityModel().rowCount();
        auto firstRoute = firstNavigation->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("controls")));
        auto secondRoute = secondNavigation->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("settings")));
        const bool secondDockWasVisible =
            secondShell->isActivityDockVisible();
        firstShell->setActivityDockVisible(!secondDockWasVisible);
        if (!firstRoute || !secondRoute
            || firstNavigation->currentRoute().value()
                != QStringLiteral("controls")
            || secondNavigation->currentRoute().value()
                != QStringLiteral("settings")
            || secondShell->isActivityDockVisible()
                != secondDockWasVisible
            || context->activityModel().rowCount() < activityRows + 2) {
            fail("multi-window smoke isolation assertions failed");
            return;
        }

        QPointer<ZzPureTools::ZzApplicationWindow> secondObserver(
            secondWindow);
        if (!secondWindow->close()) {
            fail("multi-window smoke could not close the second window");
            return;
        }
        QTimer::singleShot(0, application,
            [this, secondObserver] {
                if (!secondObserver.isNull()
                    || application->windowCount() != 1) {
                    fail("multi-window smoke did not erase the closed window");
                    return;
                }
                QCoreApplication::quit();
            });
    });
}

void ZzExampleSmokeControllerPrivate::scheduleCloseGuardSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        const int activityRows = context->activityModel().rowCount();
        QTimer::singleShot(0, application,
            [this] { chooseCloseDialogButton(); });
        QPointer<ZzPureTools::ZzApplicationWindow> observer(&window);
        const bool closed = window.close();
        if (scenario == ZzExampleSmokeScenario::CloseConfirm) {
            if (!closed) {
                fail("close-confirm smoke did not accept the close event");
                return;
            }
            QTimer::singleShot(0, application,
                [this, observer, activityRows] {
                    if (!observer.isNull()
                        || application->windowCount() != 0
                        || context->activityModel().rowCount()
                            != activityRows + 1) {
                        fail("close-confirm smoke state mismatch");
                        return;
                    }
                    QCoreApplication::quit();
                });
            return;
        }

        const bool expectsMinimized =
            scenario == ZzExampleSmokeScenario::CloseMinimize;
        if (closed || application->windowCount() != 1
            || observer.isNull()
            || window.isMinimized() != expectsMinimized
            || context->activityModel().rowCount() != activityRows + 1) {
            fail("close guard smoke state mismatch");
            return;
        }
        QTimer::singleShot(0, application, [this] {
            application->beginShutdown();
            QCoreApplication::exit(EXIT_SUCCESS);
        });
    });
}

void ZzExampleSmokeControllerPrivate::scheduleScreenshotSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        bool dprValid = false;
        const qreal expectedDpr = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_DPR").toDouble(&dprValid);
        const QString baselineRoot = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_BASELINE_DIR").trimmed();
        const QString reportRoot = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_REPORT_DIR").trimmed();
        const QString baselineSubdirectory = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_BASELINE_SUBDIR").trimmed();
        const bool safeSubdirectory = !baselineSubdirectory.isEmpty()
            && !baselineSubdirectory.contains(QLatin1Char('/'))
            && !baselineSubdirectory.contains(QLatin1Char('\\'))
            && baselineSubdirectory != QStringLiteral(".")
            && baselineSubdirectory != QStringLiteral("..");
        if (!dprValid || !std::isfinite(expectedDpr) || expectedDpr <= 0.0
            || baselineRoot.isEmpty() || reportRoot.isEmpty()
            || !safeSubdirectory) {
            fail("invalid screenshot environment");
            return;
        }

        QScreen *screen = QApplication::primaryScreen();
        if (screen == nullptr
            || std::abs(screen->devicePixelRatio() - expectedDpr) > 0.01) {
            fail(
                "screenshot DPR mismatch",
                screen == nullptr
                    ? QStringLiteral("primary screen is unavailable")
                    : QStringLiteral("actual=%1; expected=%2")
                          .arg(screen->devicePixelRatio(), 0, 'f', 2)
                          .arg(expectedDpr, 0, 'f', 2));
            return;
        }
        if (QFontInfo(QApplication::font()).family()
                != QStringLiteral("DejaVu Sans")
            || QApplication::font().pointSize() != 10) {
            fail("screenshot reference font mismatch");
            return;
        }

        auto *theme = application->themeController();
        if (theme == nullptr) {
            fail("screenshot theme controller is unavailable");
            return;
        }
        theme->setReducedMotion(true);
        window.setFixedSize(
            zzScreenshotLogicalWidth,
            zzScreenshotLogicalHeight);
        if (QWidget *focused = QApplication::focusWidget()) {
            focused->clearFocus();
        }
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        const QString baselineDirectory = QDir(baselineRoot).filePath(
            baselineSubdirectory);
        const QString reportDirectory = QDir(reportRoot).filePath(
            baselineSubdirectory);
        const bool updateBaselines = qEnvironmentVariableIntValue(
            "ZZ_UPDATE_EXAMPLE_SCREENSHOTS") == 1;
        for (const auto &[mode, fileStem] : zzScreenshotThemes()) {
            theme->setMode(mode);
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            window.repaint();
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            const QImage actual = zzRenderExampleWindow(window, expectedDpr);
            const QImage mask = zzBuildExampleTextMask(window, expectedDpr);
            const qsizetype totalPixels =
                static_cast<qsizetype>(actual.width())
                * static_cast<qsizetype>(actual.height());
            const qsizetype maskedPixels = zzMaskedPixelCount(mask);
            if (actual.size() != QSize(
                    qRound(zzScreenshotLogicalWidth * expectedDpr),
                    qRound(zzScreenshotLogicalHeight * expectedDpr))
                || !zzHasVisualContent(actual)
                || mask.size() != actual.size()
                || maskedPixels == 0
                || maskedPixels * 2 >= totalPixels) {
                fail(
                    "invalid screenshot surface",
                    QStringLiteral("theme=%1; masked=%2; total=%3")
                        .arg(QString::fromLatin1(fileStem))
                        .arg(maskedPixels)
                        .arg(totalPixels));
                return;
            }

            const QString fileName = QString::fromLatin1(fileStem)
                + QStringLiteral(".png");
            const QString baselinePath = QDir(baselineDirectory).filePath(
                fileName);
            if (updateBaselines) {
                if (!QDir().mkpath(baselineDirectory)
                    || !actual.save(baselinePath, "PNG")) {
                    fail("could not update screenshot baseline", baselinePath);
                    return;
                }
                continue;
            }

            const QImage expected(baselinePath);
            if (expected.isNull() || expected.size() != actual.size()) {
                fail("missing or invalid screenshot baseline", baselinePath);
                return;
            }
            const ZzExampleScreenshotComparison comparison =
                zzCompareExampleImages(expected, actual, mask);
            if (comparison.comparedPixels <= 0) {
                fail(
                    "screenshot comparison has no visible pixels",
                    baselinePath);
                return;
            }
            const qreal differenceRatio =
                static_cast<qreal>(comparison.differentPixels)
                / static_cast<qreal>(comparison.comparedPixels);
            const qreal maximumDifferenceRatio =
                zzScreenshotMaximumDifferenceRatio();
            if (differenceRatio <= maximumDifferenceRatio) {
                continue;
            }

            if (!QDir().mkpath(reportDirectory)) {
                fail(
                    "could not create screenshot report directory",
                    reportDirectory);
                return;
            }
            const QString actualPath = QDir(reportDirectory).filePath(
                QString::fromLatin1(fileStem) + QStringLiteral("-actual.png"));
            const QString differencePath = QDir(reportDirectory).filePath(
                QString::fromLatin1(fileStem) + QStringLiteral("-diff.png"));
            if (!actual.save(actualPath, "PNG")
                || !comparison.difference.save(differencePath, "PNG")) {
                fail(
                    "could not write screenshot difference evidence",
                    reportDirectory);
                return;
            }
            fail(
                "screenshot difference exceeds tolerance",
                QStringLiteral(
                    "theme=%1; ratio=%2; maximum=%3; actual=%4; diff=%5")
                    .arg(QString::fromLatin1(fileStem))
                    .arg(differenceRatio, 0, 'f', 6)
                    .arg(maximumDifferenceRatio, 0, 'f', 6)
                    .arg(actualPath, differencePath));
            return;
        }

        application->beginShutdown();
        QCoreApplication::exit(EXIT_SUCCESS);
    });
}

void ZzExampleSmokeControllerPrivate::chooseCloseDialogButton()
{
    auto *dialog = qobject_cast<QMessageBox *>(
        QApplication::activeModalWidget());
    if (dialog == nullptr) {
        fail("close guard smoke did not open a message box");
        return;
    }
    const QMessageBox::ButtonRole expectedRole =
        zzCloseButtonRole(scenario);
    for (QAbstractButton *button : dialog->buttons()) {
        if (dialog->buttonRole(button) == expectedRole) {
            button->click();
            return;
        }
    }
    dialog->reject();
    fail("close guard smoke could not find the expected button");
}

void ZzExampleSmokeControllerPrivate::fail(
    const char *reason,
    const QString &details) const
{
    qCritical().noquote()
        << "ZzPureToolsExample smoke failed:"
        << reason
        << details;
    QCoreApplication::exit(EXIT_FAILURE);
}

} // namespace ZzExample
