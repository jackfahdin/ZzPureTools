#include "ZzExampleShowcasePagePrivate.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzExample {

void ZzExampleShowcasePagePrivate::buildTabBars(QVBoxLayout *layout, QWidget *parent)
{
    using namespace ZzFluentUI;
    using Appearance = ZzTabBarAppearance;
    const auto tr = [](const char *text) { return QCoreApplication::translate("ZzPureToolsExample", text); };
    auto *description = new QLabel(tr("点击标签体验不同的选中动效；也可使用方向键切换。"), parent);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *rtl = new QCheckBox(QStringLiteral("RTL"), parent);
    layout->addWidget(rtl);
    QObject::connect(rtl, &QCheckBox::toggled, parent, [parent](bool enabled) {
        parent->setLayoutDirection(enabled ? Qt::RightToLeft : Qt::LeftToRight);
    });
    const QStringList names {tr("概览"), tr("文件"), tr("历史"), tr("设置")};
    const QList<QStyle::StandardPixmap> icons {QStyle::SP_ComputerIcon, QStyle::SP_DirIcon,
        QStyle::SP_FileDialogDetailedView, QStyle::SP_FileDialogContentsView};
    const auto addBar = [&](const QString &title, Appearance appearance, bool iconOnly = false) {
        layout->addWidget(new QLabel(title, parent));
        auto *bar = new ZzTabBar(parent);
        bar->setObjectName(QStringLiteral("zzExampleTabBar%1%2").arg(static_cast<int>(appearance)).arg(iconOnly ? "Icons" : ""));
        bar->setAppearance(appearance);
        bar->setAccessibleName(title);
        bar->setExpanding(false);
        for (int i = 0; i < names.size(); ++i) {
            bar->addTab(bar->style()->standardIcon(icons[i]), iconOnly ? QString() : names[i]);
            bar->setTabToolTip(i, names[i]);
            bar->setAccessibleTabName(i, names[i]);
        }
        layout->addWidget(bar);
        return bar;
    };
    addBar(QStringLiteral("Pivot Grow"), Appearance::PivotGrow);
    addBar(QStringLiteral("Pivot Slide"), Appearance::PivotSlide);
    addBar(QStringLiteral("Pivot Stretch"), Appearance::PivotStretch);
    addBar(QStringLiteral("Pill"), Appearance::Pill);
    addBar(QStringLiteral("Segmented Slide"), Appearance::SegmentedSlide);
    addBar(QStringLiteral("Segmented Fade"), Appearance::SegmentedFade);
    addBar(QStringLiteral("Segmented WinUI3"), Appearance::SegmentedWinUI3);
    addBar(QStringLiteral("Segmented WinUI3 · Icons"), Appearance::SegmentedWinUI3, true);
    auto *custom = addBar(tr("自定义分段标签"), Appearance::SegmentedSlide);
    custom->setObjectName(QStringLiteral("zzExampleCustomTabBar"));
    custom->setSegmentedRounded(true);
    custom->setExpanding(true);
    custom->setMaximumWidth(560);
    custom->setSegmentedColors(
        {.background = QColor("#d9d9dd"), .selected = QColor("#7e57e8"),
         .hover = QColor("#e6e6ea"), .pressed = QColor("#d0d0d4"), .text = {}, .selectedText = {}},
        {.background = QColor("#3f3f46"), .selected = QColor("#6e4fd6"),
         .hover = QColor("#4a4a52"), .pressed = QColor("#55555d"), .text = {}, .selectedText = {}});

    layout->addWidget(new QLabel(QStringLiteral("Capsule"), parent));
    auto *documents = new ZzTabWidget(parent);
    documents->setMinimumHeight(160);
    documents->setTabsClosable(true);
    documents->fluentTabBar()->setAppearance(Appearance::Capsule);
    for (const auto &name : names) {
        auto *body = new QLabel(name, documents);
        body->setAlignment(Qt::AlignCenter);
        documents->addTab(body, name);
    }
    QObject::connect(documents, &ZzTabWidget::tabsCloseRequested, documents,
        [documents](const QList<QWidget *> &pages) {
            for (auto *page : pages) { documents->removeTab(documents->indexOf(page)); page->deleteLater(); }
        });
    QObject::connect(documents, &ZzTabWidget::newTabRequested, documents, [documents, tr] {
        auto *body = new QLabel(tr("新建标签页"), documents);
        body->setAlignment(Qt::AlignCenter);
        documents->setCurrentIndex(documents->addTab(body, body->text()));
    });
    layout->addWidget(documents);

    layout->addWidget(new QLabel(QStringLiteral("Navigation"), parent));
    auto *row = new QHBoxLayout;
    auto *navigation = new ZzTabBar(parent);
    navigation->setObjectName(QStringLiteral("zzExampleNavigationTabBar"));
    navigation->setAppearance(Appearance::Navigation);
    navigation->setShape(QTabBar::RoundedWest);
    navigation->setExpanding(false);
    auto *pages = new QStackedWidget(parent);
    for (int i = 0; i < names.size(); ++i) {
        navigation->addTab(navigation->style()->standardIcon(icons[i]), names[i]);
        auto *body = new QLabel(names[i], pages);
        body->setAlignment(Qt::AlignCenter);
        pages->addWidget(body);
    }
    QObject::connect(navigation, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
    row->addWidget(navigation, 0, Qt::AlignTop);
    row->addWidget(pages, 1);
    layout->addLayout(row);
}

} // namespace ZzExample
