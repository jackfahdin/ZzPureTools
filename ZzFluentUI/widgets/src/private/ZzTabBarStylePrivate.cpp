#include "ZzTabBarStylePrivate.h"
#include "ZzControlAppearancePrivate.h"

#include <QtCore/QEvent>
#include <QtCore/QtMath>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <ZzFluentUI/ZzControlAppearance.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>

namespace ZzFluentUI {
namespace {
using Appearance = ZzTabBarAppearance;

bool zzPivot(Appearance value)
{
    return value == Appearance::PivotGrow || value == Appearance::PivotSlide
        || value == Appearance::PivotStretch;
}

bool zzSegmented(Appearance value)
{
    return value == Appearance::SegmentedSlide || value == Appearance::SegmentedFade
        || value == Appearance::SegmentedWinUI3;
}

qreal zzLerp(qreal from, qreal to, qreal t) { return from + (to - from) * t; }

QRectF zzLerpRect(const QRectF &from, const QRectF &to, qreal t)
{
    return {zzLerp(from.x(), to.x(), t), zzLerp(from.y(), to.y(), t),
        zzLerp(from.width(), to.width(), t), zzLerp(from.height(), to.height(), t)};
}

QColor zzColor(const QColor &overrideColor, const QColor &fallback)
{
    return overrideColor.isValid() ? overrideColor : fallback;
}

/** @brief 胶囊与导航外观下标签填充的圆角半径，单位逻辑像素。 */
constexpr qreal zzTabPillCornerRadius = 2.0;

/** @brief 标签选中层在深色主题下的中性填充色。 */
QColor zzTabNeutralSelectedDark()
{
    return QColor::fromRgb(82, 82, 84);;
}
/** @brief 标签选中层在浅色主题下的中性填充色。 */
QColor zzTabNeutralSelectedLight()
{
    return QColor::fromRgb(206, 206, 206);;
}
/** @brief 标签外描边在深色主题下的半透明白色。 */
QColor zzTabStrokeDark()
{
    return QColor::fromRgb(255, 255, 255, 18);;
}
/** @brief 标签外描边在浅色主题下的半透明黑色。 */
QColor zzTabStrokeLight()
{
    return QColor::fromRgb(0, 0, 0, 15);;
}
/** @brief WinUI3 分段选中块的半透明白色。 */
QColor zzTabSegmentedSelectedFill(bool dark)
{
    return QColor::fromRgb(255, 255, 255, dark ? 15 : 179);;
}
/** @brief 标签悬停填充在深色主题下的半透明白色。 */
QColor zzTabHoverFillDark()
{
    return QColor::fromRgb(255, 255, 255, 31);;
}
/** @brief 标签悬停填充在浅色主题下的半透明黑色。 */
QColor zzTabHoverFillLight()
{
    return QColor::fromRgb(0, 0, 0, 18);;
}
/** @brief 分段轨道背景的半透明黑色，深浅主题使用不同浓度。 */
QColor zzTabTrackFill(bool dark)
{
    return QColor::fromRgb(0, 0, 0, dark ? 26 : 6);;
}
} // namespace

ZzTabBarAppearance ZzTabBarStylePrivate::appearance(const QWidget *widget)
{
    return ZzControlAppearance::tabBarAppearance(qobject_cast<const QTabBar *>(widget));
}

bool ZzTabBarStylePrivate::vertical(QTabBar::Shape shape)
{
    return shape == QTabBar::RoundedWest || shape == QTabBar::RoundedEast
        || shape == QTabBar::TriangularWest || shape == QTabBar::TriangularEast;
}

QSize ZzTabBarStylePrivate::sizeHint(const QStyleOptionTab &tab, QSize base, const QWidget *widget)
{
    const auto value = appearance(widget);
    const bool rotated = vertical(tab.shape);
    if (rotated) base.transpose();
    const int visualHeight = qMax(tab.fontMetrics.height(), tab.icon.isNull() ? 0 : tab.iconSize.height());
    const int buttonHeight = rotated && value != Appearance::Navigation
        ? qMax(tab.leftButtonSize.width(), tab.rightButtonSize.width())
        : qMax(tab.leftButtonSize.height(), tab.rightButtonSize.height());
    int padding = zzPivot(value) ? 18 : 12;
    if (value == Appearance::SegmentedSlide || value == Appearance::SegmentedFade) padding = 20;
    base.setHeight(qMax(value == Appearance::SegmentedSlide || value == Appearance::SegmentedFade ? 42 : 32,
        qMax(visualHeight + padding, buttonHeight + 8)));
    if (value == Appearance::Capsule) base.rwidth() += 30;
    if (value == Appearance::SegmentedWinUI3 && !tab.text.isEmpty() && !tab.icon.isNull())
        base.setWidth(qMax(base.width(), 130));
    if (value == Appearance::Navigation && rotated) {
        int width = 0;
        if (const auto *bar = qobject_cast<const QTabBar *>(widget)) {
            for (int i = 0; i < bar->count(); ++i) {
                if (!bar->isTabVisible(i)) continue;
                int measured = tab.fontMetrics.size(Qt::TextShowMnemonic, bar->tabText(i)).width();
                if (!bar->tabIcon(i).isNull()) measured += bar->iconSize().width() + 8;
                for (auto position : {QTabBar::LeftSide, QTabBar::RightSide})
                    if (const auto *button = bar->tabButton(i, position)) measured += button->sizeHint().width() + 4;
                width = qMax(width, measured);
            }
        }
        return {qMax(40, width + 13), qMax(40, qMax(visualHeight, buttonHeight) + 12)};
    }
    if (rotated) base.transpose();
    return base;
}

ZzTabBarStylePrivate::ZzTabBarStylePrivate(QTabBar *bar, ZzFluentStyle *style)
    : QObject(style), bar_(bar), style_(style), selection_(this), pressure_(this)
{
    selection_.setStartValue(0.0);
    selection_.setEndValue(1.0);
    pressure_.setEasingCurve(QEasingCurve::OutCubic);
    connect(&selection_, &QVariantAnimation::valueChanged, this,
        [this](const QVariant &value) { advance(value.toReal()); });
    connect(&pressure_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        pressureValue_ = value.toReal();
        if (bar_) bar_->update();
    });
    connect(bar, &QTabBar::currentChanged, this, [this] { select(); });
    connect(bar, &QTabBar::tabMoved, this, [this] { settle(); });
    bar->installEventFilter(this);
    settle();
}

