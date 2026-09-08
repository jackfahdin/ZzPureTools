#include "ZzExampleGalleryPagePrivate.h"

#include <array>
#include <utility>

#include <QtCore/QDate>
#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtGui/QFont>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzInfoBadge.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace ZzExample {

namespace {

#if defined(ZZ_EXAMPLE_LOCAL_PREVIEW_ASSETS)
/** @brief 以保持宽高比的方式绘制本地首页预览图。 */
class ZzExampleHomePreview final : public QWidget
{
public:
    /** @brief 从编译期资源创建不参与输入的首页视觉区域。 */
    explicit ZzExampleHomePreview(QWidget *parent)
        : QWidget(parent)
        , pixmap(QStringLiteral(
              ":/ZzPureToolsExample/local-assets/home.png"))
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(180, 140);
        setAccessibleName(QCoreApplication::translate(
            "ZzPureToolsExample", "Fluent Qt Widgets 预览"));
    }

    /** @brief 返回品牌横幅右侧视觉区域的建议逻辑尺寸。 */
    [[nodiscard]] QSize sizeHint() const override
    {
        return QSize(360, 180);
    }

protected:
    /** @brief 在任意 DPR 下从原始像素图平滑缩放并居中绘制。 */
    void paintEvent(QPaintEvent *event) override
    {
        QWidget::paintEvent(event);
        if (pixmap.isNull() || rect().isEmpty()) {
            return;
        }
        QSize targetSize = pixmap.size();
        targetSize.scale(rect().size(), Qt::KeepAspectRatio);
        const QRect target(
            QPoint(
                rect().center().x() - targetSize.width() / 2,
                rect().center().y() - targetSize.height() / 2),
            targetSize);
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(target, pixmap);
    }

private:
    QPixmap pixmap;
};
#endif

/** @brief 创建具有页面级字号的标题标签。 */
[[nodiscard]] QLabel *zzPageTitle(
    const QString &text,
    QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() + 10.0);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setWordWrap(true);
    return label;
}

/** @brief 创建适合连续页面分区的紧凑标题。 */
[[nodiscard]] QLabel *zzSectionTitle(
    const QString &text,
    QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() + 3.0);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    return label;
}

/** @brief 向页面增加不使用卡片容器的分区标题和分隔线。 */
void zzAddSection(
    QVBoxLayout *layout,
    const QString &title,
    QWidget *parent)
{
    layout->addSpacing(12);
    layout->addWidget(zzSectionTitle(title, parent));
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    layout->addWidget(separator);
}

