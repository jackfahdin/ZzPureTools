#include <ZzFluentUI/ZzFluentStyle.h>

#include "private/ZzFluentStylePrivate.h"
#include "private/ZzTabBarStylePrivate.h"
#include "private/ZzControlAppearancePrivate.h"
#include "private/ZzItemViewVisual.h"
#include "private/ZzDataViewStylePrivate.h"

#include <QtCore/QThread>
#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QListView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QCalendarWidget>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtGui/QPainterPath>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

/** @brief 日历日期网格和普通图标网格不使用列表行式选择指示槽。 */
[[nodiscard]] bool zzUsesItemIndicatorStyle(
    const QWidget *widget, const QModelIndex &index)
{
    const auto *view = qobject_cast<const QAbstractItemView *>(widget);
    if (view == nullptr) {
        return false;
    }
    if (qobject_cast<const QCalendarWidget *>(view->parentWidget()) != nullptr) {
        return false;
    }
    const auto *list = qobject_cast<const QListView *>(view);
    return list == nullptr || list->viewMode() != QListView::IconMode
        || qobject_cast<const ZzFluentItemDelegate *>(view->itemDelegateForIndex(index)) != nullptr;
}

/** @brief 判断输入编辑器是否已经由父级组合控件统一绘制。 */
[[nodiscard]] bool zzInputParentOwnsSurface(
    const QWidget *widget) noexcept
{
    const auto *lineEdit = qobject_cast<const QLineEdit *>(widget);
    if (lineEdit == nullptr) {
        return false;
    }
    const QWidget *ancestor = lineEdit->parentWidget();
    while (ancestor != nullptr) {
        if (const auto *spinBox = qobject_cast<const QAbstractSpinBox *>(
                ancestor);
            spinBox != nullptr) {
            return spinBox->hasFrame();
        }
        if (const auto *comboBox = qobject_cast<const QComboBox *>(ancestor);
            comboBox != nullptr) {
            return comboBox->isEditable()
                && comboBox->lineEdit() == lineEdit;
        }
        ancestor = ancestor->parentWidget();
    }
    return false;
}

} // namespace

ZzFluentStyle::ZzFluentStyle(
    ZzThemeController *controller,
    QStyle *baseStyle)
    // 默认基样式固定为 Fusion：若回落到平台样式（如 WindowsVista），
    // 其按钮/下拉框文字会按系统主题取色，深色系统下把文字画成白色，
    // 与本样式的浅色令牌表面冲突（文字不可见）。
    : QProxyStyle(
          baseStyle != nullptr
              ? baseStyle
              : QStyleFactory::create(QStringLiteral("Fusion")))
    , d_ptr(std::make_unique<ZzFluentStylePrivate>(this, controller))
{
    QApplication::instance()->installEventFilter(this);
    // 构造即应用令牌 palette：不能等首次主题切换，否则在深色系统
    // （如 Windows 深色模式）下残留的深色 palette 会把按钮/下拉框
    // 文字画成浅色，叠在 Fluent 浅色表面上不可见。
    QApplication::setPalette(standardPalette());
}

ZzFluentStyle::~ZzFluentStyle()
{
    if (QApplication::instance() != nullptr) {
        QApplication::instance()->removeEventFilter(this);
    }
}

quint64 ZzFluentStyle::themeRevision() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->snapshot->revision();
}

std::shared_ptr<const ZzThemeSnapshot> ZzFluentStyle::themeSnapshot() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->snapshot;
}

int ZzFluentStyle::iconCacheBytes() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->cache.iconBytes();
}

bool ZzFluentStyle::isFocusVisualVisible(
    const QWidget *widget) const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->isFocusVisualVisible(widget);
}

QPixmap ZzFluentStyle::iconPixmap(
    const ZzIconDescriptor &descriptor,
    QSize logicalSize,
    qreal devicePixelRatio,
    QColor color,
    Qt::LayoutDirection direction)
{
    return d_ptr->iconPixmap(
        descriptor,
        logicalSize,
        devicePixelRatio,
        color,
        direction);
}

