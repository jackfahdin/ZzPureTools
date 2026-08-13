#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QImage>
#include <QtGui/QEnterEvent>
#include <QtGui/QFontMetrics>
#include <QtGui/QIcon>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSizeGrip>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzCarouselView.h>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzContentDialog.h>
#include <ZzFluentUI/ZzDrawer.h>
#include <ZzFluentUI/ZzExpander.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzInfoBadge.h>
#include <ZzFluentUI/ZzKeyBinder.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzNavigationDisplayMode.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzPasswordBox.h>
#include <ZzFluentUI/ZzPasswordRevealMode.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzRatingControl.h>
#include <ZzFluentUI/ZzRatingPrecision.h>
#include <ZzFluentUI/ZzRoller.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSplitButton.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzSuggestBox.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTeachingTip.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {

constexpr QSize zzLogicalSurfaceSize(1200, 800);
constexpr QPoint zzMenuOrigin(914, 590);
constexpr QPoint zzComboBoxPopupOrigin(770, 570);
constexpr QPoint zzSuggestBoxPopupOrigin(770, 660);
constexpr QPoint zzMultiSelectPopupOrigin(770, 390);
constexpr QPoint zzRollerPopupOrigin(700, 230);
constexpr std::array<QPoint, 3> zzPopupMenuOrigins{
    QPoint(70, 190),
    QPoint(450, 190),
    QPoint(830, 190)};
constexpr int zzTextMaskPadding = 2;
constexpr int zzChannelTolerance = 3;

/** @brief 返回当前 Qt minor 对应的非文字像素差异上限。 */
constexpr qreal zzMaximumDifferenceRatio()
{
#if QT_VERSION_MAJOR == ZZ_FLUENT_SCREENSHOT_REFERENCE_QT_MAJOR \
    && QT_VERSION_MINOR == ZZ_FLUENT_SCREENSHOT_REFERENCE_QT_MINOR
    return 0.005;
#else
    return 0.02;
#endif
}

/** @brief 保存截图进程移除 Qt Test 未知参数后的确定配置。 */
struct ZzScreenshotArguments final
{
    qreal expectedDpr = 0.0;
    QString baselineSubdirectory;
    std::vector<QByteArray> filteredArguments;
};

/** @brief 记录各类可见文字是否均已进入字体差异遮罩。 */
struct ZzTextMaskCoverage final
{
    int labels = 0;
    int buttons = 0;
    int lineEdits = 0;
    int textEdits = 0;
    int comboBoxes = 0;
    int tabBars = 0;
    int menus = 0;
    int menuBars = 0;
    int toolBars = 0;
    int statusBars = 0;
    int progressBars = 0;
    int itemViews = 0;
};

/** @brief 保存一次像素比较的统计和证据图。 */
struct ZzImageComparison final
{
    qsizetype comparedPixels = 0;
    qsizetype differentPixels = 0;
    QImage difference;
};

/**
 * @brief 解析并移除截图测试专用命令行参数。
 * @param argc 原始参数数量。
 * @param argv 原始参数指针。
 * @param error 失败时写入可读错误。
 * @return 完整配置；参数缺失或非法时返回空值。
 */
std::optional<ZzScreenshotArguments> zzParseArguments(
    int argc,
    char *argv[],
    QString *error)
{
    ZzScreenshotArguments result;
    result.filteredArguments.reserve(static_cast<std::size_t>(argc));
    result.filteredArguments.emplace_back(argv[0]);
    bool hasExpectedDpr = false;
    bool hasBaselineSubdirectory = false;

    for (int index = 1; index < argc; ++index) {
        const QByteArray argument(argv[index]);
        if (argument == QByteArrayLiteral("--expected-dpr")) {
            if (++index >= argc || hasExpectedDpr) {
                *error = QStringLiteral(
                    "--expected-dpr 必须且只能提供一次有效值");
                return std::nullopt;
            }
            bool valid = false;
            result.expectedDpr = QByteArray(argv[index]).toDouble(&valid);
            if (!valid || !std::isfinite(result.expectedDpr)
                || result.expectedDpr <= 0.0) {
                *error = QStringLiteral("--expected-dpr 必须是有限正数");
                return std::nullopt;
            }
            hasExpectedDpr = true;
            continue;
        }
        if (argument == QByteArrayLiteral("--baseline-subdir")) {
            if (++index >= argc || hasBaselineSubdirectory) {
                *error = QStringLiteral(
                    "--baseline-subdir 必须且只能提供一次有效值");
                return std::nullopt;
            }
            result.baselineSubdirectory = QString::fromLocal8Bit(argv[index]);
            const bool safeName = !result.baselineSubdirectory.isEmpty()
                && !result.baselineSubdirectory.contains(QLatin1Char('/'))
                && !result.baselineSubdirectory.contains(QLatin1Char('\\'))
                && result.baselineSubdirectory != QStringLiteral(".")
                && result.baselineSubdirectory != QStringLiteral("..");
            if (!safeName) {
                *error = QStringLiteral(
                    "--baseline-subdir 必须是单个安全目录名");
                return std::nullopt;
            }
            hasBaselineSubdirectory = true;
            continue;
        }
        result.filteredArguments.push_back(argument);
    }

    if (!hasExpectedDpr || !hasBaselineSubdirectory) {
        *error = QStringLiteral(
            "必须同时提供 --expected-dpr 和 --baseline-subdir");
        return std::nullopt;
    }
    return result;
}

/** @brief 把子控件逻辑矩形映射到固定截图窗口。 */
QRect zzMapToSurface(const QWidget *widget, const QRect &rect, QWidget *surface)
{
    return QRect(widget->mapTo(surface, rect.topLeft()), rect.size());
}

/** @brief 根据字体、对齐方式和容器返回紧凑文字矩形。 */
QRect zzAlignedTextRect(
    const QWidget *widget,
    QRect bounds,
    int alignment,
    const QString &text)
{
    if (text.isEmpty() || bounds.isEmpty()) {
        return {};
    }
    return widget->fontMetrics().boundingRect(bounds, alignment, text);
}

/** @brief 向物理像素遮罩加入外扩后的逻辑文字区域。 */
void zzPaintMaskRect(QPainter *painter, QRect rect)
{
    if (rect.isEmpty()) {
        return;
    }
    painter->fillRect(
        rect.adjusted(
            -zzTextMaskPadding,
            -zzTextMaskPadding,
            zzTextMaskPadding,
            zzTextMaskPadding),
        Qt::white);
}

/**
 * @brief 遍历 item view 当前可见索引并遮罩 delegate 文字。
 * @return 实际加入遮罩的非空展示文本数量。
 */
int zzMaskVisibleViewText(
    QAbstractItemView *view,
    QWidget *surface,
    QPainter *maskPainter)
{
    QAbstractItemModel *model = view->model();
    if (model == nullptr || view->viewport() == nullptr) {
        return 0;
    }

    int masked = 0;
    std::vector<QModelIndex> pending{QModelIndex()};
    while (!pending.empty()) {
        const QModelIndex parent = pending.back();
        pending.pop_back();
        const int rows = model->rowCount(parent);
        const int columns = model->columnCount(parent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex first = model->index(row, 0, parent);
            if (model->hasChildren(first)) {
                pending.push_back(first);
            }
            for (int column = 0; column < columns; ++column) {
                const QModelIndex index = model->index(row, column, parent);
                const QString text = index.data(Qt::DisplayRole).toString();
                const QRect visual = view->visualRect(index);
                if (text.isEmpty() || visual.isEmpty()
                    || !visual.intersects(view->viewport()->rect())) {
                    continue;
                }
                const QRect localText = zzAlignedTextRect(
                    view,
                    visual.adjusted(8, 0, -4, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    text);
                const QRect surfaceText(
                    view->viewport()->mapTo(surface, localText.topLeft()),
                    localText.size());
                zzPaintMaskRect(maskPainter, surfaceText);
                ++masked;
            }
        }
    }
    return masked;
}

/**
 * @brief 为全部承诺文字控件构造字体栅格化差异遮罩。
 * @param surface 固定逻辑截图窗口。
 * @param menu 单独合成到窗口的菜单。
 * @param dpr 当前进程设备像素比。
 * @param coverage 写入各控件类别覆盖数量。
 * @return 与实际截图物理尺寸相同的灰度遮罩。
 */
QImage zzBuildTextMask(
    QWidget *surface,
    QMenu *menu,
    qreal dpr,
    ZzTextMaskCoverage *coverage)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage mask(physicalSize, QImage::Format_Grayscale8);
    mask.setDevicePixelRatio(dpr);
    mask.fill(0);
    QPainter painter(&mask);

    const auto widgets = surface->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (!widget->isVisible() || qobject_cast<QMenu *>(widget) != nullptr) {
            continue;
        }
        if (auto *label = qobject_cast<QLabel *>(widget);
            label != nullptr && !label->text().isEmpty()) {
            const QRect textRect = zzAlignedTextRect(
                label,
                label->contentsRect(),
                static_cast<int>(label->alignment()),
                label->text());
            zzPaintMaskRect(&painter, zzMapToSurface(label, textRect, surface));
            ++coverage->labels;
            continue;
        }
        if (auto *button = qobject_cast<QAbstractButton *>(widget);
            button != nullptr && !button->text().isEmpty()) {
            QRect contents = button->contentsRect();
            int alignment = Qt::AlignCenter;
            if (qobject_cast<ZzFluentUI::ZzToggleSwitch *>(button) != nullptr) {
                contents.adjust(48, 0, 0, 0);
                alignment = Qt::AlignLeft | Qt::AlignVCenter;
            } else if (qobject_cast<QCheckBox *>(button) != nullptr) {
                QStyleOptionButton option;
                option.initFrom(button);
                contents = button->style()->subElementRect(
                    QStyle::SE_CheckBoxContents,
                    &option,
                    button);
                alignment = Qt::AlignLeft | Qt::AlignVCenter;
            } else if (qobject_cast<QRadioButton *>(button) != nullptr) {
                QStyleOptionButton option;
                option.initFrom(button);
                contents = button->style()->subElementRect(
                    QStyle::SE_RadioButtonContents,
                    &option,
                    button);
                alignment = Qt::AlignLeft | Qt::AlignVCenter;
            }
            const QRect textRect = zzAlignedTextRect(
                button,
                contents,
                alignment,
                button->text());
            zzPaintMaskRect(&painter, zzMapToSurface(button, textRect, surface));
            ++coverage->buttons;
            continue;
        }
        if (auto *lineEdit = qobject_cast<QLineEdit *>(widget);
            lineEdit != nullptr) {
            const QString text = lineEdit->text().isEmpty()
                ? lineEdit->placeholderText()
                : lineEdit->displayText();
            if (!text.isEmpty()) {
                QStyleOptionFrame option;
                option.initFrom(lineEdit);
                const QRect contents = lineEdit->style()->subElementRect(
                    QStyle::SE_LineEditContents,
                    &option,
                    lineEdit);
                const QRect textRect = zzAlignedTextRect(
                    lineEdit,
                    contents.adjusted(2, 0, -2, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    text);
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(lineEdit, textRect, surface));
                ++coverage->lineEdits;
            }
            continue;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(widget);
            textEdit != nullptr && !textEdit->toPlainText().isEmpty()) {
            const QRect bounds = textEdit->viewport()->rect().adjusted(4, 4, -4, -4);
            const QRect textRect = zzAlignedTextRect(
                textEdit,
                bounds,
                Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                textEdit->toPlainText());
            const QRect surfaceText(
                textEdit->viewport()->mapTo(surface, textRect.topLeft()),
                textRect.size());
            zzPaintMaskRect(&painter, surfaceText);
            ++coverage->textEdits;
            continue;
        }
        if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(widget);
            plainTextEdit != nullptr && !plainTextEdit->toPlainText().isEmpty()) {
            const QRect bounds = plainTextEdit->viewport()->rect().adjusted(
                4,
                4,
                -4,
                -4);
            const QRect textRect = zzAlignedTextRect(
                plainTextEdit,
                bounds,
                Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                plainTextEdit->toPlainText());
            const QRect surfaceText(
                plainTextEdit->viewport()->mapTo(surface, textRect.topLeft()),
                textRect.size());
            zzPaintMaskRect(&painter, surfaceText);
            ++coverage->textEdits;
            continue;
        }
        if (auto *comboBox = qobject_cast<QComboBox *>(widget);
            comboBox != nullptr && !comboBox->currentText().isEmpty()) {
            QStyleOptionComboBox option;
            option.initFrom(comboBox);
            option.currentText = comboBox->currentText();
            const QRect contents = comboBox->style()->subControlRect(
                QStyle::CC_ComboBox,
                &option,
                QStyle::SC_ComboBoxEditField,
                comboBox);
            const QRect textRect = zzAlignedTextRect(
                comboBox,
                contents,
                Qt::AlignLeft | Qt::AlignVCenter,
                comboBox->currentText());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(comboBox, textRect, surface));
            ++coverage->comboBoxes;
            continue;
        }
        if (auto *tabBar = qobject_cast<QTabBar *>(widget);
            tabBar != nullptr) {
            int tabTextCount = 0;
            for (int index = 0; index < tabBar->count(); ++index) {
                const QString text = tabBar->tabText(index);
                if (text.isEmpty()) {
                    continue;
                }
                const QRect textRect = zzAlignedTextRect(
                    tabBar,
                    tabBar->tabRect(index),
                    Qt::AlignCenter,
                    text);
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(tabBar, textRect, surface));
                ++tabTextCount;
            }
            if (tabTextCount > 0) {
                ++coverage->tabBars;
            }
            continue;
        }
        if (auto *menuBar = qobject_cast<QMenuBar *>(widget);
            menuBar != nullptr) {
            int actionTextCount = 0;
            for (QAction *action : menuBar->actions()) {
                if (action == nullptr || action->isSeparator()
                    || action->text().isEmpty()) {
                    continue;
                }
                const QRect actionRect = menuBar->actionGeometry(action);
                const QRect textRect = zzAlignedTextRect(
                    menuBar,
                    actionRect,
                    Qt::AlignCenter,
                    action->text().remove(QLatin1Char('&')));
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(menuBar, textRect, surface));
                ++actionTextCount;
            }
            if (actionTextCount > 0) {
                ++coverage->menuBars;
            }
            continue;
        }
        if (auto *toolBar = qobject_cast<QToolBar *>(widget);
            toolBar != nullptr) {
            int actionTextCount = 0;
            for (QAction *action : toolBar->actions()) {
                if (action == nullptr || action->isSeparator()
                    || action->text().isEmpty()) {
                    continue;
                }
                const QRect actionRect = toolBar->actionGeometry(action);
                const QRect textRect = zzAlignedTextRect(
                    toolBar,
                    actionRect,
                    Qt::AlignCenter,
                    action->text());
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(toolBar, textRect, surface));
                ++actionTextCount;
            }
            if (actionTextCount > 0) {
                ++coverage->toolBars;
            }
            continue;
        }
        if (auto *statusBar = qobject_cast<QStatusBar *>(widget);
            statusBar != nullptr && !statusBar->currentMessage().isEmpty()) {
            const QRect textRect = zzAlignedTextRect(
                statusBar,
                statusBar->rect().adjusted(8, 0, -8, 0),
                Qt::AlignLeft | Qt::AlignVCenter,
                statusBar->currentMessage());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(statusBar, textRect, surface));
            ++coverage->statusBars;
            continue;
        }
        if (auto *progress = qobject_cast<QProgressBar *>(widget);
            progress != nullptr && progress->isTextVisible()
            && !progress->text().isEmpty()) {
            const QRect textRect = zzAlignedTextRect(
                progress,
                progress->rect(),
                Qt::AlignCenter,
                progress->text());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(progress, textRect, surface));
            ++coverage->progressBars;
            continue;
        }
        if (auto *view = qobject_cast<QAbstractItemView *>(widget);
            view != nullptr) {
            if (zzMaskVisibleViewText(view, surface, &painter) > 0) {
                ++coverage->itemViews;
            }
        }
    }

    int menuTextCount = 0;
    for (QAction *action : menu->actions()) {
        if (action->isSeparator() || !action->isVisible()
            || action->text().isEmpty()) {
            continue;
        }
        QRect actionRect = menu->actionGeometry(action).translated(zzMenuOrigin);
        actionRect.adjust(12, 0, -12, 0);
        const QRect textRect = zzAlignedTextRect(
            menu,
            actionRect,
            Qt::AlignLeft | Qt::AlignVCenter,
            action->text());
        zzPaintMaskRect(&painter, textRect);
        ++menuTextCount;
    }
    if (menuTextCount > 0) {
        ++coverage->menus;
    }
    painter.end();
    return mask;
}

/** @brief 检查测试夹具每类承诺文字都被遮罩。 */
QString zzValidateMaskCoverage(const ZzTextMaskCoverage &coverage)
{
    QStringList missing;
    const auto require = [&missing](int count, const QString &name) {
        if (count == 0) {
            missing.append(name);
        }
    };
    require(coverage.labels, QStringLiteral("QLabel"));
    require(coverage.buttons, QStringLiteral("QAbstractButton"));
    require(coverage.lineEdits, QStringLiteral("QLineEdit"));
    require(coverage.textEdits, QStringLiteral("QTextEdit"));
    require(coverage.comboBoxes, QStringLiteral("QComboBox"));
    require(coverage.tabBars, QStringLiteral("QTabBar"));
    require(coverage.menus, QStringLiteral("QMenu"));
    require(coverage.progressBars, QStringLiteral("QProgressBar"));
    require(coverage.itemViews, QStringLiteral("item-view delegate"));
    return missing.join(QStringLiteral(", "));
}

/** @brief 在字体遮罩外按通道容差比较两张等尺寸图像。 */
ZzImageComparison zzCompareImages(
    const QImage &expectedSource,
    const QImage &actualSource,
    const QImage &mask)
{
    const QImage expected = expectedSource.convertToFormat(QImage::Format_RGBA8888);
    const QImage actual = actualSource.convertToFormat(QImage::Format_RGBA8888);
    ZzImageComparison result;
    result.difference = QImage(expected.size(), QImage::Format_RGBA8888);
    result.difference.fill(Qt::transparent);

    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            if (mask.pixelColor(x, y).value() != 0) {
                continue;
            }
            ++result.comparedPixels;
            const QColor expectedColor = expected.pixelColor(x, y);
            const QColor actualColor = actual.pixelColor(x, y);
            const int maximumDifference = std::max({
                std::abs(expectedColor.red() - actualColor.red()),
                std::abs(expectedColor.green() - actualColor.green()),
                std::abs(expectedColor.blue() - actualColor.blue()),
                std::abs(expectedColor.alpha() - actualColor.alpha())});
            if (maximumDifference <= zzChannelTolerance) {
                continue;
            }
            ++result.differentPixels;
            result.difference.setPixelColor(x, y, QColor(255, 0, 255, 255));
        }
    }
    return result;
}

/** @brief 构造全控件确定性截图面，不访问业务对象。 */
class ZzScreenshotSurface final
{
public:
    /** @brief 创建固定 1200x800 的全控件视觉夹具。 */
    ZzScreenshotSurface()
        : menu(&window)
        , navigationModel(&window)
        , tableModel(4, 3, &window)
        , treeModel(&window)
    {
        window.setObjectName(QStringLiteral("zzScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI"));
        window.setAutoFillBackground(true);
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *root = new QVBoxLayout(&window);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(12);

        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(&window);
        titleBar->setTitle(QStringLiteral("ZzFluentUI Controls"));
        titleBar->setFixedHeight(40);
        root->addWidget(titleBar);

        auto *columns = new QHBoxLayout;
        columns->setSpacing(16);
        root->addLayout(columns, 1);
        buildNavigationColumn(columns);
        buildControlColumn(columns);
        buildDataColumn(columns);

        menu.addAction(QStringLiteral("Open workspace"));
        menu.addAction(QStringLiteral("Save snapshot"));
        menu.addSeparator();
        menu.addAction(QStringLiteral("Close"));
        menu.setFixedWidth(246);
        menu.setAttribute(Qt::WA_DontShowOnScreen);
    }

    /** @brief 展示并完成全部布局、样式与菜单几何计算。 */
    void polish()
    {
        window.show();
        menu.show();
        QCoreApplication::processEvents();
        menu.adjustSize();
        menu.resize(246, menu.height());
        if (focusTarget != nullptr) {
            focusTarget->setFocus(Qt::OtherFocusReason);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏独立顶层菜单和截图窗口。 */
    void hide()
    {
        menu.hide();
        window.hide();
    }

    QWidget window;
    QMenu menu;

private:
    /** @brief 填充导航、列表和面包屑控件列。 */
    void buildNavigationColumn(QHBoxLayout *columns)
    {
        auto *container = new QWidget(&window);
        container->setFixedWidth(226);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(new QLabel(QStringLiteral("Navigation"), container));

        auto *navigation = new ZzFluentUI::ZzNavigationView(container);
        for (const QString &text : {
                 QStringLiteral("Home"),
                 QStringLiteral("Activity"),
                 QStringLiteral("Projects"),
                 QStringLiteral("Settings")}) {
            navigationModel.appendRow(new QStandardItem(text));
        }
        navigation->setModel(&navigationModel);
        navigation->setCurrentIndex(navigationModel.index(0, 0));
        navigation->setFixedHeight(204);
        layout->addWidget(navigation);

        auto *breadcrumb = new ZzFluentUI::ZzBreadcrumbBar(container);
        breadcrumb->setItems({
            QStringLiteral("Home"),
            QStringLiteral("Library"),
            QStringLiteral("Current")});
        breadcrumb->setCurrentIndex(2);
        breadcrumb->setFixedHeight(40);
        layout->addWidget(breadcrumb);

        auto *listLabel = new QLabel(QStringLiteral("Recent items"), container);
        layout->addWidget(listLabel);
        auto *list = new QListView(container);
        auto *listModel = new QStandardItemModel(list);
        for (const QString &text : {
                 QStringLiteral("Design notes"),
                 QStringLiteral("Release checklist"),
                 QStringLiteral("Performance report"),
                 QStringLiteral("Theme tokens")}) {
            listModel->appendRow(new QStandardItem(text));
        }
        list->setModel(listModel);
        list->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(list));
        list->setCurrentIndex(listModel->index(1, 0));
        layout->addWidget(list, 1);
        columns->addWidget(container);
    }

    /** @brief 填充按钮、输入、状态和反馈控件列。 */
    void buildControlColumn(QHBoxLayout *columns)
    {
        auto *container = new QWidget(&window);
        container->setFixedWidth(430);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(new QLabel(QStringLiteral("Actions"), container));

        auto *buttonRow = new QHBoxLayout;
        auto *standard = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Standard"), container);
        auto *accent = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Accent"), container);
        accent->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        auto *subtle = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Subtle"), container);
        subtle->setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
        auto *icon = new ZzFluentUI::ZzIconButton(container);
        icon->setAccessibleName(QStringLiteral("Refresh"));
        icon->setIconDescriptor({
            QStringLiteral(
                ":/zzfluent/screenshots/ZzFluentTestSquare.svg"),
            true});
        icon->setFixedSize(36, 36);
        buttonRow->addWidget(standard);
        buttonRow->addWidget(accent);
        buttonRow->addWidget(subtle);
        buttonRow->addWidget(icon);
        layout->addLayout(buttonRow);
        focusTarget = accent;

        auto *choiceRow = new QHBoxLayout;
        auto *toggle = new ZzFluentUI::ZzToggleSwitch(
            QStringLiteral("Sync"), container);
        toggle->setChecked(true);
        auto *check = new QCheckBox(QStringLiteral("Backup"), container);
        check->setChecked(true);
        auto *radio = new QRadioButton(QStringLiteral("Local"), container);
        radio->setChecked(true);
        choiceRow->addWidget(toggle);
        choiceRow->addWidget(check);
        choiceRow->addWidget(radio);
        layout->addLayout(choiceRow);

        auto *form = new QFormLayout;
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(10);
        form->setVerticalSpacing(8);
        auto *name = new QLineEdit(container);
        name->setText(QStringLiteral("Workspace"));
        auto *notes = new QTextEdit(container);
        notes->setPlainText(QStringLiteral("Fluent controls\nCross-platform UI"));
        notes->setFixedHeight(70);
        auto *mode = new QComboBox(container);
        mode->addItems({
            QStringLiteral("Balanced"),
            QStringLiteral("Compact"),
            QStringLiteral("Comfortable")});
        auto *datePicker = new ZzFluentUI::ZzCalendarPicker(container);
        datePicker->setLocale(QLocale::c());
        datePicker->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        datePicker->setDateRange(
            QDate(2026, 1, 1),
            QDate(2026, 12, 31));
        datePicker->setDate(QDate(2026, 8, 5));
        form->addRow(QStringLiteral("Name"), name);
        form->addRow(QStringLiteral("Notes"), notes);
        form->addRow(QStringLiteral("Density"), mode);
        form->addRow(QStringLiteral("Due date"), datePicker);
        layout->addLayout(form);

        auto *slider = new QSlider(Qt::Horizontal, container);
        slider->setRange(0, 100);
        slider->setValue(62);
        layout->addWidget(slider);
        auto *progress = new QProgressBar(container);
        progress->setRange(0, 100);
        progress->setValue(68);
        progress->setFormat(QStringLiteral("68% complete"));
        layout->addWidget(progress);

        auto *message = new ZzFluentUI::ZzMessageBar(container);
        message->setText(QStringLiteral("Settings saved successfully"));
        message->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
        layout->addWidget(message);

        auto *badgeRow = new QHBoxLayout;
        auto *dotBadge = new ZzFluentUI::ZzInfoBadge(container);
        dotBadge->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
        auto *countBadge = new ZzFluentUI::ZzInfoBadge(container);
        countBadge->setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        countBadge->setValue(8);
        auto *overflowBadge = new ZzFluentUI::ZzInfoBadge(container);
        overflowBadge->setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        overflowBadge->setSeverity(ZzFluentUI::ZzMessageSeverity::Warning);
        overflowBadge->setValue(120);
        badgeRow->addWidget(dotBadge);
        badgeRow->addWidget(countBadge);
        badgeRow->addWidget(overflowBadge);
        badgeRow->addStretch(1);
        layout->addLayout(badgeRow);

        auto *collapsedExpander = new ZzFluentUI::ZzExpander(container);
        collapsedExpander->setHeaderText(QStringLiteral("Advanced settings"));
        collapsedExpander->setContentWidget(new QLabel(
            QStringLiteral("Collapsed content")));
        layout->addWidget(collapsedExpander);

        auto *expandedExpander = new ZzFluentUI::ZzExpander(container);
        expandedExpander->setHeaderText(QStringLiteral("Build details"));
        expandedExpander->setContentWidget(new QLabel(
            QStringLiteral("Preset: linux-gcc-reference")));
        expandedExpander->setExpanded(true);
        layout->addWidget(expandedExpander);

        auto *pivot = new ZzFluentUI::ZzPivot(container);
        pivot->addItem(QStringLiteral("Overview"));
        pivot->addItem(QStringLiteral("Details"));
        pivot->addItem(QStringLiteral("History"));
        pivot->setCurrentIndex(1);
        layout->addWidget(pivot);
        layout->addStretch(1);
        columns->addWidget(container);
    }

    /** @brief 填充 Table、Tree 与菜单预览列。 */
    void buildDataColumn(QHBoxLayout *columns)
    {
        auto *container = new QWidget(&window);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(new QLabel(QStringLiteral("Data"), container));

        auto *table = new QTableView(container);
        for (int row = 0; row < tableModel.rowCount(); ++row) {
            for (int column = 0; column < tableModel.columnCount(); ++column) {
                tableModel.setData(
                    tableModel.index(row, column),
                    QStringLiteral("R%1 C%2").arg(row + 1).arg(column + 1));
            }
        }
        table->setModel(&tableModel);
        table->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(table));
        table->horizontalHeader()->hide();
        table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setCurrentIndex(tableModel.index(1, 1));
        table->setFixedHeight(130);
        layout->addWidget(table);

        layout->addWidget(new QLabel(QStringLiteral("Calendar"), container));
        auto *calendar = new ZzFluentUI::ZzCalendar(container);
        calendar->setLocale(QLocale::c());
        calendar->setFirstDayOfWeek(Qt::Monday);
        calendar->setDateRange(
            QDate(2026, 8, 3),
            QDate(2026, 8, 28));
        calendar->setSelectedDate(QDate(2026, 8, 6));
        calendar->setFixedHeight(270);
        layout->addWidget(calendar);

        layout->addWidget(new QLabel(QStringLiteral("Tree"), container));
        auto *rootItem = new QStandardItem(QStringLiteral("Workspace"));
        rootItem->appendRow(new QStandardItem(QStringLiteral("Sources")));
        rootItem->appendRow(new QStandardItem(QStringLiteral("Resources")));
        auto *tests = new QStandardItem(QStringLiteral("Tests"));
        tests->appendRow(new QStandardItem(QStringLiteral("Accessibility")));
        tests->appendRow(new QStandardItem(QStringLiteral("Screenshots")));
        treeModel.appendRow(rootItem);
        treeModel.appendRow(tests);
        auto *tree = new QTreeView(container);
        tree->setModel(&treeModel);
        tree->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(tree));
        tree->header()->hide();
        tree->expandAll();
        tree->setCurrentIndex(rootItem->index());
        tree->setFixedHeight(135);
        layout->addWidget(tree);
        layout->addWidget(new QLabel(QStringLiteral("Menu"), container));
        layout->addStretch(1);
        columns->addWidget(container, 1);
    }

    QPointer<QWidget> focusTarget;
    QStandardItemModel navigationModel;
    QStandardItemModel tableModel;
    QStandardItemModel treeModel;
};

