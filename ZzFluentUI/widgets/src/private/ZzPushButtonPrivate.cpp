#include "ZzPushButtonPrivate.h"

#include <ZzFluentUI/ZzPushButton.h>

namespace ZzFluentUI {

ZzPushButtonPrivate::ZzPushButtonPrivate(
    ZzPushButton *publicObject) noexcept
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzPushButtonPrivate::initStyleOption(
    QStyleOptionButton *option) const
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(option != nullptr);
    if (q_ptr == nullptr || option == nullptr) {
        return;
    }
    q_ptr->initStyleOption(option);
    if (appearance == ZzButtonAppearance::Accent) {
        option->palette.setColor(
            QPalette::Button,
            option->palette.color(QPalette::Highlight));
        option->palette.setColor(
            QPalette::ButtonText,
            option->palette.color(QPalette::HighlightedText));
    } else if (appearance == ZzButtonAppearance::Subtle) {
        QColor fill = option->palette.color(QPalette::Button);
        fill.setAlpha(0);
        option->palette.setColor(QPalette::Button, fill);
    }
}

} // namespace ZzFluentUI