QRectF ZzTabBarStylePrivate::itemRect(int index) const
{
    if (!bar_ || index < 0 || index >= bar_->count() || !bar_->isTabVisible(index)) return {};
    QRectF rect(bar_->tabRect(index));
    if (appearance_ == Appearance::SegmentedSlide || appearance_ == Appearance::SegmentedFade) {
        int first = 0, last = bar_->count() - 1;
        while (first < bar_->count() && !bar_->isTabVisible(first)) ++first;
        while (last >= 0 && !bar_->isTabVisible(last)) --last;
        int before = index == first ? 4 : 2;
        int after = index == last ? 4 : 2;
        if (vertical(bar_->shape())) rect.adjust(4, before, -4, -after);
        else {
            if (bar_->isRightToLeft()) qSwap(before, after);
            rect.adjust(before, 4, -after, -4);
        }
    } else if (appearance_ == Appearance::Navigation && vertical(bar_->shape())) {
        int first = 0, last = bar_->count() - 1;
        while (first < bar_->count() && !bar_->isTabVisible(first)) ++first;
        while (last >= 0 && !bar_->isTabVisible(last)) --last;
        rect.adjust(0, index == first ? 0 : 2, 0, index == last ? 0 : -2);
    } else if (appearance_ == Appearance::Pill || appearance_ == Appearance::Navigation) {
        rect.adjust(2, 2, -2, -2);
    }
    return rect;
}

