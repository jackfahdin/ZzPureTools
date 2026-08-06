#include "ZzExampleDataPagePrivate.h"

#include <QtCore/QAbstractItemModel>
#include <QtGui/QFont>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzScrollArea.h>

#include "ZzExampleDataPage.h"

namespace ZzExample {

namespace {

/** @brief 创建数据页共用的可换行页面标题。 */
[[nodiscard]] QLabel *zzDataPageTitle(
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

} // namespace

ZzExampleDataPagePrivate::ZzExampleDataPagePrivate(
    ZzExampleDataPage *page)
    : q_ptr(page)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzExampleDataPagePrivate::initialize(
    ZzExampleDataPageKind kind,
    const QString &title,
    QAbstractItemModel *model)
{
    Q_ASSERT(model != nullptr);
    q_ptr->setObjectName(QStringLiteral("zzExampleDataPage"));
    q_ptr->setAccessibleName(title);
    auto *rootLayout = new QVBoxLayout(q_ptr);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    auto *scrollArea = new ZzFluentUI::ZzScrollArea(q_ptr);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 28, 32, 32);
    layout->setSpacing(12);
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    layout->addWidget(zzDataPageTitle(title, content));
    auto *description = new QLabel(
        QStringLiteral("有界模型、代理筛选和局部 delegate 绘制共同保持稳定响应"),
        content);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *commands = new QHBoxLayout;
    commands->setSpacing(10);
    auto *filter = new QLineEdit(content);
    filter->setObjectName(QStringLiteral("zzExampleDataFilter"));
    filter->setAccessibleName(QStringLiteral("筛选数据"));
    filter->setPlaceholderText(QStringLiteral("筛选当前数据"));
    filter->setClearButtonEnabled(true);
    auto *append = new ZzFluentUI::ZzPushButton(
        QStringLiteral("追加记录"), content);
    append->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
    append->setIcon(append->style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    auto *reset = new ZzFluentUI::ZzPushButton(
        QStringLiteral("恢复数据"), content);
    reset->setIcon(reset->style()->standardIcon(QStyle::SP_BrowserReload));
    commands->addWidget(filter, 1);
    commands->addWidget(append);
    commands->addWidget(reset);
    layout->addLayout(commands);
    QObject::connect(
        filter,
        &QLineEdit::textChanged,
        q_ptr,
        &ZzExampleDataPage::filterRequested);
    QObject::connect(
        append,
        &QAbstractButton::clicked,
        q_ptr,
        &ZzExampleDataPage::appendRequested);
    QObject::connect(
        reset,
        &QAbstractButton::clicked,
        q_ptr,
        &ZzExampleDataPage::resetRequested);

    switch (kind) {
    case ZzExampleDataPageKind::List:
        q_ptr->setObjectName(QStringLiteral("zzExampleListPage"));
        buildList(layout, model, content);
        break;
    case ZzExampleDataPageKind::Table:
        q_ptr->setObjectName(QStringLiteral("zzExampleTablePage"));
        buildTable(layout, model, content);
        break;
    case ZzExampleDataPageKind::Tree:
        q_ptr->setObjectName(QStringLiteral("zzExampleTreePage"));
        buildTree(layout, model, content);
        break;
    }

    statusLabel = new QLabel(content);
    statusLabel->setObjectName(QStringLiteral("zzExampleDataStatus"));
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(statusLabel);
    layout->addStretch(1);
}

void ZzExampleDataPagePrivate::setStatusText(const QString &text)
{
    if (statusLabel != nullptr) {
        statusLabel->setText(text);
    }
}

void ZzExampleDataPagePrivate::buildList(
    QVBoxLayout *layout,
    QAbstractItemModel *model,
    QWidget *parent)
{
    auto *view = new QListView(parent);
    view->setObjectName(QStringLiteral("zzExampleListView"));
    view->setAccessibleName(QStringLiteral("构建任务列表"));
    view->setAlternatingRowColors(true);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setUniformItemSizes(true);
    view->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(view));
    view->setModel(model);
    view->setMinimumHeight(440);
    layout->addWidget(view);
}

void ZzExampleDataPagePrivate::buildTable(
    QVBoxLayout *layout,
    QAbstractItemModel *model,
    QWidget *parent)
{
    auto *view = new QTableView(parent);
    view->setObjectName(QStringLiteral("zzExampleTableView"));
    view->setAccessibleName(QStringLiteral("跨平台构建表格"));
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSortingEnabled(true);
    view->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(view));
    view->setModel(model);
    view->verticalHeader()->hide();
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->horizontalHeader()->setStretchLastSection(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->setMinimumHeight(440);
    layout->addWidget(view);
}

void ZzExampleDataPagePrivate::buildTree(
    QVBoxLayout *layout,
    QAbstractItemModel *model,
    QWidget *parent)
{
    auto *view = new QTreeView(parent);
    view->setObjectName(QStringLiteral("zzExampleTreeView"));
    view->setAccessibleName(QStringLiteral("构建工作区树"));
    view->setAlternatingRowColors(true);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setUniformRowHeights(true);
    view->setAnimated(true);
    view->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(view));
    view->setModel(model);
    view->header()->setStretchLastSection(true);
    view->expandToDepth(1);
    view->setMinimumHeight(440);
    layout->addWidget(view);
}

} // namespace ZzExample
