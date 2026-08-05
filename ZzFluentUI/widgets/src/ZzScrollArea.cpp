#include <ZzFluentUI/ZzScrollArea.h>

#include <QtWidgets/QFrame>

#include <ZzFluentUI/ZzScrollBar.h>

namespace ZzFluentUI {

ZzScrollArea::ZzScrollArea(QWidget *parent)
    : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBar(new ZzScrollBar(Qt::Horizontal));
    setVerticalScrollBar(new ZzScrollBar(Qt::Vertical));
}

ZzScrollArea::~ZzScrollArea() = default;

ZzScrollBar *ZzScrollArea::fluentHorizontalScrollBar() const noexcept
{
    return qobject_cast<ZzScrollBar *>(horizontalScrollBar());
}

ZzScrollBar *ZzScrollArea::fluentVerticalScrollBar() const noexcept
{
    return qobject_cast<ZzScrollBar *>(verticalScrollBar());
}

} // namespace ZzFluentUI
