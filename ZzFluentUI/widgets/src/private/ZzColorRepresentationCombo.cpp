#include "ZzColorRepresentationCombo.h"

#include <QtWidgets/QStyleOptionComboBox>
#include <QtWidgets/QStylePainter>
#include <ZzFluentUI/ZzSegoeIconFont.h>

namespace ZzFluentUI {
ZzColorRepresentationCombo::ZzColorRepresentationCombo(QWidget *parent)
    : QComboBox(parent)
{
}

void ZzColorRepresentationCombo::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.subControls = QStyle::SC_ComboBoxFrame;
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
    const QRect arrow = style()->subControlRect(QStyle::CC_ComboBox, &option,
                                                QStyle::SC_ComboBoxArrow, this);
    const auto colorGroup = isEnabled() ? QPalette::Active : QPalette::Disabled;
    ZzSegoeIconFont::icon(ZzSegoeIcon::ChevronDown, palette().color(colorGroup, QPalette::Text))
        .paint(&painter, QStyle::alignedRect(layoutDirection(), Qt::AlignCenter,
                                            QSize(12, 12), arrow));
}
} // namespace ZzFluentUI
