#include "ZzExampleShowcasePagePrivate.h"

#include <array>
#include <utility>

#include <QtGui/QAction>
#include <QtGui/QFont>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzSuggestBox.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzExample {

namespace {

/** @brief 创建组件组合页共用的可换行标题。 */
[[nodiscard]] QLabel *zzShowcaseTitle(
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

/** @brief 增加组件组合页分区标题与分隔线。 */
void zzAddShowcaseSection(
    QVBoxLayout *layout,
    const QString &text,
    QWidget *parent)
{
    layout->addSpacing(12);
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() + 3.0);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    layout->addWidget(label);
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    layout->addWidget(separator);
}

/** @brief 创建居中显示标签名的标签页内容。 */
[[nodiscard]] QWidget *zzTabPage(
    const QString &text,
    QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    auto *label = new QLabel(text, page);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    return page;
}

} // namespace

ZzExampleShowcasePagePrivate::ZzExampleShowcasePagePrivate(
    ZzExampleShowcasePage *page)
    : q_ptr(page)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzExampleShowcasePagePrivate::initialize(
    ZzExampleShowcasePage::ZzPageKind kind,
    const QString &title)
{
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
    layout->addWidget(zzShowcaseTitle(title, content));

    switch (kind) {
    case ZzExampleShowcasePage::ZzPageKind::Navigation:
        q_ptr->setObjectName(QStringLiteral("zzExampleNavigationPage"));
        buildNavigation(layout, content);
        break;
    case ZzExampleShowcasePage::ZzPageKind::Feedback:
        q_ptr->setObjectName(QStringLiteral("zzExampleFeedbackPage"));
        buildFeedback(layout, content);
        break;
    case ZzExampleShowcasePage::ZzPageKind::Icons:
        q_ptr->setObjectName(QStringLiteral("zzExampleIconsPage"));
        buildIcons(layout, content);
        break;
    }
    layout->addStretch(1);
}

void ZzExampleShowcasePagePrivate::buildNavigation(
    QVBoxLayout *layout,
    QWidget *parent)
{
    auto *description = new QLabel(
        QStringLiteral("面包屑、标签容器和窗口级双向历史共同组成导航体验"),
        parent);
    description->setWordWrap(true);
    layout->addWidget(description);

    zzAddShowcaseSection(layout, QStringLiteral("面包屑"), parent);
    auto *breadcrumb = new ZzFluentUI::ZzBreadcrumbBar(parent);
    breadcrumb->setItems({
        QStringLiteral("工作区"),
        QStringLiteral("交互"),
        QStringLiteral("导航与历史")});
    breadcrumb->setCurrentIndex(2);
    auto *breadcrumbStatus = new QLabel(
        QStringLiteral("当前位置：导航与历史"), parent);
    QObject::connect(
        breadcrumb,
        &ZzFluentUI::ZzBreadcrumbBar::indexRequested,
        breadcrumbStatus,
        [breadcrumb, breadcrumbStatus](int index) {
            breadcrumb->setCurrentIndex(index);
            const QStringList items = breadcrumb->items();
            if (index >= 0 && index < items.size()) {
                breadcrumbStatus->setText(
                    QStringLiteral("当前位置：%1").arg(items.at(index)));
            }
        });
    layout->addWidget(breadcrumb);
    layout->addWidget(breadcrumbStatus);

    zzAddShowcaseSection(layout, QStringLiteral("标签转移"), parent);
    auto *primaryTabs = new ZzFluentUI::ZzTabWidget(parent);
    auto *secondaryTabs = new ZzFluentUI::ZzTabWidget(parent);
    primaryTabs->setObjectName(QStringLiteral("zzExamplePrimaryTabs"));
    secondaryTabs->setObjectName(QStringLiteral("zzExampleSecondaryTabs"));
    primaryTabs->setMovable(true);
    secondaryTabs->setMovable(true);
    for (const QString &text : {
             QStringLiteral("概览"),
             QStringLiteral("性能"),
             QStringLiteral("发布")}) {
        primaryTabs->addTab(zzTabPage(text, primaryTabs), text);
    }
    secondaryTabs->addTab(
        zzTabPage(QStringLiteral("活动"), secondaryTabs),
        QStringLiteral("活动"));
    primaryTabs->setMinimumHeight(170);
    secondaryTabs->setMinimumHeight(170);
    auto *transferRow = new QHBoxLayout;
    auto *moveDown = new ZzFluentUI::ZzPushButton(
        QStringLiteral("移到下方"), parent);
    moveDown->setIcon(
        moveDown->style()->standardIcon(QStyle::SP_ArrowDown));
    auto *moveUp = new ZzFluentUI::ZzPushButton(
        QStringLiteral("移到上方"), parent);
    moveUp->setIcon(moveUp->style()->standardIcon(QStyle::SP_ArrowUp));
    transferRow->addWidget(moveDown);
    transferRow->addWidget(moveUp);
    transferRow->addStretch(1);
    QObject::connect(
        moveDown,
        &QAbstractButton::clicked,
        primaryTabs,
        [primaryTabs, secondaryTabs] {
            if (primaryTabs->currentIndex() >= 0) {
                static_cast<void>(primaryTabs->transferTabTo(
                    secondaryTabs, primaryTabs->currentIndex()));
            }
        });
    QObject::connect(
        moveUp,
        &QAbstractButton::clicked,
        secondaryTabs,
        [primaryTabs, secondaryTabs] {
            if (secondaryTabs->currentIndex() >= 0) {
                static_cast<void>(secondaryTabs->transferTabTo(
                    primaryTabs, secondaryTabs->currentIndex()));
            }
        });
    layout->addWidget(primaryTabs);
    layout->addLayout(transferRow);
    layout->addWidget(secondaryTabs);
}

