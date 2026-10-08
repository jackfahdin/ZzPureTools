#include <ZzFluentUI/ZzInfoBar.h>

#include "private/ZzInfoBarLayout.h"
#include "private/ZzInfoBarPrivate.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

namespace ZzFluentUI {
namespace {
    constexpr int MinHeight = 48;
    constexpr int PopupMaxHeight = 160;
    constexpr int IconColumnWidth = 30;
    constexpr int ContentPadding = 16;
    constexpr int ActionSpacing = 8;
    constexpr int ActionMinWidth = 96;
    constexpr int ActionMinHeight = 24;
    constexpr int CloseAreaWidth = 40;
    constexpr int CloseButtonSize = 32;
    constexpr int CloseMargin = 4;
    constexpr int CornerRadius = 4;

    bool darkPalette(const QPalette& palette)
    {
        return palette.color(QPalette::Window).lightness() < 128;
    }

    QColor statusColor(ZzInfoBar::Severity severity, const QPalette& palette)
    {
        const bool dark = darkPalette(palette);
        switch (severity) {
        case ZzInfoBar::Success:
            return dark ? QColor { 108, 203, 95 } : QColor { 15, 123, 15 };
        case ZzInfoBar::Warning:
            return dark ? QColor { 252, 225, 0 } : QColor { 157, 93, 0 };
        case ZzInfoBar::Error:
            return dark ? QColor { 255, 153, 164 } : QColor { 196, 43, 28 };
        case ZzInfoBar::Informational:
            return palette.color(QPalette::Highlight);
        }
        return palette.color(QPalette::Highlight);
    }

    QColor backgroundColor(ZzInfoBar::Severity severity, const QPalette& palette)
    {
        if (darkPalette(palette)) {
            switch (severity) {
            case ZzInfoBar::Success:
                return { 57, 61, 27 };
            case ZzInfoBar::Warning:
                return { 67, 53, 25 };
            case ZzInfoBar::Error:
                return { 68, 39, 38 };
            case ZzInfoBar::Informational:
                return { 255, 255, 255, 8 };
            }
        }
        switch (severity) {
        case ZzInfoBar::Success:
            return { 223, 246, 221 };
        case ZzInfoBar::Warning:
            return { 255, 244, 206 };
        case ZzInfoBar::Error:
            return { 253, 231, 233 };
        case ZzInfoBar::Informational:
            return { 246, 246, 246, 128 };
        }
        return { 246, 246, 246, 128 };
    }

    class ZzInfoBarCloseButton final : public QToolButton {
    public:
        using QToolButton::QToolButton;

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            QToolButton::paintEvent(event);
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                    QPalette::ButtonText),
                1.35, Qt::SolidLine, Qt::RoundCap));
            const QPointF center = rect().center();
            painter.drawLine(center + QPointF(-4, -4), center + QPointF(4, 4));
            painter.drawLine(center + QPointF(4, -4), center + QPointF(-4, 4));
        }
    };

    class ZzInfoBarIconWidget final : public QWidget {
    public:
        explicit ZzInfoBarIconWidget(ZzInfoBar* bar)
            : QWidget(bar)
            , bar_(bar)
        {
            setFixedWidth(IconColumnWidth);
            setMinimumHeight(MinHeight);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            setAttribute(Qt::WA_TransparentForMouseEvents);
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QColor background = statusColor(bar_->severity(), palette());
            const qreal luminance = 0.2126 * background.redF() + 0.7152 * background.greenF()
                + 0.0722 * background.blueF();
            const QColor foreground
                = luminance > 0.58 ? QColor { Qt::black } : QColor { Qt::white };
            const QPointF center(8, 24);
            painter.setPen(Qt::NoPen);
            painter.setBrush(background);
            painter.drawEllipse(center, 8, 8);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(foreground, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.save();
            painter.translate(center);
            switch (bar_->severity()) {
            case ZzInfoBar::Success:
                painter.drawLine(QPointF(-3.5, 0), QPointF(-1, 2.5));
                painter.drawLine(QPointF(-1, 2.5), QPointF(4, -2.5));
                break;
            case ZzInfoBar::Warning:
                painter.drawLine(QPointF(0, -3.5), QPointF(0, 1));
                painter.drawPoint(QPointF(0, 3.5));
                break;
            case ZzInfoBar::Error:
                painter.drawLine(QPointF(-2.8, -2.8), QPointF(2.8, 2.8));
                painter.drawLine(QPointF(2.8, -2.8), QPointF(-2.8, 2.8));
                break;
            case ZzInfoBar::Informational:
                painter.drawLine(QPointF(0, -0.5), QPointF(0, 3.5));
                painter.drawPoint(QPointF(0, -3.2));
                break;
            }
            painter.restore();
        }

    private:
        ZzInfoBar* bar_ = nullptr;
    };
} // namespace

