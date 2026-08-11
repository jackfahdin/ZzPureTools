#include "ZzNavigationViewPrivate.h"

#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QVariantAnimation>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionViewItem>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzNavigationPrivateRoles.h"
#include "ZzItemViewVisual.h"

namespace ZzFluentUI {

namespace {

constexpr int zzRegularItemHeight = 40;
constexpr int zzCompactItemHeight = 32;
constexpr int zzRegularIconExtent = 18;
constexpr int zzCompactIconExtent = 20;
constexpr int zzHoverAccentAlpha = 32;
constexpr int zzNavigationBadgeRadius = 10;

} // namespace

/** @brief 绘制导航专用展示角色，无角色时复用通用 Fluent delegate。 */
class ZzNavigationItemDelegate final : public QStyledItemDelegate
{
public:
    /** @brief 创建标准密度且拥有通用 fallback 的 private delegate。 */
    explicit ZzNavigationItemDelegate(QObject *parent)
        : QStyledItemDelegate(parent)
        , fallback_(this)
    {
    }

    /** @brief 同步固定行高和通用 fallback 密度。 */
    void setCompact(bool compact)
    {
        compact_ = compact;
        fallback_.setDensity(
            compact ? ZzItemDensity::Compact : ZzItemDensity::Standard);
    }

    /** @brief 绘制当前单个可见导航索引。 */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        Q_ASSERT(painter != nullptr);
        if (painter == nullptr) {
            return;
        }
        const bool sectionHeader = index
            .data(zzNavigationSectionHeaderRole)
            .toBool();
        const QVariant descriptorValue = index.data(static_cast<int>(
            ZzNavigationItemRole::Icon));
        const auto descriptor = descriptorValue
            .value<ZzIconDescriptor>();
        const bool hasDescriptor = descriptorValue
            .canConvert<ZzIconDescriptor>()
            && !descriptor.resourceId.trimmed().isEmpty();
        const QString badge = index.data(static_cast<int>(
            ZzNavigationItemRole::Badge)).toString();
        if (!sectionHeader && !hasDescriptor && badge.isEmpty()) {
            fallback_.paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem adjusted = option;
        initStyleOption(&adjusted, index);
        painter->save();
        if (sectionHeader) {
            drawSection(painter, adjusted, compact_);
        } else {
            drawDestination(
                painter, adjusted, descriptor, hasDescriptor, badge);
        }
        painter->restore();
    }

    /** @brief 返回与通用导航 view 一致的固定逻辑行高。 */
    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        QSize result = fallback_.sizeHint(option, index);
        result.setHeight(
            compact_ ? zzCompactItemHeight : zzRegularItemHeight);
        return result;
    }

private:
    /** @brief 绘制常规分区标题或紧凑模式的无文字分隔线。 */
    static void drawSection(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        bool compact)
    {
        if (compact) {
            constexpr int separatorWidth = 20;
            const int centerX = option.rect.center().x();
            painter->setPen(option.palette.color(QPalette::Mid));
            painter->drawLine(
                centerX - separatorWidth / 2,
                option.rect.center().y(),
                centerX + separatorWidth / 2,
                option.rect.center().y());
            return;
        }
        const QRect content = option.rect.adjusted(10, 4, -10, -4);
        QFont font = option.font;
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(option.palette.color(QPalette::WindowText));
        const QString text = option.fontMetrics.elidedText(
            option.text, Qt::ElideRight, content.width());
        painter->drawText(
            content,
            Qt::AlignLeading | Qt::AlignVCenter,
            text);
    }

