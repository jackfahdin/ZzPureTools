#include "ZzWindowKitDemoWindowPrivate.h"

#include <cstdlib>

#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzResult.h>
#include <ZzWindowKit/ZzWindowChromeConfiguration.h>

#include "ZzWindowKitDemoWindow.h"

namespace {

void zzRequireSuccess(
    const ZzCore::ZzResult<void> &result,
    const char *operation)
{
    if (result) {
        return;
    }
    qFatal(
        "%s: %s",
        operation,
        result.error().technicalMessage().toUtf8().constData());
}

} // namespace

ZzWindowKitDemoWindowPrivate::ZzWindowKitDemoWindowPrivate(
    ZzWindowKitDemoWindow *window)
    : q_ptr(window)
    , agent(std::make_unique<ZzWindowKit::ZzWindowAgent>())
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzWindowKitDemoWindowPrivate::initialize()
{
    q_ptr->setWindowTitle(QStringLiteral("ZzWindowKit"));
    q_ptr->resize(920, 600);

    auto *centralWidget = new QWidget(q_ptr);
    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    titleBar = new QWidget(centralWidget);
    titleBar->setFixedHeight(40);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 4, 0);
    titleLayout->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("ZzWindowKit"), titleBar);
    titleLayout->addWidget(title);
    titleLayout->addStretch();

    minimizeButton = new QPushButton(titleBar);
    maximizeButton = new QPushButton(titleBar);
    closeButton = new QPushButton(titleBar);
    const auto buttonSize = QSize(38, 32);
    for (auto *button : {minimizeButton, maximizeButton, closeButton}) {
        button->setFixedSize(buttonSize);
        button->setFlat(true);
        titleLayout->addWidget(button);
    }
    minimizeButton->setIcon(q_ptr->style()->standardIcon(
        QStyle::SP_TitleBarMinButton));
    maximizeButton->setIcon(q_ptr->style()->standardIcon(
        QStyle::SP_TitleBarMaxButton));
    closeButton->setIcon(q_ptr->style()->standardIcon(
        QStyle::SP_TitleBarCloseButton));
    minimizeButton->setToolTip(QStringLiteral("最小化"));
    maximizeButton->setToolTip(QStringLiteral("最大化或还原"));
    closeButton->setToolTip(QStringLiteral("关闭"));

    auto *content = new QLabel(QStringLiteral("ZzWindowKit"), centralWidget);
    content->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(titleBar);
    rootLayout->addWidget(content, 1);
    q_ptr->setCentralWidget(centralWidget);

    QObject::connect(
        minimizeButton,
        &QPushButton::clicked,
        q_ptr,
        &QWidget::showMinimized);
    QObject::connect(
        maximizeButton,
        &QPushButton::clicked,
        q_ptr,
        [this] {
            q_ptr->isMaximized()
                ? q_ptr->showNormal()
                : q_ptr->showMaximized();
        });
    QObject::connect(
        closeButton,
        &QPushButton::clicked,
        q_ptr,
        &QWidget::close);

    zzRequireSuccess(agent->attach(q_ptr), "attach");
    const ZzWindowKit::ZzWindowChromeConfiguration configuration{
        .titleBar = titleBar,
        .windowIcon = nullptr,
        .minimizeButton = minimizeButton,
        .maximizeButton = maximizeButton,
        .closeButton = closeButton,
        .interactiveWidgets = {}};
    zzRequireSuccess(
        agent->configureChrome(configuration), "configureChrome");
}
