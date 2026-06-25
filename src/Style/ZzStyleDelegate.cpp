#include "ZzFluent/Style/ZzStyleDelegate.hpp"

#include "ZzFluent/Core/ZzTheme.hpp"

QSize ZzStyleDelegate::sizeHint(const ZzStyleContext& context, const QSize& available) const
{
    const int height = context.theme != nullptr ? context.theme->metrics().controlHeight : 32;
    Q_UNUSED(available)
    return QSize(available.width(), height);
}