/** @brief 创建页面共用的无边框可滚动内容区域。 */
[[nodiscard]] std::pair<QWidget *, QVBoxLayout *> zzPageContent(
    QWidget *page)
{
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *scrollArea = new ZzFluentUI::ZzScrollArea(page);
    scrollArea->setObjectName(QStringLiteral("zzExamplePageScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("zzExamplePageContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(32, 28, 32, 32);
    contentLayout->setSpacing(12);
    scrollArea->setWidget(content);
    pageLayout->addWidget(scrollArea);
    return {content, contentLayout};
}

/** @brief 在现有控件页中并排展示选中指示条，不共享各控件的选择模型。 */
[[nodiscard]] QWidget *zzSelectionIndicatorPreview(QWidget *parent)
{
    auto *section = new QWidget(parent);
    section->setObjectName(QStringLiteral("zzExampleSelectionIndicatorPreview"));
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(QCoreApplication::translate(
        "ZzPureToolsExample",
        "点击各控件独立切换，或连续点击统一切换比较动画。主题与减少动态效果沿用应用设置。"), section);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *commands = new QHBoxLayout;
    auto *next = new ZzFluentUI::ZzPushButton(QCoreApplication::translate(
        "ZzPureToolsExample", "统一切换到下一项"), section);
    auto *rtl = new QCheckBox(QStringLiteral("RTL"), section);
    auto *largeText = new QCheckBox(QCoreApplication::translate(
        "ZzPureToolsExample", "大字体标签"), section);
    rtl->setAccessibleName(QCoreApplication::translate(
        "ZzPureToolsExample", "指示条预览使用从右到左布局"));
    commands->addWidget(next);
    commands->addWidget(rtl);
    commands->addWidget(largeText);
    commands->addStretch(1);
    layout->addLayout(commands);

    auto *host = new QWidget(section);
    auto *flow = new ZzFluentUI::ZzFlowLayout(12, 12, host);
    flow->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(host);
    const auto addPanel = [host, flow](const QString &title, QWidget *control) {
        auto *panel = new QWidget(host);
        panel->setFixedSize(260, 230);
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(0, 0, 0, 0);
        panelLayout->addWidget(zzSectionTitle(title, panel));
        control->setAccessibleName(title);
        panelLayout->addWidget(control, 1);
        flow->addWidget(panel);
    };

    const QStringList labels{
        QCoreApplication::translate("ZzPureToolsExample", "概览"),
        QCoreApplication::translate("ZzPureToolsExample", "资源"),
        QCoreApplication::translate("ZzPureToolsExample", "设置")};
    auto *model = new QStandardItemModel(3, 2, host);
    model->setHorizontalHeaderLabels({
        QCoreApplication::translate("ZzPureToolsExample", "选项"),
        QCoreApplication::translate("ZzPureToolsExample", "状态")});
    for (int row = 0; row < 3; ++row) {
        model->setData(model->index(row, 0), labels.at(row));
        model->setData(model->index(row, 0), QSize(140, 36), Qt::SizeHintRole);
        model->setData(model->index(row, 1), QCoreApplication::translate(
            "ZzPureToolsExample", "就绪"));
    }
    model->item(0, 0)->appendRow(new QStandardItem(
        QCoreApplication::translate("ZzPureToolsExample", "子项目")));

    auto *list = new QListView(host);
    auto *tree = new QTreeView(host);
    auto *table = new QTableView(host);
    auto *navigation = new ZzFluentUI::ZzNavigationView(host);
    const std::array<QAbstractItemView *, 4> views{list, tree, table, navigation};
    for (auto *view : views) {
        view->setModel(model);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
    }
    tree->setColumnHidden(1, true);
    tree->setHeaderHidden(true);
    tree->expandAll();
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(36);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionsMovable(true);
    addPanel(QCoreApplication::translate("ZzPureToolsExample", "列表"), list);
    addPanel(QCoreApplication::translate("ZzPureToolsExample", "树形"), tree);
    addPanel(QCoreApplication::translate("ZzPureToolsExample", "表格"), table);
    addPanel(QCoreApplication::translate("ZzPureToolsExample", "导航"), navigation);

    auto *activities = new QStandardItemModel(3, 1, host);
    const std::array icons{ZzFluentUI::ZzBundledSvgIcon::ComputerSystem,
        ZzFluentUI::ZzBundledSvgIcon::Pin, ZzFluentUI::ZzBundledSvgIcon::MoreLine};
    for (int row = 0; row < 3; ++row) {
        const auto index = activities->index(row, 0);
        activities->setData(index, labels.at(row));
        activities->setData(index, QVariant::fromValue(
            ZzFluentUI::ZzIconDescriptor::fromBundledSvg(
                icons.at(static_cast<std::size_t>(row)))), Qt::DecorationRole);
        activities->setData(index, QVariant::fromValue(
            row == 2 ? ZzFluentUI::ZzActivityArea::LeftSecondary
                     : ZzFluentUI::ZzActivityArea::LeftPrimary),
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));
    }
    auto *activity = new ZzFluentUI::ZzActivityBar(ZzFluentUI::ZzSidePaneEdge::Left, host);
    activity->setModel(activities);
    QObject::connect(activity, &ZzFluentUI::ZzActivityBar::activationRequested,
        activity, &ZzFluentUI::ZzActivityBar::setCurrentSourceIndex);
    addPanel(QCoreApplication::translate("ZzPureToolsExample", "活动栏"), activity);

    auto *horizontal = new QWidget(host);
    auto *horizontalLayout = new QVBoxLayout(horizontal);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    auto *tabs = new QTabBar(horizontal);
    auto *pivot = new ZzFluentUI::ZzPivot(horizontal);
    for (const auto &label : labels) {
        tabs->addTab(label);
        pivot->addItem(label);
    }
    tabs->setTabText(2, QCoreApplication::translate("ZzPureToolsExample", "项目设置 Agjp"));
    pivot->setItemText(2, QCoreApplication::translate("ZzPureToolsExample", "项目设置 Agjp"));
    const QFont tabFont = tabs->font();
    const QFont pivotFont = pivot->font();
    QObject::connect(largeText, &QCheckBox::toggled, horizontal,
        [tabs, pivot, tabFont, pivotFont](bool checked) {
            QFont nextTabFont = tabFont;
            QFont nextPivotFont = pivotFont;
            if (checked) {
                nextTabFont.setPointSize(24);
                nextPivotFont.setPointSize(24);
            }
            tabs->setFont(nextTabFont);
            pivot->setFont(nextPivotFont);
        });
    horizontalLayout->addWidget(new QLabel(QStringLiteral("Tab"), horizontal));
    horizontalLayout->addWidget(tabs);
    horizontalLayout->addWidget(new QLabel(QStringLiteral("Pivot"), horizontal));
    horizontalLayout->addWidget(pivot);
    horizontalLayout->addStretch(1);
    addPanel(QCoreApplication::translate("ZzPureToolsExample", "横向选项"), horizontal);

    const auto selectAll = [views, model, activity, activities, tabs, pivot](int row) {
        for (auto *view : views) {
            view->setCurrentIndex(model->index(row, 0));
        }
        activity->setCurrentSourceIndex(activities->index(row, 0));
        tabs->setCurrentIndex(row);
        pivot->setCurrentIndex(row);
    };
    selectAll(0);
    QObject::connect(next, &QAbstractButton::clicked, host, [selectAll, tabs] {
        selectAll((tabs->currentIndex() + 1) % 3);
    });
    rtl->setChecked(host->layoutDirection() == Qt::RightToLeft);
    QObject::connect(rtl, &QCheckBox::toggled, host, [host](bool checked) {
        host->setLayoutDirection(checked ? Qt::RightToLeft : Qt::LeftToRight);
    });
    return section;
}

} // namespace