    /** @brief 绘制带可选 descriptor 和 badge 的导航目标行。 */
    void drawDestination(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const ZzIconDescriptor &descriptor,
        bool hasDescriptor,
        const QString &badge) const
    {
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool enabled = option.state.testFlag(QStyle::State_Enabled);
        const QPalette::ColorGroup colorGroup = enabled
            ? QPalette::Normal : QPalette::Disabled;
        QColor foreground = option.palette.color(colorGroup, QPalette::Text);
        QRect content;
        const auto *fluentStyle = option.widget != nullptr
            ? qobject_cast<const ZzFluentStyle *>(option.widget->style())
            : nullptr;
        if (fluentStyle != nullptr) {
            ZzItemViewVisualOptions visualOptions;
            if (const auto *navigationView = qobject_cast<
                    const ZzNavigationView *>(option.widget)) {
                visualOptions.forceIndicator =
                    navigationView->d_ptr->forcesIndicator(option.index);
                visualOptions.indicatorScale =
                    navigationView->d_ptr->indicatorScale(
                        option.index,
                        selected);
            }
            const ZzItemViewVisualLayout layout = ZzItemViewVisual::draw(
                *fluentStyle,
                option,
                painter,
                visualOptions);
            content = layout.contentRect;
        } else {
            if (selected) {
                painter->fillRect(
                    option.rect,
                    option.palette.color(QPalette::Highlight));
                foreground = option.palette.color(
                    colorGroup,
                    QPalette::HighlightedText);
            } else if (hovered) {
                painter->fillRect(
                    option.rect,
                    option.palette.color(QPalette::AlternateBase));
                QColor accentTint = option.palette.color(QPalette::Highlight);
                accentTint.setAlpha(zzHoverAccentAlpha);
                painter->fillRect(option.rect, accentTint);
            }
            content = option.rect;
        }
        content.adjust(
            option.direction == Qt::RightToLeft
                ? (compact_ ? 6 : 8) : 0,
            3,
            option.direction == Qt::RightToLeft
                ? 0 : (compact_ ? -6 : -8),
            -3);

        if (compact_) {
            drawCompactContent(
                painter, option, content, descriptor, hasDescriptor,
                badge, foreground);
        } else {
            drawRegularContent(
                painter, option, content, descriptor, hasDescriptor,
                badge, foreground, selected);
        }
        drawFocus(painter, option);
    }

    /** @brief 绘制常规模式的 leading icon、标题和 trailing badge。 */
    static void drawRegularContent(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QRect &content,
        const ZzIconDescriptor &descriptor,
        bool hasDescriptor,
        const QString &badge,
        const QColor &foreground,
        bool selected)
    {
        constexpr int spacing = 8;
        int leading = content.left();
        int trailing = content.right();
        const QPixmap icon = navigationIcon(
            option,
            descriptor,
            hasDescriptor,
            QSize(zzRegularIconExtent, zzRegularIconExtent),
            foreground);
        if (!icon.isNull()) {
            const QRect logicalIcon(
                leading,
                content.center().y() - zzRegularIconExtent / 2,
                zzRegularIconExtent,
                zzRegularIconExtent);
            const QRect iconRect = QStyle::visualRect(
                option.direction, option.rect, logicalIcon);
            painter->drawPixmap(iconRect, icon);
            leading += zzRegularIconExtent + spacing;
        }

        if (!badge.isEmpty()) {
            const int measured = option.fontMetrics.horizontalAdvance(badge);
            const int badgeWidth = std::clamp(measured + 12, 22, 72);
            const QRect logicalBadge(
                trailing - badgeWidth + 1,
                content.center().y() - 10,
                badgeWidth,
                20);
            const QRect badgeRect = QStyle::visualRect(
                option.direction, option.rect, logicalBadge);
            const QColor badgeBackground = selected
                ? option.palette.color(QPalette::Base)
                : option.palette.color(QPalette::Highlight);
            const QColor badgeForeground = selected
                ? option.palette.color(QPalette::Text)
                : option.palette.color(QPalette::HighlightedText);
            painter->setPen(badgeForeground);
            painter->setBrush(badgeBackground);
            painter->drawRoundedRect(
                badgeRect,
                zzNavigationBadgeRadius,
                zzNavigationBadgeRadius);
            painter->drawText(
                badgeRect.adjusted(4, 0, -4, 0),
                Qt::AlignCenter,
                option.fontMetrics.elidedText(
                    badge, Qt::ElideRight, badgeRect.width() - 8));
            trailing -= badgeWidth + spacing;
        }

        const QRect logicalText(
            leading,
            content.top(),
            std::max(0, trailing - leading + 1),
            content.height());
        const QRect textRect = QStyle::visualRect(
            option.direction, option.rect, logicalText);
        painter->setPen(foreground);
        painter->setFont(option.font);
        painter->drawText(
            textRect,
            Qt::AlignLeading | Qt::AlignVCenter,
            option.fontMetrics.elidedText(
                option.text, Qt::ElideRight, textRect.width()));
    }