int ZzFluentStyle::pixelMetric(
    PixelMetric metric,
    const QStyleOption *option,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    switch (metric) {
    case PM_DefaultFrameWidth:
        // 给下拉列表留出圆角与边框空间，避免 viewport 覆盖弹出表面。
        if (widget != nullptr && widget->windowType() == Qt::Popup
            && d_ptr->isComboBoxPopupContext(widget)) {
            return qCeil(d_ptr->snapshot->metric(ZzMetricToken::CornerRadiusMedium)) + 2;
        }
        // 多行输入的 viewport 不能覆盖外层圆角和底部焦点线。
        if (qobject_cast<const QPlainTextEdit *>(widget) != nullptr
            || qobject_cast<const QTextEdit *>(widget) != nullptr) {
            return 4;
        }
        break;
    case PM_TreeViewIndentation:
        if (ZzDataViewStylePrivate::applies(widget)) {
            return 30;
        }
        break;
    case PM_ButtonMargin:
        return qRound(d_ptr->snapshot->metric(
            ZzMetricToken::HorizontalPadding));
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return 18;
    case PM_RadioButtonLabelSpacing:
    case PM_CheckBoxLabelSpacing:
        return 8;
    case PM_SliderLength:
        return 20;
    case PM_SliderThickness:
        return 24;
    case PM_ProgressBarChunkWidth:
        return 1;
    case PM_ScrollBarExtent:
        return 12;
    case PM_ScrollBarSliderMin:
    case PM_TabBarTabHSpace:
        return 24;
    case PM_TabBarTabVSpace:
        return 12;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return 2;
    case PM_MenuPanelWidth:
        return 1;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
    case PM_MenuBarHMargin:
    case PM_MenuBarVMargin:
        return 4;
    case PM_MenuBarItemSpacing:
        return 2;
    case PM_ToolBarFrameWidth:
        return 0;
    case PM_ToolBarHandleExtent:
        return 10;
    case PM_ToolBarItemSpacing:
    case PM_ToolBarItemMargin:
        return 4;
    case PM_ToolBarSeparatorExtent:
        return 8;
    case PM_ToolBarExtensionExtent:
        return 28;
    case PM_ToolBarIconSize:
        return qRound(d_ptr->snapshot->metric(
            ZzMetricToken::IconMedium));
    case PM_ToolTipLabelFrameWidth:
        return 8;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

int ZzFluentStyle::styleHint(
    StyleHint hint,
    const QStyleOption *option,
    const QWidget *widget,
    QStyleHintReturn *returnData) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (hint == SH_Menu_SubMenuPopupDelay) {
        return 200;
    }
    // 普通与可编辑组合框均使用标准下拉列表，定位与屏幕避让交给 Qt。
    if (hint == SH_ComboBox_Popup) {
        return 0;
    }
    if (hint == SH_ComboBox_ListMouseTracking) {
        return 1;
    }
    if (hint == SH_ComboBox_PopupFrameStyle) {
        return QFrame::StyledPanel | QFrame::Plain;
    }
    if (hint == SH_Widget_Animate) {
        if (d_ptr->snapshot != nullptr
            && d_ptr->snapshot->reducedMotion()) {
            return 0;
        }
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

QPalette ZzFluentStyle::standardPalette() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QPalette palette = QProxyStyle::standardPalette();
    const QColor textPrimary = d_ptr->snapshot->color(
        ZzColorToken::TextPrimary);
    const QColor textSecondary = d_ptr->snapshot->color(
        ZzColorToken::TextSecondary);
    const QColor surface = d_ptr->snapshot->color(
        ZzColorToken::Surface);
    const QColor surfaceSecondary = d_ptr->snapshot->color(
        ZzColorToken::SurfaceSecondary);
    const QColor controlFill = d_ptr->snapshot->color(
        ZzColorToken::ControlFill);
    const QColor controlStroke = d_ptr->snapshot->color(
        ZzColorToken::ControlStroke);
    const QColor accent = d_ptr->snapshot->color(ZzColorToken::Accent);
    const QColor accentText = d_ptr->snapshot->color(
        ZzColorToken::AccentText);

    palette.setColor(
        QPalette::Window,
        surface);
    palette.setColor(QPalette::WindowText, textPrimary);
    palette.setColor(
        QPalette::Base,
        surfaceSecondary);
    palette.setColor(QPalette::AlternateBase, controlFill);
    palette.setColor(QPalette::ToolTipBase, surfaceSecondary);
    palette.setColor(QPalette::ToolTipText, textPrimary);
    palette.setColor(QPalette::Text, textPrimary);
    palette.setColor(QPalette::PlaceholderText, textSecondary);
    palette.setColor(QPalette::Button, controlFill);
    palette.setColor(QPalette::ButtonText, textPrimary);
    palette.setColor(QPalette::BrightText, textPrimary);
    palette.setColor(QPalette::Light, controlStroke);
    palette.setColor(QPalette::Midlight, controlStroke);
    palette.setColor(QPalette::Mid, controlStroke);
    palette.setColor(QPalette::Dark, controlStroke);
    palette.setColor(QPalette::Shadow, controlStroke);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::Accent, accent);
    palette.setColor(QPalette::HighlightedText, accentText);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent);

    for (const QPalette::ColorRole role : {
             QPalette::WindowText,
             QPalette::Text,
             QPalette::ButtonText,
             QPalette::ToolTipText,
             QPalette::PlaceholderText}) {
        palette.setColor(QPalette::Disabled, role, textSecondary);
    }
    return palette;
}

