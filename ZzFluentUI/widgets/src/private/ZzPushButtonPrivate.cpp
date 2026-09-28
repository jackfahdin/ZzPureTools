#include "ZzPushButtonPrivate.h"
#include "ZzControlAppearancePrivate.h"

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
            ZzControlAppearancePrivate::accent(option->palette));
        const QColor text = ZzControlAppearancePrivate::text(option->palette);
        option->palette.setColor(QPalette::Active, QPalette::ButtonText, text);
        option->palette.setColor(QPalette::Inactive, QPalette::ButtonText, text);
    } else if (appearance == ZzButtonAppearance::Subtle) {
        QColor fill = option->palette.color(QPalette::Button);
        fill.setAlpha(0);
        option->palette.setColor(QPalette::Button, fill);
    }
}

} // namespace ZzFluentUI