void ZzExampleShowcasePagePrivate::buildFeedback(
    QVBoxLayout *layout,
    QWidget *parent)
{
    auto *description = new QLabel(
        QStringLiteral("反馈表面保持 Qt 原生输入、焦点、弹出层和窗口所有权语义"),
        parent);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *message = new ZzFluentUI::ZzMessageBar(parent);
    message->setObjectName(QStringLiteral("zzExampleFeedbackMessage"));
    message->setText(QStringLiteral("信息反馈已准备"));
    message->setSeverity(ZzFluentUI::ZzMessageSeverity::Information);
    message->setClosable(false);
    layout->addWidget(message);

    zzAddShowcaseSection(layout, QStringLiteral("命令与弹出层"), parent);
    auto *commandHost = new QWidget(parent);
    auto *commandLayout = new ZzFluentUI::ZzFlowLayout(10, 10, commandHost);
    commandLayout->setContentsMargins(0, 0, 0, 0);
    auto *menuButton = new ZzFluentUI::ZzPushButton(
        QStringLiteral("构建菜单"), commandHost);
    menuButton->setIcon(
        menuButton->style()->standardIcon(QStyle::SP_FileDialogListView));
    auto *menu = new QMenu(menuButton);
    menu->addAction(QStringLiteral("Debug"));
    menu->addAction(QStringLiteral("Release"));
    menu->addSeparator();
    QAction *staticAction = menu->addAction(QStringLiteral("静态链接"));
    staticAction->setCheckable(true);
    menuButton->setMenu(menu);
    auto *dialogButton = new ZzFluentUI::ZzPushButton(
        QStringLiteral("打开对话框"), commandHost);
    dialogButton->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
    dialogButton->setIcon(
        dialogButton->style()->standardIcon(QStyle::SP_DialogHelpButton));
    QObject::connect(
        menu,
        &QMenu::triggered,
        message,
        [message](QAction *action) {
            if (action != nullptr) {
                message->setSeverity(
                    ZzFluentUI::ZzMessageSeverity::Information);
                message->setText(
                    QStringLiteral("已选择：%1").arg(action->text()));
            }
        });
    QObject::connect(
        dialogButton,
        &QAbstractButton::clicked,
        q_ptr,
        [parent] {
            auto *dialog = new QMessageBox(
                QMessageBox::Information,
                QStringLiteral("ZzPureTools"),
                QStringLiteral("当前窗口已完成 Fluent 组件装配。"),
                QMessageBox::Ok,
                parent);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->setModal(false);
            dialog->show();
        });
    commandLayout->addWidget(menuButton);
    commandLayout->addWidget(dialogButton);
    layout->addWidget(commandHost);

    zzAddShowcaseSection(layout, QStringLiteral("消息严重性"), parent);
    auto *severityHost = new QWidget(parent);
    auto *severityLayout = new ZzFluentUI::ZzFlowLayout(10, 10, severityHost);
    severityLayout->setContentsMargins(0, 0, 0, 0);
    const std::array<std::pair<QString, ZzFluentUI::ZzMessageSeverity>, 4>
        severities{{
            {QStringLiteral("信息"), ZzFluentUI::ZzMessageSeverity::Information},
            {QStringLiteral("成功"), ZzFluentUI::ZzMessageSeverity::Success},
            {QStringLiteral("警告"), ZzFluentUI::ZzMessageSeverity::Warning},
            {QStringLiteral("错误"), ZzFluentUI::ZzMessageSeverity::Error},
        }};
    for (const auto &[text, severity] : severities) {
        auto *button = new ZzFluentUI::ZzPushButton(text, severityHost);
        QObject::connect(
            button,
            &QAbstractButton::clicked,
            message,
            [message, text, severity] {
                message->setSeverity(severity);
                message->setText(QStringLiteral("%1反馈示例").arg(text));
            });
        severityLayout->addWidget(button);
    }
    layout->addWidget(severityHost);

    zzAddShowcaseSection(layout, QStringLiteral("搜索建议"), parent);
    auto *suggest = new ZzFluentUI::ZzSuggestBox(parent);
    suggest->setAccessibleName(QStringLiteral("组件搜索建议"));
    suggest->setPlaceholderText(QStringLiteral("搜索组件"));
    suggest->setSuggestions({
        {QStringLiteral("window"), QStringLiteral("ZzWindowKit"), {}, {}, true},
        {QStringLiteral("fluent"), QStringLiteral("ZzFluentUI"), {}, {}, true},
        {QStringLiteral("tools"), QStringLiteral("ZzPureTools"), {}, {}, true},
        {QStringLiteral("core"), QStringLiteral("ZzCore"), {}, {}, true}});
    QObject::connect(
        suggest,
        &ZzFluentUI::ZzSuggestBox::suggestionActivated,
        message,
        [message](const ZzFluentUI::ZzSuggestion &suggestion) {
            message->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
            message->setText(
                QStringLiteral("已选择组件：%1").arg(suggestion.text));
        });
    layout->addWidget(suggest);
}