ZzInfoBar::ZzInfoBar(QWidget* parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzInfoBarPrivate>())
{
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setAttribute(Qt::WA_StyledBackground, false);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(ContentPadding, 0, 0, 0);
    root->setSpacing(0);

    d_ptr->icon = new ZzInfoBarIconWidget(this);
    root->addWidget(d_ptr->icon);
    d_ptr->panel = new ZzInfoBarLayout(this);
    root->addWidget(d_ptr->panel, 1);
    d_ptr->titleLabel = new QLabel(d_ptr->panel);
    d_ptr->titleLabel->setWordWrap(true);
    QFont titleFont = d_ptr->titleLabel->font();
    titleFont.setPixelSize(14);
    titleFont.setWeight(QFont::DemiBold);
    d_ptr->titleLabel->setFont(titleFont);
    d_ptr->messageLabel = new ZzInfoBarMessageLabel(this, d_ptr->panel);
    d_ptr->messageLabel->setWordWrap(true);
    QFont messageFont = d_ptr->messageLabel->font();
    messageFont.setPixelSize(13);
    messageFont.setWeight(QFont::Normal);
    d_ptr->messageLabel->setFont(messageFont);
    d_ptr->messageLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    d_ptr->messageLabel->setOpenExternalLinks(false);
    d_ptr->actionContainer = new QWidget(d_ptr->panel);
    d_ptr->actionLayout = new QHBoxLayout(d_ptr->actionContainer);
    d_ptr->actionLayout->setContentsMargins(0, 0, 0, 0);
    d_ptr->actionLayout->setSpacing(ActionSpacing);
    d_ptr->actionButton = new QPushButton(d_ptr->actionContainer);
    d_ptr->actionButton->setMinimumSize(ActionMinWidth, ActionMinHeight);
    d_ptr->actionLayout->addWidget(d_ptr->actionButton);
    d_ptr->panel->setItems(d_ptr->titleLabel, d_ptr->messageLabel, d_ptr->actionContainer);
    d_ptr->closeArea = new QWidget(this);
    d_ptr->closeArea->setFixedSize(CloseAreaWidth, MinHeight);
    auto* closeLayout = new QHBoxLayout(d_ptr->closeArea);
    closeLayout->setContentsMargins(CloseMargin, ActionSpacing, CloseMargin, ActionSpacing);
    d_ptr->closeButton = new ZzInfoBarCloseButton(d_ptr->closeArea);
    d_ptr->closeButton->setToolTip(tr("关闭"));
    d_ptr->closeButton->setAccessibleName(tr("关闭通知"));
    d_ptr->closeButton->setAutoRaise(true);
    d_ptr->closeButton->setFixedSize(CloseButtonSize, CloseButtonSize);
    closeLayout->addWidget(d_ptr->closeButton);
    root->addWidget(d_ptr->closeArea, 0, Qt::AlignTop);

    d_ptr->heightAnimation = new QVariantAnimation(this);
    connect(d_ptr->actionButton, &QPushButton::clicked, this, &ZzInfoBar::actionTriggered);
    connect(d_ptr->closeButton, &QToolButton::clicked, this, [this] {
        QPointer<ZzInfoBar> guard(this);
        emit closeButtonClicked();
        if (guard)
            dismiss();
    });
    connect(d_ptr->heightAnimation, &QVariantAnimation::valueChanged, this,
        [this](const QVariant& value) {
            setMaximumHeight(value.toInt());
            updateGeometry();
        });
    connect(
        d_ptr->heightAnimation, &QVariantAnimation::finished, this, &ZzInfoBar::finishTransition);
    refreshContent();
    setMaximumHeight(0);
    hide();
}
ZzInfoBar::~ZzInfoBar() { disconnect(d_ptr->actionDestroyed); }
ZzInfoBar::Severity ZzInfoBar::severity() const { return d_ptr->severity; }
void ZzInfoBar::setSeverity(Severity severity)
{
    if (severity != Informational && severity != Success && severity != Warning
        && severity != Error)
        return;
    if (d_ptr->severity == severity)
        return;
    d_ptr->severity = severity;
    d_ptr->icon->update();
    update();
    emit severityChanged(severity);
}
QString ZzInfoBar::title() const { return d_ptr->title; }
void ZzInfoBar::setTitle(const QString& title)
{
    if (d_ptr->title == title)
        return;
    d_ptr->title = title;
    d_ptr->titleLabel->setText(title);
    refreshContent();
    emit titleChanged(title);
}
QString ZzInfoBar::message() const { return d_ptr->message; }
void ZzInfoBar::setMessage(const QString& message)
{
    if (d_ptr->message == message)
        return;
    d_ptr->message = message;
    d_ptr->messageLabel->setFullText(message);
    refreshContent();
    emit messageChanged(message);
}
QString ZzInfoBar::actionButtonText() const { return d_ptr->actionButtonText; }
void ZzInfoBar::setActionButtonText(const QString& text)
{
    if (d_ptr->actionButtonText == text)
        return;
    d_ptr->actionButtonText = text;
    d_ptr->actionButton->setText(text);
    refreshContent();
    emit actionButtonTextChanged(text);
}
bool ZzInfoBar::isOpen() const { return d_ptr->open; }
bool ZzInfoBar::isClosable() const { return d_ptr->closable; }
void ZzInfoBar::setClosable(bool closable)
{
    if (d_ptr->closable == closable)
        return;
    d_ptr->closable = closable;
    d_ptr->closeArea->setVisible(closable);
    updateGeometry();
    emit closableChanged(closable);
}
bool ZzInfoBar::isIconVisible() const { return d_ptr->iconVisible; }
void ZzInfoBar::setIconVisible(bool visible)
{
    if (d_ptr->iconVisible == visible)
        return;
    d_ptr->iconVisible = visible;
    d_ptr->icon->setVisible(visible);
    updateGeometry();
    emit iconVisibleChanged(visible);
}
bool ZzInfoBar::isAnimationEnabled() const { return d_ptr->animationEnabled; }
void ZzInfoBar::setAnimationEnabled(bool enabled)
{
    if (d_ptr->animationEnabled == enabled)
        return;
    d_ptr->animationEnabled = enabled;
    if (!enabled && d_ptr->heightAnimation->state() == QAbstractAnimation::Running) {
        QPointer<ZzInfoBar> guard(this);
        d_ptr->heightAnimation->stop();
        finishTransition();
        if (!guard)
            return;
    }
    emit animationEnabledChanged(enabled);
}
int ZzInfoBar::animationDuration() const { return d_ptr->animationDuration; }
void ZzInfoBar::setAnimationDuration(int duration)
{
    duration = qMax(0, duration);
    if (d_ptr->animationDuration == duration)
        return;
    d_ptr->animationDuration = duration;
    emit animationDurationChanged(duration);
}
QPushButton* ZzInfoBar::actionButton() const { return d_ptr->actionButton; }
QWidget* ZzInfoBar::actionWidget() const { return d_ptr->actionWidget.data(); }
void ZzInfoBar::setActionWidget(QWidget* widget)
{
    if (widget == this || widget == d_ptr->actionWidget || (widget && widget->isAncestorOf(this)))
        return;
    if (widget) {
        for (QWidget* parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
            if (parent == this)
                return;
        }
    }
    QPointer<QWidget> incoming(widget);
    if (d_ptr->actionWidget) {
        QPointer<ZzInfoBar> guard(this);
        QWidget* old = d_ptr->actionWidget.data();
        old->removeEventFilter(this);
        disconnect(d_ptr->actionDestroyed);
        d_ptr->actionLayout->removeWidget(old);
        d_ptr->actionWidget = nullptr;
        old->setParent(nullptr);
        delete old;
        if (!guard) {
            delete incoming.data();
            return;
        }
    }
    if (d_ptr->actionWidget) {
        if (d_ptr->actionWidget != incoming)
            delete incoming.data();
        return;
    }
    if (incoming) {
        QPointer<ZzInfoBar> guard(this);
        incoming->setParent(d_ptr->actionContainer);
        if (!guard)
            return;
        if (d_ptr->actionWidget) {
            if (d_ptr->actionWidget != incoming)
                delete incoming.data();
            return;
        }
        if (!incoming) {
            refreshContent();
            return;
        }
        d_ptr->actionWidget = incoming;
        d_ptr->actionLayout->insertWidget(0, incoming);
        incoming->installEventFilter(this);
        incoming->show();
        d_ptr->actionDestroyed = connect(incoming, &QObject::destroyed, this, [this] {
            d_ptr->actionWidget = nullptr;
            refreshContent();
        });
    }
    refreshContent();
}
QWidget* ZzInfoBar::takeActionWidget()
{
    QWidget* widget = d_ptr->actionWidget.data();
    if (!widget)
        return nullptr;
    widget->removeEventFilter(this);
    disconnect(d_ptr->actionDestroyed);
    d_ptr->actionLayout->removeWidget(widget);
    d_ptr->actionWidget = nullptr;
    widget->setParent(nullptr);
    refreshContent();
    return widget;
}
QSize ZzInfoBar::sizeHint() const
{
    QSize hint = layout() ? layout()->sizeHint().expandedTo({ 0, MinHeight }) : QWidget::sizeHint();
    if (property("_zzInfoBarPopupSurface").toBool())
        hint.setHeight(qMin(PopupMaxHeight, hint.height()));
    return hint;
}
QSize ZzInfoBar::minimumSizeHint() const
{
    return layout() ? layout()->minimumSize().expandedTo({ 0, MinHeight })
                    : QWidget::minimumSizeHint();
}
bool ZzInfoBar::hasHeightForWidth() const { return layout() && layout()->hasHeightForWidth(); }
int ZzInfoBar::heightForWidth(int width) const
{
    int height = layout() && layout()->hasHeightForWidth()
        ? qMax(MinHeight, layout()->heightForWidth(width))
        : sizeHint().height();
    return property("_zzInfoBarPopupSurface").toBool() ? qMin(PopupMaxHeight, height) : height;
}
void ZzInfoBar::setOpen(bool open)
{
    if (d_ptr->open == open)
        return;
    d_ptr->open = open;
    QPointer<ZzInfoBar> guard(this);
    emit openChanged(open);
    if (!guard || d_ptr->open != open)
        return;
    d_ptr->heightAnimation->stop();
    if (property("_zzInfoBarPopupSurface").toBool()) {
        if (open) {
            setMaximumHeight(PopupMaxHeight);
            show();
            updateGeometry();
            emit opened();
        }
        return;
    }
    const bool reduced = property("reducedMotion").toBool()
        || qApp->property("reducedMotion").toBool()
        || !style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this);
    const bool animate = d_ptr->animationEnabled && !reduced && d_ptr->animationDuration > 0
        && parentWidget() && parentWidget()->isVisible();
    if (!animate) {
        setMaximumHeight(open ? QWIDGETSIZE_MAX : 0);
        setVisible(open);
        updateGeometry();
        if (open)
            emit opened();
        else
            emit closed();
        return;
    }
    int start = isVisible() ? height() : 0;
    int end = 0;
    if (open) {
        const int current = start;
        setMaximumHeight(QWIDGETSIZE_MAX);
        show();
        if (layout())
            layout()->activate();
        end = hasHeightForWidth() ? heightForWidth(width()) : sizeHint().height();
        start = qMin(current, end);
        setMaximumHeight(start);
    }
    d_ptr->heightAnimation->setDuration(d_ptr->animationDuration);
    d_ptr->heightAnimation->setEasingCurve(open ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    d_ptr->heightAnimation->setStartValue(start);
    d_ptr->heightAnimation->setEndValue(end);
    d_ptr->heightAnimation->start();
}
void ZzInfoBar::dismiss() { setOpen(false); }
void ZzInfoBar::finishTransition()
{
    if (d_ptr->open) {
        setMaximumHeight(QWIDGETSIZE_MAX);
        show();
        updateGeometry();
        emit opened();
    } else {
        setMaximumHeight(0);
        hide();
        updateGeometry();
        emit closed();
    }
}
void ZzInfoBar::finishPopupClose()
{
    if (d_ptr->open)
        return;
    setMaximumHeight(0);
    hide();
    updateGeometry();
    emit closed();
}
void ZzInfoBar::refreshContent()
{
    d_ptr->titleLabel->setVisible(!d_ptr->title.isEmpty());
    d_ptr->messageLabel->setVisible(!d_ptr->message.isEmpty());
    d_ptr->actionButton->setVisible(!d_ptr->actionWidget && !d_ptr->actionButtonText.isEmpty());
    d_ptr->actionContainer->setVisible(d_ptr->actionWidget || !d_ptr->actionButtonText.isEmpty());
    d_ptr->panel->refreshLayout();
    updateGeometry();
    setAccessibleName(d_ptr->title.isEmpty() ? d_ptr->message
            : d_ptr->message.isEmpty()       ? d_ptr->title
                                       : d_ptr->title + QStringLiteral(". ") + d_ptr->message);
}
bool ZzInfoBar::event(QEvent* event)
{
    const bool result = QWidget::event(event);
    if (event->type() == QEvent::DynamicPropertyChange && d_ptr && d_ptr->messageLabel) {
        d_ptr->messageLabel->refreshElision();
        updateGeometry();
    }
    if ((event->type() == QEvent::DynamicPropertyChange || event->type() == QEvent::StyleChange
            || event->type() == QEvent::EnabledChange || event->type() == QEvent::Hide)
        && d_ptr && d_ptr->heightAnimation
        && d_ptr->heightAnimation->state() == QAbstractAnimation::Running
        && (!isVisible() || !isEnabled() || property("reducedMotion").toBool()
            || qApp->property("reducedMotion").toBool()
            || !style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this))) {
        d_ptr->heightAnimation->stop();
        if (d_ptr->open && !isVisible()) {
            setMaximumHeight(QWIDGETSIZE_MAX);
            updateGeometry();
            emit opened();
        } else {
            finishTransition();
        }
    }
    return result;
}
bool ZzInfoBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == d_ptr->actionWidget && event->type() == QEvent::ParentChange
        && d_ptr->actionWidget && d_ptr->actionWidget->parentWidget() != d_ptr->actionContainer) {
        QWidget* widget = d_ptr->actionWidget.data();
        widget->removeEventFilter(this);
        disconnect(d_ptr->actionDestroyed);
        d_ptr->actionLayout->removeWidget(widget);
        d_ptr->actionWidget = nullptr;
        refreshContent();
    }
    return QWidget::eventFilter(watched, event);
}
void ZzInfoBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor border = palette().color(QPalette::Mid);
    border.setAlpha(darkPalette(palette()) ? 150 : 88);
    QColor background = backgroundColor(d_ptr->severity, palette());
    if (d_ptr->severity == Informational && property("_zzInfoBarPopupSurface").toBool())
        background = darkPalette(palette()) ? QColor { 46, 46, 46 } : QColor { 247, 247, 247 };
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(background);
    painter.drawRoundedRect(
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), CornerRadius, CornerRadius);
}
void ZzInfoBar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange || event->type() == QEvent::EnabledChange) {
        d_ptr->icon->update();
        update();
    }
}

} // namespace ZzFluentUI