QRectF ZzTabBarStylePrivate::selectionRect(int index) const
{
    const auto rect = itemRect(index);
    if (rect.isEmpty()) return {};
    if (appearance_ == Appearance::Navigation && vertical(bar_->shape())) {
        const bool east = bar_->shape() == QTabBar::RoundedEast || bar_->shape() == QTabBar::TriangularEast;
        return {east ? rect.right() - 3 : rect.left(), rect.top() + 8, 3, qMax(0.0, rect.height() - 16)};
    }
    if (!zzPivot(appearance_) && appearance_ != Appearance::SegmentedWinUI3) return rect;
    const bool winui = appearance_ == Appearance::SegmentedWinUI3;
    const qreal extent = winui ? 16 : 24;
    const qreal margin = winui ? 4 : (appearance_ == Appearance::PivotStretch ? 20 : 15);
    // 短标签也保留至少 8 px 指示条；普通标签继续使用原版边距。
    const auto indicatorLength = [extent, margin](qreal length) {
        const qreal inset = qMin(margin, qMax(0.0, (length - 8) / 2));
        return qMin(extent, qMax(0.0, length - 2 * inset));
    };
    if (vertical(bar_->shape())) {
        const qreal height = indicatorLength(rect.height());
        const bool west = bar_->shape() == QTabBar::RoundedWest || bar_->shape() == QTabBar::TriangularWest;
        return {west ? rect.right() - 4 : rect.left() + 1, rect.center().y() - height / 2, 3, height};
    }
    const qreal width = indicatorLength(rect.width());
    const bool south = bar_->shape() == QTabBar::RoundedSouth || bar_->shape() == QTabBar::TriangularSouth;
    return {rect.center().x() - width / 2, south ? rect.top() + 1 : rect.bottom() - 4, width, 3};
}

void ZzTabBarStylePrivate::settle()
{
    selection_.stop();
    pressure_.stop();
    pressed_ = -1;
    pressureValue_ = 0;
    if (!bar_) return;
    appearance_ = appearance(bar_);
    current_ = target_ = from_ = selectionRect(bar_->currentIndex());
    layout_.clear();
    weights_.fill(0, bar_->count());
    for (int i = 0; i < bar_->count(); ++i) {
        layout_.append(bar_->isTabVisible(i) ? bar_->tabRect(i) : QRect());
        if (i == bar_->currentIndex()) weights_[i] = 1;
    }
    fromWeights_ = weights_;
    bar_->update();
}

void ZzTabBarStylePrivate::synchronize()
{
    if (!bar_) return;
    bool changed = appearance_ != appearance(bar_) || layout_.size() != bar_->count()
        || target_ != selectionRect(bar_->currentIndex());
    for (int i = 0; !changed && i < bar_->count(); ++i)
        changed = layout_[i] != (bar_->isTabVisible(i) ? bar_->tabRect(i) : QRect());
    if (changed || (style_->themeSnapshot()->reducedMotion()
        && (selection_.state() == QAbstractAnimation::Running || pressure_.state() == QAbstractAnimation::Running))) settle();
}

void ZzTabBarStylePrivate::select()
{
    if (!bar_) return;
    const auto next = selectionRect(bar_->currentIndex());
    const auto snapshot = style_->themeSnapshot();
    if (appearance_ != appearance(bar_) || next.isEmpty() || current_.isEmpty()
        || !bar_->isVisible() || !bar_->isEnabled() || snapshot->duration(ZzMotionToken::Normal) <= 0
        || appearance_ == Appearance::Standard || appearance_ == Appearance::Capsule || appearance_ == Appearance::Pill) {
        settle();
        return;
    }
    selection_.stop();
    from_ = current_;
    fromWeights_ = weights_;
    target_ = next;
    int duration = 300;
    if (appearance_ == Appearance::PivotSlide) duration = 200;
    if (appearance_ == Appearance::PivotStretch) duration = 450;
    if (appearance_ == Appearance::SegmentedFade) duration = 600;
    if (appearance_ == Appearance::SegmentedWinUI3) duration = 120;
    if (appearance_ == Appearance::Navigation) duration = 160;
    const bool linear = appearance_ == Appearance::PivotGrow || appearance_ == Appearance::PivotStretch
        || appearance_ == Appearance::SegmentedSlide;
    selection_.setEasingCurve(appearance_ == Appearance::SegmentedFade ? QEasingCurve::InOutCubic
        : linear ? QEasingCurve::Linear : QEasingCurve::OutCubic);
    selection_.setDuration(duration);
    selection_.start();
    advance(0);
}

