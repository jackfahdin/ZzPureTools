#include <ZzFluentUI/ZzControlAppearance.h>

#include <QtCore/QVariant>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QProgressBar>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzSplitButton.h>
#include <ZzFluentUI/ZzTabBar.h>

#include "private/ZzControlAppearancePrivate.h"

namespace ZzFluentUI {

void ZzControlAppearance::setTabBarAppearance(QTabBar *bar, ZzTabBarAppearance appearance)
{
    if (!bar) return;
    if (appearance < ZzTabBarAppearance::Standard || appearance > ZzTabBarAppearance::Navigation)
        appearance = ZzTabBarAppearance::Standard;
    if (tabBarAppearance(bar) == appearance) return;
    bar->setProperty("zzFluentTabBarAppearance", static_cast<int>(appearance));
    // QTabBar 缓存尺寸；StyleChange 通过公开事件刷新，包括已有关闭按钮的位置。
    QEvent change(QEvent::StyleChange);
    QCoreApplication::sendEvent(bar, &change);
    bar->updateGeometry();
    bar->update();
    if (auto *fluent = qobject_cast<ZzTabBar *>(bar)) Q_EMIT fluent->appearanceChanged(appearance);
}

ZzTabBarAppearance ZzControlAppearance::tabBarAppearance(const QTabBar *bar)
{
    const int value = bar ? bar->property("zzFluentTabBarAppearance").toInt() : 0;
    return value >= 0 && value <= static_cast<int>(ZzTabBarAppearance::Navigation)
        ? static_cast<ZzTabBarAppearance>(value) : ZzTabBarAppearance::Standard;
}

void ZzControlAppearance::setTabBarRounded(QTabBar *bar, bool rounded)
{
    if (!bar || isTabBarRounded(bar) == rounded) return;
    bar->setProperty("zzFluentTabBarRounded", rounded);
    bar->update();
    if (auto *fluent = qobject_cast<ZzTabBar *>(bar)) Q_EMIT fluent->segmentedRoundedChanged(rounded);
}

bool ZzControlAppearance::isTabBarRounded(const QTabBar *bar)
{
    return bar && bar->property("zzFluentTabBarRounded").toBool();
}

void ZzControlAppearance::setTabBarColors(QTabBar *bar, const ZzTabBarColors &light, const ZzTabBarColors &dark)
{
    if (!bar) return;
    bar->setProperty("zzFluentTabBarLightColors", QVariant::fromValue(light));
    bar->setProperty("zzFluentTabBarDarkColors", QVariant::fromValue(dark));
    bar->update();
}

ZzTabBarColors ZzControlAppearance::tabBarColors(const QTabBar *bar, bool dark)
{
    return bar ? bar->property(dark ? "zzFluentTabBarDarkColors" : "zzFluentTabBarLightColors")
        .value<ZzTabBarColors>() : ZzTabBarColors {};
}

void ZzControlAppearance::setProgressBarAppearance(
    QProgressBar *progress, ZzProgressBarAppearance appearance)
{
    if (progress == nullptr) {
        return;
    }
    const auto normalized = appearance == ZzProgressBarAppearance::Thick
        ? ZzProgressBarAppearance::Thick : ZzProgressBarAppearance::Thin;
    if (progressBarAppearance(progress) == normalized) {
        return;
    }
    progress->setProperty(zzProgressBarAppearanceProperty, static_cast<int>(normalized));
    progress->update();
}

ZzProgressBarAppearance ZzControlAppearance::progressBarAppearance(const QProgressBar *progress)
{
    return progress != nullptr
            && progress->property(zzProgressBarAppearanceProperty).toInt()
                == static_cast<int>(ZzProgressBarAppearance::Thick)
        ? ZzProgressBarAppearance::Thick : ZzProgressBarAppearance::Thin;
}

void ZzControlAppearance::setButtonAppearance(QAbstractButton *button, ZzButtonAppearance appearance)
{
    if (button == nullptr) {
        return;
    }
    if (auto *split = qobject_cast<ZzSplitButton *>(button)) {
        split->setAppearance(appearance);
    } else if (auto *push = qobject_cast<ZzPushButton *>(button)) {
        push->setAppearance(appearance);
    } else {
        button->setProperty("accent", appearance == ZzButtonAppearance::Accent);
        button->setProperty("zzFluentSubtle", appearance == ZzButtonAppearance::Subtle);
        button->update();
    }
}

ZzButtonAppearance ZzControlAppearance::buttonAppearance(const QAbstractButton *button)
{
    if (const auto *split = qobject_cast<const ZzSplitButton *>(button)) {
        return split->appearance();
    }
    if (const auto *push = qobject_cast<const ZzPushButton *>(button)) {
        return push->appearance();
    }
    if (button != nullptr && button->property("accent").toBool()) {
        return ZzButtonAppearance::Accent;
    }
    return button != nullptr && button->property("zzFluentSubtle").toBool()
        ? ZzButtonAppearance::Subtle : ZzButtonAppearance::Standard;
}

void ZzControlAppearance::setAccentColor(QWidget *widget, const QColor &color)
{
    if (widget == nullptr) {
        return;
    }
    if (!color.isValid()) {
        resetAccentColor(widget);
        return;
    }
    QPalette palette = widget->palette();
    const QColor foreground = ZzControlAppearancePrivate::contrastingText(color);
    for (const auto group : {QPalette::Active, QPalette::Inactive}) {
        palette.setColor(group, QPalette::Accent, color);
        palette.setColor(group, QPalette::Highlight, color);
        palette.setColor(group, QPalette::HighlightedText, foreground);
    }
    widget->setPalette(palette);
}

void ZzControlAppearance::resetAccentColor(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }
    QPalette roles;
    roles.setResolveMask(0);
    for (const auto group : {QPalette::Active, QPalette::Inactive}) {
        for (const auto role : {QPalette::Accent, QPalette::Highlight, QPalette::HighlightedText}) {
            roles.setColor(group, role, Qt::black);
        }
    }
    QPalette palette = widget->palette();
    palette.setResolveMask(palette.resolveMask() & ~roles.resolveMask());
    widget->setPalette(palette);
}

QColor ZzControlAppearance::accentColor(const QWidget *widget)
{
    if (widget == nullptr) {
        return {};
    }
    return ZzControlAppearancePrivate::accent(widget->palette());
}

} // namespace ZzFluentUI
