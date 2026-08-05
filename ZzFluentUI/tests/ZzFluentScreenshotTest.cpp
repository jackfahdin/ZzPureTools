#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtGui/QAction>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QImage>
#include <QtGui/QEnterEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {

constexpr QSize zzLogicalSurfaceSize(1200, 800);
constexpr QPoint zzMenuOrigin(914, 590);
constexpr int zzTextMaskPadding = 2;
constexpr int zzChannelTolerance = 3;
constexpr qreal zzReferenceMaximumDifferenceRatio = 0.005;
constexpr qreal zzCompatibilityMaximumDifferenceRatio = 0.02;

/** @brief 返回当前 Qt minor 对应的非文字像素差异上限。 */
constexpr qreal zzMaximumDifferenceRatio()
{
    if constexpr (
        QT_VERSION_MAJOR == ZZ_FLUENT_SCREENSHOT_REFERENCE_QT_MAJOR
        && QT_VERSION_MINOR == ZZ_FLUENT_SCREENSHOT_REFERENCE_QT_MINOR) {
        return zzReferenceMaximumDifferenceRatio;
    }
    return zzCompatibilityMaximumDifferenceRatio;
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

        auto *tabs = new QTabBar(container);
        tabs->addTab(QStringLiteral("Overview"));
        tabs->addTab(QStringLiteral("Details"));
        tabs->addTab(QStringLiteral("History"));
        tabs->setCurrentIndex(1);
        layout->addWidget(tabs);
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