void ZzTabBarStylePrivate::advance(qreal t)
{
    if (!bar_) return;
    current_ = zzLerpRect(from_, target_, t);
    const bool alongY = vertical(bar_->shape());
    if (appearance_ == Appearance::PivotGrow || appearance_ == Appearance::Navigation) {
        current_ = target_;
        if (alongY) { current_.setHeight(target_.height() * t); current_.moveCenter(target_.center()); }
        else { current_.setWidth(target_.width() * t); current_.moveCenter(target_.center()); }
    } else if (appearance_ == Appearance::PivotStretch) {
        // 原版前 66% 在原标签内蓄力伸长，后段在目标处回收并轻微回弹。
        // from_ 始终取当前显示矩形，连续点击不会回跳到上一次标签起点。
        const auto clamp = [](qreal value) { return qBound(0.0, value, 1.0); };
        const auto smooth = [clamp](qreal value) { value = clamp(value); return value * value * (3 - 2 * value); };
        const auto cubic = [clamp](qreal value) { value = 1 - clamp(value); return 1 - value * value * value; };
        const auto back = [clamp](qreal value) { value = clamp(value) - 1; return 1 + 2.15 * value * value * value + 1.15 * value * value; };
        const qreal start = alongY ? from_.top() : from_.left();
        const qreal startEnd = alongY ? from_.bottom() : from_.right();
        const qreal end = alongY ? target_.top() : target_.left();
        const qreal endEnd = alongY ? target_.bottom() : target_.right();
        const bool forward = end >= start;
        qreal leading = start, trailing = startEnd;
        if (qAbs(end - start) < 0.5) {
            leading = zzLerp(start, end, smooth(t));
            trailing = zzLerp(startEnd, endEnd, smooth(t));
        } else if (t < 0.66) {
            const qreal released = clamp((t / 0.66 - 0.34) / 0.66);
            const qreal stretch = cubic(released * released);
            if (forward) trailing += 20 * stretch;
            else leading -= 20 * stretch;
        } else {
            const qreal delayed = clamp(((t - 0.66) / 0.34 - 0.04) / 0.96);
            const qreal settled = smooth(delayed * delayed * delayed);
            const qreal span = qMin((endEnd - end) * 0.32, 20.0);
            const qreal bounce = qSin(settled * M_PI) * (1 - settled) * 0.9;
            leading = forward ? zzLerp(end - span, end, cubic(settled)) : zzLerp(end - bounce, end, back(settled));
            trailing = forward ? zzLerp(endEnd + bounce, endEnd, back(settled)) : zzLerp(endEnd + span, endEnd, cubic(settled));
        }
        if (alongY) {
            current_.setTop(leading);
            current_.setBottom(trailing);
        } else {
            current_.setLeft(leading);
            current_.setRight(trailing);
        }
    }
    weights_.resize(bar_->count());
    for (int i = 0; i < weights_.size(); ++i)
        weights_[i] = zzLerp(fromWeights_.value(i), i == bar_->currentIndex() ? 1 : 0, t);
    bar_->update();
}

void ZzTabBarStylePrivate::press(int index)
{
    if (!bar_) return;
    if (!zzSegmented(appearance_) && appearance_ != Appearance::Navigation) return;
    pressure_.stop();
    if (index >= 0) pressed_ = index;
    const qreal target = index < 0 ? 0 : 1;
    if (style_->themeSnapshot()->duration(ZzMotionToken::Normal) <= 0) {
        pressureValue_ = target;
        bar_->update();
        return;
    }
    pressure_.setStartValue(pressureValue_);
    pressure_.setEndValue(target);
    pressure_.setDuration(120);
    pressure_.start();
}

bool ZzTabBarStylePrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (!qobject_cast<QTabBar *>(watched) || watched != bar_) return false;
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const int index = bar_->tabAt(mouse->position().toPoint());
        if (mouse->button() == Qt::LeftButton && index >= 0 && bar_->isTabEnabled(index)) press(index);
        break;
    }
    case QEvent::MouseButtonRelease:
    case QEvent::UngrabMouse:
    case QEvent::Leave: press(-1); break;
    case QEvent::Hide:
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::LayoutDirectionChange:
    case QEvent::EnabledChange:
    case QEvent::FontChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange: settle(); break;
    default: break;
    }
    return false;
}