ZzExampleGalleryPagePrivate::ZzExampleGalleryPagePrivate(
    ZzExampleGalleryPage *page)
    : q_ptr(page)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzExampleGalleryPagePrivate::initialize(
    ZzExampleGalleryPage::ZzPageKind kind,
    const QString &title)
{
    switch (kind) {
    case ZzExampleGalleryPage::ZzPageKind::Home:
        buildHome(title);
        break;
    case ZzExampleGalleryPage::ZzPageKind::Controls:
        buildControls(title);
        break;
    }
}

void ZzExampleGalleryPagePrivate::buildHome(const QString &title)
{
    q_ptr->setObjectName(QStringLiteral("zzExampleHomePage"));
    auto [content, layout] = zzPageContent(q_ptr);

    auto *brandBand = new QWidget(content);
    brandBand->setObjectName(QStringLiteral("zzExampleHomeBrandBand"));
    brandBand->setAutoFillBackground(true);
    brandBand->setMinimumHeight(210);
#if defined(ZZ_EXAMPLE_LOCAL_PREVIEW_ASSETS)
    auto *brandLayout = new QHBoxLayout(brandBand);
    brandLayout->setContentsMargins(28, 14, 20, 14);
    brandLayout->setSpacing(20);
    auto *brandTextHost = new QWidget(brandBand);
    auto *brandTextLayout = new QVBoxLayout(brandTextHost);
    brandTextLayout->setContentsMargins(0, 10, 0, 10);
    brandTextLayout->setSpacing(10);
    brandLayout->addWidget(brandTextHost, 3);
    brandLayout->addWidget(new ZzExampleHomePreview(brandBand), 2);
#else
    auto *brandTextHost = brandBand;
    auto *brandTextLayout = new QVBoxLayout(brandBand);
    brandTextLayout->setContentsMargins(28, 24, 28, 24);
    brandTextLayout->setSpacing(10);
#endif

    auto *category = new QLabel(
        QStringLiteral("Fluent UI for Qt Widgets"), brandTextHost);
    category->setObjectName(QStringLiteral("zzExampleHomeCategory"));
    auto *brand = zzPageTitle(QStringLiteral("ZzPureTools"), brandTextHost);
    brand->setObjectName(QStringLiteral("zzExampleHomeTitle"));
    auto *description = new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "面向 Qt 6.8+ 的高性能跨平台应用框架与 Fluent Widgets 组件库"),
        brandTextHost);
    description->setWordWrap(true);
    description->setMaximumWidth(760);
    auto *technology = new QLabel(
        QStringLiteral("Qt 6 | C++20 | Linux | Windows | macOS"),
        brandTextHost);
    technology->setTextInteractionFlags(Qt::TextSelectableByMouse);
    brandTextLayout->addWidget(category);
    brandTextLayout->addWidget(brand);
    brandTextLayout->addWidget(description);
    brandTextLayout->addStretch(1);
    brandTextLayout->addWidget(technology);
    layout->addWidget(brandBand);

    auto *summary = new QWidget(content);
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(4, 4, 4, 4);
    summaryLayout->setSpacing(16);
    for (const QString &metric : {
             QCoreApplication::translate("ZzPureToolsExample", "12 个集成页面"),
             QCoreApplication::translate("ZzPureToolsExample", "4 档主题与对比度"),
             QCoreApplication::translate("ZzPureToolsExample", "3 个桌面平台"),
             QCoreApplication::translate("ZzPureToolsExample", "共享/静态双构建")}) {
        auto *label = new QLabel(metric, summary);
        label->setAlignment(Qt::AlignCenter);
        summaryLayout->addWidget(label, 1);
    }
    layout->addWidget(summary);

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "快捷入口"), content);
    auto *quickHost = new QWidget(content);
    auto *quickLayout = new ZzFluentUI::ZzFlowLayout(12, 12, quickHost);
    quickLayout->setContentsMargins(0, 0, 0, 0);

    const auto addRouteCard = [this, quickHost, quickLayout](
                                  const QString &routeId,
                                  const QString &cardTitle,
                                  const QString &cardDescription,
                                  QStyle::StandardPixmap icon) {
        auto *card = new ZzFluentUI::ZzActionCard(
            cardTitle, cardDescription, quickHost);
        card->setObjectName(
            QStringLiteral("zzExampleRouteCard_%1").arg(routeId));
        card->setIcon(card->style()->standardIcon(icon));
        card->setMinimumSize(250, 84);
        card->setMaximumHeight(96);
        QObject::connect(
            card,
            &QAbstractButton::clicked,
            q_ptr,
            [this, routeId] {
                Q_EMIT q_ptr->routeRequested(routeId);
            });
        quickLayout->addWidget(card);
    };
    addRouteCard(
        QStringLiteral("controls"),
        QCoreApplication::translate("ZzPureToolsExample", "基础控件"),
        QCoreApplication::translate("ZzPureToolsExample", "按钮、输入、选择和进度"),
        QStyle::SP_FileDialogDetailedView);
    addRouteCard(
        QStringLiteral("cards"),
        QCoreApplication::translate("ZzPureToolsExample", "卡片与媒体"),
        QCoreApplication::translate("ZzPureToolsExample", "操作卡片、图片和轮播"),
        QStyle::SP_FileDialogContentsView);
    addRouteCard(
        QStringLiteral("list-view"),
        QCoreApplication::translate("ZzPureToolsExample", "数据视图"),
        QCoreApplication::translate("ZzPureToolsExample", "列表、表格和层级模型"),
        QStyle::SP_FileDialogListView);
    addRouteCard(
        QStringLiteral("platform"),
        QCoreApplication::translate("ZzPureToolsExample", "窗口与平台"),
        QCoreApplication::translate("ZzPureToolsExample", "DPI、屏幕和 WindowKit 能力"),
        QStyle::SP_ComputerIcon);
    layout->addWidget(quickHost);

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "最近状态"), content);
    auto *statusView = new QTreeWidget(content);
    statusView->setObjectName(QStringLiteral("zzExampleHomeStatusView"));
    statusView->setColumnCount(3);
    statusView->setHeaderLabels({
        QCoreApplication::translate("ZzPureToolsExample", "组件"),
        QCoreApplication::translate("ZzPureToolsExample", "状态"),
        QCoreApplication::translate("ZzPureToolsExample", "说明")});
    statusView->setRootIsDecorated(false);
    statusView->setAlternatingRowColors(true);
    statusView->setMinimumHeight(150);
    const std::array<std::array<QString, 3>, 3> rows{{
        {QStringLiteral("ZzWindowKit"), QCoreApplication::translate("ZzPureToolsExample", "就绪"),
         QCoreApplication::translate("ZzPureToolsExample", "逐窗口无边框代理")},
        {QStringLiteral("ZzFluentUI"), QCoreApplication::translate("ZzPureToolsExample", "就绪"),
         QCoreApplication::translate("ZzPureToolsExample", "应用级主题快照")},
        {QStringLiteral("ZzPureTools"), QCoreApplication::translate("ZzPureToolsExample", "就绪"),
         QCoreApplication::translate("ZzPureToolsExample", "路由与页面生命周期")},
    }};
    for (const auto &row : rows) {
        auto *item = new QTreeWidgetItem(statusView);
        for (int column = 0; column < 3; ++column) {
            item->setText(column, row.at(static_cast<std::size_t>(column)));
        }
    }
    statusView->resizeColumnToContents(0);
    statusView->resizeColumnToContents(1);
    layout->addWidget(statusView);
    layout->addStretch(1);

    q_ptr->setAccessibleName(title);
}