/** @brief 构造标准 Qt 控件广度专用的固定截图夹具。 */
class ZzStandardBreadthScreenshotSurface final
{
public:
    /** @brief 创建包含标准控件和原生模型的确定性视觉表面。 */
    ZzStandardBreadthScreenshotSurface()
        : menu(&window)
        , listModel(&window)
        , tableModel(3, 3, &window)
        , treeModel(&window)
    {
        window.setObjectName(QStringLiteral("zzStandardBreadthSurface"));
        window.setAutoFillBackground(true);
        window.setFixedSize(zzLogicalSurfaceSize);

        auto *root = new QVBoxLayout(&window);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(10);

        menuBar = new QMenuBar(&window);
        menuBar->setNativeMenuBar(false);
        QMenu *fileMenu = menuBar->addMenu(QStringLiteral("File"));
        fileMenu->addAction(QStringLiteral("Open"), QKeySequence::Open);
        fileMenu->addAction(QStringLiteral("Save"), QKeySequence::Save);
        QMenu *viewMenu = menuBar->addMenu(QStringLiteral("View"));
        QAction *compact = viewMenu->addAction(QStringLiteral("Compact"));
        compact->setCheckable(true);
        compact->setChecked(true);
        root->addWidget(menuBar);

        toolBar = new QToolBar(QStringLiteral("Standard commands"), &window);
        toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
        QAction *build = toolBar->addAction(QStringLiteral("Build"));
        build->setCheckable(true);
        build->setChecked(true);
        toolBar->addAction(QStringLiteral("Test"));
        QAction *disabledAction = toolBar->addAction(QStringLiteral("Deploy"));
        disabledAction->setEnabled(false);
        root->addWidget(toolBar);

        auto *body = new QGridLayout;
        body->setHorizontalSpacing(18);
        body->setVerticalSpacing(10);
        root->addLayout(body, 1);
        buildInputColumn(body);
        buildRangeColumn(body);
        buildViewColumn(body);

        statusBar = new QStatusBar(&window);
        statusBar->setSizeGripEnabled(false);
        statusBar->addPermanentWidget(
            new QLabel(QStringLiteral("Local reference"), statusBar));
        statusBar->showMessage(QStringLiteral("Ready"));
        root->addWidget(statusBar);

        menu.addAction(QStringLiteral("Open workspace"));
        menu.addAction(QStringLiteral("Save snapshot"));
        menu.addSeparator();
        menu.addAction(QStringLiteral("Close"));
        menu.setFixedWidth(246);
        menu.setAttribute(Qt::WA_DontShowOnScreen);
    }

    /** @brief 展示窗口、弹出菜单并完成固定布局计算。 */
    void polish()
    {
        window.show();
        menu.show();
        QCoreApplication::processEvents();
        menu.adjustSize();
        menu.resize(246, menu.height());
        if (focusTarget != nullptr) {
            focusTarget->setFocus(Qt::OtherFocusReason);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏标准控件截图夹具及其独立菜单。 */
    void hide()
    {
        menu.hide();
        window.hide();
    }

    QWidget window;
    QMenu menu;

private:
    /** @brief 填充复选、单选、编辑和组合框控件。 */
    void buildInputColumn(QGridLayout *body)
    {
        auto *column = new QWidget(&window);
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(new QLabel(QStringLiteral("Selection and text"), column));
        auto *checkBox = new QCheckBox(QStringLiteral("Enable standard control"), column);
        checkBox->setChecked(true);
        layout->addWidget(checkBox);
        auto *radioButton = new QRadioButton(QStringLiteral("Use local profile"), column);
        radioButton->setChecked(true);
        layout->addWidget(radioButton);
        auto *disabledCheckBox = new QCheckBox(QStringLiteral("Disabled state"), column);
        disabledCheckBox->setChecked(true);
        disabledCheckBox->setEnabled(false);
        layout->addWidget(disabledCheckBox);
        auto *lineEdit = new QLineEdit(column);
        lineEdit->setText(QStringLiteral("Workspace"));
        focusTarget = lineEdit;
        layout->addWidget(lineEdit);
        auto *plainTextEdit = new QPlainTextEdit(column);
        plainTextEdit->setPlainText(QStringLiteral(
            "Standard Qt text surface\nFluent visual layer\nNo business model access"));
        plainTextEdit->setFixedHeight(124);
        layout->addWidget(plainTextEdit);
        auto *comboBox = new QComboBox(column);
        comboBox->addItems({
            QStringLiteral("Linux"),
            QStringLiteral("Windows"),
            QStringLiteral("macOS")});
        comboBox->setCurrentIndex(0);
        comboBox->setLayoutDirection(Qt::RightToLeft);
        layout->addWidget(comboBox);
        auto *tabs = new QTabBar(column);
        tabs->addTab(QStringLiteral("Overview"));
        tabs->addTab(QStringLiteral("Details"));
        tabs->addTab(QStringLiteral("History"));
        tabs->setCurrentIndex(1);
        layout->addWidget(tabs);
        layout->addStretch(1);
        body->addWidget(column, 0, 0);
    }

    /** @brief 填充滑块、进度条和数字显示控件。 */
    void buildRangeColumn(QGridLayout *body)
    {
        auto *column = new QWidget(&window);
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        layout->addWidget(new QLabel(QStringLiteral("Range and display"), column));
        auto *horizontalSlider = new QSlider(Qt::Horizontal, column);
        horizontalSlider->setRange(0, 100);
        horizontalSlider->setValue(62);
        layout->addWidget(horizontalSlider);
        auto *verticalSlider = new QSlider(Qt::Vertical, column);
        verticalSlider->setRange(0, 100);
        verticalSlider->setValue(38);
        verticalSlider->setFixedHeight(120);
        layout->addWidget(verticalSlider, 0, Qt::AlignHCenter);
        auto *progress = new QProgressBar(column);
        progress->setRange(0, 100);
        progress->setValue(68);
        progress->setFormat(QStringLiteral("68% complete"));
        layout->addWidget(progress);
        auto *busyProgress = new QProgressBar(column);
        busyProgress->setRange(0, 0);
        busyProgress->setTextVisible(false);
        layout->addWidget(busyProgress);
        auto *lcd = new QLCDNumber(6, column);
        lcd->setFrameStyle(QFrame::Box | QFrame::Plain);
        lcd->setSegmentStyle(QLCDNumber::Flat);
        lcd->display(2026);
        lcd->setFixedHeight(58);
        layout->addWidget(lcd);
        layout->addStretch(1);
        body->addWidget(column, 0, 1);
    }

    /** @brief 填充列表、表格、树和工具状态表面。 */
    void buildViewColumn(QGridLayout *body)
    {
        auto *column = new QWidget(&window);
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(new QLabel(QStringLiteral("Item views"), column));
        for (const QString &text : {
                 QStringLiteral("Design notes"),
                 QStringLiteral("Release checklist"),
                 QStringLiteral("Performance report")}) {
            listModel.appendRow(new QStandardItem(text));
        }
        auto *list = new QListView(column);
        list->setModel(&listModel);
        list->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(list));
        list->setCurrentIndex(listModel.index(1, 0));
        list->setFixedHeight(112);
        layout->addWidget(list);

        for (int row = 0; row < tableModel.rowCount(); ++row) {
            for (int columnIndex = 0; columnIndex < tableModel.columnCount(); ++columnIndex) {
                tableModel.setData(
                    tableModel.index(row, columnIndex),
                    QStringLiteral("R%1 C%2").arg(row + 1).arg(columnIndex + 1));
            }
        }
        auto *table = new QTableView(column);
        table->setModel(&tableModel);
        table->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(table));
        table->horizontalHeader()->hide();
        table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setCurrentIndex(tableModel.index(1, 1));
        table->setLayoutDirection(Qt::RightToLeft);
        table->setFixedHeight(124);
        layout->addWidget(table);

        auto *rootItem = new QStandardItem(QStringLiteral("Workspace"));
        rootItem->appendRow(new QStandardItem(QStringLiteral("Sources")));
        rootItem->appendRow(new QStandardItem(QStringLiteral("Tests")));
        treeModel.appendRow(rootItem);
        auto *tree = new QTreeView(column);
        tree->setModel(&treeModel);
        tree->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(tree));
        tree->header()->hide();
        tree->expandAll();
        tree->setCurrentIndex(rootItem->index());
        tree->setFixedHeight(124);
        layout->addWidget(tree);
        layout->addStretch(1);
        body->addWidget(column, 0, 2);
    }

    QMenuBar *menuBar = nullptr;
    QToolBar *toolBar = nullptr;
    QStatusBar *statusBar = nullptr;
    QPointer<QWidget> focusTarget;
    QStandardItemModel listModel;
    QStandardItemModel tableModel;
    QStandardItemModel treeModel;
};

/** @brief 保存卡片截图中文字遮罩及其覆盖数量。 */
struct ZzCardTextMask final
{
    QImage image;
    int actionCards = 0;
    int imageCards = 0;
};

/** @brief 返回卡片标题使用的半粗体字体。 */
QFont zzCardTitleFont(const QWidget *card)
{
    QFont result = card->font();
    result.setWeight(QFont::DemiBold);
    return result;
}

/** @brief 将一段省略后文字的精确像素边界加入卡片遮罩。 */
void zzMaskCardText(
    QPainter *painter,
    const QWidget *card,
    QWidget *surface,
    const QRect &bounds,
    const QFont &font,
    const QString &text)
{
    if (bounds.isEmpty() || text.isEmpty()) {
        return;
    }
    const QFontMetrics metrics(font);
    const QString displayed = metrics.elidedText(
        text,
        Qt::ElideRight,
        bounds.width());
    const QRect textRect = metrics.boundingRect(
        bounds,
        Qt::AlignLeading | Qt::AlignVCenter,
        displayed);
    zzPaintMaskRect(
        painter,
        QRect(card->mapTo(surface, textRect.topLeft()), textRect.size()));
}

/** @brief 按操作卡生产布局公式遮罩标题与说明。 */
void zzMaskActionCardText(
    ZzFluentUI::ZzActionCard *card,
    QWidget *surface,
    QPainter *painter)
{
    constexpr int horizontalPadding = 12;
    constexpr int verticalPadding = 10;
    constexpr int contentSpacing = 10;
    constexpr int textSpacing = 2;
    constexpr int indicatorExtent = 16;
    const QRect bounds = card->rect();
    const QRect content = bounds.adjusted(
        horizontalPadding,
        verticalPadding,
        -horizontalPadding,
        -verticalPadding);
    if (content.isEmpty()) {
        return;
    }

    int logicalLeft = content.left();
    int logicalRight = content.right();
    if (!card->icon().isNull()) {
        const int styleIconExtent = card->style()->pixelMetric(
            QStyle::PM_SmallIconSize,
            nullptr,
            card);
        const QSize requested = card->iconSize().isValid()
            ? card->iconSize()
            : QSize(styleIconExtent, styleIconExtent);
        const int extent = std::clamp(
            std::max(requested.width(), requested.height()),
            1,
            content.height());
        logicalLeft += extent + contentSpacing;
    }
    if (card->isTrailingIndicatorVisible()) {
        logicalRight -= indicatorExtent + contentSpacing;
    }
    const int textWidth = std::max(0, logicalRight - logicalLeft + 1);
    if (textWidth <= 0) {
        return;
    }

    const QFont titleFont = zzCardTitleFont(card);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descriptionMetrics(card->font());
    const int titleHeight = titleMetrics.height();
    QRect logicalTitle;
    QRect logicalDescription;
    if (card->description().isEmpty()) {
        logicalTitle = QRect(
            logicalLeft,
            content.center().y() - titleHeight / 2,
            textWidth,
            titleHeight);
    } else {
        const int textHeight = titleHeight
            + textSpacing
            + descriptionMetrics.height();
        const int textTop = content.center().y() - textHeight / 2;
        logicalTitle = QRect(
            logicalLeft,
            textTop,
            textWidth,
            titleHeight);
        logicalDescription = QRect(
            logicalLeft,
            textTop + titleHeight + textSpacing,
            textWidth,
            descriptionMetrics.height());
    }
    const QRect titleRect = QStyle::visualRect(
        card->layoutDirection(),
        bounds,
        logicalTitle);
    const QRect descriptionRect = QStyle::visualRect(
        card->layoutDirection(),
        bounds,
        logicalDescription);
    zzMaskCardText(
        painter,
        card,
        surface,
        titleRect,
        titleFont,
        card->text());
    zzMaskCardText(
        painter,
        card,
        surface,
        descriptionRect,
        card->font(),
        card->description());
}

/** @brief 按图片卡生产布局公式遮罩标题与说明。 */
void zzMaskImageCardText(
    ZzFluentUI::ZzImageCard *card,
    QWidget *surface,
    QPainter *painter)
{
    constexpr int padding = 12;
    constexpr int textSpacing = 2;
    constexpr int emptyTitleHeight = 52;
    constexpr int descriptionHeight = 72;
    const QRect bounds = card->rect().adjusted(1, 1, -1, -1);
    if (bounds.isEmpty()) {
        return;
    }
    const int contentHeight = card->description().isEmpty()
        ? emptyTitleHeight
        : descriptionHeight;
    const int imageHeight = std::max(0, bounds.height() - contentHeight);
    const QRect imageRect(
        bounds.left(),
        bounds.top(),
        bounds.width(),
        imageHeight);
    const QRect textContent(
        bounds.left() + padding,
        imageRect.bottom() + 1,
        std::max(0, bounds.width() - 2 * padding),
        contentHeight);
    if (textContent.isEmpty()) {
        return;
    }

    const QFont titleFont = zzCardTitleFont(card);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descriptionMetrics(card->font());
    const int titleHeight = titleMetrics.height();
    QRect titleRect;
    QRect descriptionRect;
    if (card->description().isEmpty()) {
        titleRect = QRect(
            textContent.left(),
            textContent.center().y() - titleHeight / 2,
            textContent.width(),
            titleHeight);
    } else {
        const int totalTextHeight = titleHeight
            + textSpacing
            + descriptionMetrics.height();
        const int textTop = textContent.center().y() - totalTextHeight / 2;
        titleRect = QRect(
            textContent.left(),
            textTop,
            textContent.width(),
            titleHeight);
        descriptionRect = QRect(
            textContent.left(),
            textTop + titleHeight + textSpacing,
            textContent.width(),
            descriptionMetrics.height());
    }
    zzMaskCardText(
        painter,
        card,
        surface,
        titleRect,
        titleFont,
        card->text());
    zzMaskCardText(
        painter,
        card,
        surface,
        descriptionRect,
        card->font(),
        card->description());
}

/** @brief 构造只覆盖卡片标题和说明的字体差异遮罩。 */
ZzCardTextMask zzBuildCardTextMask(QWidget *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzCardTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto actionCards =
        surface->findChildren<ZzFluentUI::ZzActionCard *>();
    for (ZzFluentUI::ZzActionCard *card : actionCards) {
        if (card->isVisible()) {
            zzMaskActionCardText(card, surface, &painter);
            ++result.actionCards;
        }
    }
    const auto imageCards =
        surface->findChildren<ZzFluentUI::ZzImageCard *>();
    for (ZzFluentUI::ZzImageCard *card : imageCards) {
        if (card->isVisible()) {
            zzMaskImageCardText(card, surface, &painter);
            ++result.imageCards;
        }
    }
    painter.end();
    return result;
}

/** @brief 使用当前 palette 构造能明确显示裁剪和适配差异的图片。 */
QPixmap zzCardScreenshotPixmap(const QPalette &palette)
{
    QPixmap pixmap(640, 320);
    pixmap.fill(palette.color(QPalette::AlternateBase));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Highlight));
    painter.drawRect(QRect(0, 0, 205, 320));
    painter.setBrush(palette.color(QPalette::Button));
    painter.drawRect(QRect(435, 0, 205, 320));
    painter.setBrush(palette.color(QPalette::Base));
    painter.drawEllipse(QPoint(320, 160), 102, 102);
    painter.setBrush(palette.color(QPalette::Mid));
    painter.drawRoundedRect(QRect(276, 116, 88, 88), 8, 8);
    painter.end();
    return pixmap;
}

/** @brief 构造只包含卡片及其视觉状态的独立确定性截图面。 */
class ZzCardScreenshotSurface final
{
public:
    /** @brief 创建六张操作卡和四张图片卡的固定视觉矩阵。 */
    ZzCardScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzCardScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Cards"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *root = new QVBoxLayout(&window);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(24);

        auto *actions = new QGridLayout;
        actions->setContentsMargins(0, 0, 0, 0);
        actions->setHorizontalSpacing(16);
        actions->setVerticalSpacing(16);
        const auto addAction = [this, actions](
                                   int row,
                                   int column,
                                   const QString &title,
                                   const QString &description) {
            auto *card = new ZzFluentUI::ZzActionCard(
                title,
                description,
                &window);
            card->setIcon(window.style()->standardIcon(QStyle::SP_ComputerIcon));
            card->setFixedHeight(88);
            actions->addWidget(card, row, column);
            return card;
        };
        addAction(
            0,
            0,
            QStringLiteral("Normal action"),
            QStringLiteral("Default enabled card"));
        hoverCard = addAction(
            0,
            1,
            QStringLiteral("Hover action"),
            QStringLiteral("Pointer is over this card"));
        focusCard = addAction(
            0,
            2,
            QStringLiteral("Focused action"),
            QStringLiteral("Keyboard focus ring"));
        auto *disabled = addAction(
            1,
            0,
            QStringLiteral("Disabled action"),
            QStringLiteral("Unavailable command"));
        disabled->setEnabled(false);
        auto *checked = addAction(
            1,
            1,
            QStringLiteral("Checked action"),
            QStringLiteral("Persistent selection"));
        checked->setCheckable(true);
        checked->setChecked(true);
        auto *rtl = addAction(
            1,
            2,
            QStringLiteral("RTL action"),
            QStringLiteral("Mirrored content order"));
        rtl->setLayoutDirection(Qt::RightToLeft);
        root->addLayout(actions);

        auto *images = new QHBoxLayout;
        images->setContentsMargins(0, 0, 0, 0);
        images->setSpacing(16);
        const QPixmap preview = zzCardScreenshotPixmap(window.palette());
        const auto addImage = [this, images](
                                  const QString &title,
                                  const QString &description) {
            auto *card = new ZzFluentUI::ZzImageCard(
                title,
                description,
                &window);
            images->addWidget(card, 1);
            return card;
        };
        auto *crop = addImage(
            QStringLiteral("Crop image"),
            QStringLiteral("Fill and center crop"));
        crop->setPixmap(preview);
        auto *fit = addImage(
            QStringLiteral("Fit image"),
            QStringLiteral("Show the complete image"));
        fit->setPixmap(preview);
        fit->setAspectRatioMode(Qt::KeepAspectRatio);
        addImage(
            QStringLiteral("Empty image"),
            QStringLiteral("Platform file placeholder"));
        auto *disabledImage = addImage(
            QStringLiteral("Disabled image"),
            QStringLiteral("Reduced image opacity"));
        disabledImage->setPixmap(preview);
        disabledImage->setEnabled(false);
        root->addLayout(images, 1);
    }

    /** @brief 展示画面并确定性设置 hover 与键盘焦点状态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        hoverCard->setAttribute(Qt::WA_UnderMouse, true);
        hoverCard->update();
        focusCard->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏卡片截图窗口。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;

private:
    QPointer<ZzFluentUI::ZzActionCard> hoverCard;
    QPointer<ZzFluentUI::ZzActionCard> focusCard;
};

/** @brief 保存轮播截图中文字遮罩及其覆盖数量。 */
struct ZzCarouselTextMask final
{
    QImage image;
    int carousels = 0;
    int titles = 0;
    int descriptions = 0;
};

/** @brief 构造轮播图片、空态、禁用、焦点、边界和 RTL 视觉矩阵。 */
class ZzCarouselScreenshotSurface final
{
public:
    /** @brief 创建六个只消费本地 QStandardItemModel 的固定轮播。 */
    ZzCarouselScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzCarouselScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Carousel Views"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(24, 24, 24, 24);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(16);
        const QPixmap preview = zzCardScreenshotPixmap(window.palette());

        carousels_.at(0) = addCarousel(
            grid,
            0,
            0,
            QStringLiteral("Image at first boundary"),
            QStringLiteral("Seven bounded indicators from nine model rows"),
            preview,
            9,
            0);
        carousels_.at(1) = addCarousel(
            grid,
            0,
            1,
            QStringLiteral("Empty image placeholder"),
            QStringLiteral("No decoration role is supplied"),
            {},
            3,
            1);
        carousels_.at(2) = addCarousel(
            grid,
            0,
            2,
            QStringLiteral(
                "A deliberately long carousel title that must remain inside"),
            QStringLiteral(
                "A long description verifies elision without changing layout"),
            preview,
            3,
            1);
        carousels_.at(3) = addCarousel(
            grid,
            1,
            0,
            QStringLiteral("Disabled carousel"),
            QStringLiteral("Image and controls use the disabled palette"),
            preview,
            3,
            1);
        carousels_.at(3)->setEnabled(false);
        carousels_.at(4) = addCarousel(
            grid,
            1,
            1,
            QStringLiteral("Keyboard focus"),
            {},
            preview,
            3,
            1);
        carousels_.at(5) = addCarousel(
            grid,
            1,
            2,
            QStringLiteral("RTL at last boundary"),
            QStringLiteral("Buttons, icons and content direction are mirrored"),
            preview,
            3,
            2);
        carousels_.at(5)->setLayoutDirection(Qt::RightToLeft);
    }

    /** @brief 展示画面并确定性设置唯一键盘焦点状态。 */
    void polish()
    {
        window.show();
        window.activateWindow();
        QCoreApplication::processEvents();
        carousels_.at(4)->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏轮播截图窗口。 */
    void hide()
    {
        window.hide();
    }

    /** @brief 返回指定视觉位置的轮播视图。 */
    [[nodiscard]] ZzFluentUI::ZzCarouselView *carousel(
        std::size_t index) const
    {
        return carousels_.at(index);
    }

    QWidget window;

private:
    /** @brief 创建一个 model 归属视图 QObject 树的轮播单元。 */
    ZzFluentUI::ZzCarouselView *addCarousel(
        QGridLayout *grid,
        int row,
        int column,
        const QString &title,
        const QString &description,
        const QPixmap &decoration,
        int itemCount,
        int currentRow)
    {
        auto *view = new ZzFluentUI::ZzCarouselView(&window);
        view->setAccessibleName(title);
        view->setAnimationDuration(0);
        auto *model = new QStandardItemModel(view);
        for (int itemRow = 0; itemRow < itemCount; ++itemRow) {
            const bool current = itemRow == currentRow;
            auto *item = new QStandardItem(
                current
                    ? title
                    : QStringLiteral("Neighbor item %1").arg(itemRow + 1));
            item->setData(
                current ? description : QStringLiteral("Neighbor description"),
                ZzFluentUI::ZzCarouselView::DescriptionRole);
            item->setData(
                current
                    ? QStringLiteral("Accessible %1").arg(title)
                    : QStringLiteral("Accessible neighbor"),
                Qt::AccessibleTextRole);
            if (!decoration.isNull()) {
                item->setData(decoration, Qt::DecorationRole);
            }
            model->appendRow(item);
        }
        view->setModel(model);
        view->setCurrentRow(currentRow);
        grid->addWidget(view, row, column);
        return view;
    }

    std::array<QPointer<ZzFluentUI::ZzCarouselView>, 6> carousels_{};
};

