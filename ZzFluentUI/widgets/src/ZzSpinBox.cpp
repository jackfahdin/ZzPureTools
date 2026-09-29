#include <ZzFluentUI/ZzSpinBox.h>
#include "private/ZzSpinBoxPrivate.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>

namespace ZzFluentUI {

ZzSpinBox::ZzSpinBox(QWidget *parent)
    : QSpinBox(parent)
    , d_ptr(std::make_unique<ZzSpinBoxPrivate>())
{
    // Qt 基类构造期间尚不能识别派生布局，完整构造后刷新编辑器几何。
    QEvent refresh(QEvent::StyleChange);
    QCoreApplication::sendEvent(this, &refresh);
}

ZzSpinBox::~ZzSpinBox() = default;

ZzSpinBoxButtonLayout ZzSpinBox::buttonLayout() const noexcept
{
    return d_ptr->buttonLayout;
}

void ZzSpinBox::setButtonLayout(ZzSpinBoxButtonLayout layout)
{
    if (d_ptr->setButtonLayout(this, layout)) {
        Q_EMIT buttonLayoutChanged(layout);
    }
}

} // namespace ZzFluentUI
