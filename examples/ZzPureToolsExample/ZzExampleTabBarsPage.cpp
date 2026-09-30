#include "ZzExampleShowcasePagePrivate.h"

#include <array>
#include <QtCore/QCoreApplication>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace ZzExample {
namespace {
using ZzFluentUI::ZzSegoeIcon;
using ZzFluentUI::ZzSegoeIconFont;
using Appearance = ZzFluentUI::ZzTabBarAppearance;

QString trTab(const char *text)
{
    return QCoreApplication::translate("ZzPureToolsExample", text);
}

void addHeading(QVBoxLayout *layout, QWidget *parent,
    const char *title, const char *description = nullptr)
{
    auto *heading = new QLabel(trTab(title), parent);
    QFont font = heading->font();
    font.setPixelSize(14);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    if (description) {
        auto *detail = new QLabel(trTab(description), parent);
        detail->setWordWrap(true);
        detail->setStyleSheet(QStringLiteral("color: gray; font-size: 12px;"));
        layout->addWidget(detail);
    }
}

// 原示例以 isCard 属性交给样式绘制；本示例局部复刻其卡片表面。
class ZzGalleryCard final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        auto *style = qobject_cast<ZzFluentUI::ZzFluentStyle *>(QApplication::style());
        const auto snapshot = style ? style->themeSnapshot() : nullptr;
        const bool highContrast = snapshot
            && snapshot->mode() == ZzFluentUI::ZzThemeMode::HighContrast;
        const bool dark = snapshot
            ? snapshot->mode() == ZzFluentUI::ZzThemeMode::Dark
            : palette().color(QPalette::Window).lightness() < 128;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(highContrast ? palette().color(QPalette::ButtonText)
            : dark ? QColor(0, 0, 0, 25) : QColor(0, 0, 0, 15), 1));
        painter.setBrush(highContrast ? palette().color(QPalette::Base)
            : dark ? QColor(255, 255, 255, 13) : QColor(255, 255, 255, 179));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
            4, 4);
    }
};

QVBoxLayout *addCard(QVBoxLayout *outer, QWidget *parent)
{
    auto *card = new ZzGalleryCard(parent);
    card->setProperty("isCard", true);
    auto *inner = new QVBoxLayout(card);
    inner->setContentsMargins(10, 10, 10, 10);
    inner->setSpacing(10);
    outer->addWidget(card);
    return inner;
}

constexpr std::array<const char *, 5> kNames {
    "Home", "Search", "Settings", "Help", "About"};
constexpr std::array<ZzSegoeIcon, 5> kNamesIcons {
    ZzSegoeIcon::Home, ZzSegoeIcon::Search, ZzSegoeIcon::Settings,
    ZzSegoeIcon::Help, ZzSegoeIcon::Info};
constexpr std::array<ZzSegoeIcon, 5> kSegmentedIcons {
    ZzSegoeIcon::CompanionApp, ZzSegoeIcon::PlayerSettings, ZzSegoeIcon::Robot,
    ZzSegoeIcon::RingerSilent, ZzSegoeIcon::TrafficCongestionSolid};

void addFiveTabs(ZzFluentUI::ZzTabBar *bar,
    const std::array<ZzSegoeIcon, 5> *icons = nullptr, bool iconOnly = false,
    int iconPixels = 0)
{
    for (int i = 0; i < int(kNames.size()); ++i) {
        const QString name = trTab(kNames[i]);
        const QString shown = iconOnly ? QString() : name;
        if (icons) bar->addTab(ZzSegoeIconFont::galleryIcon((*icons)[i], iconPixels), shown);
        else bar->addTab(shown);
        bar->setTabToolTip(i, name);
        bar->setAccessibleTabName(i, name);
    }
}

ZzFluentUI::ZzTabBar *makeBar(QWidget *parent, Appearance appearance,
    const char *name)
{
    auto *bar = new ZzFluentUI::ZzTabBar(parent);
    bar->setObjectName(QString::fromLatin1(name));
    bar->setAppearance(appearance);
    bar->setDrawBase(false);
    bar->setExpanding(false);
    return bar;
}