QSize ZzFluentStyle::sizeFromContents(
    ContentsType type,
    const QStyleOption *option,
    const QSize &contentsSize,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QSize result = QProxyStyle::sizeFromContents(
        type,
        option,
        contentsSize,
        widget);
    if (type == CT_LineEdit || type == CT_SpinBox) {
        const auto *line = qobject_cast<const QLineEdit *>(widget);
        if (type == CT_LineEdit && line != nullptr && line->hasFrame()) {
            result.rwidth() += 2 * zzLineEditHorizontalInset;
        }
        result = result.expandedTo(QSize(96, 32));
    }
    if (type == CT_SpinBox) {
        if (const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            result = result.expandedTo(d_ptr->spinBoxSizeFromContents(spin, contentsSize, widget));
        }
    }
    if (type == CT_ComboBox) {
        const int fluentContentWidth = contentsSize.width()
            + zzComboBoxLeadingInset
            + zzComboBoxArrowWidth
            + zzComboBoxLabelHorizontalMargin;
        result.setWidth(qMax(result.width(), fluentContentWidth));
        result = result.expandedTo(QSize(96, 32));
    }
    if (type == CT_ToolButton) {
        result = result.expandedTo(QSize(32, 32));
        if (const auto *tool = qstyleoption_cast<const QStyleOptionToolButton *>(option);
            tool != nullptr && tool->features.testFlag(QStyleOptionToolButton::HasMenu)) {
            result.setWidth(qMax(result.width(), contentsSize.width() + 12 + zzToolButtonMenuWidth));
        }
    }
    if (type == CT_RadioButton || type == CT_CheckBox) {
        result.setHeight(qMax(result.height(), 28));
    }
    if (type == CT_ProgressBar) {
        const auto *progress = qstyleoption_cast<
            const QStyleOptionProgressBar *>(option);
        if (progress != nullptr) {
            const int crossAxis = progress->textVisible
                ? progress->fontMetrics.height() + 8
                : 4;
            if (progress->state.testFlag(QStyle::State_Horizontal)) {
                result.setHeight(qMax(result.height(), crossAxis));
            } else {
                result.setWidth(qMax(result.width(), crossAxis));
            }
        }
    }
    if (type == CT_TabBarTab) {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
        if (tab != nullptr) {
            if (ZzTabBarStylePrivate::appearance(widget) != ZzTabBarAppearance::Standard)
                return ZzTabBarStylePrivate::sizeHint(*tab, result, widget);
            // Preserve the measured label area when painting reserves the indicator gutter.
            const int gutter = qCeil(d_ptr->snapshot->metric(
                ZzMetricToken::SelectionIndicatorThickness)
                + d_ptr->snapshot->metric(ZzMetricToken::SelectionIndicatorContentGap));
            // The base style also shrinks unselected labels by its vertical shift.
            // Budget for both states so changing selection cannot clip the font.
            const int labelShift = qAbs(pixelMetric(PM_TabBarTabShiftVertical, option, widget));
            switch (tab->shape) {
            case QTabBar::RoundedWest:
            case QTabBar::RoundedEast:
            case QTabBar::TriangularWest:
            case QTabBar::TriangularEast:
                result.rwidth() += gutter + labelShift;
                break;
            default:
                result.rheight() += gutter + labelShift;
                break;
            }
        }
    }
    if (type == CT_ItemViewItem
        && d_ptr->isComboBoxPopupContext(widget)) {
        result.setHeight(qMax(result.height(), 32));
    }
    if (type == CT_ItemViewItem && ZzDataViewStylePrivate::applies(widget)) {
        result.setHeight(qMax(result.height(), 32));
        result.rwidth() += 12;
    }
    if (type == CT_MenuItem) {
        const auto *menuItem = qstyleoption_cast<
            const QStyleOptionMenuItem *>(option);
        const bool standardMenuItem = menuItem != nullptr
            && !d_ptr->isComboBoxPopupContext(widget);
        const bool compactSeparator = menuItem != nullptr
            && menuItem->menuItemType
                == QStyleOptionMenuItem::Separator
            && !d_ptr->isComboBoxPopupContext(widget);
        if (standardMenuItem
            && menuItem->text.contains(QLatin1Char('\t'))) {
            if (menuItem->menuItemType
                == QStyleOptionMenuItem::DefaultItem) {
                QStyleOptionMenuItem mainTextOption = *menuItem;
                mainTextOption.text.truncate(
                    mainTextOption.text.indexOf(QLatin1Char('\t')));
                mainTextOption.reservedShortcutWidth = 0;
                const QSize defaultMainText =
                    QProxyStyle::sizeFromContents(
                        CT_MenuItem,
                        &mainTextOption,
                        contentsSize,
                        widget);
                result.setWidth(qMax(
                    result.width(),
                    defaultMainText.width()));
            }
            result.rwidth() += zzMenuShortcutSpacing;
        }
        if (standardMenuItem
            && menuItem->menuItemType
                == QStyleOptionMenuItem::SubMenu) {
            QStyleOptionMenuItem contentOption = *menuItem;
            contentOption.menuItemType = QStyleOptionMenuItem::Normal;
            const QSize contentSize = QProxyStyle::sizeFromContents(
                CT_MenuItem,
                &contentOption,
                contentsSize,
                widget);
            result.setWidth(qMax(
                result.width(),
                contentSize.width() + zzMenuTrailingIndicatorWidth));
        }
        result.setHeight(qMax(
            result.height(),
            compactSeparator ? 9 : 32));
    }
    if (type == CT_MenuBar || type == CT_MenuBarItem) {
        result.setHeight(qMax(result.height(), 32));
    }
    return result;
}

