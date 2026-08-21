#include "ZzDockPanelPrivate.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzIconButton.h>

namespace ZzFluentUI {

ZzDockPanelPrivate::ZzDockPanelPrivate(ZzDockPanel *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    titleBar = new QWidget(q_ptr);
    titleBar->setObjectName(QStringLiteral("zzDockPanelTitleBar"));
    titleBar->setAccessibleName(ZzDockPanel::tr("停靠面板标题栏"));

    iconWidget = new ZzIconButton(titleBar);
    iconWidget->setObjectName(QStringLiteral("zzDockPanelIcon"));
    iconWidget->setAccessibleName(ZzDockPanel::tr("面板图标"));
    iconWidget->setFocusPolicy(Qt::NoFocus);
    iconWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    titleLabel = new QLabel(titleBar);
    titleLabel->setObjectName(QStringLiteral("zzDockPanelTitle"));
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    floatButton = new ZzIconButton(titleBar);
    floatButton->setObjectName(QStringLiteral("zzDockPanelFloatButton"));
    floatButton->setAccessibleName(ZzDockPanel::tr("浮动面板"));
    floatButton->setToolTip(ZzDockPanel::tr("浮动面板"));

    closeButton = new ZzIconButton(titleBar);
    closeButton->setObjectName(QStringLiteral("zzDockPanelCloseButton"));
    closeButton->setAccessibleName(ZzDockPanel::tr("关闭面板"));
    closeButton->setToolTip(ZzDockPanel::tr("关闭面板"));
    closeButton->setIconDescriptor(ZzIconDescriptor::fromBundledSvg(
        ZzBundledSvgIcon::Close));

    auto *layout = new QHBoxLayout(titleBar);
    layout->setContentsMargins(8, 2, 4, 2);
    layout->setSpacing(4);
    layout->addWidget(iconWidget);
    layout->addWidget(titleLabel, 1);
    layout->addWidget(floatButton);
    layout->addWidget(closeButton);
    q_ptr->setTitleBarWidget(titleBar);

    QObject::connect(
        q_ptr, &QDockWidget::windowTitleChanged,
        titleBar, [this] { refreshPresentation(); });
    QObject::connect(
        q_ptr, &QDockWidget::featuresChanged,
        titleBar, [this] { refreshPresentation(); });
    QObject::connect(
        q_ptr, &QDockWidget::topLevelChanged,
        titleBar, [this] { refreshPresentation(); });
    QObject::connect(
        floatButton, &QToolButton::clicked,
        q_ptr, [this] { toggleFloating(); });
    QObject::connect(
        closeButton, &QToolButton::clicked,
        q_ptr, &QWidget::hide);
    refreshPresentation();
}

void ZzDockPanelPrivate::refreshPresentation()
{
    titleLabel->setText(q_ptr->windowTitle());
    titleLabel->setAccessibleName(q_ptr->windowTitle());
    const QDockWidget::DockWidgetFeatures features = q_ptr->features();
    const bool canFloat = features.testFlag(QDockWidget::DockWidgetFloatable);
    const bool canClose = features.testFlag(QDockWidget::DockWidgetClosable);
    floatButton->setVisible(canFloat);
    closeButton->setVisible(canClose);
    if (q_ptr->isFloating()) {
        floatButton->setAccessibleName(ZzDockPanel::tr("重新停靠面板"));
        floatButton->setToolTip(ZzDockPanel::tr("重新停靠面板"));
        floatButton->setIconDescriptor(ZzIconDescriptor::fromBundledSvg(
            ZzBundledSvgIcon::Restore));
    } else {
        floatButton->setAccessibleName(ZzDockPanel::tr("浮动面板"));
        floatButton->setToolTip(ZzDockPanel::tr("浮动面板"));
        floatButton->setIconDescriptor(ZzIconDescriptor::fromBundledSvg(
            ZzBundledSvgIcon::Maximize));
    }
}

void ZzDockPanelPrivate::toggleFloating()
{
    if (!q_ptr->features().testFlag(QDockWidget::DockWidgetFloatable)) {
        return;
    }
    q_ptr->setFloating(!q_ptr->isFloating());
}

} // namespace ZzFluentUI