/** @brief 构造只覆盖轮播当前项标题和说明的字体差异遮罩。 */
ZzCarouselTextMask zzBuildCarouselTextMask(QWidget *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzCarouselTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    const auto carousels =
        surface->findChildren<ZzFluentUI::ZzCarouselView *>();
    for (ZzFluentUI::ZzCarouselView *carousel : carousels) {
        if (!carousel->isVisible() || carousel->viewport() == nullptr
            || !carousel->currentIndex().isValid()) {
            continue;
        }
        ++result.carousels;
        const QModelIndex current = carousel->currentIndex();
        const QString title = current.data(Qt::DisplayRole).toString();
        const QString description = current
            .data(ZzFluentUI::ZzCarouselView::DescriptionRole)
            .toString();
        const QRect itemRect = carousel->visualRect(current);
        const int bandHeight = description.isEmpty() ? 52 : 76;
        const QRect bandRect(
            itemRect.left(),
            std::max(itemRect.top(), itemRect.bottom() - bandHeight + 1),
            itemRect.width(),
            std::min(bandHeight, itemRect.height()));
        const QRect textRect = bandRect.adjusted(16, 8, -16, -8);

        QFont titleFont = carousel->font();
        titleFont.setWeight(QFont::DemiBold);
        const QFontMetrics titleMetrics(titleFont);
        const QString displayedTitle = titleMetrics.elidedText(
            title,
            Qt::ElideRight,
            textRect.width());
        const QRect titleBounds(
            textRect.left(),
            textRect.top(),
            textRect.width(),
            titleMetrics.height());
        const QRect titlePixels = titleMetrics.boundingRect(
            titleBounds,
            Qt::AlignLeading | Qt::AlignVCenter,
            displayedTitle);
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(carousel->viewport(), titlePixels, surface));
        ++result.titles;

        if (!description.isEmpty()) {
            const QFontMetrics descriptionMetrics(carousel->font());
            const QString displayedDescription =
                descriptionMetrics.elidedText(
                    description,
                    Qt::ElideRight,
                    textRect.width());
            const QRect descriptionBounds(
                textRect.left(),
                textRect.top() + titleMetrics.height() + 2,
                textRect.width(),
                std::max(
                    0,
                    textRect.height() - titleMetrics.height() - 2));
            const QRect descriptionPixels = descriptionMetrics.boundingRect(
                descriptionBounds,
                Qt::AlignLeading | Qt::AlignTop | Qt::TextWordWrap,
                displayedDescription);
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(
                    carousel->viewport(), descriptionPixels, surface));
            ++result.descriptions;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存命令与状态截图中文字遮罩及覆盖数量。 */
struct ZzCommandStatusTextMask final
{
    QImage image;
    int labels = 0;
    int toolButtons = 0;
    int statusMessages = 0;
};

/** @brief 构造标准工具栏、工具按钮与状态栏的确定性视觉矩阵。 */
class ZzCommandStatusScreenshotSurface final
{
public:
    /** @brief 创建横向、纵向、RTL、溢出和双状态栏场景。 */
    ZzCommandStatusScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzCommandStatusScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Command Workspace"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);

        auto *central = new QWidget(&window);
        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(28, 24, 28, 24);
        root->setSpacing(16);
        auto *heading = new QLabel(QStringLiteral("Release workspace"), central);
        QFont headingFont = heading->font();
        headingFont.setWeight(QFont::DemiBold);
        headingFont.setPointSize(15);
        heading->setFont(headingFont);
        root->addWidget(heading);
        root->addWidget(new QLabel(
            QStringLiteral("Build 4821 | Linux x86_64 | Release"),
            central));

        auto *summary = new QWidget(central);
        auto *summaryLayout = new QGridLayout(summary);
        summaryLayout->setContentsMargins(0, 8, 0, 8);
        summaryLayout->setHorizontalSpacing(48);
        summaryLayout->setVerticalSpacing(14);
        summaryLayout->addWidget(
            new QLabel(QStringLiteral("Configuration"), summary), 0, 0);
        summaryLayout->addWidget(
            new QLabel(QStringLiteral("Ready"), summary), 0, 1);
        summaryLayout->addWidget(
            new QLabel(QStringLiteral("Tests"), summary), 1, 0);
        summaryLayout->addWidget(
            new QLabel(QStringLiteral("97 passed"), summary), 1, 1);
        summaryLayout->setColumnStretch(2, 1);
        root->addWidget(summary);

        overflowBar_ = new QToolBar(
            QStringLiteral("Compact build commands"),
            central);
        overflowBar_->setMovable(false);
        overflowBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        overflowBar_->setFixedWidth(520);
        for (int index = 0; index < 12; ++index) {
            QAction *action = overflowBar_->addAction(
                window.style()->standardIcon(QStyle::SP_FileIcon),
                QStringLiteral("Command %1").arg(index + 1));
            if (index == 11) {
                lastOverflowAction_ = action;
            }
        }
        root->addWidget(overflowBar_, 0, Qt::AlignLeft);
        root->addStretch(1);

        normalStatus_ = new QStatusBar(central);
        normalStatus_->setSizeGripEnabled(false);
        normalStatus_->addWidget(
            new QLabel(QStringLiteral("Ready"), normalStatus_),
            1);
        normalStatus_->addPermanentWidget(
            new QLabel(QStringLiteral("main"), normalStatus_));
        root->addWidget(normalStatus_);
        window.setCentralWidget(central);

        topBar_ = new QToolBar(QStringLiteral("Build commands"), &window);
        topBar_->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
        topBar_->setMovable(true);
        topBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        topBar_->addAction(
            window.style()->standardIcon(QStyle::SP_DirOpenIcon),
            QStringLiteral("Open workspace"));
        QAction *watch = topBar_->addAction(
            window.style()->standardIcon(QStyle::SP_BrowserReload),
            QStringLiteral("Watch"));
        watch->setCheckable(true);
        watch->setChecked(true);
        QAction *unavailable = topBar_->addAction(
            window.style()->standardIcon(QStyle::SP_DialogCancelButton),
            QStringLiteral("Deploy"));
        unavailable->setEnabled(false);
        topBar_->addSeparator();
        menuAction_ = topBar_->addAction(
            window.style()->standardIcon(QStyle::SP_ComputerIcon),
            QStringLiteral("Target"));
        auto *targetMenu = new QMenu(topBar_);
        targetMenu->addAction(QStringLiteral("Linux x86_64"));
        targetMenu->addAction(QStringLiteral("Windows x86_64"));
        targetMenu->addAction(QStringLiteral("macOS universal"));
        menuAction_->setMenu(targetMenu);
        for (const QString &text : {
                 QStringLiteral("Configure"),
                 QStringLiteral("Build all"),
                 QStringLiteral("Run tests"),
                 QStringLiteral("Package")}) {
            topBar_->addAction(
                window.style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                text);
        }
        window.addToolBar(Qt::TopToolBarArea, topBar_);

        leftBar_ = new QToolBar(QStringLiteral("State commands"), &window);
        leftBar_->setAllowedAreas(Qt::LeftToolBarArea | Qt::RightToolBarArea);
        leftBar_->setMovable(true);
        leftBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
        checkedAction_ = leftBar_->addAction(
            window.style()->standardIcon(QStyle::SP_DialogApplyButton),
            QStringLiteral("Checked command"));
        checkedAction_->setCheckable(true);
        checkedAction_->setChecked(true);
        disabledAction_ = leftBar_->addAction(
            window.style()->standardIcon(QStyle::SP_TrashIcon),
            QStringLiteral("Disabled command"));
        disabledAction_->setEnabled(false);
        hoverAction_ = leftBar_->addAction(
            window.style()->standardIcon(QStyle::SP_BrowserReload),
            QStringLiteral("Hovered command"));
        pressedAction_ = leftBar_->addAction(
            window.style()->standardIcon(QStyle::SP_MediaStop),
            QStringLiteral("Pressed command"));
        focusAction_ = leftBar_->addAction(
            window.style()->standardIcon(QStyle::SP_MessageBoxInformation),
            QStringLiteral("Focused command"));
        QAction *more = leftBar_->addAction(
            window.style()->standardIcon(QStyle::SP_TitleBarUnshadeButton),
            QStringLiteral("More commands"));
        auto *moreMenu = new QMenu(leftBar_);
        moreMenu->addAction(QStringLiteral("Inspect"));
        moreMenu->addAction(QStringLiteral("Archive"));
        more->setMenu(moreMenu);
        window.addToolBar(Qt::LeftToolBarArea, leftBar_);

        rtlBar_ = new QToolBar(QStringLiteral("RTL commands"), &window);
        rtlBar_->setAllowedAreas(Qt::LeftToolBarArea | Qt::RightToolBarArea);
        rtlBar_->setMovable(true);
        rtlBar_->setLayoutDirection(Qt::RightToLeft);
        rtlBar_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        rtlBar_->addAction(
            window.style()->standardIcon(QStyle::SP_DialogOpenButton),
            QStringLiteral("Open"));
        rtlBar_->addAction(
            window.style()->standardIcon(QStyle::SP_FileDialogInfoView),
            QStringLiteral("Details"));
        rtlBar_->addSeparator();
        rtlBar_->addAction(
            window.style()->standardIcon(QStyle::SP_DialogSaveButton),
            QStringLiteral("Save"));
        window.addToolBar(Qt::RightToolBarArea, rtlBar_);

        bottomStatus_ = new QStatusBar(&window);
        bottomStatus_->setSizeGripEnabled(true);
        bottomStatus_->addPermanentWidget(new QLabel(
            QStringLiteral("Local | 8 workers"),
            bottomStatus_));
        bottomStatus_->showMessage(QStringLiteral("Synchronizing artifacts"), 0);
        window.setStatusBar(bottomStatus_);
    }

    /** @brief 展示场景并固定 hover、pressed 与键盘焦点状态。 */
    void polish()
    {
        window.show();
        window.activateWindow();
        QCoreApplication::processEvents();
        hoverButton_ = qobject_cast<QToolButton *>(
            leftBar_->widgetForAction(hoverAction_));
        pressedButton_ = qobject_cast<QToolButton *>(
            leftBar_->widgetForAction(pressedAction_));
        focusButton_ = qobject_cast<QToolButton *>(
            leftBar_->widgetForAction(focusAction_));
        if (hoverButton_ != nullptr) {
            hoverButton_->setAttribute(Qt::WA_UnderMouse, true);
            hoverButton_->update();
        }
        if (pressedButton_ != nullptr) {
            pressedButton_->setDown(true);
        }
        if (focusButton_ != nullptr) {
            focusButton_->setFocus(Qt::TabFocusReason);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏命令与状态截图窗口。 */
    void hide()
    {
        window.hide();
    }

    /** @brief 返回顶部文本工具栏。 */
    [[nodiscard]] QToolBar *topBar() const noexcept
    {
        return topBar_;
    }

    /** @brief 返回左侧图标工具栏。 */
    [[nodiscard]] QToolBar *leftBar() const noexcept
    {
        return leftBar_;
    }

    /** @brief 返回右侧 RTL 工具栏。 */
    [[nodiscard]] QToolBar *rtlBar() const noexcept
    {
        return rtlBar_;
    }

    /** @brief 返回保证产生溢出的窄工具栏。 */
    [[nodiscard]] QToolBar *overflowBar() const noexcept
    {
        return overflowBar_;
    }

    /** @brief 返回窄工具栏最后一个 action。 */
    [[nodiscard]] QAction *lastOverflowAction() const noexcept
    {
        return lastOverflowAction_;
    }

    /** @brief 返回已选中 action。 */
    [[nodiscard]] QAction *checkedAction() const noexcept
    {
        return checkedAction_;
    }

    /** @brief 返回已禁用 action。 */
    [[nodiscard]] QAction *disabledAction() const noexcept
    {
        return disabledAction_;
    }

    /** @brief 返回带标准菜单的 action。 */
    [[nodiscard]] QAction *menuAction() const noexcept
    {
        return menuAction_;
    }

    /** @brief 返回固定按下状态的工具按钮。 */
    [[nodiscard]] QToolButton *pressedButton() const noexcept
    {
        return pressedButton_;
    }

    /** @brief 返回固定键盘焦点的工具按钮。 */
    [[nodiscard]] QToolButton *focusButton() const noexcept
    {
        return focusButton_;
    }

    /** @brief 返回固定 hover 状态的工具按钮。 */
    [[nodiscard]] QToolButton *hoverButton() const noexcept
    {
        return hoverButton_;
    }

    /** @brief 返回没有临时消息的内嵌状态栏。 */
    [[nodiscard]] QStatusBar *normalStatus() const noexcept
    {
        return normalStatus_;
    }

    /** @brief 返回带临时消息和 size grip 的窗口状态栏。 */
    [[nodiscard]] QStatusBar *bottomStatus() const noexcept
    {
        return bottomStatus_;
    }

    QMainWindow window;

private:
    QPointer<QToolBar> topBar_;
    QPointer<QToolBar> leftBar_;
    QPointer<QToolBar> rtlBar_;
    QPointer<QToolBar> overflowBar_;
    QPointer<QStatusBar> normalStatus_;
    QPointer<QStatusBar> bottomStatus_;
    QPointer<QAction> lastOverflowAction_;
    QPointer<QAction> checkedAction_;
    QPointer<QAction> disabledAction_;
    QPointer<QAction> menuAction_;
    QPointer<QAction> hoverAction_;
    QPointer<QAction> pressedAction_;
    QPointer<QAction> focusAction_;
    QPointer<QToolButton> hoverButton_;
    QPointer<QToolButton> pressedButton_;
    QPointer<QToolButton> focusButton_;
};

/** @brief 构造命令与状态截图的跨字体栅格化差异遮罩。 */
ZzCommandStatusTextMask zzBuildCommandStatusTextMask(
    QWidget *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzCommandStatusTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    for (QLabel *label : surface->findChildren<QLabel *>()) {
        if (!label->isVisible() || label->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            label,
            label->contentsRect(),
            static_cast<int>(label->alignment()),
            label->text());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(label, textRect, surface));
        ++result.labels;
    }

    for (QToolButton *button : surface->findChildren<QToolButton *>()) {
        if (!button->isVisible() || button->text().isEmpty()
            || button->toolButtonStyle() == Qt::ToolButtonIconOnly) {
            continue;
        }
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(
                button,
                button->contentsRect().adjusted(2, 2, -2, -2),
                surface));
        ++result.toolButtons;
    }

    for (QStatusBar *status : surface->findChildren<QStatusBar *>()) {
        if (!status->isVisible() || status->currentMessage().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            status,
            status->contentsRect().adjusted(6, 0, -160, 0),
            Qt::AlignLeft | Qt::AlignVCenter,
            status->currentMessage());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(status, textRect, surface));
        ++result.statusMessages;
    }
    painter.end();
    return result;
}

/** @brief 保存标签页截图中文字遮罩及覆盖数量。 */
struct ZzTabTextMask final
{
    QImage image;
    int tabBars = 0;
    int tabTexts = 0;
};

/** @brief 构造只包含标签页关键视觉状态的独立确定性截图面。 */
class ZzTabScreenshotSurface final
{
public:
    /** @brief 创建普通、溢出、RTL 和垂直标签页视觉矩阵。 */
    ZzTabScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzTabScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Tabs"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(28, 28, 28, 28);
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(24);

        auto *normal = createTabs(&window);
        addPage(normal, QStringLiteral("Overview"));
        addPage(normal, QStringLiteral("Details"));
        addPage(normal, QStringLiteral("History"));
        addPage(normal, QStringLiteral("Disabled"));
        normal->setTabEnabled(3, false);
        normal->setCurrentIndex(1);
        focusBar = normal->fluentTabBar();
        grid->addWidget(normal, 0, 0);

        auto *overflow = createTabs(&window);
        overflow->setTabsClosable(true);
        for (int index = 0; index < 12; ++index) {
            addPage(
                overflow,
                QStringLiteral("Long workspace tab %1").arg(index + 1));
        }
        overflow->setCurrentIndex(4);
        hoverBar = overflow->fluentTabBar();
        grid->addWidget(overflow, 0, 1);

        auto *rtl = createTabs(&window);
        rtl->setLayoutDirection(Qt::RightToLeft);
        rtl->setTabsClosable(true);
        addPage(rtl, QStringLiteral("Primary"));
        addPage(rtl, QStringLiteral("Secondary"));
        addPage(rtl, QStringLiteral("Archive"));
        rtl->setCurrentIndex(0);
        grid->addWidget(rtl, 1, 0);

        auto *vertical = createTabs(&window);
        vertical->setTabPosition(QTabWidget::West);
        addPage(vertical, QStringLiteral("General"));
        addPage(vertical, QStringLiteral("Appearance"));
        addPage(vertical, QStringLiteral("Advanced"));
        addPage(vertical, QStringLiteral("About"));
        vertical->setCurrentIndex(2);
        grid->addWidget(vertical, 1, 1);
    }

    /** @brief 展示画面并确定性设置 hover 与键盘焦点状态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (hoverBar != nullptr && hoverBar->count() > 1) {
            const QPoint localPosition = hoverBar->tabRect(1).center();
            const QPoint globalPosition = hoverBar->mapToGlobal(localPosition);
            QEnterEvent enterEvent{
                QPointF(localPosition),
                QPointF(localPosition),
                QPointF(globalPosition)};
            QCoreApplication::sendEvent(hoverBar, &enterEvent);
            QMouseEvent moveEvent(
                QEvent::MouseMove,
                QPointF(localPosition),
                QPointF(globalPosition),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(hoverBar, &moveEvent);
        }
        if (focusBar != nullptr) {
            focusBar->setFocus(Qt::TabFocusReason);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏标签页截图窗口。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;

private:
    /** @brief 创建尺寸稳定且不带业务行为的标签容器。 */
    static ZzFluentUI::ZzTabWidget *createTabs(QWidget *parent)
    {
        auto *tabs = new ZzFluentUI::ZzTabWidget(parent);
        tabs->setMinimumSize(520, 340);
        return tabs;
    }

    /** @brief 向指定容器添加无文字内容页，避免重复字体遮罩。 */
    static void addPage(
        ZzFluentUI::ZzTabWidget *tabs,
        const QString &text)
    {
        auto *page = new QWidget(tabs);
        page->setAutoFillBackground(true);
        QPalette pagePalette = page->palette();
        pagePalette.setColor(
            QPalette::Window,
            tabs->palette().color(QPalette::AlternateBase));
        page->setPalette(pagePalette);
        tabs->addTab(page, text);
    }

    QPointer<ZzFluentUI::ZzTabBar> hoverBar;
    QPointer<ZzFluentUI::ZzTabBar> focusBar;
};

/** @brief 保存环形进度截图中文字遮罩及覆盖数量。 */
struct ZzProgressRingTextMask final
{
    QImage image;
    int progressRings = 0;
    int textRings = 0;
};

/** @brief 构造只包含环形进度关键视觉状态的独立确定性截图面。 */
class ZzProgressRingScreenshotSurface final
{
public:
    /** @brief 创建十个确定、不确定、线宽和方向状态的固定矩阵。 */
    ZzProgressRingScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzProgressRingScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Progress Rings"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(60, 60, 60, 60);
        grid->setHorizontalSpacing(40);
        grid->setVerticalSpacing(48);

        const auto addRing = [this, grid](
                                 int row,
                                 int column,
                                 int value,
                                 int ringWidth = 4) {
            auto *ring = new ZzFluentUI::ZzProgressRing(&window);
            ring->setFixedSize(150, 150);
            ring->setValue(value);
            ring->setRingWidth(ringWidth);
            grid->addWidget(ring, row, column, Qt::AlignCenter);
            return ring;
        };

        addRing(0, 0, 0);
        addRing(0, 1, 25);
        addRing(0, 2, 50);
        addRing(0, 3, 72);
        addRing(0, 4, 100);
        addRing(1, 0, 40, 2);
        auto *wide = addRing(1, 1, 60, 8);
        wide->setRange(20, 120);
        wide->setValue(70);
        auto *inverted = addRing(1, 2, 25, 5);
        inverted->setInvertedAppearance(true);
        auto *busy = addRing(1, 3, 0, 5);
        busy->setTextVisible(false);
        busy->setRange(0, 0);
        auto *disabledBusy = addRing(1, 4, 0, 5);
        disabledBusy->setTextVisible(false);
        disabledBusy->setRange(0, 0);
        disabledBusy->setEnabled(false);
    }

    /** @brief 展示并完成 palette、字体和 reduced-motion 状态同步。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏环形进度截图窗口。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;
};

/** @brief 构造只包含滚动控件关键视觉状态的独立确定性截图面。 */
class ZzScrollControlsScreenshotSurface final
{
public:
    /**
     * @brief 创建普通、交互、RTL、禁用、原生回退和双轴滚动状态。
     */
    ZzScrollControlsScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzScrollControlsScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Scroll Controls"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);

        createHorizontal(80, 8, 18);
        hoverBar = createHorizontal(160, 24, 38);
        pressedBar = createHorizontal(240, 44, 54);
        auto *rtl = createHorizontal(320, 18, 72);
        rtl->setLayoutDirection(Qt::RightToLeft);
        auto *disabled = createHorizontal(400, 54, 46);
        disabled->setEnabled(false);

        auto *standard = new QScrollBar(Qt::Horizontal, &window);
        standard->setGeometry(60, 480, 500, 12);
        standard->setRange(0, 100);
        standard->setPageStep(32);
        standard->setValue(62);
        standard->setAttribute(Qt::WA_UnderMouse, true);

        createVertical(680, 5, 24);
        auto *longVertical = createVertical(800, 72, 68);
        longVertical->setInvertedAppearance(true);

        area = new ZzFluentUI::ZzScrollArea(&window);
        area->setGeometry(650, 470, 490, 260);
        auto *content = new QWidget;
        content->setFixedSize(760, 430);
        content->setAutoFillBackground(true);
        QPalette contentPalette = content->palette();
        contentPalette.setColor(
            QPalette::Window,
            window.palette().color(QPalette::AlternateBase));
        content->setPalette(contentPalette);
        auto *accentBlock = new QWidget(content);
        accentBlock->setGeometry(80, 70, 520, 250);
        accentBlock->setAutoFillBackground(true);
        QPalette accentPalette = accentBlock->palette();
        accentPalette.setColor(
            QPalette::Window,
            window.palette().color(QPalette::Highlight));
        accentBlock->setPalette(accentPalette);
        area->setWidget(content);
    }

    /** @brief 展示画面并把 hover 与 pressed 同步到确定终态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (area != nullptr) {
            if (auto *horizontal = area->fluentHorizontalScrollBar();
                horizontal != nullptr) {
                horizontal->setValue(horizontal->maximum() / 2);
            }
            if (auto *vertical = area->fluentVerticalScrollBar();
                vertical != nullptr) {
                vertical->setValue(vertical->maximum() / 2);
            }
        }
        if (hoverBar != nullptr) {
            hoverBar->setAttribute(Qt::WA_UnderMouse, true);
            const QPointF center = QRectF(hoverBar->rect()).center();
            QEnterEvent enter(center, center, center);
            QCoreApplication::sendEvent(hoverBar, &enter);
        }
        if (pressedBar != nullptr) {
            pressedBar->setAttribute(Qt::WA_UnderMouse, true);
            QStyleOptionSlider option;
            option.initFrom(pressedBar);
            option.subControls = QStyle::SC_All;
            option.orientation = pressedBar->orientation();
            option.minimum = pressedBar->minimum();
            option.maximum = pressedBar->maximum();
            option.pageStep = pressedBar->pageStep();
            option.singleStep = pressedBar->singleStep();
            option.sliderPosition = pressedBar->sliderPosition();
            option.sliderValue = pressedBar->value();
            option.upsideDown = false;
            const QRect slider = pressedBar->style()->subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarSlider,
                pressedBar);
            QTest::mousePress(
                pressedBar,
                Qt::LeftButton,
                Qt::NoModifier,
                slider.center());
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回 pressed 示例是否保持 Qt 原生拖动按下状态。 */
    [[nodiscard]] bool pressedStateIsActive() const noexcept
    {
        return pressedBar != nullptr && pressedBar->isSliderDown();
    }

    /** @brief 返回当前画布内仍在运行的滚动条动画数量。 */
    [[nodiscard]] qsizetype runningAnimationCount() const
    {
        const auto animations = window.findChildren<QAbstractAnimation *>();
        return std::count_if(
            animations.cbegin(),
            animations.cend(),
            [](const QAbstractAnimation *animation) {
                return animation->state() == QAbstractAnimation::Running;
            });
    }

    /** @brief 隐藏滚动控件截图窗口并停止全部呈现动画。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;

private:
    /** @brief 创建固定位置和范围的水平 Fluent 滚动条。 */
    ZzFluentUI::ZzScrollBar *createHorizontal(
        int top,
        int pageStep,
        int value)
    {
        auto *scrollBar = new ZzFluentUI::ZzScrollBar(
            Qt::Horizontal,
            &window);
        scrollBar->setGeometry(60, top, 500, 12);
        scrollBar->setRange(0, 100);
        scrollBar->setPageStep(pageStep);
        scrollBar->setValue(value);
        return scrollBar;
    }

    /** @brief 创建固定位置和范围的垂直 Fluent 滚动条。 */
    ZzFluentUI::ZzScrollBar *createVertical(
        int left,
        int pageStep,
        int value)
    {
        auto *scrollBar = new ZzFluentUI::ZzScrollBar(
            Qt::Vertical,
            &window);
        scrollBar->setGeometry(left, 80, 12, 320);
        scrollBar->setRange(0, 100);
        scrollBar->setPageStep(pageStep);
        scrollBar->setValue(value);
        return scrollBar;
    }

    QPointer<ZzFluentUI::ZzScrollBar> hoverBar;
    QPointer<ZzFluentUI::ZzScrollBar> pressedBar;
    QPointer<ZzFluentUI::ZzScrollArea> area;
};

/** @brief 保存标准文本输入截图中文字遮罩及控件覆盖数量。 */
struct ZzTextInputTextMask final
{
    QImage image;
    int lineEdits = 0;
    int textEdits = 0;
    int plainTextEdits = 0;
};

/** @brief 构造只包含标准文本输入关键视觉状态的确定性截图面。 */
class ZzTextInputScreenshotSurface final
{
public:
    /** @brief 创建编辑器类型、方向、文本和交互状态的固定矩阵。 */
    ZzTextInputScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzTextInputScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Text Inputs"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(70, 54, 70, 54);
        grid->setHorizontalSpacing(58);
        grid->setVerticalSpacing(54);

        const auto addLine = [this, grid](
                                 int row,
                                 int column,
                                 const QString &text) {
            auto *editor = new QLineEdit(&window);
            editor->setText(text);
            editor->setFixedSize(300, 48);
            grid->addWidget(editor, row, column);
            return editor;
        };

        addLine(0, 0, QStringLiteral("Workspace"));
        auto *placeholder = addLine(0, 1, {});
        placeholder->setPlaceholderText(QStringLiteral("Search settings"));
        auto *password = addLine(0, 2, QStringLiteral("Fluent-2026"));
        password->setEchoMode(QLineEdit::Password);

        auto *clearAction = addLine(1, 0, QStringLiteral("Clear action"));
        clearAction->setClearButtonEnabled(true);
        auto *rtl = addLine(1, 1, QStringLiteral("RTL input"));
        rtl->setLayoutDirection(Qt::RightToLeft);
        rtl->setAlignment(Qt::AlignRight);
        auto *readOnly = addLine(1, 2, QStringLiteral("Read-only value"));
        readOnly->setReadOnly(true);

        focusEditor = addLine(2, 0, QStringLiteral("Focused text"));
        focusEditor->selectAll();
        hoverEditor = addLine(2, 1, QStringLiteral("Hovered text"));
        auto *disabled = addLine(2, 2, QStringLiteral("Disabled value"));
        disabled->setEnabled(false);

        auto *rich = new QTextEdit(&window);
        rich->setHtml(QStringLiteral(
            "<b>Rich text</b><br>Selection and formatting"));
        rich->setFixedSize(300, 104);
        grid->addWidget(rich, 3, 0);
        auto *plain = new QPlainTextEdit(&window);
        plain->setPlainText(QStringLiteral(
            "Plain text\nLarge-document editor"));
        plain->setFixedSize(300, 104);
        grid->addWidget(plain, 3, 1);
        auto *browser = new QTextBrowser(&window);
        browser->setPlainText(QStringLiteral(
            "Read-only browser\nLink-ready content"));
        browser->setFixedSize(300, 104);
        grid->addWidget(browser, 3, 2);
    }

    /** @brief 展示画面并通过真实事件固定 focus 与 hover 状态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (focusEditor != nullptr) {
            focusEditor->setFocus(Qt::TabFocusReason);
            focusEditor->selectAll();
        }
        if (hoverEditor != nullptr) {
            hoverEditor->setAttribute(Qt::WA_UnderMouse, true);
            const QPoint center = hoverEditor->rect().center();
            const QPoint globalCenter = hoverEditor->mapToGlobal(center);
            QEnterEvent enter{
                QPointF(center),
                QPointF(center),
                QPointF(globalCenter)};
            QCoreApplication::sendEvent(hoverEditor, &enter);
            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(center),
                QPointF(globalCenter),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(hoverEditor, &move);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回 clear action 是否由 Qt 原生编辑器创建。 */
    [[nodiscard]] bool hasNativeClearAction() const
    {
        const auto lineEdits = window.findChildren<QLineEdit *>();
        return std::any_of(
            lineEdits.cbegin(),
            lineEdits.cend(),
            [](const QLineEdit *editor) {
                return !editor->findChildren<QAbstractButton *>().isEmpty();
            });
    }

    /** @brief 隐藏标准文本输入截图窗口。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;

private:
    QPointer<QLineEdit> focusEditor;
    QPointer<QLineEdit> hoverEditor;
};

/** @brief 为独立标准文本输入画面精确遮罩编辑器文字。 */
ZzTextInputTextMask zzBuildTextInputTextMask(QWidget *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzTextInputTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    const auto lineEdits = surface->findChildren<QLineEdit *>();
    for (QLineEdit *lineEdit : lineEdits) {
        if (!lineEdit->isVisible()) {
            continue;
        }
        ++result.lineEdits;
        const QString text = lineEdit->text().isEmpty()
            ? lineEdit->placeholderText()
            : lineEdit->displayText();
        if (text.isEmpty()) {
            continue;
        }
        QStyleOptionFrame option;
        option.initFrom(lineEdit);
        const QRect contents = lineEdit->style()->subElementRect(
            QStyle::SE_LineEditContents,
            &option,
            lineEdit);
        const QRect textRect = zzAlignedTextRect(
            lineEdit,
            contents.adjusted(2, 0, -2, 0),
            static_cast<int>(lineEdit->alignment() | Qt::AlignVCenter),
            text);
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(lineEdit, textRect, surface));
    }

    const auto textEdits = surface->findChildren<QTextEdit *>();
    for (QTextEdit *textEdit : textEdits) {
        if (!textEdit->isVisible() || textEdit->viewport() == nullptr) {
            continue;
        }
        ++result.textEdits;
        if (textEdit->toPlainText().isEmpty()) {
            continue;
        }
        const QRect bounds = textEdit->viewport()->rect().adjusted(
            4,
            4,
            -4,
            -4);
        const QRect textRect = zzAlignedTextRect(
            textEdit,
            bounds,
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            textEdit->toPlainText());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(textEdit->viewport(), textRect, surface));
    }

    const auto plainTextEdits = surface->findChildren<QPlainTextEdit *>();
    for (QPlainTextEdit *textEdit : plainTextEdits) {
        if (!textEdit->isVisible() || textEdit->viewport() == nullptr) {
            continue;
        }
        ++result.plainTextEdits;
        if (textEdit->toPlainText().isEmpty()) {
            continue;
        }
        const QRect bounds = textEdit->viewport()->rect().adjusted(
            4,
            4,
            -4,
            -4);
        const QRect textRect = zzAlignedTextRect(
            textEdit,
            bounds,
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            textEdit->toPlainText());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(textEdit->viewport(), textRect, surface));
    }
    painter.end();
    return result;
}

/** @brief 保存组合框截图中文字遮罩及闭合面、编辑器和 popup 覆盖数量。 */
struct ZzComboBoxTextMask final
{
    QImage image;
    int comboBoxes = 0;
    int closedLabels = 0;
    int editableEditors = 0;
    int popupItems = 0;
};

/** @brief 构造标准组合框闭合面与 popup 关键视觉状态的确定性截图面。 */
class ZzComboBoxScreenshotSurface final
{
public:
    /** @brief 创建文字、图标、方向、交互与 popup 状态的固定矩阵。 */
    ZzComboBoxScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzComboBoxScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Combo Boxes"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(70, 62, 70, 62);
        grid->setHorizontalSpacing(58);
        grid->setVerticalSpacing(76);

        const auto addComboBox = [this, grid](int row, int column) {
            auto *comboBox = new QComboBox(&window);
            comboBox->setFixedSize(300, 48);
            grid->addWidget(comboBox, row, column);
            return comboBox;
        };

        auto *normal = addComboBox(0, 0);
        normal->addItems({
            QStringLiteral("Balanced"),
            QStringLiteral("Compact"),
            QStringLiteral("Comfortable")});

        auto *placeholder = addComboBox(0, 1);
        placeholder->setPlaceholderText(QStringLiteral("Select environment"));
        placeholder->addItems({
            QStringLiteral("Local"),
            QStringLiteral("Remote")});
        placeholder->setCurrentIndex(-1);

        auto *icon = addComboBox(0, 2);
        icon->addItem(
            QIcon(QStringLiteral(
                ":/zzfluent/screenshots/ZzFluentTestSquare.svg")),
            QStringLiteral("Icon option"));
        icon->addItem(QStringLiteral("Text option"));