void galleryColors(ZzFluentUI::ZzTabBar *bar, bool purple)
{
    bar->setSegmentedRounded(true);
    bar->setSegmentedColors(
        {.background = QColor("#D9D9DD"),
         .selected = QColor(purple ? "#7E57E8" : "#FFFFFF"),
         .hover = QColor("#E6E6EA"), .pressed = QColor("#D0D0D4"),
         .text = Qt::black, .selectedText = Qt::black},
        {.background = QColor("#3F3F46"),
         .selected = QColor(purple ? "#6E4FD6" : "#5C5C64"),
         .hover = QColor("#4A4A52"), .pressed = QColor("#55555D"),
         .text = Qt::white, .selectedText = Qt::white});
}

class ZzGalleryPageLabel final : public QLabel
{
public:
    using QLabel::QLabel;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QLabel::paintEvent(event);
        QPainter painter(this);
        painter.setPen(QPen(QColor(0, 0, 0, 24), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

QLabel *coloredPage(const QString &name, const QColor &color, QWidget *parent)
{
    auto *page = new ZzGalleryPageLabel(name, parent);
    page->setAlignment(Qt::AlignCenter);
    QPalette palette = page->palette();
    palette.setColor(QPalette::Window, color);
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Text, Qt::black);
    page->setPalette(palette);
    page->setAutoFillBackground(true);
    return page;
}

// 两张页面快照仅在切换期间覆盖堆叠页，快速切换或尺寸变化即清理。
class ZzVerticalPageStack final : public QStackedWidget
{
public:
    explicit ZzVerticalPageStack(QWidget *parent) : QStackedWidget(parent)
    {
        overlay_ = new QWidget(this);
        overlay_->setAutoFillBackground(true);
        overlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
        outgoing_ = new QLabel(overlay_);
        incoming_ = new QLabel(overlay_);
        overlay_->hide();
    }

    void switchTo(int index)
    {
        if (index < 0 || index >= count() || index == currentIndex()) return;
        clear();
        const int previous = currentIndex();
        const QPixmap oldImage = currentWidget()->grab();
        setCurrentIndex(index);
        auto *style = qobject_cast<ZzFluentUI::ZzFluentStyle *>(QApplication::style());
        const bool reduced = style && style->themeSnapshot()->reducedMotion();
        if (reduced || !isVisible() || oldImage.isNull()
            || width() < 1 || height() < 1) return;
        const QPixmap newImage = currentWidget()->grab();
        if (newImage.isNull()) return;

        const int direction = index > previous ? 1 : -1;
        overlay_->setPalette(palette());
        overlay_->setGeometry(rect());
        outgoing_->setPixmap(oldImage);
        incoming_->setPixmap(newImage);
        outgoing_->setGeometry(rect());
        incoming_->setGeometry(0, direction * height(), width(), height());
        overlay_->show();
        overlay_->raise();
        outgoing_->show();
        incoming_->show();

        animation_ = new QParallelAnimationGroup(this);
        for (auto *image : {outgoing_, incoming_}) {
            auto *motion = new QPropertyAnimation(image, "pos", animation_);
            motion->setDuration(220);
            motion->setEasingCurve(QEasingCurve::OutCubic);
            motion->setStartValue(image->pos());
            motion->setEndValue(image == outgoing_
                ? QPoint(0, -direction * height()) : QPoint(0, 0));
            animation_->addAnimation(motion);
        }
        connect(animation_, &QParallelAnimationGroup::finished,
            this, [this] { clear(); });
        animation_->start();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        clear();
        QStackedWidget::resizeEvent(event);
    }
    void hideEvent(QHideEvent *event) override
    {
        clear();
        QStackedWidget::hideEvent(event);
    }

private:
    void clear()
    {
        if (animation_) {
            animation_->stop();
            animation_->deleteLater();
            animation_ = nullptr;
        }
        overlay_->hide();
        outgoing_->clear();
        incoming_->clear();
    }
    QWidget *overlay_ = nullptr;
    QLabel *outgoing_ = nullptr;
    QLabel *incoming_ = nullptr;
    QParallelAnimationGroup *animation_ = nullptr;
};
} // namespace

void ZzExampleShowcasePagePrivate::buildTabBars(QVBoxLayout *layout, QWidget *parent)
{
    layout->setSpacing(15);
    auto *rtl = new QCheckBox(QStringLiteral("RTL"), parent);
    layout->addWidget(rtl);
    QObject::connect(rtl, &QCheckBox::toggled, parent, [parent](bool enabled) {
        parent->setLayoutDirection(enabled ? Qt::RightToLeft : Qt::LeftToRight);
    });

    auto *pivot = addCard(layout, parent);
    const auto addPivot = [pivot, parent](const char *title,
                              const char *detail, Appearance style, const char *name) {
        addHeading(pivot, parent, title, detail);
        auto *bar = makeBar(parent, style, name);
        QFont font = bar->font();
        font.setPixelSize(15);
        font.setBold(true);
        bar->setFont(font);
        bar->setIconSize(QSize(22, 22));
        addFiveTabs(bar, &kNamesIcons, false, 22);
        pivot->addWidget(bar);
    };
    addPivot("Pivot Grow TabBar", "特点：选中时会有一个生长动画效果。",
        Appearance::PivotGrow, "zzExamplePivotGrowBar");
    addPivot("Pivot Slide TabBar", "特点：选中时会有一个滑动动画效果。",
        Appearance::PivotSlide, "zzExamplePivotSlideBar");
    addPivot("Pivot Stretch TabBar", "特点：选中时会有一个拉伸动画效果。",
        Appearance::PivotStretch, "zzExamplePivotStretchBar");

    auto *segmented = addCard(layout, parent);
    const auto addSegmented = [segmented, parent](const char *title,
                                  const char *detail, Appearance style, const char *name) {
        addHeading(segmented, parent, title, detail);
        auto *bar = makeBar(parent, style, name);
        addFiveTabs(bar, &kSegmentedIcons);
        segmented->addWidget(bar);
    };
    addSegmented("Segmented Slide TabBar",
        "特点：Segmented风格，选中时会有一个滑动动画效果。",
        Appearance::SegmentedSlide, "zzExampleSegmentedSlideBar");
    addSegmented("Segmented Fade TabBar",
        "特点：选中时会有一个淡入淡出动画效果。",
        Appearance::SegmentedFade, "zzExampleSegmentedFadeBar");
    addSegmented("Segmented WinUI3 TabBar",
        "特点：Segmented风格，WinUI3 的选中指示器效果。",
        Appearance::SegmentedWinUI3, "zzExampleSegmentedWinUI3Bar");
    auto *winIcons = makeBar(parent, Appearance::SegmentedWinUI3,
        "zzExampleSegmentedWinUI3IconsBar");
    addFiveTabs(winIcons, &kSegmentedIcons, true);
    segmented->addWidget(winIcons);

    addHeading(segmented, parent, "Segmented Gallery Style",
        "特点：半圆胶囊 + 自定义背景/选中/悬停/按下色");
    auto *weekly = makeBar(parent, Appearance::SegmentedSlide,
        "zzExampleGalleryWeeklyBar");
    galleryColors(weekly, false);
    weekly->setExpanding(true);
    weekly->setMaximumWidth(450);
    for (const char *name : {"Weekly", "Daily", "Monthly"})
        weekly->addTab(trTab(name));
    weekly->setCurrentIndex(1);
    segmented->addWidget(weekly);
    auto *purple = makeBar(parent, Appearance::SegmentedSlide,
        "zzExampleGalleryPurpleBar");
    galleryColors(purple, true);
    purple->setExpanding(true);
    purple->setMaximumWidth(600);
    for (const char *name : {"Overview", "Stats", "Goals", "History"})
        purple->addTab(trTab(name));
    segmented->addWidget(purple);
    auto *iconGallery = makeBar(parent, Appearance::SegmentedSlide,
        "zzExampleGalleryIconsBar");
    galleryColors(iconGallery, false);
    iconGallery->setMaximumWidth(224);
    constexpr std::array<ZzSegoeIcon, 4> galleryGlyphs {
        ZzSegoeIcon::Camera, ZzSegoeIcon::Video,
        ZzSegoeIcon::MusicInfo, ZzSegoeIcon::Cloud};
    constexpr std::array<const char *, 4> galleryNames {
        "Camera", "Video", "Music", "Cloud"};
    for (int i = 0; i < int(galleryGlyphs.size()); ++i) {
        iconGallery->addTab(ZzSegoeIconFont::galleryIcon(galleryGlyphs[i]), QString());
        iconGallery->setTabToolTip(i, trTab(galleryNames[i]));
        iconGallery->setAccessibleTabName(i, trTab(galleryNames[i]));
    }
    iconGallery->setCurrentIndex(1);
    segmented->addWidget(iconGallery);

    auto *pillCard = addCard(layout, parent);
    addHeading(pillCard, parent, "Pill TabBar");
    auto *pill = makeBar(parent, Appearance::Pill, "zzExamplePillBar");
    pill->setTabsClosable(true);
    addFiveTabs(pill);
    QObject::connect(pill, &QTabBar::tabCloseRequested, pill,
        [pill](int index) { pill->removeTab(index); });
    pillCard->addWidget(pill);

    auto *capsuleCard = addCard(layout, parent);
    addHeading(capsuleCard, parent, "Capsule TabBar", "特点：浏览器标签样式。");
    auto *documents = new ZzFluentUI::ZzTabWidget(parent);
    documents->setObjectName(QStringLiteral("zzExampleCapsuleTabs"));
    documents->setMinimumHeight(200);
    documents->setTabsClosable(true);
    documents->setMovable(true);
    documents->fluentTabBar()->setAppearance(Appearance::Capsule);
    documents->fluentTabBar()->setDrawBase(false);
    documents->fluentTabBar()->setExpanding(false);
    documents->fluentTabBar()->newTabButton()->hide();
    constexpr std::array<const char *, 5> fullNames {
        "Home Page", "Search Page", "Settings Page", "Help Page", "About Page"};
    constexpr std::array<const char *, 5> pageColors {
        "#FFE4E1", "#E0FFFF", "#F0FFF0", "#FFFACD", "#E6E6FA"};
    for (int i = 0; i < int(kNames.size()); ++i) {
        auto *body = coloredPage(trTab(fullNames[i]), QColor(pageColors[i]), documents);
        body->setObjectName(QStringLiteral("zzExampleCapsuleBody%1").arg(i));
        documents->addTab(body, ZzSegoeIconFont::galleryIcon(kNamesIcons[i]),
            trTab(kNames[i]));
    }
    QObject::connect(documents, &ZzFluentUI::ZzTabWidget::tabsCloseRequested,
        documents, [documents](const QList<QWidget *> &pages) {
            for (auto *page : pages) {
                const int index = documents->indexOf(page);
                if (index >= 0) documents->removeTab(index);
                page->deleteLater();
            }
        });
    capsuleCard->addWidget(documents);

    auto *navigationCard = addCard(layout, parent);
    addHeading(navigationCard, parent, "Navigation TabBar",
        "特点：适合用于侧边栏的导航菜单，选项卡垂直排列，选中时指示器有个变长效果");
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);
    auto *navigation = makeBar(parent, Appearance::Navigation,
        "zzExampleNavigationTabBar");
    navigation->setShape(QTabBar::RoundedWest);
    navigation->setProperty("TextAlign", int(Qt::AlignVCenter | Qt::AlignLeft));
    auto *pages = new ZzVerticalPageStack(parent);
    pages->setObjectName(QStringLiteral("zzExampleNavigationPages"));
    pages->setMinimumHeight(300);
    constexpr std::array<const char *, 5> navNames {
        "Overview", "Files", "History", "Insights", "Settings"};
    constexpr std::array<const char *, 5> navFullNames {
        "Overview Page", "Files Page", "History Page", "Insights Page", "Settings Page"};
    constexpr std::array<ZzSegoeIcon, 5> navGlyphs {
        ZzSegoeIcon::CompanionApp, ZzSegoeIcon::Folder, ZzSegoeIcon::History,
        ZzSegoeIcon::Unknown, ZzSegoeIcon::Settings};
    constexpr std::array<const char *, 5> navColors {
        "#F4F8FF", "#F0FBF6", "#FFF8EE", "#F8F3FF", "#F5F5F5"};
    for (int i = 0; i < int(navNames.size()); ++i) {
        navigation->addTab(ZzSegoeIconFont::galleryIcon(navGlyphs[i]),
            trTab(navNames[i]));
        auto *body = coloredPage(trTab(navFullNames[i]), QColor(navColors[i]), pages);
        body->setObjectName(QStringLiteral("zzExampleNavigationBody%1").arg(i));
        body->setMinimumHeight(220);
        pages->addWidget(body);
    }
    QObject::connect(navigation, &QTabBar::currentChanged, pages,
        [pages](int index) { pages->switchTo(index); });
    row->addWidget(navigation, 0, Qt::AlignTop);
    row->addWidget(pages, 1);
    navigationCard->addLayout(row);
}
} // namespace ZzExample
