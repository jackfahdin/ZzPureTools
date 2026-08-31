#include "ZzExampleSystemPagePrivate.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QSignalBlocker>
#include <QtGui/QFont>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

#include "ZzExampleSystemPage.h"

namespace ZzExample {

namespace {

/** @brief 创建系统页共用的可换行标题。 */
[[nodiscard]] QLabel *zzSystemPageTitle(
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

ZzExampleSystemPagePrivate::ZzExampleSystemPagePrivate(
    ZzExampleSystemPage *page)
    : q_ptr(page)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzExampleSystemPagePrivate::initialize(
    ZzExampleSystemPageKind kind,
    const QString &title,
    QAbstractItemModel *model)
{
    Q_ASSERT(model != nullptr);
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
    layout->addWidget(zzSystemPageTitle(title, content));

    switch (kind) {
    case ZzExampleSystemPageKind::Platform:
        q_ptr->setObjectName(QStringLiteral("zzExamplePlatformPage"));
        break;
    case ZzExampleSystemPageKind::Settings:
        q_ptr->setObjectName(QStringLiteral("zzExampleSettingsPage"));
        buildSettings(layout, content);
        break;
    case ZzExampleSystemPageKind::About:
        q_ptr->setObjectName(QStringLiteral("zzExampleAboutPage"));
        break;
    }

    auto *snapshot = new QTableView(content);
    snapshot->setObjectName(QStringLiteral("zzExampleSystemSnapshot"));
    snapshot->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "系统信息快照"));
    snapshot->setModel(model);
    snapshot->setItemDelegate(
        new ZzFluentUI::ZzFluentItemDelegate(snapshot));
    snapshot->setEditTriggers(QAbstractItemView::NoEditTriggers);
    snapshot->setSelectionBehavior(QAbstractItemView::SelectRows);
    snapshot->verticalHeader()->hide();
    snapshot->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Interactive);
    snapshot->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    snapshot->horizontalHeader()->resizeSection(0, 180);
    snapshot->setMinimumHeight(
        kind == ZzExampleSystemPageKind::Settings ? 180 : 420);
    layout->addWidget(snapshot);
    statusLabel = new QLabel(content);
    statusLabel->setObjectName(QStringLiteral("zzExampleSystemStatus"));
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);
    layout->addStretch(1);
}

void ZzExampleSystemPagePrivate::setSettingsSnapshot(
    int themeMode,
    int logLevel,
    bool reducedMotion,
    bool activityDockVisible)
{
    if (themeModeBox == nullptr || logLevelBox == nullptr
        || reducedMotionSwitch == nullptr
        || activityDockSwitch == nullptr) {
        return;
    }
    const QSignalBlocker themeBlocker(themeModeBox);
    const QSignalBlocker logBlocker(logLevelBox);
    const QSignalBlocker motionBlocker(reducedMotionSwitch);
    const QSignalBlocker dockBlocker(activityDockSwitch);
    themeModeBox->setCurrentIndex(themeMode);
    logLevelBox->setCurrentIndex(logLevel);
    reducedMotionSwitch->setChecked(reducedMotion);
    activityDockSwitch->setChecked(activityDockVisible);
}

void ZzExampleSystemPagePrivate::setStatusText(const QString &text)
{
    if (statusLabel != nullptr) {
        statusLabel->setText(text);
    }
}

void ZzExampleSystemPagePrivate::buildSettings(
    QVBoxLayout *layout,
    QWidget *parent)
{
    auto *description = new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "外观、日志与当前窗口状态通过 Presenter 持久化"),
        parent);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    themeModeBox = new QComboBox(parent);
    themeModeBox->addItems({
        QCoreApplication::translate("ZzPureToolsExample", "跟随系统"),
        QCoreApplication::translate("ZzPureToolsExample", "浅色"),
        QCoreApplication::translate("ZzPureToolsExample", "深色"),
        QCoreApplication::translate("ZzPureToolsExample", "高对比度")});
    logLevelBox = new QComboBox(parent);
    logLevelBox->addItems({
        QStringLiteral("Trace"),
        QStringLiteral("Debug"),
        QStringLiteral("Info"),
        QStringLiteral("Warning"),
        QStringLiteral("Error"),
        QStringLiteral("Critical"),
        QStringLiteral("Off")});
    reducedMotionSwitch = new ZzFluentUI::ZzToggleSwitch(parent);
    activityDockSwitch = new ZzFluentUI::ZzToggleSwitch(parent);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "主题模式"), themeModeBox);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "日志等级"), logLevelBox);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "减少动效"), reducedMotionSwitch);
    form->addRow(QCoreApplication::translate("ZzPureToolsExample", "活动 Dock"), activityDockSwitch);
    layout->addLayout(form);
    QObject::connect(
        themeModeBox,
        &QComboBox::currentIndexChanged,
        q_ptr,
        &ZzExampleSystemPage::themeModeRequested);
    QObject::connect(
        logLevelBox,
        &QComboBox::currentIndexChanged,
        q_ptr,
        &ZzExampleSystemPage::logLevelRequested);
    QObject::connect(
        reducedMotionSwitch,
        &QAbstractButton::toggled,
        q_ptr,
        &ZzExampleSystemPage::reducedMotionRequested);
    QObject::connect(
        activityDockSwitch,
        &QAbstractButton::toggled,
        q_ptr,
        &ZzExampleSystemPage::activityDockVisibilityRequested);
}

} // namespace ZzExample