        auto *editable = addComboBox(1, 0);
        editable->setEditable(true);
        editable->setInsertPolicy(QComboBox::NoInsert);
        editable->addItems({
            QStringLiteral("Debug"),
            QStringLiteral("Release"),
            QStringLiteral("RelWithDebInfo")});
        editable->setEditText(QStringLiteral("Editable target"));
        editable->setCompleter(new QCompleter(
            QStringList{
                QStringLiteral("Debug"),
                QStringLiteral("Release"),
                QStringLiteral("RelWithDebInfo")},
            editable));

        auto *longText = addComboBox(1, 1);
        longText->addItem(QStringLiteral(
            "A deliberately long selection that must stay inside the field"));

        auto *rightToLeft = addComboBox(1, 2);
        rightToLeft->setLayoutDirection(Qt::RightToLeft);
        rightToLeft->addItems({
            QStringLiteral("RTL primary"),
            QStringLiteral("RTL secondary")});

        auto *focused = addComboBox(2, 0);
        focused->addItems({
            QStringLiteral("Focused option"),
            QStringLiteral("Alternative")});
        focusComboBox = focused;

        auto *hovered = addComboBox(2, 1);
        hovered->addItems({
            QStringLiteral("Hovered option"),
            QStringLiteral("Alternative")});
        hoverComboBox = hovered;

        auto *disabled = addComboBox(2, 2);
        disabled->addItem(QStringLiteral("Disabled option"));
        disabled->setEnabled(false);

        popupComboBox = addComboBox(3, 0);
        auto *popupModel = new QStandardItemModel(popupComboBox);
        auto *selectedItem = new QStandardItem(
            QIcon(QStringLiteral(
                ":/zzfluent/screenshots/ZzFluentTestSquare.svg")),
            QStringLiteral("Selected item"));
        popupModel->appendRow(selectedItem);
        popupModel->appendRow(new QStandardItem(
            QStringLiteral("Hovered item")));
        auto *disabledItem = new QStandardItem(
            QStringLiteral("Disabled item"));
        disabledItem->setEnabled(false);
        popupModel->appendRow(disabledItem);
        popupModel->appendRow(new QStandardItem(
            QStringLiteral("Long popup item for clipping")));
        popupComboBox->setModel(popupModel);
        popupComboBox->setCurrentIndex(0);
        popupComboBox->setMaxVisibleItems(4);
    }

    /** @brief 展示画面并通过真实事件固定 focus、hover 与 popup item 状态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (focusComboBox != nullptr) {
            focusComboBox->setFocus(Qt::TabFocusReason);
        }
        if (hoverComboBox != nullptr) {
            hoverComboBox->setAttribute(Qt::WA_UnderMouse, true);
            const QPoint center = hoverComboBox->rect().center();
            const QPoint globalCenter = hoverComboBox->mapToGlobal(center);
            QEnterEvent enter{
                QPointF(center),
                QPointF(center),
                QPointF(globalCenter)};
            QCoreApplication::sendEvent(hoverComboBox, &enter);
            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(center),
                QPointF(globalCenter),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(hoverComboBox, &move);
        }
        if (popupComboBox != nullptr) {
            popupComboBox->showPopup();
            QCoreApplication::processEvents();
            QAbstractItemView *popupView = popupComboBox->view();
            if (popupView != nullptr && popupView->viewport() != nullptr) {
                popupView->setCurrentIndex(popupComboBox->model()->index(0, 0));
                const QModelIndex hovered = popupComboBox->model()->index(1, 0);
                popupView->scrollTo(hovered);
                const QRect hoveredRect = popupView->visualRect(hovered);
                popupView->viewport()->setAttribute(Qt::WA_UnderMouse, true);
                const QPoint center = hoveredRect.center();
                const QPoint globalCenter =
                    popupView->viewport()->mapToGlobal(center);
                QMouseEvent move(
                    QEvent::MouseMove,
                    QPointF(center),
                    QPointF(globalCenter),
                    Qt::NoButton,
                    Qt::NoButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(popupView->viewport(), &move);
            }
        }
        if (focusComboBox != nullptr) {
            focusComboBox->setFocus(Qt::TabFocusReason);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回组合框 popup 的公开顶层窗口；未创建时返回空指针。 */
    [[nodiscard]] QWidget *popupWindow() const noexcept
    {
        if (popupComboBox == nullptr || popupComboBox->view() == nullptr) {
            return nullptr;
        }
        return popupComboBox->view()->window();
    }

    /** @brief 返回打开 popup 的标准组合框。 */
    [[nodiscard]] QComboBox *openComboBox() const noexcept
    {
        return popupComboBox;
    }

    /** @brief 关闭 popup 并隐藏组合框截图窗口。 */
    void hide()
    {
        if (popupComboBox != nullptr) {
            popupComboBox->hidePopup();
        }
        window.hide();
    }

    QWidget window;

private:
    QPointer<QComboBox> focusComboBox;
    QPointer<QComboBox> hoverComboBox;
    QPointer<QComboBox> popupComboBox;
};

/** @brief 为独立组合框画面遮罩闭合文字、编辑器文字与 popup item 文字。 */
ZzComboBoxTextMask zzBuildComboBoxTextMask(
    ZzComboBoxScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzComboBoxTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    const auto comboBoxes = surface->window.findChildren<QComboBox *>();
    for (QComboBox *comboBox : comboBoxes) {
        if (!comboBox->isVisible()) {
            continue;
        }
        ++result.comboBoxes;
        if (comboBox->isEditable()) {
            QLineEdit *editor = comboBox->lineEdit();
            if (editor == nullptr || editor->displayText().isEmpty()) {
                continue;
            }
            QStyleOptionFrame option;
            option.initFrom(editor);
            const QRect contents = editor->style()->subElementRect(
                QStyle::SE_LineEditContents,
                &option,
                editor);
            const QRect textRect = zzAlignedTextRect(
                editor,
                contents.adjusted(2, 0, -2, 0),
                static_cast<int>(editor->alignment() | Qt::AlignVCenter),
                editor->displayText());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(editor, textRect, &surface->window));
            ++result.editableEditors;
            continue;
        }

        const QString text = comboBox->currentIndex() < 0
            ? comboBox->placeholderText()
            : comboBox->currentText();
        if (text.isEmpty()) {
            continue;
        }
        QStyleOptionComboBox option;
        option.initFrom(comboBox);
        option.rect = comboBox->rect();
        option.editable = comboBox->isEditable();
        QRect contents = comboBox->style()->subControlRect(
            QStyle::CC_ComboBox,
            &option,
            QStyle::SC_ComboBoxEditField,
            comboBox);
        if (comboBox->currentIndex() >= 0
            && !comboBox->itemIcon(comboBox->currentIndex()).isNull()) {
            const int decorationWidth = comboBox->iconSize().width() + 4;
            QRect logicalContents = QStyle::visualRect(
                comboBox->layoutDirection(),
                comboBox->rect(),
                contents);
            logicalContents.adjust(decorationWidth, 0, 0, 0);
            contents = QStyle::visualRect(
                comboBox->layoutDirection(),
                comboBox->rect(),
                logicalContents);
        }
        const int alignment = static_cast<int>(
            comboBox->layoutDirection() == Qt::RightToLeft
                ? Qt::AlignRight | Qt::AlignVCenter
                : Qt::AlignLeft | Qt::AlignVCenter);
        const QRect textRect = zzAlignedTextRect(
            comboBox,
            contents,
            alignment,
            text);
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(comboBox, textRect, &surface->window));
        ++result.closedLabels;
    }

    QComboBox *openComboBox = surface->openComboBox();
    QWidget *popupWindow = surface->popupWindow();
    if (openComboBox != nullptr && popupWindow != nullptr) {
        QAbstractItemView *popupView = openComboBox->view();
        QAbstractItemModel *model = popupView->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            const QString text = index.data(Qt::DisplayRole).toString();
            const QRect visual = popupView->visualRect(index);
            if (text.isEmpty() || visual.isEmpty()) {
                continue;
            }
            QRect contents = visual.adjusted(12, 0, -8, 0);
            if (!index.data(Qt::DecorationRole).value<QIcon>().isNull()) {
                contents.adjust(openComboBox->iconSize().width() + 4, 0, 0, 0);
            }
            const QRect localText = zzAlignedTextRect(
                popupView,
                contents,
                Qt::AlignLeft | Qt::AlignVCenter,
                text);
            const QPoint popupOffset = popupView->viewport()->mapTo(
                popupWindow,
                localText.topLeft());
            zzPaintMaskRect(
                &painter,
                QRect(zzComboBoxPopupOrigin + popupOffset, localText.size()));
            ++result.popupItems;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存搜索建议框截图的输入文字、popup 文字与覆盖数量。 */
struct ZzSuggestBoxTextMask final
{
    QImage image;
    int suggestBoxes = 0;
    int inputTexts = 0;
    int popupItems = 0;
};

/** @brief 构造搜索输入状态与真实 completer popup 的确定性截图面。 */
class ZzSuggestBoxScreenshotSurface final
{
public:
    /** @brief 创建 placeholder、状态、方向、长文本与 popup 固定矩阵。 */
    ZzSuggestBoxScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzSuggestBoxScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Suggest Boxes"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(70, 62, 70, 62);
        grid->setHorizontalSpacing(58);
        grid->setVerticalSpacing(76);

        const auto addBox = [this, grid](int row, int column) {
            auto *box = new ZzFluentUI::ZzSuggestBox(&window);
            box->setFixedSize(300, 48);
            grid->addWidget(box, row, column);
            return box;
        };

        auto *placeholder = addBox(0, 0);
        placeholder->setPlaceholderText(QStringLiteral("Search commands"));

        auto *typed = addBox(0, 1);
        typed->setText(QStringLiteral("Typed command"));

        auto *readOnly = addBox(0, 2);
        readOnly->setText(QStringLiteral("Read-only search"));
        readOnly->setReadOnly(true);

        auto *disabled = addBox(1, 0);
        disabled->setText(QStringLiteral("Disabled search"));
        disabled->setEnabled(false);

        auto *rightToLeft = addBox(1, 1);
        rightToLeft->setLayoutDirection(Qt::RightToLeft);
        rightToLeft->setText(QStringLiteral("RTL suggestion search"));

        auto *longText = addBox(1, 2);
        longText->setText(QStringLiteral(
            "A deliberately long query that must remain inside the field"));
        longText->setCursorPosition(0);

        auto *caseSensitive = addBox(2, 0);
        caseSensitive->setCaseSensitivity(Qt::CaseSensitive);
        caseSensitive->setText(QStringLiteral("Case-sensitive query"));

        auto *hovered = addBox(2, 1);
        hovered->setText(QStringLiteral("Hovered search"));
        hoverBox = hovered;

        popupBox = addBox(2, 2);
        popupBox->setPlaceholderText(QStringLiteral("Search suggestions"));
        popupBox->setMaximumVisibleItems(4);
        popupBox->setSuggestions({
            {QStringLiteral("open"), QStringLiteral("Open workspace"),
             QIcon(QStringLiteral(
                 ":/zzfluent/screenshots/ZzFluentTestSquare.svg")),
             1, true},
            {QStringLiteral("settings"), QStringLiteral("Open settings"),
             {}, 2, true},
            {QStringLiteral("disabled"),
             QStringLiteral("Unavailable command"), {}, 3, false},
            {QStringLiteral("clean"), QStringLiteral("Clean build output"),
             {}, 4, true},
            {QStringLiteral("long"),
             QStringLiteral(
                 "A long popup suggestion that must elide inside the view"),
             {}, 5, true}});
    }

    /** @brief 展示画面并固定 hover、focus、selected 与 popup hover 状态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (hoverBox != nullptr) {
            hoverBox->setAttribute(Qt::WA_UnderMouse, true);
            const QPoint center = hoverBox->rect().center();
            const QPoint globalCenter = hoverBox->mapToGlobal(center);
            QEnterEvent enter{
                QPointF(center), QPointF(center), QPointF(globalCenter)};
            QCoreApplication::sendEvent(hoverBox, &enter);
            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(center),
                QPointF(globalCenter),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(hoverBox, &move);
        }
        if (popupBox != nullptr) {
            popupBox->setFocus(Qt::TabFocusReason);
            popupBox->showSuggestions();
            QCoreApplication::processEvents();
            QAbstractItemView *popup = popupBox->completer()->popup();
            if (popup != nullptr && popup->model() != nullptr
                && popup->viewport() != nullptr) {
                popupBox->completer()->setCurrentRow(0);
                popup->setCurrentIndex(popupBox->completer()->currentIndex());
                const QModelIndex hovered = popup->model()->index(1, 0);
                popup->scrollTo(hovered);
                const QRect hoveredRect = popup->visualRect(hovered);
                popup->viewport()->setAttribute(Qt::WA_UnderMouse, true);
                const QPoint center = hoveredRect.center();
                const QPoint globalCenter =
                    popup->viewport()->mapToGlobal(center);
                QMouseEvent move(
                    QEvent::MouseMove,
                    QPointF(center),
                    QPointF(globalCenter),
                    Qt::NoButton,
                    Qt::NoButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(popup->viewport(), &move);
            }
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回当前打开的搜索建议框。 */
    [[nodiscard]] ZzFluentUI::ZzSuggestBox *openBox() const noexcept
    {
        return popupBox;
    }

    /** @brief 返回 completer 公开 popup 顶层窗口。 */
    [[nodiscard]] QWidget *popupWindow() const noexcept
    {
        if (popupBox == nullptr || popupBox->completer() == nullptr
            || popupBox->completer()->popup() == nullptr) {
            return nullptr;
        }
        return popupBox->completer()->popup()->window();
    }

    /** @brief 关闭 popup 并隐藏截图窗口。 */
    void hide()
    {
        if (popupBox != nullptr) {
            popupBox->hideSuggestions();
        }
        window.hide();
    }

    QWidget window;

private:
    QPointer<ZzFluentUI::ZzSuggestBox> hoverBox;
    QPointer<ZzFluentUI::ZzSuggestBox> popupBox;
};

/** @brief 遮罩搜索输入与 popup item 文字，保留其余视觉像素比较。 */
ZzSuggestBoxTextMask zzBuildSuggestBoxTextMask(
    ZzSuggestBoxScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzSuggestBoxTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    const auto boxes =
        surface->window.findChildren<ZzFluentUI::ZzSuggestBox *>();
    for (ZzFluentUI::ZzSuggestBox *box : boxes) {
        if (!box->isVisible()) {
            continue;
        }
        ++result.suggestBoxes;
        const QString text = box->displayText().isEmpty()
            ? box->placeholderText()
            : box->displayText();
        if (text.isEmpty()) {
            continue;
        }
        QStyleOptionFrame option;
        option.initFrom(box);
        const QRect contents = box->style()->subElementRect(
            QStyle::SE_LineEditContents,
            &option,
            box);
        const QRect textRect = zzAlignedTextRect(
            box,
            contents.adjusted(2, 0, -2, 0),
            static_cast<int>(box->alignment() | Qt::AlignVCenter),
            text);
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(box, textRect, &surface->window));
        ++result.inputTexts;
    }

    ZzFluentUI::ZzSuggestBox *openBox = surface->openBox();
    QWidget *popupWindow = surface->popupWindow();
    if (openBox != nullptr && popupWindow != nullptr) {
        QAbstractItemView *popup = openBox->completer()->popup();
        QAbstractItemModel *model = popup->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            const QString text = index.data(Qt::DisplayRole).toString();
            const QRect visual = popup->visualRect(index);
            if (text.isEmpty() || visual.isEmpty()
                || !visual.intersects(popup->viewport()->rect())) {
                continue;
            }
            QRect contents = visual.adjusted(12, 0, -8, 0);
            if (!index.data(Qt::DecorationRole).value<QIcon>().isNull()) {
                contents.adjust(popup->iconSize().width() + 4, 0, 0, 0);
            }
            const QRect localText = zzAlignedTextRect(
                popup,
                contents,
                Qt::AlignLeft | Qt::AlignVCenter,
                text);
            const QPoint popupOffset = popup->viewport()->mapTo(
                popupWindow,
                localText.topLeft());
            zzPaintMaskRect(
                &painter,
                QRect(zzSuggestBoxPopupOrigin + popupOffset,
                      localText.size()));
            ++result.popupItems;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存多选组合框截图的闭合文字、popup 文字与覆盖数量。 */
struct ZzMultiSelectTextMask final
{
    QImage image;
    int comboBoxes = 0;
    int closedTexts = 0;
    int popupItems = 0;
};

/** @brief 构造多选摘要、方向、禁用和真实复选 popup 的截图面。 */
class ZzMultiSelectScreenshotSurface final
{
public:
    /** @brief 创建九个闭合状态和一个打开 popup 的固定矩阵。 */
    ZzMultiSelectScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzMultiSelectScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Multi Select"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(70, 62, 70, 62);
        grid->setHorizontalSpacing(58);
        grid->setVerticalSpacing(76);

        const auto addBox = [this, grid](int row, int column) {
            auto *box =
                new ZzFluentUI::ZzMultiSelectComboBox(&window);
            box->setFixedSize(300, 48);
            grid->addWidget(box, row, column);
            return box;
        };

        auto *placeholder = addBox(0, 0);
        placeholder->setPlaceholderText(QStringLiteral("Select scopes"));
        placeholder->setOptions({
            {QStringLiteral("local"), QStringLiteral("Local"), {}, {},
             true, false}});

        auto *single = addBox(0, 1);
        single->setOptions({
            {QStringLiteral("single"), QStringLiteral("Single selection"),
             {}, {}, true, true}});

        auto *multiple = addBox(0, 2);
        multiple->setOptions({
            {QStringLiteral("alpha"), QStringLiteral("Alpha"), {}, {},
             true, true},
            {QStringLiteral("beta"), QStringLiteral("Beta"), {}, {},
             true, true}});

        auto *duplicates = addBox(1, 0);
        duplicates->setOptions({
            {QStringLiteral("first"), QStringLiteral("Same"), {}, {},
             true, true},
            {QStringLiteral("second"), QStringLiteral("Same"), {}, {},
             true, true}});

        auto *comma = addBox(1, 1);
        comma->setOptions({
            {QStringLiteral("comma"), QStringLiteral("Logs, metrics"), {},
             {}, true, true}});

        auto *longSummary = addBox(1, 2);
        longSummary->setOptions({
            {QStringLiteral("long-a"),
             QStringLiteral("A deliberately long selected scope"), {}, {},
             true, true},
            {QStringLiteral("long-b"), QStringLiteral("Another scope"), {},
             {}, true, true}});

        auto *disabled = addBox(2, 0);
        disabled->setOptions({
            {QStringLiteral("disabled"), QStringLiteral("Disabled value"),
             {}, {}, true, true}});
        disabled->setEnabled(false);

        auto *rightToLeft = addBox(2, 1);
        rightToLeft->setLayoutDirection(Qt::RightToLeft);
        rightToLeft->setOptions({
            {QStringLiteral("rtl-a"), QStringLiteral("RTL primary"), {}, {},
             true, true},
            {QStringLiteral("rtl-b"), QStringLiteral("RTL secondary"), {},
             {}, true, true}});

        popupBox = addBox(2, 2);
        popupBox->setMaxVisibleItems(5);
        popupBox->setOptions({
            {QStringLiteral("icon"), QStringLiteral("Icon selected"),
             QIcon(QStringLiteral(
                 ":/zzfluent/screenshots/ZzFluentTestSquare.svg")),
             1, true, true},
            {QStringLiteral("unchecked"), QStringLiteral("Keyboard current"),
             {}, 2, true, false},
            {QStringLiteral("disabled-row"), QStringLiteral("Disabled row"),
             {}, 3, false, true},
            {QStringLiteral("hovered"), QStringLiteral("Hovered checked"),
             {}, 4, true, true},
            {QStringLiteral("long"),
             QStringLiteral("A long popup option that must elide safely"),
             {}, 5, true, false}});
    }

    /** @brief 展示窗口并固定 popup 的键盘 current 与 hover 状态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (popupBox != nullptr) {
            popupBox->setFocus(Qt::TabFocusReason);
            popupBox->showPopup();
            QCoreApplication::processEvents();
            QAbstractItemView *popup = popupBox->view();
            if (popup != nullptr && popup->model() != nullptr
                && popup->viewport() != nullptr) {
                popup->setCurrentIndex(popup->model()->index(1, 0));
                const QModelIndex hovered = popup->model()->index(3, 0);
                popup->scrollTo(hovered);
                const QRect hoveredRect = popup->visualRect(hovered);
                popup->viewport()->setAttribute(Qt::WA_UnderMouse, true);
                const QPoint center = hoveredRect.center();
                const QPoint globalCenter =
                    popup->viewport()->mapToGlobal(center);
                QMouseEvent move(
                    QEvent::MouseMove,
                    QPointF(center),
                    QPointF(globalCenter),
                    Qt::NoButton,
                    Qt::NoButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(popup->viewport(), &move);
                popup->setCurrentIndex(popup->model()->index(1, 0));
            }
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回打开 popup 的多选组合框。 */
    [[nodiscard]] ZzFluentUI::ZzMultiSelectComboBox *openBox() const noexcept
    {
        return popupBox;
    }

    /** @brief 返回标准 popup 的顶层窗口。 */
    [[nodiscard]] QWidget *popupWindow() const noexcept
    {
        if (popupBox == nullptr || popupBox->view() == nullptr) {
            return nullptr;
        }
        return popupBox->view()->window();
    }

    /** @brief 关闭 popup 并隐藏截图窗口。 */
    void hide()
    {
        if (popupBox != nullptr) {
            popupBox->hidePopup();
        }
        window.hide();
    }

    QWidget window;

private:
    QPointer<ZzFluentUI::ZzMultiSelectComboBox> popupBox;
};

/** @brief 遮罩多选摘要和 popup item 文字，保留复选视觉像素。 */
ZzMultiSelectTextMask zzBuildMultiSelectTextMask(
    ZzMultiSelectScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzMultiSelectTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    const auto boxes = surface->window.findChildren<
        ZzFluentUI::ZzMultiSelectComboBox *>();
    for (ZzFluentUI::ZzMultiSelectComboBox *box : boxes) {
        if (!box->isVisible()) {
            continue;
        }
        ++result.comboBoxes;
        QLineEdit *editor = box->lineEdit();
        if (editor == nullptr) {
            continue;
        }
        const QString text = editor->displayText().isEmpty()
            ? editor->placeholderText()
            : editor->displayText();
        if (text.isEmpty()) {
            continue;
        }
        QStyleOptionFrame option;
        option.initFrom(editor);
        const QRect contents = editor->style()->subElementRect(
            QStyle::SE_LineEditContents,
            &option,
            editor);
        const QRect textRect = zzAlignedTextRect(
            editor,
            contents.adjusted(2, 0, -2, 0),
            static_cast<int>(editor->alignment() | Qt::AlignVCenter),
            text);
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(editor, textRect, &surface->window));
        ++result.closedTexts;
    }

    ZzFluentUI::ZzMultiSelectComboBox *openBox = surface->openBox();
    QWidget *popupWindow = surface->popupWindow();
    if (openBox != nullptr && popupWindow != nullptr) {
        QAbstractItemView *popup = openBox->view();
        QAbstractItemModel *model = popup->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            const QString text = index.data(Qt::DisplayRole).toString();
            const QRect visual = popup->visualRect(index);
            if (text.isEmpty() || visual.isEmpty()
                || !visual.intersects(popup->viewport()->rect())) {
                continue;
            }
            QStyleOptionViewItem option;
            option.initFrom(popup);
            option.rect = visual;
            option.text = text;
            option.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
            option.features = QStyleOptionViewItem::HasDisplay
                | QStyleOptionViewItem::HasCheckIndicator;
            option.checkState = static_cast<Qt::CheckState>(
                index.data(Qt::CheckStateRole).toInt());
            const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (!icon.isNull()) {
                option.features |= QStyleOptionViewItem::HasDecoration;
                option.icon = icon;
                option.decorationSize = popup->iconSize();
                option.decorationPosition = QStyleOptionViewItem::Left;
            }
            const QRect localText = popup->style()->subElementRect(
                QStyle::SE_ItemViewItemText,
                &option,
                popup);
            const QPoint popupOffset = popup->viewport()->mapTo(
                popupWindow,
                localText.topLeft());
            zzPaintMaskRect(
                &painter,
                QRect(zzMultiSelectPopupOrigin + popupOffset,
                      localText.size()));
            ++result.popupItems;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存滚轮截图的行文字、摘要与标准按钮覆盖数量。 */
struct ZzRollerTextMask final
{
    QImage image;
    int rollers = 0;
    int rollerTexts = 0;
    int pickerSummaries = 0;
    int popupButtons = 0;
};

/** @brief 构造滚轮边界、方向、禁用和三列 popup 的固定截图面。 */
class ZzRollerScreenshotSurface final
{
public:
    /** @brief 创建五个独立滚轮、两个选择器和一个打开 popup。 */
    ZzRollerScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzRollerScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Roller Controls"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);

        const auto addRoller = [this](
                                   const QRect &geometry,
                                   int visibleItems) {
            auto *roller = new ZzFluentUI::ZzRoller(&window);
            roller->setVisibleItemCount(visibleItems);
            roller->setGeometry(geometry);
            return roller;
        };

        auto *empty = addRoller(QRect(60, 60, 230, 108), 3);
        empty->setAccessibleName(QStringLiteral("Empty roller"));

        auto *first = addRoller(QRect(330, 60, 230, 180), 5);
        first->setItems({
            QStringLiteral("First"),
            QStringLiteral("Second"),
            QStringLiteral("Third")});
        first->setWrapping(false);
        first->setCurrentIndex(0);

        auto *looping = addRoller(QRect(60, 260, 230, 180), 5);
        looping->setItems({
            QStringLiteral("Zero"), QStringLiteral("One"),
            QStringLiteral("Two"), QStringLiteral("Three"),
            QStringLiteral("Four"), QStringLiteral("Five"),
            QStringLiteral("Six"), QStringLiteral("Seven")});
        looping->setCurrentIndex(4);
        looping->setWrapping(true);
        looping->setFocus(Qt::TabFocusReason);

        auto *disabled = addRoller(QRect(330, 280, 230, 108), 3);
        disabled->setItems({
            QStringLiteral("Disabled previous"),
            QStringLiteral("Disabled current with long text"),
            QStringLiteral("Disabled next")});
        disabled->setCurrentIndex(1);
        disabled->setEnabled(false);

        auto *rightToLeft = addRoller(QRect(60, 460, 500, 324), 9);
        rightToLeft->setLayoutDirection(Qt::RightToLeft);
        rightToLeft->setItems({
            QStringLiteral("RTL 00"), QStringLiteral("RTL 01"),
            QStringLiteral("RTL 02"), QStringLiteral("RTL 03"),
            QStringLiteral("RTL 04"), QStringLiteral("RTL 05"),
            QStringLiteral("RTL selected long value"),
            QStringLiteral("RTL 07"), QStringLiteral("RTL 08"),
            QStringLiteral("RTL 09"), QStringLiteral("RTL 10"),
            QStringLiteral("RTL 11")});
        rightToLeft->setCurrentIndex(6);

        popupPicker = new ZzFluentUI::ZzRollerPicker(&window);
        popupPicker->setAccessibleName(QStringLiteral("Appointment time"));
        popupPicker->setGeometry(700, 70, 420, 48);
        QStringList hours;
        for (int hour = 1; hour <= 12; ++hour) {
            hours.append(QString::number(hour));
        }
        popupPicker->setColumns({
            {QStringLiteral("hour"), hours, 8, true, 112},
            {QStringLiteral("minute"),
             {QStringLiteral("00"), QStringLiteral("15"),
              QStringLiteral("30"), QStringLiteral("45")},
             2, true, 112},
            {QStringLiteral("period"),
             {QStringLiteral("AM"), QStringLiteral("PM")},
             0, false, 88}});

        auto *disabledPicker =
            new ZzFluentUI::ZzRollerPicker(&window);
        disabledPicker->setGeometry(700, 145, 420, 48);
        disabledPicker->setColumns({
            {QStringLiteral("disabled"),
             {QStringLiteral("Disabled picker")},
             0, false, 180}});
        disabledPicker->setEnabled(false);
    }

    /** @brief 展示窗口，打开 popup 并固定焦点和 hover 行。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (popupPicker == nullptr) {
            return;
        }
        popupPicker->showPopup();
        QCoreApplication::processEvents();
        QWidget *popup = popupWindow();
        if (popup == nullptr) {
            return;
        }
        QList<ZzFluentUI::ZzRoller *> popupRollers;
        const auto rollers = popupPicker->findChildren<
            ZzFluentUI::ZzRoller *>();
        for (ZzFluentUI::ZzRoller *roller : rollers) {
            if (roller->window() == popup) {
                popupRollers.append(roller);
            }
        }
        if (!popupRollers.isEmpty()) {
            popupRollers.constFirst()->setFocus(Qt::TabFocusReason);
        }
        if (popupRollers.size() >= 2) {
            ZzFluentUI::ZzRoller *hovered = popupRollers.at(1);
            hovered->setAttribute(Qt::WA_UnderMouse, true);
            const QPoint local(
                hovered->width() / 2,
                hovered->height() / 2 + hovered->itemHeight());
            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(local),
                QPointF(hovered->mapToGlobal(local)),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(hovered, &move);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回选择器拥有的唯一 Qt::Popup 顶层窗口。 */
    [[nodiscard]] QWidget *popupWindow() const noexcept
    {
        if (popupPicker == nullptr) {
            return nullptr;
        }
        const auto widgets = popupPicker->findChildren<QWidget *>();
        for (QWidget *widget : widgets) {
            if (widget->isWindow()
                && widget->windowFlags().testFlag(Qt::Popup)) {
                return widget;
            }
        }
        return nullptr;
    }

    /** @brief 返回打开 popup 的源选择器。 */
    [[nodiscard]] ZzFluentUI::ZzRollerPicker *openPicker() const noexcept
    {
        return popupPicker;
    }

    /** @brief 回滚并关闭 popup 后隐藏截图窗口。 */
    void hide()
    {
        if (popupPicker != nullptr) {
            popupPicker->cancelPopup();
        }
        window.hide();
    }

    QWidget window;

private:
    QPointer<ZzFluentUI::ZzRollerPicker> popupPicker;
};

