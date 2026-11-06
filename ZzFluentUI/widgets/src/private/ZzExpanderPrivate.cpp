#include "ZzExpanderPrivate.h"

#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <QtGui/QFont>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzExpander.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

namespace ZzFluentUI {

namespace {

/** @brief 为 Expander header 补齐跨平台一致的 Enter/Return 激活语义。 */
class ZzExpanderHeaderButton final : public QToolButton
{
public:
    /** @brief 创建由 Expander 固定拥有的 header 按钮。 */
    explicit ZzExpanderHeaderButton(ZzExpanderPrivate *state)
        : QToolButton(state->q_ptr)
        , state_(state)
    {
        setAttribute(Qt::WA_Hover);
    }

    /** @brief 为尾端 Chevron 预留空间，标题过长时允许布局收窄。 */
    QSize sizeHint() const override
    {
        const auto snapshot = state_->theme.snapshot();
        const int padding = qCeil(snapshot->metric(ZzMetricToken::HorizontalPadding));
        const int extent = qCeil(snapshot->metric(ZzMetricToken::ControlHeight));
        const int vertical = qCeil(snapshot->metric(ZzMetricToken::VerticalPadding));
        return QSize(fontMetrics().size(Qt::TextShowMnemonic | Qt::TextSingleLine, text()).width()
                + extent + 3 * padding,
            std::max(extent + 2 * vertical, fontMetrics().height() + 2 * vertical));
    }

    /** @brief 最小宽度容纳箭头和内边距，不强迫长标题撑宽窗口。 */
    QSize minimumSizeHint() const override
    {
        const auto snapshot = state_->theme.snapshot();
        return QSize(qCeil(snapshot->metric(ZzMetricToken::ControlHeight)
            + 3 * snapshot->metric(ZzMetricToken::HorizontalPadding)), sizeHint().height());
    }

protected:
    /** @brief 绘制尾端字体箭头、局部悬停反馈、省略标题与焦点环。 */
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        const auto snapshot = state_->theme.snapshot();
        const qreal padding = snapshot->metric(ZzMetricToken::HorizontalPadding);
        const qreal extent = snapshot->metric(ZzMetricToken::ControlHeight);
        const qreal trailing = snapshot->metric(ZzMetricToken::VerticalPadding);
        const QRectF chevron(isRightToLeft() ? trailing : width() - trailing - extent,
            (height() - extent) / 2.0, extent, extent);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (isEnabled() && (underMouse() || isDown())) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(snapshot->color(isDown()
                ? ZzColorToken::ControlFillPressed : ZzColorToken::ControlFillHover));
            const qreal radius = snapshot->metric(ZzMetricToken::CornerRadiusSmall);
            painter.drawRoundedRect(chevron, radius, radius);
        }
        const QColor foreground = snapshot->color(isEnabled()
            ? ZzColorToken::TextPrimary : ZzColorToken::TextSecondary);
        painter.setPen(foreground);
        const QRectF textRect(isRightToLeft() ? chevron.right() + padding : padding, 0,
            std::max<qreal>(0, width() - extent - trailing - 2 * padding), height());
        painter.setFont(font());
        int textFlags = Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextShowMnemonic
            | (isRightToLeft() ? Qt::AlignRight : Qt::AlignLeft);
        if (!style()->styleHint(QStyle::SH_UnderlineShortcut, nullptr, this)) {
            textFlags |= Qt::TextHideMnemonic;
        }
        painter.drawText(textRect, textFlags,
            fontMetrics().elidedText(text(), Qt::ElideRight, qFloor(textRect.width()),
                Qt::TextShowMnemonic));
        painter.save();
        painter.translate(chevron.center());
        painter.rotate(180.0 * state_->expansionProgress);
        painter.setFont(ZzSegoeIconFont::font(qCeil(snapshot->metric(ZzMetricToken::IconSmall))));
        painter.drawText(QRectF(-extent / 2, -extent / 2, extent, extent), Qt::AlignCenter,
            QString(QChar(static_cast<char16_t>(ZzSegoeIcon::ChevronDownMed))));
        painter.restore();
        if (hasFocus()) {
            ZzFluentPainter::drawFocusRing(&painter, QRectF(rect()).adjusted(3, 3, -3, -3),
                *snapshot, devicePixelRatioF());
        }
    }