void ZzExampleShowcasePagePrivate::buildIcons(
    QVBoxLayout *layout,
    QWidget *parent)
{
    auto *description = new QLabel(
        QStringLiteral("跨平台 Qt 标准图标随当前样式、主题与设备像素比解析"),
        parent);
    description->setWordWrap(true);
    layout->addWidget(description);
    zzAddShowcaseSection(layout, QStringLiteral("标准图标"), parent);

    struct ZzIconEntry final
    {
        QStyle::StandardPixmap icon;
        const char *name;
    };
    constexpr std::array<ZzIconEntry, 20> entries{{
        {QStyle::SP_DirHomeIcon, "主页"},
        {QStyle::SP_DirOpenIcon, "打开"},
        {QStyle::SP_FileIcon, "文件"},
        {QStyle::SP_DriveHDIcon, "磁盘"},
        {QStyle::SP_ComputerIcon, "计算机"},
        {QStyle::SP_BrowserReload, "刷新"},
        {QStyle::SP_BrowserStop, "停止"},
        {QStyle::SP_ArrowBack, "返回"},
        {QStyle::SP_ArrowForward, "前进"},
        {QStyle::SP_ArrowUp, "向上"},
        {QStyle::SP_ArrowDown, "向下"},
        {QStyle::SP_DialogOkButton, "确认"},
        {QStyle::SP_DialogCancelButton, "取消"},
        {QStyle::SP_DialogSaveButton, "保存"},
        {QStyle::SP_DialogHelpButton, "帮助"},
        {QStyle::SP_MediaPlay, "播放"},
        {QStyle::SP_MediaPause, "暂停"},
        {QStyle::SP_MediaStop, "媒体停止"},
        {QStyle::SP_MediaVolume, "音量"},
        {QStyle::SP_MediaVolumeMuted, "静音"},
    }};
    auto *iconHost = new QWidget(parent);
    auto *iconLayout = new ZzFluentUI::ZzFlowLayout(10, 10, iconHost);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    for (const auto &entry : entries) {
        const QString name = QString::fromUtf8(entry.name);
        auto *button = new QToolButton(iconHost);
        button->setIcon(button->style()->standardIcon(entry.icon));
        button->setIconSize(QSize(28, 28));
        button->setText(name);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setAccessibleName(name);
        button->setToolTip(name);
        button->setFixedSize(96, 72);
        iconLayout->addWidget(button);
    }
    layout->addWidget(iconHost);
}

} // namespace ZzExample