/** @brief 遮罩滚轮可见行、Picker 摘要和标准按钮文字。 */
ZzRollerTextMask zzBuildRollerTextMask(
    ZzRollerScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzRollerTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0, 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    QWidget *popupWindow = surface->popupWindow();

    const auto rollers = surface->window.findChildren<
        ZzFluentUI::ZzRoller *>();
    for (ZzFluentUI::ZzRoller *roller : rollers) {
        if (!roller->isVisible()) {
            continue;
        }
        ++result.rollers;
        const int half = roller->visibleItemCount() / 2;
        for (int offset = -half; offset <= half; ++offset) {
            int index = roller->currentIndex() + offset;
            if (roller->wrapping() && roller->itemCount() > 0) {
                const int count = roller->itemCount();
                index = ((index % count) + count) % count;
            }
            const QString text = roller->itemText(index);
            if (text.isEmpty()) {
                continue;
            }
            const int visualRow = offset + half;
            const QRect rowRect(
                8,
                visualRow * roller->itemHeight(),
                std::max(0, roller->width() - 16),
                roller->itemHeight());
            const QRect textRect = zzAlignedTextRect(
                roller,
                rowRect,
                Qt::AlignCenter | Qt::TextSingleLine,
                text);
            if (roller->window() == &surface->window) {
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(
                        roller,
                        textRect,
                        &surface->window));
            } else if (popupWindow != nullptr
                       && roller->window() == popupWindow) {
                const QPoint popupOffset = roller->mapTo(
                    popupWindow,
                    textRect.topLeft());
                zzPaintMaskRect(
                    &painter,
                    QRect(zzRollerPopupOrigin + popupOffset,
                          textRect.size()));
            }
            ++result.rollerTexts;
        }
    }

    const auto pickers = surface->window.findChildren<
        ZzFluentUI::ZzRollerPicker *>();
    for (ZzFluentUI::ZzRollerPicker *picker : pickers) {
        if (!picker->isVisible() || picker->text().isEmpty()) {
            continue;
        }
        QStyleOptionButton option;
        option.initFrom(picker);
        option.text = picker->text();
        const QRect contents = picker->style()->subElementRect(
            QStyle::SE_PushButtonContents,
            &option,
            picker);
        const QRect textRect = zzAlignedTextRect(
            picker,
            contents,
            Qt::AlignCenter | Qt::TextSingleLine,
            picker->text());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(picker, textRect, &surface->window));
        ++result.pickerSummaries;
    }

    if (popupWindow != nullptr) {
        const auto buttons = popupWindow->findChildren<QPushButton *>();
        for (QPushButton *button : buttons) {
            if (!button->isVisible() || button->text().isEmpty()) {
                continue;
            }
            const QRect textRect = zzAlignedTextRect(
                button,
                button->rect().adjusted(22, 2, -4, -2),
                Qt::AlignCenter | Qt::TextSingleLine,
                button->text());
            const QPoint popupOffset = button->mapTo(
                popupWindow,
                textRect.topLeft());
            zzPaintMaskRect(
                &painter,
                QRect(zzRollerPopupOrigin + popupOffset,
                      textRect.size()));
            ++result.popupButtons;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存流式布局截图中文字遮罩和覆盖数量。 */
struct ZzFlowLayoutTextMask final
{
    QImage image;
    int labels = 0;
    int buttons = 0;
};

/** @brief 构造窄、宽和 RTL 三组固定流式布局截图面。 */
class ZzFlowLayoutScreenshotSurface final
{
public:
    /** @brief 创建三组具有相同逻辑顺序的工作区操作按钮。 */
    ZzFlowLayoutScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzFlowLayoutScreenshot"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Flow Layout"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);

        addGroup(
            0,
            QStringLiteral("Compact commands"),
            QRect(60, 110, 300, 180),
            Qt::LeftToRight);
        addGroup(
            1,
            QStringLiteral("Workspace actions"),
            QRect(420, 110, 720, 90),
            Qt::LeftToRight);
        addGroup(
            2,
            QStringLiteral("Priority actions"),
            QRect(60, 410, 360, 140),
            Qt::RightToLeft);
    }

    /** @brief 展示窗口并同步激活全部流式布局。 */
    void polish()
    {
        window.show();
        for (ZzFluentUI::ZzFlowLayout *layout : layouts_) {
            Q_ASSERT(layout != nullptr);
            if (layout != nullptr) {
                static_cast<void>(layout->activate());
            }
        }
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏截图窗口。 */
    void hide()
    {
        window.hide();
    }

    /** @brief 返回指定展示组的宿主。 */
    [[nodiscard]] QWidget *host(std::size_t index) const
    {
        return hosts_.at(index);
    }

    /** @brief 返回指定展示组的按钮集合。 */
    [[nodiscard]] const std::vector<ZzFluentUI::ZzPushButton *> &buttons(
        std::size_t index) const
    {
        return buttons_.at(index);
    }

    QWidget window;

private:
    /** @brief 创建一组固定尺寸、状态和逻辑顺序的按钮。 */
    void addGroup(
        std::size_t index,
        const QString &title,
        const QRect &geometry,
        Qt::LayoutDirection direction)
    {
        auto *label = new QLabel(title, &window);
        label->setGeometry(
            geometry.x(),
            geometry.y() - 38,
            geometry.width(),
            28);
        QFont titleFont = label->font();
        titleFont.setBold(true);
        label->setFont(titleFont);
        auto *groupHost = new QWidget(&window);
        groupHost->setObjectName(
            QStringLiteral("zzFlowLayoutGroup%1").arg(index));
        groupHost->setLayoutDirection(direction);
        groupHost->setGeometry(geometry);
        auto *layout = new ZzFluentUI::ZzFlowLayout(8, 10, groupHost);
        layout->setContentsMargins(12, 12, 12, 12);
        hosts_.at(index) = groupHost;
        layouts_.at(index) = layout;

        constexpr std::array<int, 5> widths{88, 112, 96, 144, 104};
        const std::array<QString, 5> texts{
            QStringLiteral("Open"),
            QStringLiteral("Build all"),
            QStringLiteral("Tests"),
            QStringLiteral("Package artifacts"),
            QStringLiteral("Publish")};
        std::vector<ZzFluentUI::ZzPushButton *> &groupButtons =
            buttons_.at(index);
        groupButtons.reserve(widths.size());
        for (std::size_t buttonIndex = 0;
             buttonIndex < widths.size();
             ++buttonIndex) {
            auto *button = new ZzFluentUI::ZzPushButton(
                texts.at(buttonIndex),
                groupHost);
            button->setFixedSize(widths.at(buttonIndex), 36);
            if (buttonIndex == 1U) {
                button->setAppearance(
                    ZzFluentUI::ZzButtonAppearance::Accent);
            } else if (buttonIndex == 4U) {
                button->setAppearance(
                    ZzFluentUI::ZzButtonAppearance::Subtle);
            }
            if (buttonIndex == 2U) {
                button->setCheckable(true);
                button->setChecked(true);
            }
            if (buttonIndex == 3U) {
                button->setEnabled(false);
            }
            layout->addWidget(button);
            groupButtons.push_back(button);
        }
    }

    std::array<QWidget *, 3> hosts_{};
    std::array<ZzFluentUI::ZzFlowLayout *, 3> layouts_{};
    std::array<std::vector<ZzFluentUI::ZzPushButton *>, 3> buttons_;
};

/** @brief 遮罩流式布局标题和按钮文字。 */
ZzFlowLayoutTextMask zzBuildFlowLayoutTextMask(
    ZzFlowLayoutScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzFlowLayoutTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    const auto labels = surface->window.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (!label->isVisible() || label->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            label,
            label->rect(),
            static_cast<int>(label->alignment()),
            label->text());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(label, textRect, &surface->window));
        ++result.labels;
    }

    const auto buttons = surface->window.findChildren<
        ZzFluentUI::ZzPushButton *>();
    for (ZzFluentUI::ZzPushButton *button : buttons) {
        if (!button->isVisible() || button->text().isEmpty()) {
            continue;
        }
        QStyleOptionButton option;
        option.initFrom(button);
        option.text = button->text();
        const QRect contents = button->style()->subElementRect(
            QStyle::SE_PushButtonContents,
            &option,
            button);
        const QRect textRect = zzAlignedTextRect(
            button,
            contents,
            Qt::AlignCenter | Qt::TextSingleLine,
            button->text());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(button, textRect, &surface->window));
        ++result.buttons;
    }
    painter.end();
    return result;
}

/** @brief 保存数字显示截图中的字体遮罩和标签覆盖数量。 */
struct ZzDigitalDisplayTextMask final
{
    QImage image;
    int labels = 0;
};

/** @brief 构造标准数字显示控件的固定状态截图面。 */
class ZzDigitalDisplayScreenshotSurface final
{
public:
    /** @brief 创建常规、进制、禁用和透明六种显示状态。 */
    ZzDigitalDisplayScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzDigitalDisplayScreenshot"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Digital Display"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);

        addDisplay(
            0,
            QStringLiteral("Decimal"),
            QRect(70, 140, 300, 120),
            4821,
            QLCDNumber::Dec,
            QLCDNumber::Outline,
            true,
            true);
        addDisplay(
            1,
            QStringLiteral("Negative"),
            QRect(450, 140, 300, 120),
            -125,
            QLCDNumber::Dec,
            QLCDNumber::Filled,
            true,
            true);
        addDisplay(
            2,
            QStringLiteral("Decimal point"),
            QRect(830, 140, 300, 120),
            -12,
            QLCDNumber::Dec,
            QLCDNumber::Flat,
            true,
            true);
        displays_.at(2)->setSmallDecimalPoint(true);
        displays_.at(2)->display(-12.5);
        addDisplay(
            3,
            QStringLiteral("Hexadecimal"),
            QRect(70, 470, 300, 120),
            48879,
            QLCDNumber::Hex,
            QLCDNumber::Outline,
            true,
            true);
        addDisplay(
            4,
            QStringLiteral("Disabled"),
            QRect(450, 470, 300, 120),
            731,
            QLCDNumber::Dec,
            QLCDNumber::Flat,
            false,
            true);
        addDisplay(
            5,
            QStringLiteral("No frame"),
            QRect(830, 470, 300, 120),
            2026,
            QLCDNumber::Dec,
            QLCDNumber::Flat,
            true,
            false);
    }

    /** @brief 展示并同步完成样式和布局事件。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏截图窗口。 */
    void hide()
    {
        window.hide();
    }

    /** @brief 返回指定逻辑位置的数字显示控件。 */
    [[nodiscard]] QLCDNumber *display(std::size_t index) const
    {
        return displays_.at(index);
    }

    QWidget window;

private:
    /** @brief 创建一个固定模式、段样式和 frame 状态的展示组。 */
    void addDisplay(
        std::size_t index,
        const QString &title,
        const QRect &geometry,
        int value,
        QLCDNumber::Mode mode,
        QLCDNumber::SegmentStyle segmentStyle,
        bool enabled,
        bool framed)
    {
        auto *label = new QLabel(title, &window);
        label->setGeometry(
            geometry.x(),
            geometry.y() - 40,
            geometry.width(),
            28);
        QFont labelFont = label->font();
        labelFont.setBold(true);
        label->setFont(labelFont);

        auto *digitalDisplay = new QLCDNumber(8, &window);
        digitalDisplay->setAccessibleName(title);
        digitalDisplay->setGeometry(geometry);
        digitalDisplay->setMode(mode);
        digitalDisplay->setSegmentStyle(segmentStyle);
        digitalDisplay->display(value);
        digitalDisplay->setEnabled(enabled);
        digitalDisplay->setFrameStyle(
            framed ? QFrame::Box | QFrame::Plain : QFrame::NoFrame);
        displays_.at(index) = digitalDisplay;
    }

    std::array<QLCDNumber *, 6> displays_{};
};

/** @brief 遮罩数字显示组标题，数码段保持严格像素比较。 */
ZzDigitalDisplayTextMask zzBuildDigitalDisplayTextMask(
    ZzDigitalDisplayScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzDigitalDisplayTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto labels = surface->window.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (!label->isVisible() || label->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            label,
            label->rect(),
            static_cast<int>(label->alignment()),
            label->text());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(label, textRect, &surface->window));
        ++result.labels;
    }
    painter.end();
    return result;
}

/** @brief 绘制一个由标准 tooltip primitive 和 QLabel 组成的确定性提示。 */
class ZzToolTipScreenshotFixture final : public QWidget
{
public:
    /** @brief 创建固定文本、尺寸与换行策略的提示夹具。 */
    ZzToolTipScreenshotFixture(
        const QString &text,
        bool richText,
        QWidget *parent)
        : QWidget(parent)
        , label(new QLabel(text, this))
    {
        setFixedSize(330, richText ? 92 : 58);
        label->setTextFormat(richText ? Qt::RichText : Qt::PlainText);
        label->setWordWrap(richText);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setGeometry(rect().adjusted(12, 8, -12, -8));
        QPalette labelPalette = palette();
        labelPalette.setColor(
            QPalette::WindowText,
            labelPalette.color(QPalette::ToolTipText));
        label->setPalette(labelPalette);
    }

protected:
    /** @brief 仅绘制标准 tooltip 面板，文字继续由 QLabel 绘制。 */
    void paintEvent(QPaintEvent *event) override
    {
        if (event == nullptr) {
            return;
        }
        QStyleOption option;
        option.initFrom(this);
        option.rect = rect();
        QPainter painter(this);
        style()->drawPrimitive(
            QStyle::PE_PanelTipLabel,
            &option,
            &painter,
            this);
    }

private:
    QLabel *const label;
};

/** @brief 绘制实际菜单无法保持的按下态，其他内容仍委托标准 style。 */
class ZzPressedMenuItemScreenshotFixture final : public QWidget
{
public:
    /** @brief 创建含 pressed 与 disabled 两个固定状态的预览面。 */
    explicit ZzPressedMenuItemScreenshotFixture(QWidget *parent)
        : QWidget(parent)
    {
        setFixedSize(330, 116);
    }

    /** @brief 返回两个菜单项文字的局部遮罩矩形。 */
    [[nodiscard]] std::array<QRect, 2> textRects() const noexcept
    {
        return {
            QRect(48, 12, 230, 40),
            QRect(48, 64, 230, 40)};
    }

protected:
    /** @brief 用公开 style option 绘制菜单 panel 和两个状态项。 */
    void paintEvent(QPaintEvent *event) override
    {
        if (event == nullptr) {
            return;
        }
        QPainter painter(this);
        QStyleOption panel;
        panel.initFrom(this);
        panel.rect = rect();
        style()->drawPrimitive(
            QStyle::PE_PanelMenu,
            &panel,
            &painter,
            this);

        QStyleOptionMenuItem pressed;
        pressed.initFrom(this);
        pressed.rect = QRect(8, 8, 314, 44);
        pressed.state = QStyle::State_Enabled | QStyle::State_Sunken;
        pressed.palette = palette();
        pressed.menuItemType = QStyleOptionMenuItem::Normal;
        pressed.text = QStringLiteral("Pressed command\tCtrl+P");
        pressed.reservedShortcutWidth = 76;
        style()->drawControl(
            QStyle::CE_MenuItem,
            &pressed,
            &painter,
            this);

        QStyleOptionMenuItem disabled = pressed;
        disabled.rect = QRect(8, 60, 314, 44);
        disabled.state = QStyle::State_None;
        disabled.text = QStringLiteral("Disabled command");
        disabled.reservedShortcutWidth = 0;
        style()->drawControl(
            QStyle::CE_MenuItem,
            &disabled,
            &painter,
            this);
    }
};

/** @brief 保存弹出表面截图中文字遮罩与各类覆盖数量。 */
struct ZzPopupSurfaceTextMask final
{
    QImage image;
    int menuBars = 0;
    int menuBarItems = 0;
    int menus = 0;
    int menuItems = 0;
    int shortcuts = 0;
    int toolTips = 0;
    int previewItems = 0;
};

/** @brief 构造标准菜单、菜单栏、提示与方向状态的固定截图面。 */
class ZzPopupSurfaceScreenshotSurface final
{
public:
    /** @brief 创建真实 Qt popup surface 与确定性补充状态。 */
    ZzPopupSurfaceScreenshotSurface()
        : primaryMenu(&window)
        , rtlMenu(&window)
        , stateMenu(&window)
    {
        window.setObjectName(QStringLiteral("zzPopupSurfaceScreenshot"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Popup Surfaces"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);

        auto *primaryBar = new QMenuBar(&window);
        primaryBar->setNativeMenuBar(false);
        primaryBar->setGeometry(60, 50, 1080, 40);
        QMenu *fileMenu = primaryBar->addMenu(QStringLiteral("&File"));
        fileMenu->addAction(QStringLiteral("&Open"), QKeySequence::Open);
        fileMenu->addAction(QStringLiteral("&Save"), QKeySequence::Save);
        QMenu *editMenu = primaryBar->addMenu(QStringLiteral("&Edit"));
        editMenu->addAction(QStringLiteral("&Undo"), QKeySequence::Undo);
        QAction *disabledBarAction = primaryBar->addAction(
            QStringLiteral("Disabled"));
        disabledBarAction->setEnabled(false);
        primaryMenuBar = primaryBar;
        activeMenuBarAction = editMenu->menuAction();

        auto *rightToLeftBar = new QMenuBar(&window);
        rightToLeftBar->setNativeMenuBar(false);
        rightToLeftBar->setLayoutDirection(Qt::RightToLeft);
        rightToLeftBar->setGeometry(60, 110, 1080, 40);
        QMenu *rtlFile = rightToLeftBar->addMenu(QStringLiteral("RTL File"));
        rtlFile->addAction(QStringLiteral("RTL command"));
        rightToLeftBar->addAction(QStringLiteral("RTL Help"));
        rtlMenuBar = rightToLeftBar;
        activeRtlMenuBarAction = rtlFile->menuAction();

        configurePrimaryMenu();
        configureRtlMenu();
        configureStateMenu();

        auto *plainTip = new ZzToolTipScreenshotFixture(
            QStringLiteral("Plain tooltip with standard timing semantics"),
            false,
            &window);
        plainTip->move(70, 580);
        auto *richTip = new ZzToolTipScreenshotFixture(
            QStringLiteral(
                "<b>Build complete</b><br/>Rich text remains owned by Qt."),
            true,
            &window);
        richTip->move(450, 560);
        pressedPreview = new ZzPressedMenuItemScreenshotFixture(&window);
        pressedPreview->move(830, 560);
    }

    /** @brief 展示并完成真实菜单几何、active action 与 palette 计算。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (primaryMenuBar != nullptr && activeMenuBarAction != nullptr) {
            primaryMenuBar->setActiveAction(activeMenuBarAction);
        }
        if (rtlMenuBar != nullptr && activeRtlMenuBarAction != nullptr) {
            rtlMenuBar->setActiveAction(activeRtlMenuBarAction);
        }
        for (QMenu *menu : menus()) {
            menu->setMinimumWidth(300);
            menu->setPalette(QApplication::palette());
            menu->setAttribute(Qt::WA_DontShowOnScreen);
            menu->show();
            QCoreApplication::processEvents();
            menu->adjustSize();
        }
        primaryMenu.setActiveAction(primaryActiveAction);
        rtlMenu.setActiveAction(rtlActiveAction);
        stateMenu.setActiveAction(stateActiveAction);
        QCoreApplication::processEvents();
    }

    /** @brief 返回两个用于截图的实际 menu bar。 */
    [[nodiscard]] std::array<QMenuBar *, 2> menuBars() const noexcept
    {
        return {primaryMenuBar, rtlMenuBar};
    }

    /** @brief 返回三个用于截图的实际 popup menu。 */
    [[nodiscard]] std::array<QMenu *, 3> menus() noexcept
    {
        return {&primaryMenu, &rtlMenu, &stateMenu};
    }

    /** @brief 返回 pressed 补充夹具。 */
    [[nodiscard]] ZzPressedMenuItemScreenshotFixture *preview()
        const noexcept
    {
        return pressedPreview;
    }

    /** @brief 隐藏全部顶层菜单和主窗口。 */
    void hide()
    {
        for (QMenu *menu : menus()) {
            menu->hide();
        }
        window.hide();
    }

    QWidget window;

private:
    /** @brief 填充 LTR 菜单的 section、icon、check、radio 和 shortcut。 */
    void configurePrimaryMenu()
    {
        primaryMenu.addSection(QStringLiteral("Workspace"));
        QAction *open = primaryMenu.addAction(
            QIcon(QStringLiteral(
                ":/zzfluent/screenshots/ZzFluentTestSquare.svg")),
            QStringLiteral("&Open workspace"));
        open->setShortcut(QKeySequence::Open);
        primaryMenu.setDefaultAction(open);
        QAction *automatic = primaryMenu.addAction(
            QStringLiteral("Automatic sync"));
        automatic->setCheckable(true);
        automatic->setChecked(true);
        primaryActiveAction = automatic;
        primaryMenu.addSeparator();
        auto *modeGroup = new QActionGroup(&primaryMenu);
        modeGroup->setExclusive(true);
        QAction *local = primaryMenu.addAction(QStringLiteral("Local mode"));
        QAction *remote = primaryMenu.addAction(QStringLiteral("Remote mode"));
        local->setCheckable(true);
        remote->setCheckable(true);
        local->setChecked(true);
        modeGroup->addAction(local);
        modeGroup->addAction(remote);
        QMenu *exportMenu = primaryMenu.addMenu(QStringLiteral("Export"));
        exportMenu->addAction(QStringLiteral("JSON"));
        exportMenu->addAction(QStringLiteral("CSV"));
    }

    /** @brief 填充 RTL 菜单并选中 submenu 以显示镜像 chevron。 */
    void configureRtlMenu()
    {
        rtlMenu.setLayoutDirection(Qt::RightToLeft);
        rtlMenu.addSection(QStringLiteral("RTL commands"));
        QAction *icon = rtlMenu.addAction(
            QIcon(QStringLiteral(
                ":/zzfluent/screenshots/ZzFluentTestSquare.svg")),
            QStringLiteral("Icon command"));
        icon->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
        QAction *radio = rtlMenu.addAction(QStringLiteral("Selected radio"));
        radio->setCheckable(true);
        radio->setChecked(true);
        QMenu *submenu = rtlMenu.addMenu(QStringLiteral("RTL submenu"));
        submenu->addAction(QStringLiteral("Nested command"));
        rtlActiveAction = submenu->menuAction();
        QAction *disabled = rtlMenu.addAction(QStringLiteral("Disabled RTL"));
        disabled->setEnabled(false);
    }

    /** @brief 填充普通、hover、disabled、separator 与 submenu 状态。 */
    void configureStateMenu()
    {
        stateMenu.addAction(QStringLiteral("Normal command"));
        QAction *hovered = stateMenu.addAction(QStringLiteral("Hovered command"));
        stateActiveAction = hovered;
        QAction *disabled = stateMenu.addAction(QStringLiteral("Disabled command"));
        disabled->setEnabled(false);
        stateMenu.addSeparator();
        QAction *shortcut = stateMenu.addAction(QStringLiteral("Shortcut command"));
        shortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
        QMenu *submenu = stateMenu.addMenu(QStringLiteral("Submenu"));
        submenu->addAction(QStringLiteral("Nested"));
    }

    QMenu primaryMenu;
    QMenu rtlMenu;
    QMenu stateMenu;
    QPointer<QMenuBar> primaryMenuBar;
    QPointer<QMenuBar> rtlMenuBar;
    QPointer<QAction> activeMenuBarAction;
    QPointer<QAction> activeRtlMenuBarAction;
    QPointer<QAction> primaryActiveAction;
    QPointer<QAction> rtlActiveAction;
    QPointer<QAction> stateActiveAction;
    QPointer<ZzPressedMenuItemScreenshotFixture> pressedPreview;
};

/** @brief 保存内容对话框场景的文字遮罩和覆盖数量。 */
struct ZzContentDialogTextMask final
{
    QImage image;
    int labels = 0;
    int buttons = 0;
};

/** @brief 构造宿主遮罩、三按钮和自定义内容的固定内容对话框场景。 */
class ZzContentDialogScreenshotSurface final
{
public:
    /** @brief 创建带背景信息的固定宿主和窗口模态内容对话框。 */
    ZzContentDialogScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzContentDialogScreenshot"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Content Dialog"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *layout = new QVBoxLayout(&window);
        layout->setContentsMargins(80, 72, 80, 72);
        layout->setSpacing(24);
        auto *heading = new QLabel(
            QStringLiteral("Release workspace"), &window);
        QFont headingFont = heading->font();
        headingFont.setPointSize(18);
        headingFont.setWeight(QFont::DemiBold);
        heading->setFont(headingFont);
        layout->addWidget(heading);
        layout->addWidget(new QLabel(
            QStringLiteral("Artifact: ZzPureTools 0.1.0"), &window));
        layout->addWidget(new QLabel(
            QStringLiteral("Target: Linux x86_64 | Release"), &window));
        auto *hostAction = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Publish package"), &window);
        hostAction->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        layout->addWidget(hostAction, 0, Qt::AlignLeft);
        layout->addStretch(1);

        dialog_ = new ZzFluentUI::ZzContentDialog(&window);
        dialog_->setTitle(QStringLiteral("Publish release?"));
        dialog_->setText(QStringLiteral(
            "The package will be published to the selected channel."));
        dialog_->setContentWidget(new QLabel(
            QStringLiteral("Channel: Preview")));
        dialog_->setPrimaryButtonText(QStringLiteral("Publish"));
        dialog_->setPrimaryButtonVisible(true);
        dialog_->setSecondaryButtonText(QStringLiteral("Review"));
        dialog_->setSecondaryButtonVisible(true);
        dialog_->setSecondaryButtonEnabled(false);
        dialog_->setCloseButtonText(QStringLiteral("Cancel"));
        dialog_->setDefaultButton(
            ZzFluentUI::ZzContentDialogButton::Primary);
        dialog_->setWindowModality(Qt::WindowModal);
    }

    /** @brief 显示宿主和对话框，并固定对话框在逻辑画布中心。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        dialog_->show();
        QCoreApplication::processEvents();
        dialog_->adjustSize();
        dialogOrigin_ = QPoint(
            (window.width() - dialog_->width()) / 2,
            (window.height() - dialog_->height()) / 2);
        dialog_->move(window.mapToGlobal(dialogOrigin_));
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏对话框和宿主以清理实例遮罩。 */
    void hide()
    {
        dialog_->hide();
        window.hide();
    }

    /** @brief 返回当前内容对话框。 */
    [[nodiscard]] ZzFluentUI::ZzContentDialog *dialog() const noexcept
    {
        return dialog_;
    }

    /** @brief 返回对话框在宿主画布中的合成原点。 */
    [[nodiscard]] QPoint dialogOrigin() const noexcept
    {
        return dialogOrigin_;
    }

    QWidget window;

private:
    QPointer<ZzFluentUI::ZzContentDialog> dialog_;
    QPoint dialogOrigin_;
};

/** @brief 构造宿主与顶层内容对话框中文字的物理像素遮罩。 */
ZzContentDialogTextMask zzBuildContentDialogTextMask(
    ZzContentDialogScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzContentDialogTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    ZzFluentUI::ZzContentDialog *dialog = surface->dialog();

    const auto mapRect = [surface, dialog](
                             QWidget *widget,
                             const QRect &rect) {
        if (widget->window() == dialog) {
            return QRect(
                widget->mapTo(dialog, rect.topLeft())
                    + surface->dialogOrigin(),
                rect.size());
        }
        return zzMapToSurface(widget, rect, &surface->window);
    };
    for (QLabel *label : surface->window.findChildren<QLabel *>()) {
        if (!label->isVisible() || label->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            label,
            label->contentsRect(),
            static_cast<int>(label->alignment()),
            label->text());
        zzPaintMaskRect(&painter, mapRect(label, textRect));
        ++result.labels;
    }
    for (QPushButton *button :
         surface->window.findChildren<QPushButton *>()) {
        if (!button->isVisible() || button->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            button,
            button->contentsRect(),
            Qt::AlignCenter,
            button->text());
        zzPaintMaskRect(&painter, mapRect(button, textRect));
        ++result.buttons;
    }
    painter.end();
    return result;
}

/** @brief 保存教学提示场景的文字遮罩和覆盖数量。 */
struct ZzTeachingTipTextMask final
{
    QImage image;
    int labels = 0;
    int buttons = 0;
};

/** @brief 构造固定目标、箭头、自定义内容和 Action 的教学提示场景。 */
class ZzTeachingTipScreenshotSurface final
{
public:
    /** @brief 创建固定宿主和以 Bottom 方向定位的教学提示。 */
    ZzTeachingTipScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzTeachingTipScreenshot"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Teaching Tip"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *heading = new QLabel(
            QStringLiteral("Build workspace"), &window);
        heading->setGeometry(80, 72, 420, 42);
        QFont headingFont = heading->font();
        headingFont.setPointSize(18);
        headingFont.setWeight(QFont::DemiBold);
        heading->setFont(headingFont);
        auto *description = new QLabel(
            QStringLiteral("Use the highlighted command to run validation."),
            &window);
        description->setGeometry(80, 126, 520, 32);
        target_ = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Run validation"), &window);
        target_->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        target_->setGeometry(510, 310, 180, 38);

