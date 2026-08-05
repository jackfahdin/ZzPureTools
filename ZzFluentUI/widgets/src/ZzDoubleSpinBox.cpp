#include <ZzFluentUI/ZzDoubleSpinBox.h>

namespace ZzFluentUI {

ZzDoubleSpinBox::ZzDoubleSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
{
    setButtonSymbols(QAbstractSpinBox::PlusMinus);
}

ZzDoubleSpinBox::~ZzDoubleSpinBox() = default;

} // namespace ZzFluentUI