    /** @brief 绘制紧凑模式的居中 icon 和 trailing 状态点。 */
    static void drawCompactContent(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QRect &content,
        const ZzIconDescriptor &descriptor,
        bool hasDescriptor,
        const QString &badge,
        const QColor &foreground)
    {
        const QPixmap icon = navigationIcon(
            option,
            descriptor,
            hasDescriptor,
            QSize(zzCompactIconExtent, zzCompactIconExtent),
            foreground);
        if (!icon.isNull()) {
            const QRect iconRect(
                content.center().x() - zzCompactIconExtent / 2,
                content.center().y() - zzCompactIconExtent / 2,
                zzCompactIconExtent,
                zzCompactIconExtent);
            painter->drawPixmap(iconRect, icon);
        }
        if (!badge.isEmpty()) {
            const QRect logicalDot(
                content.right() - 7,
                content.top() + 1,
                7,
                7);
            const QRect dotRect = QStyle::visualRect(
                option.direction, option.rect, logicalDot);
            painter->setPen(option.palette.color(QPalette::Base));
            painter->setBrush(option.palette.color(QPalette::Highlight));
            painter->drawEllipse(dotRect);
        }
    }

    /** @brief 使用 Fluent cache 或标准 QIcon 返回当前状态图标。 */
    [[nodiscard]] static QPixmap navigationIcon(
        const QStyleOptionViewItem &option,
        const ZzIconDescriptor &descriptor,
        bool hasDescriptor,
        const QSize &logicalSize,
        const QColor &color)
    {
        const QWidget *const widget = option.widget;
        if (hasDescriptor && widget != nullptr) {
            if (auto *fluentStyle = qobject_cast<ZzFluentStyle *>(
                    widget->style())) {
                return fluentStyle->iconPixmap(
                    descriptor,
                    logicalSize,
                    widget->devicePixelRatioF(),
                    color,
                    option.direction);
            }
        }
        if (option.icon.isNull()) {
            return {};
        }
        const QIcon::Mode mode = option.state.testFlag(QStyle::State_Enabled)
            ? QIcon::Normal : QIcon::Disabled;
        const qreal dpr = widget != nullptr
            ? widget->devicePixelRatioF() : 1.0;
        return option.icon.pixmap(
            logicalSize, dpr, mode, QIcon::Off);
    }

    /** @brief 使用平台 primitive 绘制当前行焦点矩形。 */
    static void drawFocus(
        QPainter *painter,
        const QStyleOptionViewItem &option)
    {
        if (!option.state.testFlag(QStyle::State_HasFocus)) {
            return;
        }
        QStyleOptionFocusRect focus;
        focus.rect = option.rect.adjusted(1, 1, -1, -1);
        focus.state = option.state;
        focus.direction = option.direction;
        focus.palette = option.palette;
        focus.fontMetrics = option.fontMetrics;
        QStyle *style = option.widget != nullptr
            ? option.widget->style() : QApplication::style();
        style->drawPrimitive(
            QStyle::PE_FrameFocusRect,
            &focus,
            painter,
            option.widget);
    }

    ZzFluentItemDelegate fallback_;
    bool compact_ = false;
};

ZzNavigationViewPrivate::ZzNavigationViewPrivate(
    ZzNavigationView *publicObject) noexcept
    : q_ptr(publicObject)
    , transition(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    delegate = new ZzNavigationItemDelegate(q_ptr);
    q_ptr->setItemDelegate(delegate);
    QObject::connect(
        transition.animation(),
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this] {
            repaintTransitionIndexes();
        });
    QObject::connect(
        transition.animation(),
        &QVariantAnimation::finished,
        q_ptr,
        [this] {
            repaintTransitionIndexes();
        });
}