        tip_ = new ZzFluentUI::ZzTeachingTip(&window);
        tip_->setTitle(QStringLiteral("Validate before publishing"));
        tip_->setText(QStringLiteral(
            "Run the full reference checks before creating a package."));
        tip_->setContentWidget(new QLabel(
            QStringLiteral("Preset: linux-gcc-reference")));
        tip_->setActionText(QStringLiteral("View checks"));
        tip_->setActionVisible(true);
        tip_->setLightDismissEnabled(false);
        tip_->setPreferredPlacement(
            ZzFluentUI::ZzTeachingTipPlacement::Bottom);
        tip_->setTargetWidget(target_);
    }

    /** @brief 显示宿主与提示并记录顶层窗口合成原点。 */
    void polish()
    {
        const QRect available = window.screen()->availableGeometry();
        const QPoint targetGlobalTopLeft(
            available.center().x() - target_->width() / 2,
            available.top() + 48);
        window.move(targetGlobalTopLeft - target_->pos());
        window.show();
        QCoreApplication::processEvents();
        tip_->showForTarget();
        QCoreApplication::processEvents();
        tipOrigin_ = window.mapFromGlobal(tip_->pos());
    }

    /** @brief 隐藏提示和宿主。 */
    void hide()
    {
        tip_->hide();
        window.hide();
    }

    /** @brief 返回截图教学提示。 */
    [[nodiscard]] ZzFluentUI::ZzTeachingTip *tip() const noexcept
    {
        return tip_;
    }

    /** @brief 返回截图目标按钮。 */
    [[nodiscard]] ZzFluentUI::ZzPushButton *target() const noexcept
    {
        return target_;
    }

    /** @brief 返回提示在宿主画布中的合成原点。 */
    [[nodiscard]] QPoint tipOrigin() const noexcept
    {
        return tipOrigin_;
    }

    QWidget window;

private:
    QPointer<ZzFluentUI::ZzTeachingTip> tip_;
    QPointer<ZzFluentUI::ZzPushButton> target_;
    QPoint tipOrigin_;
};

/** @brief 构造宿主与顶层教学提示中文字的物理像素遮罩。 */
ZzTeachingTipTextMask zzBuildTeachingTipTextMask(
    ZzTeachingTipScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzTeachingTipTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    ZzFluentUI::ZzTeachingTip *tip = surface->tip();

    const auto mapRect = [surface, tip](
                             QWidget *widget,
                             const QRect &rect) {
        if (widget->window() == tip) {
            return QRect(
                widget->mapTo(tip, rect.topLeft()) + surface->tipOrigin(),
                rect.size());
        }
        return zzMapToSurface(widget, rect, &surface->window);
    };
    for (QLabel *label : surface->window.findChildren<QLabel *>()) {
        if (!label->isVisible() || label->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            label,
            label->contentsRect(),
            static_cast<int>(label->alignment()),
            label->text());
        zzPaintMaskRect(&painter, mapRect(label, textRect));
        ++result.labels;
    }
    for (QPushButton *button :
         surface->window.findChildren<QPushButton *>()) {
        if (!button->isVisible() || button->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            button,
            button->contentsRect(),
            Qt::AlignCenter,
            button->text());
        zzPaintMaskRect(&painter, mapRect(button, textRect));
        ++result.buttons;
    }
    painter.end();
    return result;
}

/** @brief 为菜单栏、菜单、shortcut、tooltip 与补充状态构造文字遮罩。 */
ZzPopupSurfaceTextMask zzBuildPopupSurfaceTextMask(
    ZzPopupSurfaceScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzPopupSurfaceTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0,
        0,
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);

    for (QMenuBar *menuBar : surface->menuBars()) {
        if (menuBar == nullptr || !menuBar->isVisible()) {
            continue;
        }
        ++result.menuBars;
        for (QAction *action : menuBar->actions()) {
            if (!action->isVisible() || action->text().isEmpty()) {
                continue;
            }
            const QRect geometry = menuBar->actionGeometry(action);
            const QRect textRect = zzAlignedTextRect(
                menuBar,
                geometry.adjusted(8, 0, -8, 0),
                Qt::AlignCenter,
                action->text());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(menuBar, textRect, &surface->window));
            ++result.menuBarItems;
        }
    }

    const auto menus = surface->menus();
    for (std::size_t menuIndex = 0; menuIndex < menus.size(); ++menuIndex) {
        QMenu *menu = menus[menuIndex];
        if (menu == nullptr || !menu->isVisible()) {
            continue;
        }
        ++result.menus;
        for (QAction *action : menu->actions()) {
            if (!action->isVisible() || action->text().isEmpty()) {
                continue;
            }
            const QRect geometry = menu->actionGeometry(action);
            const bool hasShortcut = !action->shortcut().isEmpty();
            QRect mainContents = geometry.adjusted(
                42,
                0,
                hasShortcut ? -118 : -34,
                0);
            const int mainAlignment = static_cast<int>(
                menu->layoutDirection() == Qt::RightToLeft
                    ? Qt::AlignRight | Qt::AlignVCenter
                    : Qt::AlignLeft | Qt::AlignVCenter);
            const QRect mainText = zzAlignedTextRect(
                menu,
                mainContents,
                mainAlignment,
                action->text());
            zzPaintMaskRect(
                &painter,
                mainText.translated(zzPopupMenuOrigins[menuIndex]));
            ++result.menuItems;

            if (hasShortcut) {
                const QString shortcut = action->shortcut().toString(
                    QKeySequence::NativeText);
                QRect shortcutContents = geometry.adjusted(170, 0, -32, 0);
                const int shortcutAlignment = static_cast<int>(
                    menu->layoutDirection() == Qt::RightToLeft
                        ? Qt::AlignLeft | Qt::AlignVCenter
                        : Qt::AlignRight | Qt::AlignVCenter);
                const QRect shortcutText = zzAlignedTextRect(
                    menu,
                    shortcutContents,
                    shortcutAlignment,
                    shortcut);
                zzPaintMaskRect(
                    &painter,
                    shortcutText.translated(zzPopupMenuOrigins[menuIndex]));
                ++result.shortcuts;
            }
        }
    }

    const auto labels = surface->window.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (!label->isVisible() || label->text().isEmpty()) {
            continue;
        }
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(label, label->rect(), &surface->window));
        ++result.toolTips;
    }

    ZzPressedMenuItemScreenshotFixture *preview = surface->preview();
    if (preview != nullptr) {
        for (const QRect &textRect : preview->textRects()) {
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(preview, textRect, &surface->window));
            ++result.previewItems;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存数值输入截图中文字遮罩及控件覆盖数量。 */
struct ZzSpinBoxTextMask final
{
    QImage image;
    int spinBoxes = 0;
    int editors = 0;
};

/** @brief 构造只包含数值输入关键视觉状态的独立确定性截图面。 */
class ZzSpinBoxScreenshotSurface final
{
public:
    /** @brief 创建符号、方向、文字和交互状态的固定矩阵。 */
    ZzSpinBoxScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzSpinBoxScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("ZzFluentUI Spin Boxes"));
        window.setAutoFillBackground(true);
        window.setPalette(QApplication::palette());
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *grid = new QGridLayout(&window);
        grid->setContentsMargins(70, 64, 70, 64);
        grid->setHorizontalSpacing(58);
        grid->setVerticalSpacing(72);

        const auto addInteger = [this, grid](
                                    int row,
                                    int column,
                                    int value) {
            auto *spinBox = new ZzFluentUI::ZzSpinBox(&window);
            spinBox->setRange(-100, 100);
            spinBox->setValue(value);
            spinBox->setFixedSize(300, 48);
            grid->addWidget(spinBox, row, column);
            return spinBox;
        };
        const auto addFloating = [this, grid](
                                     int row,
                                     int column,
                                     qreal value) {
            auto *spinBox = new ZzFluentUI::ZzDoubleSpinBox(&window);
            spinBox->setRange(-100.0, 100.0);
            spinBox->setDecimals(2);
            spinBox->setValue(value);
            spinBox->setFixedSize(300, 48);
            grid->addWidget(spinBox, row, column);
            return spinBox;
        };

        addInteger(0, 0, 24);
        auto *floating = addFloating(0, 1, 1.25);
        floating->setSuffix(QStringLiteral(" ms"));
        auto *standard = new QSpinBox(&window);
        standard->setRange(-100, 100);
        standard->setValue(36);
        standard->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        standard->setFixedSize(300, 48);
        grid->addWidget(standard, 0, 2);

        auto *arrows = addInteger(1, 0, 42);
        arrows->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        auto *readOnly = addFloating(1, 1, 3.14);
        readOnly->setButtonSymbols(QAbstractSpinBox::NoButtons);
        readOnly->setReadOnly(true);
        auto *rtl = addInteger(1, 2, -18);
        rtl->setLayoutDirection(Qt::RightToLeft);

        auto *prefix = addInteger(2, 0, 48);
        prefix->setPrefix(QStringLiteral("0x"));
        prefix->setDisplayIntegerBase(16);
        auto *special = addInteger(2, 1, 0);
        special->setRange(0, 100);
        special->setSpecialValueText(QStringLiteral("Automatic"));
        focusBox = addFloating(2, 2, 6.5);

        hoverBox = addInteger(3, 0, 27);
        pressedBox = addInteger(3, 1, 12);
        pressedInitialValue = pressedBox->value();
        auto *disabled = addFloating(3, 2, 8.75);
        disabled->setEnabled(false);
    }

    /** @brief 展示画面并通过真实事件固定 focus、hover 和 pressed。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        if (focusBox != nullptr) {
            focusBox->setFocus(Qt::TabFocusReason);
        }
        if (hoverBox != nullptr) {
            hoverBox->setAttribute(Qt::WA_UnderMouse, true);
            const QPoint buttonCenter = upButtonCenter(hoverBox);
            const QPoint globalCenter = hoverBox->mapToGlobal(buttonCenter);
            QEnterEvent enter{
                QPointF(buttonCenter),
                QPointF(buttonCenter),
                QPointF(globalCenter)};
            QCoreApplication::sendEvent(hoverBox, &enter);
            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(buttonCenter),
                QPointF(globalCenter),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(hoverBox, &move);
        }
        if (pressedBox != nullptr) {
            pressedBox->setAttribute(Qt::WA_UnderMouse, true);
            QTest::mousePress(
                pressedBox,
                Qt::LeftButton,
                Qt::NoModifier,
                upButtonCenter(pressedBox));
        }
        QCoreApplication::processEvents();
    }

    /** @brief 返回按下示例是否通过原生命中执行了增量动作。 */
    [[nodiscard]] bool pressedStepWasTriggered() const noexcept
    {
        return pressedBox != nullptr
            && pressedBox->value() == pressedInitialValue + 1;
    }

    /** @brief 隐藏数值输入截图窗口。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;

private:
    /** @brief 使用公开 style option 计算指定控件的上按钮中心。 */
    static QPoint upButtonCenter(QAbstractSpinBox *spinBox)
    {
        QStyleOptionSpinBox option;
        option.initFrom(spinBox);
        option.rect = spinBox->rect();
        option.buttonSymbols = spinBox->buttonSymbols();
        option.subControls = QStyle::SC_All;
        option.stepEnabled = QAbstractSpinBox::StepUpEnabled
            | QAbstractSpinBox::StepDownEnabled;
        option.frame = spinBox->hasFrame();
        return spinBox->style()->subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxUp,
            spinBox).center();
    }

    QPointer<ZzFluentUI::ZzDoubleSpinBox> focusBox;
    QPointer<ZzFluentUI::ZzSpinBox> hoverBox;
    QPointer<ZzFluentUI::ZzSpinBox> pressedBox;
    int pressedInitialValue = 0;
};

/** @brief 保存 Drawer 截图文字遮罩和实际覆盖数量。 */
struct ZzDrawerTextMask final
{
    QImage image;
    int labels = 0;
    int buttons = 0;
};

/** @brief 构造左侧模态与右侧非模态 Drawer 的并列确定性截图面。 */
class ZzDrawerScreenshotSurface final
{
public:
    /** @brief 创建两个独立宿主及其调用方提供的本地内容。 */
    ZzDrawerScreenshotSurface()
    {
        window.setObjectName(QStringLiteral("zzDrawerScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("Drawer states"));
        window.setAutoFillBackground(true);
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *layout = new QHBoxLayout(&window);
        layout->setContentsMargins(56, 56, 56, 56);
        layout->setSpacing(40);
        modalDrawer_ = addHost(
            layout,
            QStringLiteral("Modal host"),
            QStringLiteral("Workspace content"),
            QStringLiteral("Release options"),
            QStringLiteral("Confirm the selected package"),
            true,
            ZzFluentUI::ZzDrawerEdge::Left);
        nonModalDrawer_ = addHost(
            layout,
            QStringLiteral("Non-modal host"),
            QStringLiteral("Live task list"),
            QStringLiteral("Background task"),
            QStringLiteral("Build artifacts are ready"),
            false,
            ZzFluentUI::ZzDrawerEdge::Right);
    }

    /** @brief 展示宿主并同步打开两个 reduced-motion 终态。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        modalDrawer_->openDrawer();
        nonModalDrawer_->openDrawer();
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏固定截图窗口。 */
    void hide()
    {
        window.hide();
    }

    /** @brief 返回左侧模态 Drawer。 */
    [[nodiscard]] ZzFluentUI::ZzDrawer *modalDrawer() const noexcept
    {
        return modalDrawer_;
    }

    /** @brief 返回右侧非模态 Drawer。 */
    [[nodiscard]] ZzFluentUI::ZzDrawer *nonModalDrawer() const noexcept
    {
        return nonModalDrawer_;
    }

    QWidget window;

private:
    /** @brief 增加一个带底层内容和 Drawer 内容的固定宿主。 */
    ZzFluentUI::ZzDrawer *addHost(
        QHBoxLayout *layout,
        const QString &hostTitle,
        const QString &hostText,
        const QString &drawerTitle,
        const QString &drawerText,
        bool modal,
        ZzFluentUI::ZzDrawerEdge edge)
    {
        auto *host = new QWidget(&window);
        host->setObjectName(hostTitle);
        host->setAutoFillBackground(true);
        auto *hostLayout = new QVBoxLayout(host);
        hostLayout->setContentsMargins(28, 28, 28, 28);
        hostLayout->setSpacing(12);
        const Qt::Alignment hostAlignment =
            edge == ZzFluentUI::ZzDrawerEdge::Left
            ? Qt::AlignRight : Qt::AlignLeft;
        auto *titleLabel = new QLabel(hostTitle, host);
        titleLabel->setAlignment(hostAlignment);
        auto *textLabel = new QLabel(hostText, host);
        textLabel->setAlignment(hostAlignment);
        auto *hostAction = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Host action"), host);
        hostAction->setFixedWidth(180);
        hostLayout->addWidget(titleLabel);
        hostLayout->addWidget(textLabel);
        hostLayout->addWidget(hostAction, 0, hostAlignment);
        hostLayout->addStretch(1);
        layout->addWidget(host, 1);

        auto *drawer = new ZzFluentUI::ZzDrawer(host);
        drawer->setModal(modal);
        drawer->setEdge(edge);
        drawer->setWidthHint(280);
        auto *content = new QWidget;
        auto *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(12);
        contentLayout->addWidget(new QLabel(drawerTitle, content));
        auto *body = new QLabel(drawerText, content);
        body->setWordWrap(true);
        contentLayout->addWidget(body);
        contentLayout->addStretch(1);
        contentLayout->addWidget(new ZzFluentUI::ZzPushButton(
            QStringLiteral("Close drawer"), content));
        drawer->setContentWidget(content);
        return drawer;
    }

    QPointer<ZzFluentUI::ZzDrawer> modalDrawer_;
    QPointer<ZzFluentUI::ZzDrawer> nonModalDrawer_;
};

/** @brief 保存输入扩展截图的文字遮罩和组件数量。 */
struct ZzInputExpansionTextMask final
{
    QImage image;
    int labels = 0;
    int passwordBoxes = 0;
    int splitButtons = 0;
    int keyBinders = 0;
    int colorEditors = 0;
};

/** @brief 构造第三批输入组件的独立确定性截图面。 */
class ZzInputExpansionScreenshotSurface final
{
public:
    /** @brief 创建第三批五个输入组件的稳定视觉状态。 */
    ZzInputExpansionScreenshotSurface()
    {
        window.setObjectName(
            QStringLiteral("zzInputExpansionScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("Input expansion states"));
        window.setAutoFillBackground(true);
        window.setFixedSize(zzLogicalSurfaceSize);
        auto *layout = new QVBoxLayout(&window);
        layout->setContentsMargins(72, 56, 72, 56);
        layout->setSpacing(16);

        auto *title = new QLabel(
            QStringLiteral("Password input states"),
            &window);
        layout->addWidget(title);
        auto *form = new QFormLayout;
        form->setContentsMargins(0, 0, 0, 0);
        form->setVerticalSpacing(14);
        form->setHorizontalSpacing(24);

        peekBox_ = addPasswordBox(
            form,
            QStringLiteral("Peek"),
            ZzFluentUI::ZzPasswordRevealMode::Peek,
            true);
        addPasswordBox(
            form,
            QStringLiteral("Hidden"),
            ZzFluentUI::ZzPasswordRevealMode::Hidden,
            true);
        addPasswordBox(
            form,
            QStringLiteral("Visible"),
            ZzFluentUI::ZzPasswordRevealMode::Visible,
            true);
        addPasswordBox(
            form,
            QStringLiteral("Disabled"),
            ZzFluentUI::ZzPasswordRevealMode::Peek,
            false);
        layout->addLayout(form);

        auto *lowerLayout = new QHBoxLayout;
        lowerLayout->setContentsMargins(0, 0, 0, 0);
        lowerLayout->setSpacing(28);
        auto *leftLayout = new QVBoxLayout;
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(16);

        auto *splitTitle = new QLabel(
            QStringLiteral("Split command states"),
            &window);
        leftLayout->addWidget(splitTitle);
        auto *splitLayout = new QHBoxLayout;
        splitLayout->setContentsMargins(0, 0, 0, 0);
        splitLayout->setSpacing(16);
        splitMenu_ = new QMenu(&window);
        splitMenu_->addAction(QStringLiteral("Secondary command"));
        hoveredSplit_ = addSplitButton(
            splitLayout,
            QStringLiteral("Standard"),
            ZzFluentUI::ZzButtonAppearance::Standard,
            true);
        addSplitButton(
            splitLayout,
            QStringLiteral("Accent"),
            ZzFluentUI::ZzButtonAppearance::Accent,
            true);
        addSplitButton(
            splitLayout,
            QStringLiteral("Subtle"),
            ZzFluentUI::ZzButtonAppearance::Subtle,
            true);
        addSplitButton(
            splitLayout,
            QStringLiteral("Disabled"),
            ZzFluentUI::ZzButtonAppearance::Standard,
            false);
        leftLayout->addLayout(splitLayout);

        auto *ratingTitle = new QLabel(
            QStringLiteral("Rating states"),
            &window);
        leftLayout->addWidget(ratingTitle);
        auto *ratingLayout = new QHBoxLayout;
        ratingLayout->setContentsMargins(0, 0, 0, 0);
        ratingLayout->setSpacing(32);
        addRatingControl(
            ratingLayout,
            QStringLiteral("Whole"),
            4.0,
            ZzFluentUI::ZzRatingPrecision::Whole,
            false,
            true);
        addRatingControl(
            ratingLayout,
            QStringLiteral("Half"),
            3.5,
            ZzFluentUI::ZzRatingPrecision::Half,
            false,
            true);
        addRatingControl(
            ratingLayout,
            QStringLiteral("Read only"),
            4.0,
            ZzFluentUI::ZzRatingPrecision::Whole,
            true,
            true);
        addRatingControl(
            ratingLayout,
            QStringLiteral("Disabled"),
            2.5,
            ZzFluentUI::ZzRatingPrecision::Half,
            false,
            false);
        ratingLayout->addStretch(1);
        leftLayout->addLayout(ratingLayout);

        auto *shortcutTitle = new QLabel(
            QStringLiteral("Shortcut recorder"),
            &window);
        leftLayout->addWidget(shortcutTitle);
        auto *shortcutForm = new QFormLayout;
        shortcutForm->setContentsMargins(0, 0, 0, 0);
        shortcutForm->setHorizontalSpacing(24);
        auto *keyBinder = new ZzFluentUI::ZzKeyBinder(
            QKeySequence(QKeyCombination(
                Qt::ControlModifier | Qt::ShiftModifier,
                Qt::Key_P)),
            &window);
        keyBinder->setAccessibleName(QStringLiteral("Primary shortcut"));
        shortcutForm->addRow(
            QStringLiteral("Primary shortcut"),
            keyBinder);
        leftLayout->addLayout(shortcutForm);
        leftLayout->addStretch(1);
        lowerLayout->addLayout(leftLayout, 1);

        auto *colorLayout = new QVBoxLayout;
        colorLayout->setContentsMargins(0, 0, 0, 0);
        colorLayout->setSpacing(10);
        colorLayout->addWidget(new QLabel(
            QStringLiteral("Color picker"),
            &window));
        auto *colorPicker = new ZzFluentUI::ZzColorPicker(&window);
        colorPicker->setAccessibleName(QStringLiteral("RGBA color"));
        colorPicker->setFixedWidth(384);
        colorPicker->setPaletteColors({
            QColor(QStringLiteral("#0078d4")),
            QColor(QStringLiteral("#107c10")),
            QColor(QStringLiteral("#ffb900")),
            QColor(QStringLiteral("#d13438")),
            QColor(QStringLiteral("#881798")),
            QColor(QStringLiteral("#00b7c3")),
            QColor(QStringLiteral("#ffffff")),
            QColor(QStringLiteral("#000000"))});
        colorPicker->setAlphaEnabled(true);
        colorPicker->setCurrentColor(
            QColor::fromRgba(qRgba(64, 128, 192, 128)));
        colorLayout->addWidget(colorPicker);
        colorLayout->addStretch(1);
        lowerLayout->addLayout(colorLayout);
        layout->addLayout(lowerLayout, 1);
        layout->addStretch(1);
    }

    /** @brief 展示窗口并把键盘焦点放到 Peek 输入框。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        peekBox_->setFocus(Qt::OtherFocusReason);
        QTest::mouseMove(
            hoveredSplit_,
            QPoint(
                hoveredSplit_->width() - 12,
                hoveredSplit_->height() / 2));
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏固定截图窗口。 */
    void hide()
    {
        window.hide();
    }

    QWidget window;

private:
    /** @brief 增加一个带固定模式和值的 PasswordBox。 */
    ZzFluentUI::ZzPasswordBox *addPasswordBox(
        QFormLayout *form,
        const QString &label,
        ZzFluentUI::ZzPasswordRevealMode mode,
        bool enabled)
    {
        auto *box = new ZzFluentUI::ZzPasswordBox(&window);
        box->setAccessibleName(label);
        box->setText(QStringLiteral("Fluent-2026"));
        box->setRevealMode(mode);
        box->setEnabled(enabled);
        form->addRow(label, box);
        return box;
    }

    /** @brief 增加固定外观且借用同一菜单的 SplitButton。 */
    ZzFluentUI::ZzSplitButton *addSplitButton(
        QHBoxLayout *layout,
        const QString &text,
        ZzFluentUI::ZzButtonAppearance appearance,
        bool enabled)
    {
        auto *button = new ZzFluentUI::ZzSplitButton(text, &window);
        button->setAccessibleName(text);
        button->setAppearance(appearance);
        button->setMenu(splitMenu_);
        button->setEnabled(enabled);
        button->setFixedWidth(120);
        layout->addWidget(button);
        return button;
    }

    /** @brief 增加一个带状态标题的固定评分控件。 */
    void addRatingControl(
        QHBoxLayout *layout,
        const QString &label,
        qreal rating,
        ZzFluentUI::ZzRatingPrecision precision,
        bool readOnly,
        bool enabled)
    {
        auto *host = new QWidget(&window);
        auto *hostLayout = new QVBoxLayout(host);
        hostLayout->setContentsMargins(0, 0, 0, 0);
        hostLayout->setSpacing(6);
        hostLayout->addWidget(new QLabel(label, host));
        auto *control = new ZzFluentUI::ZzRatingControl(host);
        control->setAccessibleName(label);
        control->setPrecision(precision);
        control->setRating(rating);
        control->setReadOnly(readOnly);
        control->setEnabled(enabled);
        hostLayout->addWidget(control);
        layout->addWidget(host);
    }

    QPointer<ZzFluentUI::ZzPasswordBox> peekBox_;
    QPointer<ZzFluentUI::ZzSplitButton> hoveredSplit_;
    QPointer<QMenu> splitMenu_;
};

/** @brief 为输入扩展画面的标签与输入文本构造字体差异遮罩。 */
ZzInputExpansionTextMask zzBuildInputExpansionTextMask(
    QWidget *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzInputExpansionTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto widgets = surface->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (!widget->isVisible()) {
            continue;
        }
        if (auto *label = qobject_cast<QLabel *>(widget);
            label != nullptr && !label->text().isEmpty()) {
            const QRect textRect = zzAlignedTextRect(
                label,
                label->contentsRect(),
                static_cast<int>(label->alignment()),
                label->text());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(label, textRect, surface));
            ++result.labels;
            continue;
        }
        if (auto *button =
                qobject_cast<ZzFluentUI::ZzSplitButton *>(widget);
            button != nullptr && !button->text().isEmpty()) {
            QRect textBounds = button->contentsRect();
            int menuExtent = 32;
            if (const auto *fluentStyle =
                    qobject_cast<const ZzFluentUI::ZzFluentStyle *>(
                        button->style())) {
                menuExtent = qCeil(fluentStyle->themeSnapshot()->metric(
                    ZzFluentUI::ZzMetricToken::SplitButtonMenuExtent));
            }
            textBounds.adjust(0, 0, -menuExtent, 0);
            const QRect textRect = zzAlignedTextRect(
                button,
                textBounds,
                Qt::AlignCenter,
                button->text());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(button, textRect, surface));
            ++result.splitButtons;
            continue;
        }
        if (auto *keyBinder =
                qobject_cast<ZzFluentUI::ZzKeyBinder *>(widget);
            keyBinder != nullptr) {
            auto *editor = keyBinder->findChild<QLineEdit *>(
                QString(),
                Qt::FindDirectChildrenOnly);
            if (editor != nullptr) {
                const QMargins margins = editor->textMargins();
                const QRect textRect = editor->contentsRect().adjusted(
                    margins.left(),
                    0,
                    -margins.right(),
                    0);
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(editor, textRect, surface));
            }
            ++result.keyBinders;
            continue;
        }
        if (auto *editor = qobject_cast<QLineEdit *>(widget);
            editor != nullptr) {
            QWidget *ancestor = editor->parentWidget();
            while (ancestor != nullptr
                   && qobject_cast<ZzFluentUI::ZzColorPicker *>(ancestor)
                       == nullptr) {
                ancestor = ancestor->parentWidget();
            }
            if (ancestor != nullptr) {
                const QMargins margins = editor->textMargins();
                const QRect textRect = editor->contentsRect().adjusted(
                    margins.left(),
                    0,
                    -margins.right(),
                    0);
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(editor, textRect, surface));
                ++result.colorEditors;
                continue;
            }
        }
        auto *box = qobject_cast<ZzFluentUI::ZzPasswordBox *>(widget);
        if (box == nullptr) {
            continue;
        }
        const QMargins margins = box->textMargins();
        const QRect textRect = box->contentsRect().adjusted(
            margins.left(),
            0,
            -margins.right(),
            0);
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(box, textRect, surface));
        ++result.passwordBoxes;
    }
    painter.end();
    return result;
}