    /** @brief 使用 Enter 或 Return 时执行一次 click，其他按键沿用 Qt。 */
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event != nullptr
            && (event->key() == Qt::Key_Enter
                || event->key() == Qt::Key_Return)) {
            click();
            event->accept();
            return;
        }
        QToolButton::keyPressEvent(event);
    }

private:
    ZzExpanderPrivate *const state_;
};

} // namespace

ZzExpanderPrivate::ZzExpanderPrivate(ZzExpander *q)
    : q_ptr(q)
    , theme(q)
    , headerButton(new ZzExpanderHeaderButton(this))
    , contentHost(new QWidget(q))
    , contentLayout(new QVBoxLayout(contentHost))
    , heightAnimation(new QVariantAnimation(q))
{
    Q_ASSERT(q_ptr != nullptr);
    auto *rootLayout = new QVBoxLayout(q_ptr);
    rootLayout->setObjectName(QStringLiteral("zzExpanderRootLayout"));
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    headerButton->setObjectName(QStringLiteral("zzExpanderHeaderButton"));
    headerButton->setCheckable(true);
    headerButton->setChecked(false);
    headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    headerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerButton->setFocusPolicy(Qt::StrongFocus);

    contentHost->setObjectName(QStringLiteral("zzExpanderContentHost"));
    contentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    contentHost->setMaximumHeight(0);
    contentHost->hide();
    contentLayout->setSpacing(0);

    rootLayout->addWidget(headerButton);
    rootLayout->addWidget(contentHost);

    heightAnimation->setEasingCurve(QEasingCurve::InOutSine);
    QObject::connect(
        heightAnimation,
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this](const QVariant &value) {
            const qreal progress = heightAnimation->easingCurve().valueForProgress(
                static_cast<qreal>(heightAnimation->currentTime())
                / std::max(1, heightAnimation->duration()));
            expansionProgress = transitionStartProgress
                + ((expanded ? 1.0 : 0.0) - transitionStartProgress) * progress;
            contentHost->setMaximumHeight(std::max(0, value.toInt()));
            headerButton->update();
            q_ptr->update();
            q_ptr->updateGeometry();
        });
    QObject::connect(
        heightAnimation,
        &QVariantAnimation::finished,
        q_ptr,
        [this] {
            settleTransition();
        });
    QObject::connect(
        headerButton,
        &QToolButton::clicked,
        q_ptr,
        [this](bool checked) {
            q_ptr->setExpanded(checked);
        });
}

ZzExpanderPrivate::~ZzExpanderPrivate()
{
    QObject::disconnect(contentDestroyedConnection);
}

void ZzExpanderPrivate::refreshPresentation()
{
    const auto snapshot = theme.snapshot();
    headerButton->setText(headerText);
    headerButton->setChecked(expanded);
    headerButton->setArrowType(expanded
        ? Qt::DownArrow
        : (q_ptr->layoutDirection() == Qt::RightToLeft
              ? Qt::LeftArrow
              : Qt::RightArrow));
    headerButton->setFont(snapshot->font(ZzTypographyToken::BodyStrong));
    headerButton->setMinimumHeight(qCeil(
        snapshot->metric(ZzMetricToken::ControlHeight)));

    const QString actionText = expanded
        ? ZzExpander::tr("折叠内容")
        : ZzExpander::tr("展开内容");
    headerButton->setAccessibleDescription(actionText);
    headerButton->setToolTip(actionText);
    headerButton->setAccessibleName(headerText.isEmpty()
        ? ZzExpander::tr("可折叠内容") : headerText);

    const int horizontalPadding = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    const int verticalPadding = qCeil(
        snapshot->metric(ZzMetricToken::VerticalPadding));
    contentLayout->setContentsMargins(
        horizontalPadding,
        verticalPadding,
        horizontalPadding,
        verticalPadding);
    if (heightAnimation->state() != QAbstractAnimation::Stopped
        && ZzAnimationPolicy::adjustedDuration(
               snapshot->duration(ZzMotionToken::Normal),
               snapshot->reducedMotion(),
               false)
            <= 0) {
        settleTransition();
    } else {
        retargetExpandedHeight();
    }
    q_ptr->updateGeometry();
    headerButton->updateGeometry();
    headerButton->update();
    q_ptr->update();
}

void ZzExpanderPrivate::refreshTheme()
{
    theme.refreshFallback();
    refreshPresentation();
}

