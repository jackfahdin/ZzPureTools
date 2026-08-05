#include <ZzFluentUI/ZzSpinBox.h>

namespace ZzFluentUI {

ZzSpinBox::ZzSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    setButtonSymbols(QAbstractSpinBox::PlusMinus);
}

ZzSpinBox::~ZzSpinBox() = default;

} // namespace ZzFluentUI
