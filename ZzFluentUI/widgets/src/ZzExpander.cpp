#include <ZzFluentUI/ZzExpander.h>

#include <utility>

#include <QtCore/QEvent>

#include "private/ZzExpanderPrivate.h"

namespace ZzFluentUI {

ZzExpander::ZzExpander(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExpanderPrivate>(this))
{
    d_ptr->refreshPresentation();
}

ZzExpander::~ZzExpander() = default;

QString ZzExpander::headerText() const
{
    return d_ptr->headerText;
}

void ZzExpander::setHeaderText(QString text)
{
    if (d_ptr->headerText == text) {
        return;
    }
    d_ptr->headerText = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT headerTextChanged(d_ptr->headerText);
}

bool ZzExpander::isExpanded() const noexcept
{
    return d_ptr->expanded;
}

void ZzExpander::setExpanded(bool expanded)
{
    if (d_ptr->expanded == expanded) {
        return;
    }
    d_ptr->expanded = expanded;
    d_ptr->startTransition();
    d_ptr->refreshPresentation();
    Q_EMIT expandedChanged(expanded);
}

QWidget *ZzExpander::contentWidget() const noexcept
{
    return d_ptr->contentWidget.data();
}

void ZzExpander::setContentWidget(QWidget *widget)
{
    d_ptr->setContentWidget(widget);
}

QWidget *ZzExpander::takeContentWidget()
{
    return d_ptr->takeContentWidget();
}

bool ZzExpander::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (event != nullptr && event->type() == QEvent::LayoutRequest) {
        d_ptr->retargetExpandedHeight();
    }
    return handled;
}

void ZzExpander::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange) {
        d_ptr->refreshTheme();
        return;
    }
    if (event->type() == QEvent::LanguageChange
        || event->type() == QEvent::FontChange
        || event->type() == QEvent::LayoutDirectionChange
        || event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->refreshPresentation();
    }
}

} // namespace ZzFluentUI
