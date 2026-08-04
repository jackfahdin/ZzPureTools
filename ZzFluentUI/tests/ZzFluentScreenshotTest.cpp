#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtGui/QAction>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QImage>
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

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {

constexpr QSize zzLogicalSurfaceSize(1200, 800);
constexpr QPoint zzMenuOrigin(914, 590);
constexpr int zzTextMaskPadding = 2;
constexpr int zzChannelTolerance = 3;
constexpr qreal zzMaximumDifferenceRatio = 0.005;

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
                label->alignment(),
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
        form->addRow(QStringLiteral("Name"), name);
        form->addRow(QStringLiteral("Notes"), notes);
        form->addRow(QStringLiteral("Density"), mode);
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
        table->setFixedHeight(216);
        layout->addWidget(table);

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
        tree->setFixedHeight(230);
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
        if (differenceRatio <= zzMaximumDifferenceRatio) {
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
                "非文字区域差异比例 %1 超过 0.5%，actual=%2，diff=%3")
                .arg(differenceRatio, 0, 'f', 6)
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

int main(int argc, char *argv[])
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

#include "ZzFluentScreenshotTest.moc"
