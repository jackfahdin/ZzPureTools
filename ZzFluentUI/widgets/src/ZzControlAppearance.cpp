#include <ZzFluentUI/ZzControlAppearance.h>

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzSplitButton.h>

#include "private/ZzControlAppearancePrivate.h"

namespace ZzFluentUI {

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