void ZzTabBarStylePrivate::drawLabel(const QStyleOptionTab &tab, QPainter *painter, const QColor &text)
{
    painter->save();
    QRect rect = tab.rect;
    const bool rotated = vertical(tab.shape) && appearance_ != Appearance::Navigation;
    if (rotated) {
        const bool west = tab.shape == QTabBar::RoundedWest || tab.shape == QTabBar::TriangularWest;
        painter->translate(west ? rect.bottomLeft() + QPoint(0, 1) : rect.topRight() + QPoint(1, 0));
        painter->rotate(west ? -90 : 90);
        rect = QRect(0, 0, rect.height(), rect.width());
    }
    const bool navigation = appearance_ == Appearance::Navigation && vertical(tab.shape);
    const bool selected = tab.state.testFlag(QStyle::State_Selected);
    int verticalShift = style_->pixelMetric(QStyle::PM_TabBarTabShiftVertical, &tab, bar_);
    if (tab.shape == QTabBar::RoundedSouth || tab.shape == QTabBar::TriangularSouth) verticalShift = -verticalShift;
    const int horizontalShift = style_->pixelMetric(QStyle::PM_TabBarTabShiftHorizontal, &tab, bar_);
    QRect content = navigation ? rect.adjusted(5, 4, -8, -4)
        : rect.adjusted(12, (selected ? 0 : verticalShift) + 6, (selected ? 0 : horizontalShift) - 12, -6);
    if (!tab.leftButtonSize.isEmpty()) content.adjust((rotated ? tab.leftButtonSize.height() : tab.leftButtonSize.width()) + 4, 0, 0, 0);
    if (!tab.rightButtonSize.isEmpty()) content.adjust(0, 0, -(rotated ? tab.rightButtonSize.height() : tab.rightButtonSize.width()) - 4, 0);
    const QSize iconSize = tab.icon.isNull() ? QSize(0, 0) : tab.icon.actualSize(tab.iconSize,
        tab.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled);
    const bool winui = appearance_ == Appearance::SegmentedWinUI3;
    const int gap = !tab.text.isEmpty() && !tab.icon.isNull() ? (navigation ? 8 : winui ? 6 : 4) : 0;
    const int textWidth = qMax(0, qMin(content.width() - iconSize.width() - gap,
        tab.fontMetrics.size(Qt::TextShowMnemonic, tab.text).width()));
    const int totalWidth = textWidth + iconSize.width() + gap;
    const bool iconOnly = tab.text.isEmpty();
    const QRect centeredArea = tab.leftButtonSize.isEmpty() && tab.rightButtonSize.isEmpty() ? rect : content;
    const int left = winui ? centeredArea.left() + (centeredArea.width() - totalWidth) / 2
        : iconOnly ? centeredArea.center().x() - iconSize.width() / 2 : content.left();
    QRect iconRect(left, content.center().y() - iconSize.height() / 2, iconSize.width(), iconSize.height());
    QRect textRect = content;
    if (!tab.icon.isNull()) textRect.setLeft(left + iconSize.width() + gap);
    if (winui) {
        iconRect.moveTop(rect.center().y() - iconSize.height() / 2);
        textRect = QRect(left + iconSize.width() + (gap ? gap - 1 : 0), rect.top(), textWidth, rect.height());
    } else if (iconOnly) iconRect.moveTop(rect.center().y() - iconSize.height() / 2);
    // Qt 的纵向标签按钮不随 RTL 镜像；旋转文字必须保留同一端的占位。
    if (!rotated) {
        iconRect = QStyle::visualRect(tab.direction, rect, iconRect);
        textRect = QStyle::visualRect(tab.direction, rect, textRect);
    }
    if (!tab.icon.isNull()) {
        const auto mode = tab.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
        const auto state = tab.state.testFlag(QStyle::State_Selected) ? QIcon::On : QIcon::Off;
        const bool tint = ZzSegoeIconFont::usesForegroundColor(tab.icon)
            || style_->themeSnapshot()->mode() == ZzThemeMode::HighContrast;
        const auto icon = tint ? ZzSegoeIconFont::withForegroundColor(tab.icon, text) : tab.icon;
        icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);
    }
    auto palette = tab.palette;
    palette.setColor(QPalette::WindowText, text);
    const bool enabled = tab.state.testFlag(QStyle::State_Enabled);
    int flags = static_cast<int>(Qt::AlignVCenter)
        | static_cast<int>((navigation || appearance_ == Appearance::Capsule) ? Qt::AlignLeading : Qt::AlignHCenter)
        | static_cast<int>(Qt::TextShowMnemonic);
    if (!style_->styleHint(QStyle::SH_UnderlineShortcut, &tab, bar_)) flags |= static_cast<int>(Qt::TextHideMnemonic);
    style_->drawItemText(painter, textRect, flags, palette, enabled,
        tab.fontMetrics.elidedText(tab.text, bar_->elideMode(), textWidth, Qt::TextShowMnemonic), QPalette::WindowText);
    painter->restore();
    if (tab.state.testFlag(QStyle::State_HasFocus)) {
        QStyleOptionFocusRect focus;
        focus.QStyleOption::operator=(tab);
        focus.rect = tab.rect.adjusted(2, 2, -2, -2);
        style_->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, bar_);
    }
}