void ZzNavigationViewPrivate::activateIndex(
    const QModelIndex &index)
{
    if (!index.isValid()
        || !index.flags().testFlag(Qt::ItemIsEnabled)
        || !index.flags().testFlag(Qt::ItemIsSelectable)) {
        return;
    }
    Q_EMIT q_ptr->navigationRequested(index);
}

void ZzNavigationViewPrivate::setCompactPresentation(bool useCompact)
{
    if (delegate != nullptr) {
        delegate->setCompact(useCompact);
    }
}

void ZzNavigationViewPrivate::bindSelectionModel()
{
    QItemSelectionModel *const selectionModel = q_ptr->selectionModel();
    if (observedSelectionModel == selectionModel) {
        return;
    }

    QObject::disconnect(selectionChangedConnection);
    QObject::disconnect(modelResetConnection);
    observedSelectionModel = selectionModel;
    selectionChangedConnection = {};
    modelResetConnection = {};
    if (selectionModel == nullptr) {
        transition.transitionTo({}, 0);
        return;
    }

    selectionChangedConnection = QObject::connect(
        selectionModel,
        &QItemSelectionModel::selectionChanged,
        q_ptr,
        [this] {
            transitionToSelection();
        });
    if (selectionModel->model() != nullptr) {
        modelResetConnection = QObject::connect(
            selectionModel->model(),
            &QAbstractItemModel::modelReset,
            q_ptr,
            [this] {
                transition.transitionTo(selectedIndex(), 0);
                repaintTransitionIndexes();
            });
    }
    transition.transitionTo(selectedIndex(), 0);
    repaintTransitionIndexes();
}

qreal ZzNavigationViewPrivate::indicatorScale(
    const QModelIndex &index,
    bool staticallySelected) const noexcept
{
    return transition.scaleFor(index, staticallySelected);
}

bool ZzNavigationViewPrivate::forcesIndicator(
    const QModelIndex &index) const noexcept
{
    return transition.forcesIndicator(index);
}

void ZzNavigationViewPrivate::finishTransition()
{
    repaintTransitionIndexes();
    transition.finish();
    repaintTransitionIndexes();
}

QModelIndex ZzNavigationViewPrivate::selectedIndex() const
{
    if (observedSelectionModel == nullptr) {
        return {};
    }
    const QModelIndexList selected = observedSelectionModel->selectedIndexes();
    for (const QModelIndex &index : selected) {
        if (index.isValid() && index.column() == 0
            && index.flags().testFlag(Qt::ItemIsEnabled)
            && index.flags().testFlag(Qt::ItemIsSelectable)
            && !index.data(zzNavigationSectionHeaderRole).toBool()) {
            return index;
        }
    }
    return {};
}

void ZzNavigationViewPrivate::transitionToSelection()
{
    repaintTransitionIndexes();
    transition.transitionTo(selectedIndex(), transitionDuration());
    repaintTransitionIndexes();
}

void ZzNavigationViewPrivate::repaintTransitionIndexes() const
{
    QWidget *const viewport = q_ptr->viewport();
    if (viewport == nullptr) {
        return;
    }
    const auto updateIndex = [this, viewport](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QRect itemRect = q_ptr->visualRect(index);
        if (!itemRect.isEmpty()) {
            viewport->update(itemRect);
        }
    };
    updateIndex(transition.outgoingIndex());
    updateIndex(transition.incomingIndex());
}

int ZzNavigationViewPrivate::transitionDuration() const
{
    const auto *fluentStyle = qobject_cast<const ZzFluentStyle *>(
        q_ptr->style());
    if (fluentStyle == nullptr) {
        return 0;
    }
    const auto snapshot = fluentStyle->themeSnapshot();
    if (snapshot == nullptr) {
        return 0;
    }
    return ZzAnimationPolicy::adjustedDuration(
        snapshot->duration(ZzMotionToken::Normal),
        snapshot->reducedMotion(),
        false);
}

} // namespace ZzFluentUI