void ZzExampleGalleryPagePrivate::buildControls(const QString &title)
{
    q_ptr->setObjectName(QStringLiteral("zzExampleControlsPage"));
    auto [content, layout] = zzPageContent(q_ptr);
    layout->addWidget(zzPageTitle(title, content));
    auto *description = new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "常用输入与状态控件在统一主题、键盘和无障碍语义下协同工作"),
        content);
    description->setWordWrap(true);
    layout->addWidget(description);

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "选中指示条对比"), content);
    layout->addWidget(zzSelectionIndicatorPreview(content));

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "命令与状态"), content);
    auto *commandRow = new QHBoxLayout;
    commandRow->setSpacing(10);
    auto *standardButton = new ZzFluentUI::ZzPushButton(
        QCoreApplication::translate("ZzPureToolsExample", "标准"), content);
    auto *accentButton = new ZzFluentUI::ZzPushButton(
        QCoreApplication::translate("ZzPureToolsExample", "主要操作"), content);
    accentButton->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
    auto *subtleButton = new ZzFluentUI::ZzPushButton(
        QCoreApplication::translate("ZzPureToolsExample", "次要操作"), content);
    subtleButton->setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
    auto *disabledButton = new ZzFluentUI::ZzPushButton(
        QCoreApplication::translate("ZzPureToolsExample", "不可用"), content);
    disabledButton->setEnabled(false);
    auto *checkableButton = new ZzFluentUI::ZzPushButton(
        QCoreApplication::translate("ZzPureToolsExample", "保持预览"),
        content);
    checkableButton->setObjectName(
        QStringLiteral("zzExampleCheckableButton"));
    checkableButton->setAccessibleName(
        QCoreApplication::translate("ZzPureToolsExample", "保持预览"));
    checkableButton->setCheckable(true);
    checkableButton->setChecked(true);
    checkableButton->setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
    auto *enabledSwitch = new ZzFluentUI::ZzToggleSwitch(
        QCoreApplication::translate("ZzPureToolsExample", "启用标准按钮"), content);
    enabledSwitch->setChecked(true);
    commandRow->addWidget(standardButton);
    commandRow->addWidget(accentButton);
    commandRow->addWidget(subtleButton);
    commandRow->addWidget(disabledButton);
    commandRow->addWidget(checkableButton);
    commandRow->addStretch(1);
    commandRow->addWidget(enabledSwitch);
    layout->addLayout(commandRow);

    auto *message = new ZzFluentUI::ZzMessageBar(content);
    message->setObjectName(QStringLiteral("zzExampleControlsMessage"));
    message->setText(QCoreApplication::translate("ZzPureToolsExample", "控件状态已准备"));
    message->setSeverity(ZzFluentUI::ZzMessageSeverity::Information);
    QObject::connect(
        enabledSwitch,
        &QAbstractButton::toggled,
        standardButton,
        &QWidget::setEnabled);
    QObject::connect(
        accentButton,
        &QAbstractButton::clicked,
        message,
        [message] {
            message->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
            message->setText(QCoreApplication::translate("ZzPureToolsExample", "主要操作已执行"));
            message->show();
        });
    QObject::connect(
        checkableButton,
        &QAbstractButton::toggled,
        message,
        [message](bool checked) {
            message->setText(QCoreApplication::translate(
                "ZzPureToolsExample",
                checked ? "预览已保持" : "预览已释放"));
            message->setSeverity(
                checked
                    ? ZzFluentUI::ZzMessageSeverity::Information
                    : ZzFluentUI::ZzMessageSeverity::Warning);
            message->show();
        });
    QObject::connect(
        message,
        &ZzFluentUI::ZzMessageBar::closeRequested,
        message,
        &QWidget::hide);
    auto *feedbackRow = new QHBoxLayout;
    feedbackRow->setSpacing(10);
    feedbackRow->addWidget(message, 1);
    auto *successBadge = new ZzFluentUI::ZzInfoBadge(content);
    successBadge->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
    auto *countBadge = new ZzFluentUI::ZzInfoBadge(content);
    countBadge->setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
    countBadge->setValue(12);
    feedbackRow->addWidget(successBadge);
    feedbackRow->addWidget(countBadge);
    layout->addLayout(feedbackRow);

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "文本与选择"), content);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    auto *nameEdit = new QLineEdit(content);
    nameEdit->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "项目名称"));
    nameEdit->setPlaceholderText(QStringLiteral("ZzPureToolsExample"));
    auto *notesEdit = new QPlainTextEdit(content);
    notesEdit->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "项目说明"));
    notesEdit->setPlaceholderText(QCoreApplication::translate("ZzPureToolsExample", "项目说明"));
    notesEdit->setMaximumHeight(88);
    auto *environment = new QComboBox(content);
    environment->addItems({
        QStringLiteral("Linux Desktop"),
        QStringLiteral("Windows Desktop"),
        QStringLiteral("macOS Desktop")});
    auto *scopes = new ZzFluentUI::ZzMultiSelectComboBox(content);
    scopes->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "构建范围"));
    scopes->setPlaceholderText(QCoreApplication::translate("ZzPureToolsExample", "选择构建范围"));
    scopes->setOptions({
        {QStringLiteral("shared"), QCoreApplication::translate("ZzPureToolsExample", "共享库"), {}, {}, true, true},
        {QStringLiteral("static"), QCoreApplication::translate("ZzPureToolsExample", "静态库"), {}, {}, true, true},
        {QStringLiteral("tests"), QCoreApplication::translate("ZzPureToolsExample", "测试"), {}, {}, true, false},
        {QStringLiteral("legacy"), QCoreApplication::translate("ZzPureToolsExample", "旧版兼容"), {}, {}, false, false}});
    auto *check = new QCheckBox(QCoreApplication::translate("ZzPureToolsExample", "启用严格警告"), content);
    check->setChecked(true);
    auto *radioHost = new QWidget(content);
    auto *radioLayout = new QHBoxLayout(radioHost);
    radioLayout->setContentsMargins(0, 0, 0, 0);
    auto *balanced = new QRadioButton(QCoreApplication::translate("ZzPureToolsExample", "均衡"), radioHost);
    auto *performance = new QRadioButton(QCoreApplication::translate("ZzPureToolsExample", "性能优先"), radioHost);
    balanced->setChecked(true);
    radioLayout->addWidget(balanced);
    radioLayout->addWidget(performance);
    radioLayout->addStretch(1);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "名称"), nameEdit);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "说明"), notesEdit);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "平台"), environment);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "范围"), scopes);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "诊断"), check);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "策略"), radioHost);
    layout->addLayout(form);

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "数值、日期与滚轮"), content);
    auto *valueForm = new QFormLayout;
    valueForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    valueForm->setHorizontalSpacing(18);
    valueForm->setVerticalSpacing(12);
    auto *workers = new ZzFluentUI::ZzSpinBox(content);
    workers->setRange(1, 64);
    workers->setValue(8);
    workers->setSuffix(QCoreApplication::translate("ZzPureToolsExample", " 线程"));
    auto *budget = new ZzFluentUI::ZzDoubleSpinBox(content);
    budget->setRange(0.1, 50.0);
    budget->setDecimals(1);
    budget->setSingleStep(0.5);
    budget->setValue(8.0);
    budget->setSuffix(QStringLiteral(" MiB"));
    auto *date = new ZzFluentUI::ZzCalendarPicker(content);
    date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    date->setDateRange(QDate(2026, 1, 1), QDate(2035, 12, 31));
    date->setDate(QDate(2026, 8, 6));
    auto *time = new ZzFluentUI::ZzRollerPicker(content);
    time->setColumns({
        {QStringLiteral("hour"),
         {QStringLiteral("08"), QStringLiteral("09"), QStringLiteral("10"),
          QStringLiteral("11"), QStringLiteral("12")},
         1, true, 88},
        {QStringLiteral("minute"),
         {QStringLiteral("00"), QStringLiteral("15"), QStringLiteral("30"),
          QStringLiteral("45")},
         2, true, 88}});
    valueForm->addRow(QCoreApplication::translate("ZzPureToolsExample", "并发任务"), workers);
    valueForm->addRow(QCoreApplication::translate("ZzPureToolsExample", "资源预算"), budget);
    valueForm->addRow(QCoreApplication::translate("ZzPureToolsExample", "计划日期"), date);
    valueForm->addRow(QCoreApplication::translate("ZzPureToolsExample", "计划时间"), time);
    layout->addLayout(valueForm);

    zzAddSection(layout, QCoreApplication::translate("ZzPureToolsExample", "进度"), content);
    auto *progressRow = new QHBoxLayout;
    progressRow->setSpacing(12);
    auto *slider = new QSlider(Qt::Horizontal, content);
    slider->setRange(0, 100);
    slider->setValue(68);
    slider->setMinimumWidth(220);
    auto *progress = new QProgressBar(content);
    progress->setRange(0, 100);
    progress->setValue(68);
    progress->setMinimumWidth(220);
    auto *ring = new ZzFluentUI::ZzProgressRing(content);
    ring->setValue(68);
    auto *busyRing = new ZzFluentUI::ZzProgressRing(content);
    busyRing->setTextVisible(false);
    busyRing->setRange(0, 0);
    progressRow->addWidget(slider, 1);
    progressRow->addWidget(progress, 1);
    progressRow->addWidget(ring);
    progressRow->addWidget(busyRing);
    layout->addLayout(progressRow);
    QObject::connect(
        slider,
        &QSlider::valueChanged,
        progress,
        &QProgressBar::setValue);
    QObject::connect(
        slider,
        &QSlider::valueChanged,
        ring,
        &QProgressBar::setValue);
    layout->addStretch(1);

    q_ptr->setAccessibleName(title);
}

} // namespace ZzExample