void ZzTabBarStylePrivate::draw(const QStyleOptionTab &tab, QPainter *painter)
{
    synchronize();
    // Qt 为拖动快照重定位 option.rect，但 tabIndex 仍指向原标签。
    const int index = tab.tabIndex >= 0 ? tab.tabIndex : bar_->tabAt(tab.rect.center());
    if (index < 0 || index >= bar_->count()) return;
    const QPoint offset = tab.rect.topLeft() - bar_->tabRect(index).topLeft();
    const bool moving = tab.position == QStyleOptionTab::Moving || !offset.isNull();
    const auto snapshot = style_->themeSnapshot();
    const bool hc = snapshot->mode() == ZzThemeMode::HighContrast;
    const bool dark = snapshot->mode() == ZzThemeMode::Dark;
    const bool enabled = tab.state.testFlag(QStyle::State_Enabled);
    const bool selected = tab.state.testFlag(QStyle::State_Selected);
    const bool hovered = enabled && tab.state.testFlag(QStyle::State_MouseOver);
    const bool segmented = zzSegmented(appearance_);
    const bool pressed = enabled && index == pressed_ && pressureValue_ > 0;
    ZzTabBarColors colors;
    if (segmented && !hc) {
        colors = ZzControlAppearance::tabBarColors(bar_);
        if (dark) {
            const auto overrides = ZzControlAppearance::tabBarColors(bar_, true);
            colors = {zzColor(overrides.background, colors.background), zzColor(overrides.selected, colors.selected),
                zzColor(overrides.hover, colors.hover), zzColor(overrides.pressed, colors.pressed),
                zzColor(overrides.text, colors.text), zzColor(overrides.selectedText, colors.selectedText)};
        }
    }
    const QColor accent = ZzControlAppearancePrivate::accent(tab.palette);
    // 标签选中层与页面 Base 区分；否则浅色分段轨道和选中块会融为一体。
    const QColor neutralSelected = dark ? zzTabNeutralSelectedDark() : zzTabNeutralSelectedLight();
    const QColor stroke = hc ? tab.palette.color(QPalette::WindowText)
        : (dark ? zzTabStrokeDark() : zzTabStrokeLight());
    const QColor selectedFill = zzColor(colors.selected, hc ? tab.palette.color(QPalette::Highlight)
        : (appearance_ == Appearance::SegmentedWinUI3 ? zzTabSegmentedSelectedFill(dark) : neutralSelected));
    const QColor hoverFill = zzColor(colors.hover, dark ? zzTabHoverFillDark() : zzTabHoverFillLight());
    const QColor pressFill = zzColor(colors.pressed, snapshot->color(ZzColorToken::ControlFillPressed));
    QColor foreground = zzColor(colors.text, selected || hovered ? tab.palette.color(QPalette::WindowText)
        : snapshot->color(ZzColorToken::TextSecondary));
    if (selected && !zzPivot(appearance_)) foreground = zzColor(colors.selectedText, hc ? tab.palette.color(QPalette::HighlightedText)
        : (colors.selected.isValid() ? ZzControlAppearancePrivate::contrastingText(selectedFill) : tab.palette.color(QPalette::WindowText)));
    const QColor tabText = bar_->tabTextColor(index);
    if (!hc && tabText.isValid() && !(selected ? colors.selectedText : colors.text).isValid())
        foreground = tabText;
    if (!enabled) foreground = tab.palette.color(QPalette::Disabled, QPalette::WindowText);
    const auto rect = itemRect(index).translated(offset);
    const QRectF selectionRectToDraw = moving
        ? (selected ? selectionRect(index).translated(offset) : QRectF()) : current_;
    const bool round = segmented && ZzControlAppearance::isTabBarRounded(bar_);
    const qreal radius = round ? qMin(rect.width(), rect.height()) / 2 : 4;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QRect clip = tab.rect;
    if (appearance_ == Appearance::Capsule && selected) {
        if (vertical(tab.shape)) clip.adjust(0, -7, 0, 7);
        else clip.adjust(-7, 0, 7, 0);
    }
    painter->setClipRect(clip, Qt::IntersectClip);
    auto rounded = [&](const QRectF &r, const QColor &fill, qreal rads) {
        if (r.isEmpty()) return;
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(r, rads, rads);
    };
    if (segmented) {
        QRectF track;
        for (const auto &r : layout_) if (!r.isEmpty()) track = track.united(r);
        if (moving) track = tab.rect;
        painter->setPen(stroke);
        painter->setBrush(zzColor(colors.background, hc ? tab.palette.color(QPalette::Window) : zzTabTrackFill(dark)));
        const qreal trackRadius = round ? qMin(track.width(), track.height()) / 2 : 4;
        painter->drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), trackRadius, trackRadius);
    }
    if (appearance_ == Appearance::Capsule) {
        if (selected || hovered) {
            // 浏览器式上圆角页签，页面侧保持平直；变换覆盖四种位置。
            painter->save();
            QRectF local(0, 0, rect.width(), rect.height());
            painter->translate(rect.topLeft());
            if (vertical(tab.shape)) {
                const bool west = tab.shape == QTabBar::RoundedWest || tab.shape == QTabBar::TriangularWest;
                painter->translate(west ? 0 : rect.width(), west ? rect.height() : 0);
                painter->rotate(west ? -90 : 90);
                local.setSize({rect.height(), rect.width()});
            } else if (tab.shape == QTabBar::RoundedSouth || tab.shape == QTabBar::TriangularSouth) {
                painter->translate(rect.width(), rect.height()); painter->rotate(180);
            }
            QPainterPath path;
            path.moveTo(selected ? -7 : 0, local.height());
            if (selected) path.quadTo(0, local.height(), 0, local.height() - 7);
            path.lineTo(0, 7); path.quadTo(0, 0, 7, 0);
            path.lineTo(local.width() - 7, 0); path.quadTo(local.width(), 0, local.width(), 7);
            if (selected) {
                path.lineTo(local.width(), local.height() - 7);
                path.quadTo(local.width(), local.height(), local.width() + 7, local.height());
            } else path.lineTo(local.width(), local.height());
            path.closeSubpath();
            painter->fillPath(path, selected ? selectedFill : hoverFill);
            painter->restore();
        }
    } else if (appearance_ == Appearance::Pill || (appearance_ == Appearance::Navigation && !vertical(tab.shape))) {
        painter->setPen(stroke);
        painter->setBrush(selected ? selectedFill : hovered ? hoverFill : tab.palette.color(QPalette::Window));
        painter->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), zzTabPillCornerRadius, zzTabPillCornerRadius);
    } else if (!zzPivot(appearance_)) {
        if (pressed || hovered) rounded(rect, pressed ? pressFill : hoverFill, radius);
        if (appearance_ == Appearance::SegmentedSlide) rounded(selectionRectToDraw.adjusted(0.5, 0.5, -0.5, -0.5), selectedFill, radius);
        else if (appearance_ == Appearance::SegmentedFade) {
            painter->setOpacity(weights_.value(index)); rounded(rect, selectedFill, radius); painter->setOpacity(1);
        } else if (selected) {
            rounded(appearance_ == Appearance::Navigation ? rect : rect.adjusted(0.5, 0.5, -0.5, -0.5), selectedFill, radius);
            if (appearance_ == Appearance::SegmentedWinUI3) {
                painter->setPen(stroke);
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
            }
        }
    }
    if (zzPivot(appearance_) || (appearance_ == Appearance::Navigation && vertical(tab.shape))) {
        rounded(selectionRectToDraw, enabled ? (hc && appearance_ == Appearance::Navigation ? foreground : accent)
            : tab.palette.color(QPalette::Disabled, QPalette::WindowText), 1.5);
    } else if (appearance_ == Appearance::SegmentedWinUI3) {
        auto indicator = selectionRect(index).translated(offset);
        const auto center = indicator.center();
        const qreal scale = weights_.value(index) * (1 - (pressed ? 0.5 * pressureValue_ : 0));
        if (vertical(tab.shape)) indicator.setHeight(indicator.height() * scale);
        else indicator.setWidth(indicator.width() * scale);
        indicator.moveCenter(center);
        rounded(indicator, hc && selected ? foreground : accent, 1.5);
    }
    painter->restore();
    drawLabel(tab, painter, foreground);
}

} // namespace ZzFluentUI