void ZzFluentStyle::drawPrimitive(
    PrimitiveElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (element == PE_IndicatorArrowDown && option != nullptr && painter != nullptr
        && qobject_cast<const QToolButton *>(widget) != nullptr) {
        // 菜单按钮使用细线折角，位置仍由 Qt 的菜单子区域计算。
        const QPointF center = QRectF(option->rect).center();
        const qreal halfWidth = qMin(3.0, option->rect.width() / 4.0);
        QPainterPath path;
        path.moveTo(center + QPointF(-halfWidth, -halfWidth / 2.0));
        path.lineTo(center + QPointF(0, halfWidth / 2.0));
        path.lineTo(center + QPointF(halfWidth, -halfWidth / 2.0));
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(option->palette.color(
            option->state.testFlag(State_Enabled) ? QPalette::Active : QPalette::Disabled,
            QPalette::ButtonText), 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(path);
        painter->restore();
        return;
    }
    if (element == PE_IndicatorItemViewItemCheck && option != nullptr && painter != nullptr
        && ZzDataViewStylePrivate::applies(widget)) {
        QStyleOption check = *option;
        check.rect.adjust(-1, -1, 1, 1);
        d_ptr->drawCheckIndicator(&check, painter, false);
        return;
    }
    if ((element == PE_IndicatorCheckBox
         || element == PE_IndicatorRadioButton)
        && option != nullptr && painter != nullptr) {
        d_ptr->drawCheckIndicator(
            option,
            painter,
            element == PE_IndicatorRadioButton);
        return;
    }
    const bool textFrame = element == PE_Frame
        && (qobject_cast<const QLineEdit *>(widget) != nullptr
            || qobject_cast<const QTextEdit *>(widget) != nullptr
            || qobject_cast<const QPlainTextEdit *>(widget) != nullptr);
    if (element == PE_FrameGroupBox && painter != nullptr) {
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            d_ptr->drawGroupBoxFrame(frame, painter);
            return;
        }
    }
    if ((element == PE_PanelLineEdit
         || element == PE_FrameLineEdit
         || textFrame)
        && option != nullptr && painter != nullptr) {
        // QAbstractSpinBox 在 CC_SpinBox 中已经绘制完整输入表面；
        // 忽略内部 QLineEdit 的面板，避免 hover 填充覆盖外框。
        if (zzInputParentOwnsSurface(widget)) {
            return;
        }
        d_ptr->drawInputPanel(option, painter, widget);
        return;
    }
    if (element == PE_Frame && option != nullptr && painter != nullptr
        && widget != nullptr && widget->windowType() == Qt::Popup
        && d_ptr->isComboBoxPopupContext(widget)) {
        d_ptr->drawMenuPanel(option, painter);
        return;
    }
    if (element == PE_PanelTipLabel
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolTipPanel(option, painter);
        return;
    }
    if (element == PE_PanelMenu
        && option != nullptr && painter != nullptr) {
        d_ptr->drawMenuPanel(option, painter);
        return;
    }
    if (element == PE_FrameMenu && painter != nullptr) {
        return;
    }
    if (element == PE_PanelMenuBar
        && option != nullptr && painter != nullptr) {
        d_ptr->drawMenuBarPanel(option, painter);
        return;
    }
    const bool accentMenuPanel = element == PE_IndicatorButtonDropDown
        && widget != nullptr && widget->property("accent").toBool();
    if ((element == PE_PanelButtonTool || accentMenuPanel)
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolButtonPanel(option, painter, widget);
        return;
    }
    if (element == PE_PanelToolBar
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolBarPanel(option, painter);
        return;
    }
    if (element == PE_IndicatorToolBarHandle
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolBarHandle(option, painter);
        return;
    }
    if (element == PE_IndicatorToolBarSeparator
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolBarSeparator(option, painter);
        return;
    }
    if (element == PE_PanelStatusBar
        && option != nullptr && painter != nullptr) {
        d_ptr->drawStatusBarPanel(option, painter);
        return;
    }
    if (element == PE_FrameStatusBarItem && painter != nullptr) {
        return;
    }
    if (element == PE_FrameTabWidget
        && qobject_cast<const ZzTabWidget *>(widget) != nullptr) {
        // ZzTabWidget 的页面表面由工作区绘制；Fusion 的原生页框会在
        // 窄视口左侧留下一条独立边线，与标签栏和内容表面不一致。
        return;
    }
    if (element == PE_FrameTabBarBase
        && ZzTabBarStylePrivate::appearance(widget) != ZzTabBarAppearance::Standard) return;
    if (element == PE_FrameTabBarBase
        && qobject_cast<const ZzTabBar *>(widget) != nullptr
        && widget->parentWidget() != nullptr
        && qobject_cast<const ZzTabWidget *>(
               widget->parentWidget()) != nullptr) {
        // documentMode 下 Qt 仍会给标签栏绘制 Fusion 顶边；工作区由
        // 自身布局提供分隔关系，不需要再叠加一条原生高亮线。
        return;
    }
    if (element == PE_PanelScrollAreaCorner
        && option != nullptr && painter != nullptr) {
        painter->fillRect(
            option->rect,
            option->palette.color(QPalette::Window));
        return;
    }
    if (element == PE_IndicatorBranch
        && option != nullptr && painter != nullptr && widget != nullptr) {
        const auto *pane = qobject_cast<const ZzNavigationPane *>(
            widget->parentWidget());
        if (pane != nullptr && pane->treeView() == widget) {
            d_ptr->drawNavigationBranch(option, painter);
            return;
        }
        if (ZzDataViewStylePrivate::applies(widget)) {
            QStyleOption branch = *option;
            // 普通数据树的箭头向内容侧移动，给行 leading 指示条留白。
            branch.rect.translate(option->direction == Qt::RightToLeft ? -4 : 4, 0);
            if (d_ptr->snapshot->mode() == ZzThemeMode::HighContrast
                && (option->state.testFlag(State_Selected)
                    || (option->state.testFlag(State_Enabled) && option->state.testFlag(State_MouseOver)))) {
                branch.palette.setBrush(QPalette::Text, option->palette.brush(QPalette::HighlightedText));
            } else if (!option->state.testFlag(State_Open)) {
                branch.palette.setColor(QPalette::Text,
                    option->palette.color(QPalette::Disabled, QPalette::Text));
            }
            d_ptr->drawNavigationBranch(&branch, painter);
            return;
        }
    }
    if (element == PE_PanelItemViewRow
        && option != nullptr && painter != nullptr) {
        const auto *item = qstyleoption_cast<
            const QStyleOptionViewItem *>(option);
        if (item != nullptr) {
            const QWidget *viewWidget = qobject_cast<const QTreeView *>(widget) != nullptr
                ? widget : widget != nullptr ? widget->parentWidget() : item->widget;
            if (ZzDataViewStylePrivate::applies(viewWidget)) {
                ZzDataViewStylePrivate::drawTreeRow(*this, *item, painter, widget);
                return;
            }
            d_ptr->drawItemViewRow(item, painter, widget);
            return;
        }
    }
    if (element == PE_FrameFocusRect
        && option != nullptr && painter != nullptr) {
        if (widget != nullptr && !isFocusVisualVisible(widget)) {
            return;
        }
        const qreal dpr = widget != nullptr
            ? widget->devicePixelRatioF()
            : 1.0;
        ZzFluentPainter::drawFocusRing(
            painter,
            option->rect,
            *d_ptr->snapshot,
            dpr);
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

bool ZzFluentStyle::eventFilter(QObject *watched, QEvent *event)
{
    d_ptr->handleInputEvent(watched, event);
    const auto *eventWidget = qobject_cast<const QWidget *>(watched);
    if (eventWidget != nullptr && (eventWidget->style() == this
        || (qobject_cast<const QAbstractItemView *>(eventWidget->parentWidget()) != nullptr
            && eventWidget->parentWidget()->style() == this))) {
        ZzDataViewStylePrivate::handleEvent(watched, event);
    }
    if (event->type() == QEvent::DynamicPropertyChange) {
        const auto *change = static_cast<const QDynamicPropertyChangeEvent *>(event);
        if (change->propertyName() == "accent" || change->propertyName() == "zzFluentSubtle") {
            if (auto *widget = qobject_cast<QWidget *>(watched); widget != nullptr && widget->style() == this) {
                widget->update();
            }
        }
    }
    return QProxyStyle::eventFilter(watched, event);
}

void ZzFluentStyle::drawControl(
    ControlElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if ((element == CE_HeaderSection || element == CE_HeaderEmptyArea)
        && painter != nullptr && option != nullptr) {
        const auto *headerWidget = qobject_cast<const QHeaderView *>(widget);
        if (headerWidget != nullptr && ZzDataViewStylePrivate::applies(headerWidget->parentWidget())) {
            if (element == CE_HeaderEmptyArea) {
                painter->fillRect(option->rect, option->palette.brush(QPalette::Button));
            } else if (const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
                ZzDataViewStylePrivate::drawHeader(*this, *header, painter);
            }
            return;
        }
    }
    if (element == CE_ToolButtonLabel && widget != nullptr
        && widget->property("accent").toBool()) {
        if (const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            QStyleOptionToolButton label = *button;
            const QColor text = button->state.testFlag(State_Enabled)
                ? ZzControlAppearancePrivate::text(button->palette)
                : button->palette.color(QPalette::Disabled, QPalette::ButtonText);
            label.palette.setColor(QPalette::ButtonText, text);
            label.palette.setColor(QPalette::WindowText, text);
            QProxyStyle::drawControl(element, &label, painter, widget);
            return;
        }
    }
    if (element == CE_ShapedFrame
        && qobject_cast<const QLCDNumber *>(widget) != nullptr) {
        const auto *frame = qstyleoption_cast<
            const QStyleOptionFrame *>(option);
        if (frame != nullptr && painter != nullptr) {
            d_ptr->drawDigitalDisplayFrame(frame, painter);
            return;
        }
    }
    if (element == CE_ItemViewItem
        && option != nullptr && painter != nullptr
        && option->version != zzItemContentOptionVersion) {
        const auto *item = qstyleoption_cast<
            const QStyleOptionViewItem *>(option);
        if (item != nullptr) {
            if (d_ptr->isComboBoxPopupContext(widget)) {
                d_ptr->drawComboBoxPopupItem(item, painter, widget);
                return;
            }
            const QWidget *viewWidget = item->widget != nullptr ? item->widget : widget;
            if (ZzDataViewStylePrivate::applies(viewWidget)) {
                QStyleOptionViewItem adjusted = *item;
                adjusted.widget = viewWidget;
                ZzDataViewStylePrivate::drawItem(*this, adjusted, painter);
                return;
            }
            if (zzUsesItemIndicatorStyle(viewWidget, item->index)) {
                QStyleOptionViewItem adjusted = *item;
                adjusted.widget = viewWidget;
                const auto layout = ZzItemViewVisual::draw(*this, adjusted, painter,
                    {.drawSurface = qobject_cast<const QTreeView *>(viewWidget) == nullptr,
                     .ownsIndicator = ZzItemViewVisual::ownsIndicator(viewWidget, item->index)});
                adjusted.rect = layout.contentRect;
                adjusted.version = zzItemContentOptionVersion;
                adjusted.state.setFlag(State_Selected, false);
                adjusted.state.setFlag(State_MouseOver, false);
                QProxyStyle::drawControl(element, &adjusted, painter, widget);
                return;
            }
        }
    }
    if (element == CE_PushButton) {
        const auto *button = qstyleoption_cast<
            const QStyleOptionButton *>(option);
        if (button != nullptr && painter != nullptr) {
            d_ptr->drawPushButton(button, painter, widget);
            return;
        }
    }
    if (element == CE_ProgressBar) {
        const auto *progress = qstyleoption_cast<
            const QStyleOptionProgressBar *>(option);
        if (progress != nullptr && painter != nullptr) {
            d_ptr->drawProgressBar(progress, painter, widget);
            return;
        }
    }
    if (element == CE_TabBarTab) {
        const auto *tab = qstyleoption_cast<
            const QStyleOptionTab *>(option);
        if (tab != nullptr && painter != nullptr) {
            d_ptr->drawTabBarTab(tab, painter, widget);
            return;
        }
    }
    if (element == CE_ToolBar
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolBarPanel(option, painter);
        return;
    }
    if (element == CE_MenuEmptyArea
        && option != nullptr && painter != nullptr) {
        d_ptr->drawMenuEmptyArea(option, painter);
        return;
    }
    if (element == CE_MenuBarEmptyArea
        && option != nullptr && painter != nullptr) {
        d_ptr->drawMenuBarEmptyArea(option, painter);
        return;
    }
    if (element == CE_MenuBarItem) {
        const auto *menuItem = qstyleoption_cast<
            const QStyleOptionMenuItem *>(option);
        if (menuItem != nullptr && painter != nullptr) {
            d_ptr->drawMenuBarItem(menuItem, painter, widget);
            return;
        }
    }
    if (element == CE_MenuItem) {
        const auto *menuItem = qstyleoption_cast<
            const QStyleOptionMenuItem *>(option);
        if (menuItem != nullptr && painter != nullptr) {
            if (d_ptr->isComboBoxPopupContext(widget)) {
                d_ptr->drawComboBoxPopupMenuItem(
                    menuItem,
                    painter,
                    widget);
                return;
            }
            d_ptr->drawMenuItem(menuItem, painter, widget);
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void ZzFluentStyle::drawComplexControl(
    ComplexControl control,
    const QStyleOptionComplex *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (control == CC_GroupBox && painter != nullptr) {
        const auto *group = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
        if (group == nullptr) return;
        painter->save();
        if (group->subControls.testFlag(SC_GroupBoxFrame)) {
            QStyleOptionFrame frame;
            frame.QStyleOption::operator=(*group);
            frame.features = group->features;
            frame.lineWidth = group->lineWidth;
            frame.midLineWidth = group->midLineWidth;
            frame.rect = subControlRect(control, group, SC_GroupBoxFrame, widget);
            painter->save();
            if (!group->text.isEmpty()) {
                const QRect label = subControlRect(control, group, SC_GroupBoxLabel, widget);
                QRect cutout = label;
                if (group->subControls.testFlag(SC_GroupBoxCheckBox)) {
                    cutout |= subControlRect(control, group, SC_GroupBoxCheckBox, widget);
                }
                // 与参考项目的 Fusion 组合方式一致：只让开标题上半区，
                // 不将位于文字下方的完整上边框裁断；同时保留调用方裁剪。
                cutout.adjust(-2, 0, 2, 3 - label.height() / 2);
                painter->setClipRegion(QRegion(group->rect) - cutout, Qt::IntersectClip);
            }
            drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
            painter->restore();
        }
        // 标题、焦点和勾选框继续复用公共流程；独立画框避免系统高对比绕过样式。
        QStyleOptionGroupBox foreground(*group);
        foreground.subControls &= ~SC_GroupBoxFrame;
        QCommonStyle::drawComplexControl(control, &foreground, painter, widget);
        painter->restore();
        return;
    }
    if (control == CC_ToolButton && painter != nullptr) {
        if (const auto *tool = qstyleoption_cast<const QStyleOptionToolButton *>(option);
            tool != nullptr && tool->features.testFlag(QStyleOptionToolButton::HasMenu)) {
            d_ptr->drawToolButtonWithMenu(tool, painter, widget);
            return;
        }
    }
    if (control == CC_ToolButton && widget != nullptr
        && (widget->property("accent").toBool()
            || (widget->property("zzFluentSubtle").isValid()
                && !widget->property("zzFluentSubtle").toBool()))) {
        if (const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            QStyleOptionToolButton adjusted = *button;
            if (widget->property("accent").toBool()) {
                const QColor text = button->state.testFlag(State_Enabled)
                    ? ZzControlAppearancePrivate::text(button->palette)
                    : button->palette.color(QPalette::Disabled, QPalette::ButtonText);
                adjusted.palette.setColor(QPalette::ButtonText, text);
                adjusted.palette.setColor(QPalette::WindowText, text);
            }
            // 显式标准/强调色外观不受 autoRaise 的静止隐藏影响；仅调整绘制选项，
            // 保留原控件的 autoRaise、菜单子区域和 Qt 激活行为。
            adjusted.state &= ~State_AutoRaise;
            adjusted.state |= State_Raised;
            QProxyStyle::drawComplexControl(control, &adjusted, painter, widget);
            return;
        }
    }
    if (control == CC_Slider) {
        const auto *slider = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (slider != nullptr && painter != nullptr) {
            d_ptr->drawSlider(slider, painter, widget);
            return;
        }
    }
    if (control == CC_ComboBox) {
        const auto *combo = qstyleoption_cast<
            const QStyleOptionComboBox *>(option);
        if (combo != nullptr && painter != nullptr) {
            d_ptr->drawComboBox(combo, painter, widget);
            return;
        }
    }
    if (control == CC_SpinBox) {
        const auto *spinBox = qstyleoption_cast<
            const QStyleOptionSpinBox *>(option);
        if (spinBox != nullptr && painter != nullptr) {
            d_ptr->drawSpinBox(spinBox, painter, widget);
            return;
        }
    }
    if (control == CC_ScrollBar) {
        const auto *scrollBar = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (scrollBar != nullptr && painter != nullptr) {
            d_ptr->drawScrollBar(scrollBar, painter, widget);
            return;
        }
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

QRect ZzFluentStyle::subElementRect(
    SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    if (element == SE_LineEditContents) {
        const auto *line = qobject_cast<const QLineEdit *>(widget);
        if (line != nullptr && line->hasFrame()) {
            auto rect = QProxyStyle::subElementRect(element, option, widget);
            const int inset = qMin(zzLineEditHorizontalInset, qMax(0, rect.width() / 2));
            return rect.adjusted(inset, 0, -inset, 0);
        }
    }
    if (ZzTabBarStylePrivate::appearance(widget) == ZzTabBarAppearance::Navigation) {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
        if (tab && ZzTabBarStylePrivate::vertical(tab->shape)) {
            if (element == SE_TabBarTabLeftButton || element == SE_TabBarTabRightButton) {
                const bool left = element == SE_TabBarTabLeftButton;
                const QSize size = left ? tab->leftButtonSize : tab->rightButtonSize;
                QRect button(left ? tab->rect.left() + 4 : tab->rect.right() - size.width() - 3,
                    tab->rect.center().y() - size.height() / 2, size.width(), size.height());
                return visualRect(tab->direction, tab->rect, button);
            }
            if (element == SE_TabBarTabText) {
                QRect text = tab->rect.adjusted(13, 4, -8, -4);
                if (!tab->icon.isNull()) text.adjust(tab->iconSize.width() + 8, 0, 0, 0);
                if (!tab->leftButtonSize.isEmpty()) text.adjust(tab->leftButtonSize.width() + 4, 0, 0, 0);
                if (!tab->rightButtonSize.isEmpty()) text.adjust(0, 0, -tab->rightButtonSize.width() - 4, 0);
                return visualRect(tab->direction, tab->rect, text);
            }
        }
    }
    if (element == SE_TabBarTabText) {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
        if (ZzTabBarStylePrivate::appearance(widget) != ZzTabBarAppearance::Standard)
            return QProxyStyle::subElementRect(element, option, widget);
        if (tab != nullptr && tab->version != zzItemContentOptionVersion) {
            auto content = *tab;
            const int gutter = qCeil(d_ptr->snapshot->metric(
                ZzMetricToken::SelectionIndicatorThickness)
                + d_ptr->snapshot->metric(ZzMetricToken::SelectionIndicatorContentGap));
            switch (tab->shape) {
            case QTabBar::RoundedSouth:
            case QTabBar::TriangularSouth: content.rect.adjust(0, gutter, 0, 0); break;
            case QTabBar::RoundedEast:
            case QTabBar::TriangularEast: content.rect.adjust(gutter, 0, 0, 0); break;
            case QTabBar::RoundedWest:
            case QTabBar::TriangularWest: content.rect.adjust(0, 0, -gutter, 0); break;
            default: content.rect.adjust(0, 0, 0, -gutter); break;
            }
            content.version = zzItemContentOptionVersion;
            return QProxyStyle::subElementRect(element, &content, widget);
        }
    }
    if (element == SE_ItemViewItemText || element == SE_ItemViewItemDecoration
        || element == SE_ItemViewItemCheckIndicator || element == SE_ItemViewItemFocusRect) {
        const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option);
        if (item != nullptr && item->version != zzItemContentOptionVersion
            && zzUsesItemIndicatorStyle(item->widget, item->index)) {
            auto content = *item;
            content.rect = ZzDataViewStylePrivate::applies(item->widget)
                ? ZzDataViewStylePrivate::contentRect(*item)
                : ZzItemViewVisual::layout(*d_ptr->snapshot, *item,
                {.ownsIndicator = ZzItemViewVisual::ownsIndicator(item->widget, item->index)})
                .contentRect;
            content.version = zzItemContentOptionVersion;
            return QProxyStyle::subElementRect(element, &content, widget);
        }
    }
    return QProxyStyle::subElementRect(element, option, widget);
}

QRect ZzFluentStyle::subControlRect(
    ComplexControl control,
    const QStyleOptionComplex *option,
    SubControl subControl,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (control == CC_ToolButton
        && (subControl == SC_ToolButton || subControl == SC_ToolButtonMenu)) {
        if (const auto *tool = qstyleoption_cast<const QStyleOptionToolButton *>(option);
            tool != nullptr && tool->features.testFlag(QStyleOptionToolButton::HasMenu)) {
            // 固定宽度的标题栏按钮也必须保留完整图标；紧凑时优先收窄箭头区。
            const int iconSpace = tool->icon.isNull() ? 0 : tool->iconSize.width() + 4;
            const int menuWidth = qMin(zzToolButtonMenuWidth,
                qMax(0, tool->rect.width() - iconSpace));
            const QRect logical = subControl == SC_ToolButtonMenu
                ? QRect(tool->rect.right() - menuWidth + 1, tool->rect.top(), menuWidth, tool->rect.height())
                : tool->rect.adjusted(0, 0, -menuWidth, 0);
            return visualRect(tool->direction, tool->rect, logical);
        }
    }
    QRect result = QProxyStyle::subControlRect(
        control,
        option,
        subControl,
        widget);
    if (control == CC_GroupBox && subControl == SC_GroupBoxCheckBox && option != nullptr) {
        result.moveTop(option->rect.top() + 1);
    }
    if (control == CC_Slider && subControl == SC_SliderHandle) {
        const int length = pixelMetric(PM_SliderLength, option, widget);
        const QPoint center = result.center();
        result.setSize(QSize(length, length));
        result.moveCenter(center);
    }
    if (control == CC_ComboBox
        && (subControl == SC_ComboBoxFrame
            || subControl == SC_ComboBoxEditField
            || subControl == SC_ComboBoxArrow)) {
        const auto *comboBox = qstyleoption_cast<
            const QStyleOptionComboBox *>(option);
        if (comboBox != nullptr) {
            return d_ptr->comboBoxSubControlRect(comboBox, subControl);
        }
    }
    if (control == CC_SpinBox) {
        const auto *spinBox = qstyleoption_cast<
            const QStyleOptionSpinBox *>(option);
        if (spinBox != nullptr) {
            return d_ptr->spinBoxSubControlRect(spinBox, subControl, widget);
        }
    }
    if (control == CC_ScrollBar) {
        const auto *scrollBar = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (scrollBar != nullptr) {
            return d_ptr->scrollBarSubControlRect(scrollBar, subControl);
        }
    }
    return result;
}

void ZzFluentStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    ZzDataViewStylePrivate::polish(widget, true);
}

void ZzFluentStyle::unpolish(QWidget *widget)
{
    if (auto *bar = qobject_cast<QTabBar *>(widget)) delete d_ptr->tabStyles.take(bar);
    ZzDataViewStylePrivate::polish(widget, false);
    QProxyStyle::unpolish(widget);
}

QStyle::SubControl ZzFluentStyle::hitTestComplexControl(
    ComplexControl control,
    const QStyleOptionComplex *option,
    const QPoint &position,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (control == CC_ComboBox) {
        const auto *comboBox = qstyleoption_cast<
            const QStyleOptionComboBox *>(option);
        if (comboBox != nullptr) {
            return d_ptr->hitTestComboBox(comboBox, position);
        }
    }
    if (control == CC_SpinBox) {
        const auto *spinBox = qstyleoption_cast<
            const QStyleOptionSpinBox *>(option);
        if (spinBox != nullptr) {
            return d_ptr->hitTestSpinBox(spinBox, position, widget);
        }
    }
    if (control == CC_ScrollBar) {
        const auto *scrollBar = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (scrollBar != nullptr) {
            return d_ptr->hitTestScrollBar(scrollBar, position);
        }
    }
    return QProxyStyle::hitTestComplexControl(
        control,
        option,
        position,
        widget);
}

} // namespace ZzFluentUI
