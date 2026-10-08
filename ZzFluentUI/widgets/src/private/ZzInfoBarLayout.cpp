#include "ZzInfoBarLayout.h"

#include <QtCore/QEvent>
#include <QtGui/QFontMetrics>
#include <QtGui/QResizeEvent>
#include <QtGui/QTextLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzInfoBar.h>

namespace ZzFluentUI {
namespace {
    constexpr int MinHeight = 48;
    constexpr int TitleTop = 14;
    constexpr int MessageGap = 12;
    constexpr int ActionGap = 16;
    constexpr int ActionTop = 8;
    constexpr int VerticalTop = 14;
    constexpr int VerticalMessageGap = 4;
    constexpr int VerticalActionGap = 12;
    constexpr int VerticalBottom = 18;

    int itemHeight(QWidget* item, int width)
    {
        if (!item || item->isHidden())
            return 0;
        return item->hasHeightForWidth() ? item->heightForWidth(qMax(0, width))
                                         : item->sizeHint().height();
    }
} // namespace

ZzInfoBarLayout::ZzInfoBarLayout(QWidget* parent)
    : QWidget(parent)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}

void ZzInfoBarLayout::setItems(QWidget* title, QWidget* message, QWidget* action)
{
    title_ = title;
    message_ = message;
    action_ = action;
    refreshLayout();
}

void ZzInfoBarLayout::refreshLayout()
{
    updateGeometry();
    positionItems();
}

int ZzInfoBarLayout::naturalWidth() const
{
    int width = 0;
    bool previous = false;
    for (auto* item : { title_, message_, action_ }) {
        if (!item || item->isHidden())
            continue;
        if (previous)
            width += item == action_ ? ActionGap : MessageGap;
        width += item->sizeHint().width();
        previous = true;
    }
    return width;
}

int ZzInfoBarLayout::horizontalHeight() const
{
    int height = MinHeight;
    for (auto* item : { title_, message_, action_ }) {
        if (item && !item->isHidden())
            height = qMax(
                height, (item == action_ ? ActionTop : TitleTop) + item->sizeHint().height());
    }
    return height;
}

int ZzInfoBarLayout::verticalHeight(int width) const
{
    int height = VerticalTop;
    bool previous = false;
    for (auto* item : { title_, message_, action_ }) {
        if (!item || item->isHidden())
            continue;
        if (previous)
            height += item == action_ ? VerticalActionGap : VerticalMessageGap;
        height += itemHeight(item, width);
        previous = true;
    }
    return previous ? qMax(MinHeight, height + VerticalBottom) : MinHeight;
}

QSize ZzInfoBarLayout::sizeHint() const { return { naturalWidth(), horizontalHeight() }; }

QSize ZzInfoBarLayout::minimumSizeHint() const
{
    int width = 0;
    for (auto* item : { title_, message_, action_ }) {
        if (item && !item->isHidden())
            width = qMax(width, item->minimumSizeHint().width());
    }
    return { width, MinHeight };
}

bool ZzInfoBarLayout::hasHeightForWidth() const { return true; }
int ZzInfoBarLayout::heightForWidth(int width) const
{
    return width >= naturalWidth() ? horizontalHeight() : verticalHeight(width);
}

void ZzInfoBarLayout::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionItems();
}

void ZzInfoBarLayout::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LayoutDirectionChange)
        positionItems();
}