/** @brief 为 Drawer 独立画面的可见标签和按钮构造字体差异遮罩。 */
ZzDrawerTextMask zzBuildDrawerTextMask(QWidget *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzDrawerTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8), 0, 0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto widgets = surface->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (!widget->isVisible()) {
            continue;
        }
        if (auto *label = qobject_cast<QLabel *>(widget);
            label != nullptr && !label->text().isEmpty()) {
            const QRect textRect = zzAlignedTextRect(
                label,
                label->contentsRect(),
                static_cast<int>(label->alignment()),
                label->text());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(label, textRect, surface));
            ++result.labels;
            continue;
        }
        if (auto *button = qobject_cast<QAbstractButton *>(widget);
            button != nullptr && !button->text().isEmpty()) {
            const QRect textRect = zzAlignedTextRect(
                button,
                button->contentsRect(),
                Qt::AlignCenter,
                button->text());
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(button, textRect, surface));
            ++result.buttons;
        }
    }
    painter.end();
    return result;
}

/** @brief 保存导航面板截图中文字遮罩及其覆盖数量。 */
struct ZzNavigationPaneTextMask final
{
    QImage image;
    int panes = 0;
    int sections = 0;
    int titles = 0;
    int badges = 0;
};

/** @brief 构造常规、紧凑和 RTL 导航面板的确定性截图面。 */
class ZzNavigationPaneScreenshotSurface final
{
public:
    /** @brief 创建共享本地模型且不访问路由或业务对象的三列画面。 */
    ZzNavigationPaneScreenshotSurface()
        : model_(&window)
    {
        window.setObjectName(
            QStringLiteral("zzNavigationPaneScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("Navigation pane states"));
        window.setAutoFillBackground(true);
        window.setFixedSize(zzLogicalSurfaceSize);
        buildModel();

        auto *layout = new QHBoxLayout(&window);
        layout->setContentsMargins(80, 80, 80, 80);
        layout->setSpacing(56);
        layout->addStretch(1);
        regularPane_ = addPane(
            layout,
            ZzFluentUI::ZzNavigationDisplayMode::Regular,
            Qt::LeftToRight,
            0);
        compactPane_ = addPane(
            layout,
            ZzFluentUI::ZzNavigationDisplayMode::Compact,
            Qt::LeftToRight,
            5);
        rtlPane_ = addPane(
            layout,
            ZzFluentUI::ZzNavigationDisplayMode::Regular,
            Qt::RightToLeft,
            3);
        layout->addStretch(1);
    }

    /** @brief 展示画面并通过真实事件固定 hover 与键盘焦点。 */
    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
        hoverView_ = primaryView(regularPane_);
        focusView_ = primaryView(rtlPane_);
        if (hoverView_ != nullptr && hoverView_->model() != nullptr) {
            const QModelIndex hovered = hoverView_->model()->index(6, 0);
            const QPoint center = hoverView_->visualRect(hovered).center();
            QWidget *viewport = hoverView_->viewport();
            viewport->setAttribute(Qt::WA_UnderMouse, true);
            const QPoint globalCenter = viewport->mapToGlobal(center);
            QEnterEvent enter{
                QPointF(center),
                QPointF(center),
                QPointF(globalCenter)};
            QCoreApplication::sendEvent(viewport, &enter);
            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(center),
                QPointF(globalCenter),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &move);
        }
        if (focusView_ != nullptr) {
            focusView_->setFocus(Qt::TabFocusReason);
        }
        QCoreApplication::processEvents();
    }

    /** @brief 隐藏固定截图窗口。 */
    void hide()
    {
        window.hide();
    }

    /** @brief 返回常规 LTR 导航面板。 */
    [[nodiscard]] ZzFluentUI::ZzNavigationPane *regularPane() const noexcept
    {
        return regularPane_;
    }

    /** @brief 返回紧凑 LTR 导航面板。 */
    [[nodiscard]] ZzFluentUI::ZzNavigationPane *compactPane() const noexcept
    {
        return compactPane_;
    }

    /** @brief 返回常规 RTL 导航面板。 */
    [[nodiscard]] ZzFluentUI::ZzNavigationPane *rtlPane() const noexcept
    {
        return rtlPane_;
    }

    /** @brief 返回固定 hover 行所属主视图。 */
    [[nodiscard]] ZzFluentUI::ZzNavigationView *hoverView() const noexcept
    {
        return hoverView_;
    }

    /** @brief 返回固定键盘焦点所属 RTL 主视图。 */
    [[nodiscard]] ZzFluentUI::ZzNavigationView *focusView() const noexcept
    {
        return focusView_;
    }

    QWidget window;

private:
    /** @brief 填充分区、两类图标、徽标、禁用项和页脚数据。 */
    void buildModel()
    {
        auto *home = new QStandardItem(QStringLiteral("Home"));
        home->setData(
            QStringLiteral("Workspace"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Section));
        home->setData(
            QVariant::fromValue(ZzFluentUI::ZzIconDescriptor{
                QStringLiteral(
                    ":/zzfluent/screenshots/ZzFluentTestSquare.svg"),
                true}),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Icon));
        home->setData(
            QStringLiteral("3"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge));
        model_.appendRow(home);

        model_.appendRow(new QStandardItem(
            window.style()->standardIcon(QStyle::SP_BrowserReload),
            QStringLiteral("Activity")));
        auto *reports = new QStandardItem(
            window.style()->standardIcon(QStyle::SP_FileIcon),
            QStringLiteral("Reports"));
        reports->setEnabled(false);
        model_.appendRow(reports);

        auto *projects = new QStandardItem(
            window.style()->standardIcon(QStyle::SP_DirIcon),
            QStringLiteral("Projects"));
        projects->setData(
            QStringLiteral("Library"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Section));
        model_.appendRow(projects);
        auto *archive = new QStandardItem(
            window.style()->standardIcon(QStyle::SP_DialogSaveButton),
            QStringLiteral("Archive"));
        archive->setData(
            QStringLiteral("NEW"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge));
        model_.appendRow(archive);

        auto *settings = new QStandardItem(
            window.style()->standardIcon(
                QStyle::SP_FileDialogDetailedView),
            QStringLiteral("Settings"));
        settings->setData(
            QVariant::fromValue(
                ZzFluentUI::ZzNavigationPlacement::Footer),
            static_cast<int>(
                ZzFluentUI::ZzNavigationItemRole::Placement));
        model_.appendRow(settings);
        auto *about = new QStandardItem(
            window.style()->standardIcon(
                QStyle::SP_MessageBoxInformation),
            QStringLiteral("About"));
        about->setData(
            QVariant::fromValue(
                ZzFluentUI::ZzNavigationPlacement::Footer),
            static_cast<int>(
                ZzFluentUI::ZzNavigationItemRole::Placement));
        model_.appendRow(about);
    }

    /** @brief 创建固定高度、方向和选择状态的一列导航面板。 */
    ZzFluentUI::ZzNavigationPane *addPane(
        QHBoxLayout *layout,
        ZzFluentUI::ZzNavigationDisplayMode mode,
        Qt::LayoutDirection direction,
        int selectedSourceRow)
    {
        auto *pane = new ZzFluentUI::ZzNavigationPane(&window);
        pane->setModel(&model_);
        pane->setDisplayMode(mode);
        pane->setLayoutDirection(direction);
        pane->setCurrentSourceIndex(model_.index(selectedSourceRow, 0));
        pane->setFixedHeight(640);
        layout->addWidget(pane);
        return pane;
    }

    /** @brief 返回拥有较多投影行的主导航视图。 */
    static ZzFluentUI::ZzNavigationView *primaryView(
        ZzFluentUI::ZzNavigationPane *pane)
    {
        if (pane == nullptr) {
            return nullptr;
        }
        ZzFluentUI::ZzNavigationView *result = nullptr;
        int maximumRows = -1;
        const auto views =
            pane->findChildren<ZzFluentUI::ZzNavigationView *>();
        for (auto *view : views) {
            const int rows = view->model() != nullptr
                ? view->model()->rowCount() : 0;
            if (rows > maximumRows) {
                maximumRows = rows;
                result = view;
            }
        }
        return result;
    }

    QStandardItemModel model_;
    QPointer<ZzFluentUI::ZzNavigationPane> regularPane_;
    QPointer<ZzFluentUI::ZzNavigationPane> compactPane_;
    QPointer<ZzFluentUI::ZzNavigationPane> rtlPane_;
    QPointer<ZzFluentUI::ZzNavigationView> hoverView_;
    QPointer<ZzFluentUI::ZzNavigationView> focusView_;
};

/** @brief 为导航面板精确遮罩分区、常规标题和常规徽标文字。 */
ZzNavigationPaneTextMask zzBuildNavigationPaneTextMask(
    QWidget *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzNavigationPaneTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0,
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto panes =
        surface->findChildren<ZzFluentUI::ZzNavigationPane *>();
    for (auto *pane : panes) {
        if (!pane->isVisible()) {
            continue;
        }
        ++result.panes;
        const auto views =
            pane->findChildren<ZzFluentUI::ZzNavigationView *>();
        for (auto *view : views) {
            if (!view->isVisible() || view->model() == nullptr) {
                continue;
            }
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                const QModelIndex index = view->model()->index(row, 0);
                const QRect itemRect = view->visualRect(index);
                if (!itemRect.intersects(view->viewport()->rect())) {
                    continue;
                }
                const QString text = index.data(Qt::DisplayRole).toString();
                if (index.flags() == Qt::NoItemFlags) {
                    if (pane->isCompact()) {
                        continue;
                    }
                    const QRect content = itemRect.adjusted(10, 4, -10, -4);
                    QFont sectionFont = view->font();
                    sectionFont.setWeight(QFont::DemiBold);
                    const QFontMetrics sectionMetrics(sectionFont);
                    const QString visibleText = view->fontMetrics().elidedText(
                        text,
                        Qt::ElideRight,
                        content.width());
                    const QRect textRect = sectionMetrics.boundingRect(
                        content,
                        Qt::AlignLeading | Qt::AlignVCenter,
                        visibleText);
                    zzPaintMaskRect(
                        &painter,
                        zzMapToSurface(
                            view->viewport(), textRect, surface));
                    ++result.sections;
                    continue;
                }
                if (pane->isCompact()) {
                    continue;
                }

                constexpr int iconExtent = 18;
                constexpr int spacing = 8;
                const QRect content = itemRect.adjusted(8, 3, -8, -3);
                int leading = content.left();
                int trailing = content.right();
                const QVariant descriptorValue = index.data(static_cast<int>(
                    ZzFluentUI::ZzNavigationItemRole::Icon));
                const auto descriptor = descriptorValue
                    .value<ZzFluentUI::ZzIconDescriptor>();
                const QIcon standardIcon = qvariant_cast<QIcon>(
                    index.data(Qt::DecorationRole));
                const bool hasIcon =
                    (descriptorValue.canConvert<
                         ZzFluentUI::ZzIconDescriptor>()
                     && !descriptor.resourceId.trimmed().isEmpty())
                    || !standardIcon.isNull();
                if (hasIcon) {
                    leading += iconExtent + spacing;
                }

                const QString badge = index.data(static_cast<int>(
                    ZzFluentUI::ZzNavigationItemRole::Badge)).toString();
                if (!badge.isEmpty()) {
                    const int measured =
                        view->fontMetrics().horizontalAdvance(badge);
                    const int badgeWidth =
                        std::clamp(measured + 12, 22, 72);
                    const QRect logicalBadge(
                        trailing - badgeWidth + 1,
                        content.center().y() - 10,
                        badgeWidth,
                        20);
                    const QRect badgeRect = QStyle::visualRect(
                        view->layoutDirection(),
                        itemRect,
                        logicalBadge);
                    const QRect badgeTextRect = zzAlignedTextRect(
                        view->viewport(),
                        badgeRect.adjusted(4, 0, -4, 0),
                        Qt::AlignCenter,
                        badge);
                    zzPaintMaskRect(
                        &painter,
                        zzMapToSurface(
                            view->viewport(), badgeTextRect, surface));
                    trailing -= badgeWidth + spacing;
                    ++result.badges;
                }

                const QRect logicalText(
                    leading,
                    content.top(),
                    std::max(0, trailing - leading + 1),
                    content.height());
                const QRect titleBounds = QStyle::visualRect(
                    view->layoutDirection(),
                    itemRect,
                    logicalText);
                const QString visibleText = view->fontMetrics().elidedText(
                    text,
                    Qt::ElideRight,
                    titleBounds.width());
                const QRect titleRect = zzAlignedTextRect(
                    view->viewport(),
                    titleBounds,
                    Qt::AlignLeading | Qt::AlignVCenter,
                    visibleText);
                zzPaintMaskRect(
                    &painter,
                    zzMapToSurface(
                        view->viewport(), titleRect, surface));
                ++result.titles;
            }
        }
    }
    painter.end();
    return result;
}

/** @brief 为独立数值输入画面精确遮罩内部编辑器文字。 */
ZzSpinBoxTextMask zzBuildSpinBoxTextMask(QWidget *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzSpinBoxTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto spinBoxes = surface->findChildren<QAbstractSpinBox *>();
    for (QAbstractSpinBox *spinBox : spinBoxes) {
        if (!spinBox->isVisible()) {
            continue;
        }
        ++result.spinBoxes;
        auto *editor = spinBox->findChild<QLineEdit *>();
        if (editor == nullptr || !editor->isVisible()
            || editor->displayText().isEmpty()) {
            continue;
        }
        QStyleOptionFrame option;
        option.initFrom(editor);
        const QRect contents = editor->style()->subElementRect(
            QStyle::SE_LineEditContents,
            &option,
            editor);
        const QRect textRect = zzAlignedTextRect(
            editor,
            contents.adjusted(2, 0, -2, 0),
            static_cast<int>(editor->alignment() | Qt::AlignVCenter),
            editor->displayText());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(editor, textRect, surface));
        ++result.editors;
    }
    painter.end();
    return result;
}

/** @brief 为独立环形进度画面构造只覆盖居中文字的像素遮罩。 */
ZzProgressRingTextMask zzBuildProgressRingTextMask(
    QWidget *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzProgressRingTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto rings = surface->findChildren<ZzFluentUI::ZzProgressRing *>();
    for (ZzFluentUI::ZzProgressRing *ring : rings) {
        if (!ring->isVisible()) {
            continue;
        }
        ++result.progressRings;
        if (!ring->isTextVisible() || ring->text().isEmpty()) {
            continue;
        }
        const QRect textRect = zzAlignedTextRect(
            ring,
            ring->rect(),
            Qt::AlignCenter,
            ring->text());
        zzPaintMaskRect(
            &painter,
            zzMapToSurface(ring, textRect, surface));
        ++result.textRings;
    }
    painter.end();
    return result;
}

/** @brief 为独立标签页画面构造只覆盖标签文字的像素遮罩。 */
ZzTabTextMask zzBuildTabTextMask(QWidget *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    ZzTabTextMask result{
        QImage(physicalSize, QImage::Format_Grayscale8),
        0,
        0};
    result.image.setDevicePixelRatio(dpr);
    result.image.fill(0);
    QPainter painter(&result.image);
    const auto tabBars = surface->findChildren<ZzFluentUI::ZzTabBar *>();
    for (ZzFluentUI::ZzTabBar *tabBar : tabBars) {
        if (!tabBar->isVisible()) {
            continue;
        }
        ++result.tabBars;
        for (int index = 0; index < tabBar->count(); ++index) {
            const QString text = tabBar->tabText(index);
            if (text.isEmpty()) {
                continue;
            }
            const QRect textRect = zzAlignedTextRect(
                tabBar,
                tabBar->tabRect(index),
                Qt::AlignCenter,
                text);
            zzPaintMaskRect(
                &painter,
                zzMapToSurface(tabBar, textRect, surface));
            ++result.tabTexts;
        }
    }
    painter.end();
    return result;
}

/** @brief 将独立标签页窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderTabSurface(ZzTabScreenshotSurface *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立环形进度窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderProgressRingSurface(
    ZzProgressRingScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立滚动控件窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderScrollControlsSurface(
    ZzScrollControlsScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立标准文本输入窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderTextInputSurface(
    ZzTextInputScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将组合框窗口和公开 popup 窗口合成到固定物理画布。 */
QImage zzRenderComboBoxSurface(
    ZzComboBoxScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    QWidget *popupWindow = surface->popupWindow();
    if (popupWindow != nullptr && popupWindow->isVisible()) {
        popupWindow->render(&painter, zzComboBoxPopupOrigin);
    }
    painter.end();
    return image;
}

/** @brief 将搜索建议框窗口和 completer popup 合成到固定物理画布。 */
QImage zzRenderSuggestBoxSurface(
    ZzSuggestBoxScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    QWidget *popupWindow = surface->popupWindow();
    if (popupWindow != nullptr && popupWindow->isVisible()) {
        popupWindow->render(&painter, zzSuggestBoxPopupOrigin);
    }
    painter.end();
    return image;
}

/** @brief 将多选组合框窗口和标准 popup 合成到固定物理画布。 */
QImage zzRenderMultiSelectSurface(
    ZzMultiSelectScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    QWidget *popupWindow = surface->popupWindow();
    if (popupWindow != nullptr && popupWindow->isVisible()) {
        popupWindow->render(&painter, zzMultiSelectPopupOrigin);
    }
    painter.end();
    return image;
}

/** @brief 将滚轮主窗口和标准 Picker popup 合成到固定物理画布。 */
QImage zzRenderRollerSurface(
    ZzRollerScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    QWidget *popupWindow = surface->popupWindow();
    if (popupWindow != nullptr && popupWindow->isVisible()) {
        popupWindow->render(&painter, zzRollerPopupOrigin);
    }
    painter.end();
    return image;
}

/** @brief 将流式布局窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderFlowLayoutSurface(
    ZzFlowLayoutScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将数字显示窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderDigitalDisplaySurface(
    ZzDigitalDisplayScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 把主窗口和三个标准 popup menu 合成到固定物理画布。 */
QImage zzRenderPopupSurface(
    ZzPopupSurfaceScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    const auto menus = surface->menus();
    for (std::size_t index = 0; index < menus.size(); ++index) {
        if (menus[index] != nullptr && menus[index]->isVisible()) {
            menus[index]->render(&painter, zzPopupMenuOrigins[index]);
        }
    }
    painter.end();
    return image;
}

/** @brief 把父窗口遮罩和独立内容对话框合成到固定物理画布。 */
QImage zzRenderContentDialogSurface(
    ZzContentDialogScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    surface->dialog()->render(&painter, surface->dialogOrigin());
    painter.end();
    return image;
}

/** @brief 把宿主和独立教学提示合成到固定物理画布。 */
QImage zzRenderTeachingTipSurface(
    ZzTeachingTipScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    surface->tip()->render(&painter, surface->tipOrigin());
    painter.end();
    return image;
}

/** @brief 将独立数值输入窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderSpinBoxSurface(
    ZzSpinBoxScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立 Drawer 宿主窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderDrawerSurface(
    ZzDrawerScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将第三批输入组件窗口渲染到指定 DPR 的物理画布。 */