void ZzExpanderPrivate::setContentWidget(QWidget *widget)
{
    if (contentWidget == widget) {
        return;
    }
    if (widget == q_ptr || widget == headerButton || widget == contentHost
        || (widget != nullptr && widget->isAncestorOf(q_ptr))) {
        qWarning("ZzExpander rejected a cyclic content widget");
        return;
    }

    heightAnimation->stop();
    QObject::disconnect(contentDestroyedConnection);
    contentDestroyedConnection = {};
    QWidget *const oldWidget = contentWidget.data();
    contentWidget = nullptr;
    if (widget != nullptr && oldWidget != nullptr
        && oldWidget->isAncestorOf(widget)) {
        widget->setParent(contentHost);
    }
    if (oldWidget != nullptr) {
        contentLayout->removeWidget(oldWidget);
        delete oldWidget;
    }

    contentWidget = widget;
    if (widget != nullptr) {
        widget->setParent(contentHost);
        contentLayout->addWidget(widget);
        contentDestroyedConnection = QObject::connect(
            widget,
            &QObject::destroyed,
            q_ptr,
            [this] {
                heightAnimation->stop();
                contentWidget = nullptr;
                contentDestroyedConnection = {};
                settleTransition();
                Q_EMIT q_ptr->contentWidgetChanged(nullptr);
            });
    }
    settleTransition();
    Q_EMIT q_ptr->contentWidgetChanged(widget);
}

QWidget *ZzExpanderPrivate::takeContentWidget()
{
    QWidget *const widget = contentWidget.data();
    if (widget == nullptr) {
        return nullptr;
    }
    heightAnimation->stop();
    QObject::disconnect(contentDestroyedConnection);
    contentDestroyedConnection = {};
    contentWidget = nullptr;
    contentLayout->removeWidget(widget);
    widget->setParent(nullptr);
    settleTransition();
    Q_EMIT q_ptr->contentWidgetChanged(nullptr);
    return widget;
}

void ZzExpanderPrivate::startTransition()
{
    int startHeight = contentHost->maximumHeight();
    if (startHeight == QWIDGETSIZE_MAX) {
        startHeight = contentHost->height();
    }
    startHeight = std::max(0, startHeight);
    heightAnimation->stop();

    if (expanded) {
        contentHost->show();
    } else {
        restoreHeaderFocusIfNeeded();
    }
    const int endHeight = expanded ? expandedContentHeight() : 0;
    const int duration = transitionDuration();
    if (duration <= 0 || startHeight == endHeight
        || contentWidget == nullptr) {
        settleTransition();
        return;
    }

    contentHost->setMaximumHeight(startHeight);
    transitionStartProgress = expansionProgress;
    heightAnimation->setStartValue(startHeight);
    heightAnimation->setEndValue(endHeight);
    heightAnimation->setDuration(duration);
    heightAnimation->start();
}

void ZzExpanderPrivate::retargetExpandedHeight()
{
    if (!expanded || heightAnimation->state() == QAbstractAnimation::Stopped) {
        return;
    }
    heightAnimation->setEndValue(expandedContentHeight());
}

int ZzExpanderPrivate::expandedContentHeight() const
{
    if (contentWidget == nullptr) {
        return 0;
    }
    contentLayout->activate();
    return std::max(0, contentLayout->sizeHint().height());
}

int ZzExpanderPrivate::transitionDuration() const
{
    const auto snapshot = theme.snapshot();
    return ZzAnimationPolicy::adjustedDuration(
        snapshot->duration(ZzMotionToken::Normal),
        snapshot->reducedMotion(),
        false);
}

void ZzExpanderPrivate::settleTransition()
{
    heightAnimation->stop();
    expansionProgress = expanded ? 1.0 : 0.0;
    headerButton->update();
    if (expanded && contentWidget != nullptr) {
        contentHost->setMaximumHeight(QWIDGETSIZE_MAX);
        contentHost->show();
    } else {
        contentHost->setMaximumHeight(0);
        contentHost->hide();
    }
    q_ptr->updateGeometry();
    q_ptr->update();
}

void ZzExpanderPrivate::restoreHeaderFocusIfNeeded()
{
    QWidget *const focusWidget = QApplication::focusWidget();
    if (focusWidget != nullptr
        && (focusWidget == contentHost
            || contentHost->isAncestorOf(focusWidget))) {
        headerButton->setFocus(Qt::OtherFocusReason);
    }
}

} // namespace ZzFluentUI