void ZzInfoBarLayout::positionItems()
{
    const int contentWidth = qMax(0, width());
    const auto place = [this](QWidget* item, const QRect& logical) {
        item->setGeometry(QStyle::visualRect(layoutDirection(), rect(), logical));
    };
    if (contentWidth >= naturalWidth()) {
        int x = 0;
        if (title_ && !title_->isHidden()) {
            const QSize hint = title_->sizeHint();
            place(title_, { x, TitleTop, hint.width(), hint.height() });
            x += hint.width();
        }
        if (message_ && !message_->isHidden()) {
            if (title_ && !title_->isHidden())
                x += MessageGap;
            const int actionWidth
                = action_ && !action_->isHidden() ? ActionGap + action_->sizeHint().width() : 0;
            const int messageWidth = qMax(0, contentWidth - x - actionWidth);
            place(message_, { x, TitleTop, messageWidth, itemHeight(message_, messageWidth) });
            x += messageWidth;
        }
        if (action_ && !action_->isHidden()) {
            if ((title_ && !title_->isHidden()) || (message_ && !message_->isHidden()))
                x += ActionGap;
            const QSize hint = action_->sizeHint();
            place(action_, { x, ActionTop, hint.width(), hint.height() });
        }
        return;
    }

    int y = VerticalTop;
    bool previous = false;
    int messageLimit = height() - VerticalTop - VerticalBottom;
    if (title_ && !title_->isHidden())
        messageLimit -= itemHeight(title_, contentWidth) + VerticalMessageGap;
    if (action_ && !action_->isHidden())
        messageLimit -= itemHeight(action_, contentWidth) + VerticalActionGap;
    for (auto* item : { title_, message_, action_ }) {
        if (!item || item->isHidden())
            continue;
        if (previous)
            y += item == action_ ? VerticalActionGap : VerticalMessageGap;
        int h = itemHeight(item, contentWidth);
        if (item == message_)
            h = qMin(h, qMax(0, messageLimit));
        place(item, { 0, y, contentWidth, h });
        y += h;
        previous = true;
    }
}

ZzInfoBarMessageLabel::ZzInfoBarMessageLabel(ZzInfoBar* bar, QWidget* parent)
    : QLabel(parent)
    , bar_(bar)
{
}

void ZzInfoBarMessageLabel::setFullText(const QString& text)
{
    if (fullText_ == text)
        return;
    fullText_ = text;
    refreshElision();
}

bool ZzInfoBarMessageLabel::isPopup() const
{
    return bar_->property("_zzInfoBarPopupSurface").toBool();
}

void ZzInfoBarMessageLabel::refreshElision()
{
    const int lines
        = height() > 0 ? qBound(1, height() / qMax(1, QFontMetrics(font()).lineSpacing()), 3) : 3;
    const QString text = isPopup() ? elidedText(width(), lines) : fullText_;
    if (QLabel::text() != text)
        QLabel::setText(text);
    setToolTip(text == fullText_ ? QString() : fullText_);
}

QSize ZzInfoBarMessageLabel::sizeHint() const
{
    return isPopup() && !fullText_.isEmpty() ? QFontMetrics(font()).boundingRect(fullText_).size()
                                             : QLabel::sizeHint();
}

int ZzInfoBarMessageLabel::heightForWidth(int width) const
{
    return isPopup() ? wrappedHeight(qMax(0, width)) : QLabel::heightForWidth(width);
}

int ZzInfoBarMessageLabel::wrappedHeight(int width) const
{
    if (fullText_.isEmpty() || width <= 0)
        return 0;
    QTextLayout layout(fullText_, font());
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setTextDirection(layoutDirection());
    layout.setTextOption(option);
    layout.beginLayout();
    qreal h = 0;
    for (int i = 0; i < 3; ++i) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        h += line.height();
    }
    layout.endLayout();
    return qCeil(h);
}

QString ZzInfoBarMessageLabel::elidedText(int width, int maximumLines) const
{
    if (fullText_.isEmpty() || width <= 0)
        return fullText_;
    QTextLayout layout(fullText_, font());
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setTextDirection(layoutDirection());
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine last;
    for (int i = 0; i < maximumLines; ++i) {
        last = layout.createLine();
        if (!last.isValid())
            break;
        last.setLineWidth(width);
    }
    layout.endLayout();
    if (!last.isValid() || last.textStart() + last.textLength() >= fullText_.size())
        return fullText_;
    const int start = last.textStart();
    QString remainder = fullText_.mid(start);
    remainder.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return fullText_.left(start)
        + QFontMetrics(font()).elidedText(remainder, Qt::ElideRight, width);
}

void ZzInfoBarMessageLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    refreshElision();
}

void ZzInfoBarMessageLabel::changeEvent(QEvent* event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::LayoutDirectionChange)
        refreshElision();
}

} // namespace ZzFluentUI