QImage zzRenderInputExpansionSurface(
    ZzInputExpansionScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立导航面板窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderNavigationPaneSurface(
    ZzNavigationPaneScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立卡片窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderCardSurface(ZzCardScreenshotSurface *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立轮播窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderCarouselSurface(
    ZzCarouselScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 将独立命令与状态窗口渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderCommandStatusSurface(
    ZzCommandStatusScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    painter.end();
    return image;
}

/** @brief 把窗口和独立菜单渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderSurface(ZzScreenshotSurface *surface, qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    surface->menu.render(&painter, zzMenuOrigin);
    painter.end();
    return image;
}

/** @brief 将标准控件广度夹具渲染到指定 DPR 的固定物理画布。 */
QImage zzRenderStandardBreadthSurface(
    ZzStandardBreadthScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzLogicalSurfaceSize.width() * dpr),
        qRound(zzLogicalSurfaceSize.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    surface->menu.render(&painter, zzMenuOrigin);
    painter.end();
    return image;
}

} // namespace

/** @brief 验证 Fluent 控件在固定 Linux 参考环境中的视觉基线。 */
class ZzFluentScreenshotTest final : public QObject
{
    Q_OBJECT

public:
    /** @brief 保存进程级 DPR 和 baseline 子目录。 */
    ZzFluentScreenshotTest(qreal expectedDpr, QString baselineSubdirectory)
        : expectedDpr_(expectedDpr)
        , baselineSubdirectory_(std::move(baselineSubdirectory))
    {
    }

private Q_SLOTS:
    // QApplication 接管 setStyle() 传入对象；静态分析器不了解 Qt 所有权。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void initTestCase()
    {
        QLocale::setDefault(QLocale::c());
        QApplication::setLayoutDirection(Qt::LeftToRight);
        QVERIFY2(
            QFontDatabase::families().contains(QStringLiteral("DejaVu Sans")),
            "参考环境缺少 DejaVu Sans 字体");
        const QFont referenceFont(QStringLiteral("DejaVu Sans"), 10);
        QApplication::setFont(referenceFont);
        QCOMPARE(QFontInfo(QApplication::font()).family(), QStringLiteral("DejaVu Sans"));
        QCOMPARE(QApplication::font().pointSize(), 10);

        QScreen *screen = QApplication::primaryScreen();
        QVERIFY2(screen != nullptr, "截图测试需要可查询 DPR 的主屏幕");
        actualDpr_ = screen->devicePixelRatio();
        QVERIFY2(
            std::abs(actualDpr_ - expectedDpr_) <= 0.01,
            qPrintable(QStringLiteral("实际 DPR %1 与期望 %2 不符")
                           .arg(actualDpr_, 0, 'f', 2)
                           .arg(expectedDpr_, 0, 'f', 2)));

        QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"));
        QVERIFY2(fusion != nullptr, "参考环境缺少 Fusion 样式");
        controller_ = std::make_unique<ZzFluentUI::ZzThemeController>();
        controller_->setReducedMotion(true);
        QApplication::setStyle(
            new ZzFluentUI::ZzFluentStyle(controller_.get(), fusion));
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    /** @brief 提供标准控件广度截图的三种主题数据。 */
    void rendersStandardBreadthThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("standard-breadth-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("standard-breadth-light");
        QTest::newRow("standard-breadth-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("standard-breadth-dark");
        QTest::newRow("standard-breadth-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("standard-breadth-high-contrast");
    }

    /**
     * @brief 比较标准控件广度固定场景，覆盖主题、DPR、禁用和选中表面。
     */
    void rendersStandardBreadthThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzStandardBreadthScreenshotSurface surface;
        surface.polish();
        const QImage actual = zzRenderStandardBreadthSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);

        ZzTextMaskCoverage coverage;
        const QImage mask = zzBuildTextMask(
            &surface.window,
            &surface.menu,
            actualDpr_,
            &coverage);
        const QString missingCoverage = zzValidateMaskCoverage(coverage);
        QVERIFY2(
            missingCoverage.isEmpty(),
            qPrintable(QStringLiteral("标准控件广度文字遮罩漏掉：%1")
                           .arg(missingCoverage)));
        QVERIFY(coverage.menuBars > 0);
        QVERIFY(coverage.toolBars > 0);
        QVERIFY(coverage.statusBars > 0);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 标准控件广度非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("light");
        QTest::newRow("dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("dark");
        QTest::newRow("high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("high-contrast");
    }

    void rendersThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzScreenshotSurface surface;
        surface.window.setPalette(QApplication::palette());
        surface.menu.setPalette(QApplication::palette());
        surface.polish();
        const QImage actual = zzRenderSurface(&surface, actualDpr_);
        QCOMPARE(
            actual.size(),
            QSize(
                qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));

        ZzTextMaskCoverage coverage;
        const QImage mask = zzBuildTextMask(
            &surface.window,
            &surface.menu,
            actualDpr_,
            &coverage);
        const QString missingCoverage = zzValidateMaskCoverage(coverage);
        QVERIFY2(
            missingCoverage.isEmpty(),
            qPrintable(QStringLiteral("文字遮罩漏掉控件类别：%1")
                           .arg(missingCoverage)));
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 非文字区域差异比例 %3 超过门限 %4，actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersCardThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("cards-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("cards-light");
        QTest::newRow("cards-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("cards-dark");
        QTest::newRow("cards-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("cards-high-contrast");
    }

    void rendersCardThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzCardScreenshotSurface surface;
        surface.polish();
        const QImage actual = zzRenderCardSurface(&surface, actualDpr_);
        QCOMPARE(
            actual.size(),
            QSize(
                qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));
        const ZzCardTextMask mask = zzBuildCardTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.actionCards, 6);
        QCOMPARE(mask.imageCards, 4);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 卡片非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersCarouselThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("carousel-view-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("carousel-view-light");
        QTest::newRow("carousel-view-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("carousel-view-dark");
        QTest::newRow("carousel-view-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("carousel-view-high-contrast");
    }

    void rendersCarouselThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzCarouselScreenshotSurface surface;
        surface.polish();
        QCOMPARE(
            surface.window.findChildren<ZzFluentUI::ZzCarouselView *>()
                .size(),
            6);
        QCOMPARE(
            surface.window.findChildren<QAbstractAnimation *>().size(),
            6);
        QCOMPARE(surface.carousel(0)->currentRow(), 0);
        QCOMPARE(surface.carousel(0)->model()->rowCount(), 9);
        QVERIFY(surface.carousel(1)
                    ->currentIndex()
                    .data(Qt::DecorationRole)
                    .isNull());
        QVERIFY(surface.carousel(2)
                    ->currentIndex()
                    .data(Qt::DisplayRole)
                    .toString()
                    .size()
                > 40);
        QVERIFY(!surface.carousel(3)->isEnabled());
        QVERIFY(surface.carousel(4)->hasFocus());
        QCOMPARE(surface.carousel(5)->layoutDirection(), Qt::RightToLeft);
        QCOMPARE(surface.carousel(5)->currentRow(), 2);

        const QImage actual = zzRenderCarouselSurface(&surface, actualDpr_);
        QCOMPARE(
            actual.size(),
            QSize(
                qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));
        const ZzCarouselTextMask mask = zzBuildCarouselTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.carousels, 6);
        QCOMPARE(mask.titles, 6);
        QCOMPARE(mask.descriptions, 5);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 轮播非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersCommandStatusThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("command-status-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("command-status-light");
        QTest::newRow("command-status-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("command-status-dark");
        QTest::newRow("command-status-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("command-status-high-contrast");
    }

    void rendersCommandStatusThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzCommandStatusScreenshotSurface surface;
        surface.polish();
        QCOMPARE(surface.window.findChildren<QToolBar *>().size(), 4);
        QCOMPARE(surface.topBar()->orientation(), Qt::Horizontal);
        QCOMPARE(surface.leftBar()->orientation(), Qt::Vertical);
        QCOMPARE(surface.rtlBar()->orientation(), Qt::Vertical);
        QCOMPARE(surface.rtlBar()->layoutDirection(), Qt::RightToLeft);
        QVERIFY(surface.checkedAction()->isChecked());
        QVERIFY(!surface.disabledAction()->isEnabled());
        QVERIFY(surface.menuAction()->menu() != nullptr);
        QVERIFY(surface.hoverButton() != nullptr);
        QVERIFY(surface.hoverButton()->underMouse());
        QVERIFY(surface.pressedButton() != nullptr);
        QVERIFY(surface.pressedButton()->isDown());
        QVERIFY(surface.focusButton() != nullptr);
        QVERIFY(surface.focusButton()->hasFocus());
        QWidget *overflowWidget = surface.overflowBar()->widgetForAction(
            surface.lastOverflowAction());
        QVERIFY(overflowWidget != nullptr);
        QVERIFY(!overflowWidget->isVisible());
        QVERIFY(surface.normalStatus()->currentMessage().isEmpty());
        QCOMPARE(
            surface.bottomStatus()->currentMessage(),
            QStringLiteral("Synchronizing artifacts"));
        QVERIFY(surface.bottomStatus()->isSizeGripEnabled());
        QSizeGrip *sizeGrip =
            surface.bottomStatus()->findChild<QSizeGrip *>();
        QVERIFY(sizeGrip != nullptr);
        QVERIFY(sizeGrip->isVisible());

        const QImage actual = zzRenderCommandStatusSurface(
            &surface,
            actualDpr_);
        QCOMPARE(
            actual.size(),
            QSize(
                qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));
        const ZzCommandStatusTextMask mask =
            zzBuildCommandStatusTextMask(&surface.window, actualDpr_);
        QVERIFY(mask.labels >= 8);
        QVERIFY(mask.toolButtons >= 8);
        QCOMPARE(mask.statusMessages, 1);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 命令与状态非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersTabThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("tabs-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("tabs-light");
        QTest::newRow("tabs-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("tabs-dark");
        QTest::newRow("tabs-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("tabs-high-contrast");
    }

    void rendersTabThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzTabScreenshotSurface surface;
        surface.polish();
        const QImage actual = zzRenderTabSurface(&surface, actualDpr_);
        QCOMPARE(
            actual.size(),
            QSize(
                qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));
        const ZzTabTextMask mask = zzBuildTabTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.tabBars, 4);
        QVERIFY(mask.tabTexts >= 20);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 标签页非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersProgressRingThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("progress-rings-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("progress-rings-light");
        QTest::newRow("progress-rings-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("progress-rings-dark");
        QTest::newRow("progress-rings-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("progress-rings-high-contrast");
    }

    void rendersProgressRingThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzProgressRingScreenshotSurface surface;
        surface.polish();
        const QImage actual = zzRenderProgressRingSurface(
            &surface,
            actualDpr_);
        QCOMPARE(
            actual.size(),
            QSize(
                qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));
        const ZzProgressRingTextMask mask = zzBuildProgressRingTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.progressRings, 10);
        QCOMPARE(mask.textRings, 8);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 环形进度非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersScrollControlThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("scroll-controls-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("scroll-controls-light");
        QTest::newRow("scroll-controls-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("scroll-controls-dark");
        QTest::newRow("scroll-controls-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("scroll-controls-high-contrast");
    }

    void rendersScrollControlThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzScrollControlsScreenshotSurface surface;
        surface.polish();
        QVERIFY(surface.pressedStateIsActive());
        QCOMPARE(surface.runningAnimationCount(), 0);
        QCOMPARE(
            surface.window.findChildren<ZzFluentUI::ZzScrollBar *>().size(),
            9);
        QCOMPARE(surface.window.findChildren<QScrollBar *>().size(), 10);
        const QImage actual = zzRenderScrollControlsSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        QImage mask(expectedPhysicalSize, QImage::Format_Grayscale8);
        mask.setDevicePixelRatio(actualDpr_);
        mask.fill(0);
        surface.hide();
        QCOMPARE(surface.runningAnimationCount(), 0);

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 滚动控件像素差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersSpinBoxThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("spin-box-controls-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("spin-box-controls-light");
        QTest::newRow("spin-box-controls-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("spin-box-controls-dark");
        QTest::newRow("spin-box-controls-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("spin-box-controls-high-contrast");
    }

    void rendersComboBoxThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("combo-box-controls-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("combo-box-controls-light");
        QTest::newRow("combo-box-controls-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("combo-box-controls-dark");
        QTest::newRow("combo-box-controls-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("combo-box-controls-high-contrast");
    }

    void rendersSuggestBoxThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("suggest-box-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("suggest-box-light");
        QTest::newRow("suggest-box-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("suggest-box-dark");
        QTest::newRow("suggest-box-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("suggest-box-high-contrast");
    }

    void rendersMultiSelectThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("multi-select-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("multi-select-light");
        QTest::newRow("multi-select-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("multi-select-dark");
        QTest::newRow("multi-select-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("multi-select-high-contrast");
    }

    void rendersRollerThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("roller-controls-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("roller-controls-light");
        QTest::newRow("roller-controls-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("roller-controls-dark");
        QTest::newRow("roller-controls-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("roller-controls-high-contrast");
    }

    void rendersFlowLayoutThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("flow-layout-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("flow-layout-light");
        QTest::newRow("flow-layout-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("flow-layout-dark");
        QTest::newRow("flow-layout-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("flow-layout-high-contrast");
    }

    void rendersDigitalDisplayThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("digital-display-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("digital-display-light");
        QTest::newRow("digital-display-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("digital-display-dark");
        QTest::newRow("digital-display-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("digital-display-high-contrast");
    }

    void rendersPopupSurfaceThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("popup-surfaces-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("popup-surfaces-light");
        QTest::newRow("popup-surfaces-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("popup-surfaces-dark");
        QTest::newRow("popup-surfaces-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("popup-surfaces-high-contrast");
    }

    void rendersContentDialogThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("content-dialog-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("content-dialog-light");
        QTest::newRow("content-dialog-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("content-dialog-dark");
        QTest::newRow("content-dialog-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("content-dialog-high-contrast");
    }

    void rendersTeachingTipThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("teaching-tip-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("teaching-tip-light");
        QTest::newRow("teaching-tip-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("teaching-tip-dark");
        QTest::newRow("teaching-tip-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("teaching-tip-high-contrast");
    }

    void rendersTextInputThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("text-input-controls-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("text-input-controls-light");
        QTest::newRow("text-input-controls-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("text-input-controls-dark");
        QTest::newRow("text-input-controls-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("text-input-controls-high-contrast");
    }

    void rendersTextInputThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzTextInputScreenshotSurface surface;
        surface.polish();
        QVERIFY(surface.hasNativeClearAction());
        const QImage actual = zzRenderTextInputSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzTextInputTextMask mask = zzBuildTextInputTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.lineEdits, 9);
        QCOMPARE(mask.textEdits, 2);
        QCOMPARE(mask.plainTextEdits, 1);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 文本输入非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersSpinBoxThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzSpinBoxScreenshotSurface surface;
        surface.polish();
        QVERIFY(surface.pressedStepWasTriggered());
        QCOMPARE(
            surface.window.findChildren<QAbstractSpinBox *>().size(),
            12);
        const QImage actual = zzRenderSpinBoxSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzSpinBoxTextMask mask = zzBuildSpinBoxTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.spinBoxes, 12);
        QCOMPARE(mask.editors, 12);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 数值输入非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersComboBoxThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzComboBoxScreenshotSurface surface;
        surface.polish();
        QWidget *popupWindow = surface.popupWindow();
        QVERIFY(popupWindow != nullptr);
        QVERIFY(popupWindow->isVisible());
        QVERIFY(surface.openComboBox() != nullptr);
        QCOMPARE(surface.openComboBox()->style(), QApplication::style());
        QCOMPARE(surface.openComboBox()->view()->style(),
                 QApplication::style());
        QCOMPARE(surface.openComboBox()->view()->viewport()->style(),
                 QApplication::style());
        QCOMPARE(surface.window.findChildren<QComboBox *>().size(), 10);
        const QImage actual = zzRenderComboBoxSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzComboBoxTextMask mask = zzBuildComboBoxTextMask(
            &surface,
            actualDpr_);
        QCOMPARE(mask.comboBoxes, 10);
        QCOMPARE(mask.closedLabels, 9);
        QCOMPARE(mask.editableEditors, 1);
        QCOMPARE(mask.popupItems, 4);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 组合框非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersSuggestBoxThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzSuggestBoxScreenshotSurface surface;
        surface.polish();
        ZzFluentUI::ZzSuggestBox *openBox = surface.openBox();
        QWidget *popupWindow = surface.popupWindow();
        QVERIFY(openBox != nullptr);
        QVERIFY(openBox->completer() != nullptr);
        QVERIFY(openBox->completer()->popup() != nullptr);
        QVERIFY(popupWindow != nullptr);
        QVERIFY(popupWindow->isVisible());
        QCOMPARE(openBox->style(), QApplication::style());
        QCOMPARE(openBox->completer()->popup()->style(),
                 QApplication::style());
        QCOMPARE(openBox->completer()->popup()->viewport()->style(),
                 QApplication::style());
        QCOMPARE(
            surface.window.findChildren<ZzFluentUI::ZzSuggestBox *>().size(),
            9);
        const QImage actual = zzRenderSuggestBoxSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzSuggestBoxTextMask mask = zzBuildSuggestBoxTextMask(
            &surface,
            actualDpr_);
        QCOMPARE(mask.suggestBoxes, 9);
        QCOMPARE(mask.inputTexts, 9);
        QCOMPARE(mask.popupItems, 5);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 搜索建议框非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersMultiSelectThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzMultiSelectScreenshotSurface surface;
        surface.polish();
        ZzFluentUI::ZzMultiSelectComboBox *openBox = surface.openBox();
        QWidget *popupWindow = surface.popupWindow();
        if (openBox == nullptr || openBox->view() == nullptr
            || popupWindow == nullptr) {
            QFAIL("多选组合框截图面缺少标准popup");
            return;
        }
        QVERIFY(popupWindow->isVisible());
        QCOMPARE(openBox->style(), QApplication::style());
        QCOMPARE(openBox->view()->style(), QApplication::style());
        QCOMPARE(openBox->view()->viewport()->style(),
                 QApplication::style());
        QCOMPARE(
            surface.window.findChildren<
                ZzFluentUI::ZzMultiSelectComboBox *>().size(),
            9);
        QCOMPARE(openBox->selectionCount(), 3);
        QCOMPARE(openBox->currentIndex(), -1);
        const QImage actual = zzRenderMultiSelectSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzMultiSelectTextMask mask = zzBuildMultiSelectTextMask(
            &surface,
            actualDpr_);
        QCOMPARE(mask.comboBoxes, 9);
        QCOMPARE(mask.closedTexts, 9);
        QCOMPARE(mask.popupItems, 5);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 多选组合框非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersRollerThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzRollerScreenshotSurface surface;
        surface.polish();
        ZzFluentUI::ZzRollerPicker *openPicker = surface.openPicker();
        QWidget *popupWindow = surface.popupWindow();
        if (openPicker == nullptr || popupWindow == nullptr) {
            QFAIL("滚轮选择器截图面缺少标准popup");
            return;
        }
        QVERIFY(openPicker->isPopupVisible());
        QVERIFY(popupWindow->isVisible());
        QCOMPARE(openPicker->style(), QApplication::style());
        QCOMPARE(popupWindow->style(), QApplication::style());
        QCOMPARE(
            surface.window.findChildren<ZzFluentUI::ZzRoller *>().size(),
            9);
        QCOMPARE(
            surface.window.findChildren<
                ZzFluentUI::ZzRollerPicker *>().size(),
            2);
        QCOMPARE(
            popupWindow->findChildren<QDialogButtonBox *>().size(),
            1);
        const QImage actual = zzRenderRollerSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzRollerTextMask mask = zzBuildRollerTextMask(
            &surface,
            actualDpr_);
        QCOMPARE(mask.rollers, 8);
        QVERIFY(mask.rollerTexts >= 36);
        QCOMPARE(mask.pickerSummaries, 2);
        QCOMPARE(mask.popupButtons, 2);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 滚轮选择控件非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersFlowLayoutThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzFlowLayoutScreenshotSurface surface;
        surface.polish();
        QCOMPARE(
            surface.window.findChildren<
                ZzFluentUI::ZzFlowLayout *>().size(),
            3);
        QCOMPARE(
            surface.window.findChildren<
                ZzFluentUI::ZzPushButton *>().size(),
            15);
        const auto &narrowButtons = surface.buttons(0);
        const auto &wideButtons = surface.buttons(1);
        const auto &rightToLeftButtons = surface.buttons(2);
        QCOMPARE(narrowButtons.at(0)->y(), narrowButtons.at(1)->y());
        QVERIFY(narrowButtons.at(2)->y() > narrowButtons.at(1)->y());
        QVERIFY(narrowButtons.at(4)->y() > narrowButtons.at(2)->y());
        for (const ZzFluentUI::ZzPushButton *button : wideButtons) {
            QCOMPARE(button->y(), wideButtons.front()->y());
        }
        QVERIFY(rightToLeftButtons.at(0)->x()
                > rightToLeftButtons.at(1)->x());
        QVERIFY(rightToLeftButtons.at(1)->x()
                > rightToLeftButtons.at(2)->x());
        for (std::size_t group = 0; group < 3; ++group) {
            QWidget *const host = surface.host(group);
            const auto &buttons = surface.buttons(group);
            for (std::size_t index = 0; index < buttons.size(); ++index) {
                QVERIFY(host->rect().contains(buttons.at(index)->geometry()));
                for (std::size_t other = index + 1;
                     other < buttons.size();
                     ++other) {
                    QVERIFY(!buttons.at(index)->geometry().intersects(
                        buttons.at(other)->geometry()));
                }
            }
        }

        const QImage actual = zzRenderFlowLayoutSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzFlowLayoutTextMask mask = zzBuildFlowLayoutTextMask(
            &surface,
            actualDpr_);
        QCOMPARE(mask.labels, 3);
        QCOMPARE(mask.buttons, 15);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 流式布局非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersDigitalDisplayThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzDigitalDisplayScreenshotSurface surface;
        surface.polish();
        QCOMPARE(surface.window.findChildren<QLCDNumber *>().size(), 6);
        QCOMPARE(surface.display(0)->mode(), QLCDNumber::Dec);
        QCOMPARE(surface.display(1)->intValue(), -125);
        QVERIFY(surface.display(2)->smallDecimalPoint());
        QCOMPARE(surface.display(2)->value(), -12.5);
        QCOMPARE(surface.display(3)->mode(), QLCDNumber::Hex);
        QCOMPARE(surface.display(3)->intValue(), 48879);
        QVERIFY(!surface.display(4)->isEnabled());
        QCOMPARE(surface.display(5)->frameShape(), QFrame::NoFrame);
        for (std::size_t index = 0; index < 6; ++index) {
            QLCDNumber *display = surface.display(index);
            QVERIFY(surface.window.rect().contains(display->geometry()));
            for (std::size_t other = index + 1; other < 6; ++other) {
                QVERIFY(!display->geometry().intersects(
                    surface.display(other)->geometry()));
            }
        }

        const QImage actual = zzRenderDigitalDisplaySurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzDigitalDisplayTextMask mask =
            zzBuildDigitalDisplayTextMask(&surface, actualDpr_);
        QCOMPARE(mask.labels, 6);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 数字显示非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersPopupSurfaceThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzPopupSurfaceScreenshotSurface surface;
        surface.polish();
        for (QMenu *menu : surface.menus()) {
            QVERIFY(menu != nullptr);
            QVERIFY(menu->isVisible());
            QCOMPARE(menu->style(), QApplication::style());
        }
        const QImage actual = zzRenderPopupSurface(&surface, actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzPopupSurfaceTextMask mask = zzBuildPopupSurfaceTextMask(
            &surface,
            actualDpr_);
        QCOMPARE(mask.menuBars, 2);
        QCOMPARE(mask.menuBarItems, 5);
        QCOMPARE(mask.menus, 3);
        QVERIFY(mask.menuItems >= 16);
        QVERIFY(mask.shortcuts >= 3);
        QCOMPARE(mask.toolTips, 2);
        QCOMPARE(mask.previewItems, 2);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 弹出表面非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersContentDialogThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzContentDialogScreenshotSurface surface;
        surface.polish();
        QVERIFY(surface.dialog() != nullptr);
        QVERIFY(surface.dialog()->isVisible());
        QVERIFY(surface.dialog()->isModal());
        QCOMPARE(
            surface.window.findChildren<ZzFluentUI::ZzContentDialog *>()
                .size(),
            1);
        QWidget *overlay = surface.window.findChild<QWidget *>(
            QStringLiteral("zzContentDialogOverlay"),
            Qt::FindDirectChildrenOnly);
        QVERIFY(overlay != nullptr);
        QCOMPARE(overlay->geometry(), surface.window.rect());

        const QImage actual = zzRenderContentDialogSurface(
            &surface, actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzContentDialogTextMask mask =
            zzBuildContentDialogTextMask(&surface, actualDpr_);
        QVERIFY(mask.labels >= 5);
        QCOMPARE(mask.buttons, 4);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 内容对话框非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersTeachingTipThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzTeachingTipScreenshotSurface surface;
        surface.polish();
        QVERIFY(surface.tip() != nullptr);
        QVERIFY(surface.tip()->isVisible());
        QCOMPARE(
            surface.tip()->effectivePlacement(),
            ZzFluentUI::ZzTeachingTipPlacement::Bottom);
        QCOMPARE(surface.tip()->targetWidget(), surface.target());
        auto *closeButton = surface.tip()->findChild<QToolButton *>(
            QStringLiteral("zzTeachingTipCloseButton"));
        QVERIFY(closeButton != nullptr);
        QVERIFY(!closeButton->icon().isNull());
        QVERIFY(surface.target()->screen()->availableGeometry().contains(
            surface.tip()->frameGeometry()));

        const QImage actual = zzRenderTeachingTipSurface(
            &surface, actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzTeachingTipTextMask mask = zzBuildTeachingTipTextMask(
            &surface, actualDpr_);
        QVERIFY(mask.labels >= 5);
        QCOMPARE(mask.buttons, 2);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 教学提示非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersInputExpansionThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("input-expansion-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("input-expansion-light");
        QTest::newRow("input-expansion-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("input-expansion-dark");
        QTest::newRow("input-expansion-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("input-expansion-high-contrast");
    }

    void rendersInputExpansionThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzInputExpansionScreenshotSurface surface;
        surface.polish();
        const auto boxes = surface.window.findChildren<
            ZzFluentUI::ZzPasswordBox *>();
        QCOMPARE(boxes.size(), 4);
        QCOMPARE(
            boxes.at(0)->revealMode(),
            ZzFluentUI::ZzPasswordRevealMode::Peek);
        QCOMPARE(
            boxes.at(1)->revealMode(),
            ZzFluentUI::ZzPasswordRevealMode::Hidden);
        QCOMPARE(
            boxes.at(2)->revealMode(),
            ZzFluentUI::ZzPasswordRevealMode::Visible);
        QVERIFY(!boxes.at(3)->isEnabled());
        QCOMPARE(
            surface.window.findChildren<ZzFluentUI::ZzIconButton *>()
                .size(),
            4);
        const auto splitButtons = surface.window.findChildren<
            ZzFluentUI::ZzSplitButton *>();
        QCOMPARE(splitButtons.size(), 4);
        QCOMPARE(
            splitButtons.at(1)->appearance(),
            ZzFluentUI::ZzButtonAppearance::Accent);
        QCOMPARE(
            splitButtons.at(2)->appearance(),
            ZzFluentUI::ZzButtonAppearance::Subtle);
        QVERIFY(!splitButtons.at(3)->isEnabled());
        const auto ratings = surface.window.findChildren<
            ZzFluentUI::ZzRatingControl *>();
        QCOMPARE(ratings.size(), 4);
        QCOMPARE(ratings.at(0)->rating(), 4.0);
        QCOMPARE(
            ratings.at(1)->precision(),
            ZzFluentUI::ZzRatingPrecision::Half);
        QCOMPARE(ratings.at(1)->rating(), 3.5);
        QVERIFY(ratings.at(2)->isReadOnly());
        QVERIFY(!ratings.at(3)->isEnabled());
        const auto keyBinders = surface.window.findChildren<
            ZzFluentUI::ZzKeyBinder *>();
        QCOMPARE(keyBinders.size(), 1);
        QCOMPARE(
            keyBinders.constFirst()->keySequence(),
            QKeySequence(QKeyCombination(
                Qt::ControlModifier | Qt::ShiftModifier,
                Qt::Key_P)));
        QVERIFY(!keyBinders.constFirst()->isRecording());
        const auto colorPickers = surface.window.findChildren<
            ZzFluentUI::ZzColorPicker *>();
        QCOMPARE(colorPickers.size(), 1);
        QCOMPARE(colorPickers.constFirst()->paletteColorCount(), 8);
        QVERIFY(colorPickers.constFirst()->isAlphaEnabled());
        QCOMPARE(
            colorPickers.constFirst()->currentColor().rgba(),
            qRgba(64, 128, 192, 128));

        const QImage actual = zzRenderInputExpansionSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzInputExpansionTextMask mask =
            zzBuildInputExpansionTextMask(
                &surface.window,
                actualDpr_);
        QVERIFY(mask.labels >= 18);
        QCOMPARE(mask.passwordBoxes, 4);
        QCOMPARE(mask.splitButtons, 4);
        QCOMPARE(mask.keyBinders, 1);
        QCOMPARE(mask.colorEditors, 5);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 输入扩展非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersDrawerThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("drawer-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("drawer-light");
        QTest::newRow("drawer-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("drawer-dark");
        QTest::newRow("drawer-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("drawer-high-contrast");
    }

    void rendersDrawerThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzDrawerScreenshotSurface surface;
        surface.polish();
        ZzFluentUI::ZzDrawer *const modal = surface.modalDrawer();
        ZzFluentUI::ZzDrawer *const nonModal = surface.nonModalDrawer();
        QVERIFY(modal != nullptr);
        QVERIFY(nonModal != nullptr);
        QVERIFY(modal->isOpen());
        QVERIFY(nonModal->isOpen());
        QVERIFY(modal->isModal());
        QVERIFY(!nonModal->isModal());
        QCOMPARE(modal->edge(), ZzFluentUI::ZzDrawerEdge::Left);
        QCOMPARE(nonModal->edge(), ZzFluentUI::ZzDrawerEdge::Right);
        auto *modalPanel = modal->findChild<QWidget *>(
            QStringLiteral("zzDrawerPanelHost"));
        auto *nonModalPanel = nonModal->findChild<QWidget *>(
            QStringLiteral("zzDrawerPanelHost"));
        QVERIFY(modalPanel != nullptr);
        QVERIFY(nonModalPanel != nullptr);
        QCOMPARE(modalPanel->geometry(), QRect(0, 0, 280, modal->height()));
        QCOMPARE(
            nonModalPanel->geometry(),
            QRect(nonModal->width() - 280, 0, 280, nonModal->height()));
        QVERIFY(nonModal->mask().contains(nonModalPanel->geometry().center()));
        QVERIFY(!nonModal->mask().contains(QPoint(0, nonModal->height() / 2)));

        const QImage actual = zzRenderDrawerSurface(&surface, actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const ZzDrawerTextMask mask = zzBuildDrawerTextMask(
            &surface.window,
            actualDpr_);
        QCOMPARE(mask.labels, 8);
        QCOMPARE(mask.buttons, 4);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 Drawer 非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void rendersNavigationPaneThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("navigation-pane-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("navigation-pane-light");
        QTest::newRow("navigation-pane-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("navigation-pane-dark");
        QTest::newRow("navigation-pane-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("navigation-pane-high-contrast");
    }

    void rendersNavigationPaneThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));

        ZzNavigationPaneScreenshotSurface surface;
        surface.polish();
        QCOMPARE(
            surface.window
                .findChildren<ZzFluentUI::ZzNavigationPane *>()
                .size(),
            3);
        QVERIFY(surface.regularPane() != nullptr);
        QVERIFY(surface.compactPane() != nullptr);
        QVERIFY(surface.rtlPane() != nullptr);
        QVERIFY(!surface.regularPane()->isCompact());
        QVERIFY(surface.compactPane()->isCompact());
        QVERIFY(!surface.rtlPane()->isCompact());
        QCOMPARE(
            surface.rtlPane()->layoutDirection(),
            Qt::RightToLeft);
        QCOMPARE(surface.regularPane()->currentSourceIndex().row(), 0);
        QCOMPARE(surface.compactPane()->currentSourceIndex().row(), 5);
        QCOMPARE(surface.rtlPane()->currentSourceIndex().row(), 3);
        QVERIFY(!surface.regularPane()->model()
                     ->index(2, 0)
                     .flags()
                     .testFlag(Qt::ItemIsEnabled));
        QCOMPARE(
            surface.regularPane()->model()
                ->index(5, 0)
                .data(static_cast<int>(
                    ZzFluentUI::ZzNavigationItemRole::Placement))
                .value<ZzFluentUI::ZzNavigationPlacement>(),
            ZzFluentUI::ZzNavigationPlacement::Footer);
        QVERIFY(surface.hoverView() != nullptr);
        QVERIFY(surface.hoverView()->viewport()->underMouse());
        QVERIFY(surface.focusView() != nullptr);
        QVERIFY(surface.focusView()->hasFocus());

        const QImage actual = zzRenderNavigationPaneSurface(
            &surface,
            actualDpr_);
        const QSize expectedPhysicalSize(
            qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
            qRound(zzLogicalSurfaceSize.height() * expectedDpr_));
        QCOMPARE(actual.size(), expectedPhysicalSize);
        const QModelIndex hoveredIndex =
            surface.hoverView()->model()->index(6, 0);
        const QRect hoveredRect =
            surface.hoverView()->visualRect(hoveredIndex);
        const QPoint hoverLogical = surface.hoverView()->viewport()->mapTo(
            &surface.window,
            hoveredRect.center());
        const QPoint blankLogical = surface.hoverView()->viewport()->mapTo(
            &surface.window,
            QPoint(
                hoveredRect.center().x(),
                hoveredRect.bottom() + 80));
        const auto physicalPoint = [this](const QPoint &logical) {
            return QPoint(
                qRound(logical.x() * actualDpr_),
                qRound(logical.y() * actualDpr_));
        };
        QVERIFY(actual.rect().contains(physicalPoint(hoverLogical)));
        QVERIFY(actual.rect().contains(physicalPoint(blankLogical)));
        QVERIFY(
            actual.pixelColor(physicalPoint(hoverLogical))
            != actual.pixelColor(physicalPoint(blankLogical)));
        const ZzNavigationPaneTextMask mask =
            zzBuildNavigationPaneTextMask(
                &surface.window,
                actualDpr_);
        QCOMPARE(mask.panes, 3);
        QCOMPARE(mask.sections, 4);
        QCOMPARE(mask.titles, 14);
        QCOMPARE(mask.badges, 4);
        surface.hide();

        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY2(
                QDir().mkpath(baselineDirectory),
                qPrintable(QStringLiteral("无法创建 baseline 目录：%1")
                               .arg(baselineDirectory)));
            QVERIFY2(
                actual.save(baselinePath, "PNG"),
                qPrintable(QStringLiteral("无法写入 baseline：%1")
                               .arg(baselinePath)));
            return;
        }

        QImage expected(baselinePath);
        QVERIFY2(
            !expected.isNull(),
            qPrintable(QStringLiteral("缺少或无法读取 baseline：%1")
                           .arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(
            expected,
            actual,
            mask.image);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal differenceRatio =
            static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        const qreal maximumDifferenceRatio = zzMaximumDifferenceRatio();
        if (differenceRatio <= maximumDifferenceRatio) {
            return;
        }

        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_FLUENT_SCREENSHOT_REPORT_DIR))
                                            .filePath(baselineSubdirectory_);
        QVERIFY2(
            QDir().mkpath(reportDirectory),
            qPrintable(QStringLiteral("无法创建截图报告目录：%1")
                           .arg(reportDirectory)));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(
            QStringLiteral(
                "Qt %1.%2 导航面板非文字区域差异比例 %3 超过门限 %4，"
                "actual=%5，diff=%6")
                .arg(QT_VERSION_MAJOR)
                .arg(QT_VERSION_MINOR)
                .arg(differenceRatio, 0, 'f', 6)
                .arg(maximumDifferenceRatio, 0, 'f', 6)
                .arg(actualPath, diffPath)));
    }

    void cleanupTestCase()
    {
        QApplication::setStyle(
            QStyleFactory::create(QStringLiteral("Fusion")));
        controller_.reset();
    }

private:
    qreal expectedDpr_ = 1.0;
    QString baselineSubdirectory_;
    qreal actualDpr_ = 1.0;
    std::unique_ptr<ZzFluentUI::ZzThemeController> controller_;
};

namespace {

/**
 * @brief 解析固定视觉参数并运行截图测试。
 * @return 测试退出码。
 */
int zzRunFluentScreenshotTest(int argc, char *argv[])
{
    QString parseError;
    std::optional<ZzScreenshotArguments> arguments = zzParseArguments(
        argc,
        argv,
        &parseError);
    if (!arguments.has_value()) {
        std::fprintf(stderr, "%s\n", qPrintable(parseError));
        return EXIT_FAILURE;
    }

    std::vector<char *> filteredPointers;
    filteredPointers.reserve(arguments->filteredArguments.size());
    for (QByteArray &argument : arguments->filteredArguments) {
        filteredPointers.push_back(argument.data());
    }
    int filteredArgc = static_cast<int>(filteredPointers.size());
    QApplication application(filteredArgc, filteredPointers.data());
    ZzFluentScreenshotTest test(
        arguments->expectedDpr,
        arguments->baselineSubdirectory);
    return QTest::qExec(&test, filteredArgc, filteredPointers.data());
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        return zzRunFluentScreenshotTest(argc, argv);
    } catch (const std::exception &exception) {
        std::fprintf(
            stderr,
            "ZzFluent screenshot test failed: %s\n",
            exception.what());
    } catch (...) {
        std::fprintf(
            stderr,
            "ZzFluent screenshot test failed: unknown exception\n");
    }
    return EXIT_FAILURE;
}

#include "ZzFluentScreenshotTest.moc"
