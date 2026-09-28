#include "ZzCalendarPrivate.h"

#include <algorithm>

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QPalette>
#include <QtGui/QPen>
#include <QtGui/QPainterPath>
#include <QtGui/QTextCharFormat>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QStyleOptionToolButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzMetricToken.h>

#include "ZzControlAppearancePrivate.h"

namespace ZzFluentUI {

namespace {
constexpr int zzCalendarMinimumDayExtent = 40;
constexpr int zzCalendarContentGap = 4;
constexpr int zzCalendarCellTextMargin = 3;
constexpr int zzCalendarButtonExtent = 32;
constexpr qreal zzCalendarChevronExtent = 3.0;
constexpr qreal zzCalendarChevronStroke = 1.5;
} // namespace

ZzCalendarPrivate::ZzCalendarPrivate(ZzCalendar *q)
    : QObject(q)
    , q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
    for (std::size_t index = 0; index < dayTexts.size(); ++index) {
        dayTexts[index] = QString::number(static_cast<int>(index) + 1);
    }

    if (auto *view = q_ptr->findChild<QTableView *>()) {
        dateView = view;
        view->setFrameShape(QFrame::NoFrame);
        view->setMouseTracking(true);
        view->viewport()->setMouseTracking(true);
        view->viewport()->installEventFilter(this);
        hoverViewport = view->viewport();
    }
    q_ptr->setAutoFillBackground(false);
    configureNavigation();
    refreshVisuals();
}

void ZzCalendarPrivate::configureNavigation()
{
    navigation = q_ptr->findChild<QWidget *>(QStringLiteral("qt_calendar_navigationbar"));
    previousButton = q_ptr->findChild<QToolButton *>(QStringLiteral("qt_calendar_prevmonth"));
    nextButton = q_ptr->findChild<QToolButton *>(QStringLiteral("qt_calendar_nextmonth"));
    monthButton = q_ptr->findChild<QToolButton *>(QStringLiteral("qt_calendar_monthbutton"));
    yearButton = q_ptr->findChild<QToolButton *>(QStringLiteral("qt_calendar_yearbutton"));
    yearEditor = q_ptr->findChild<QSpinBox *>(QStringLiteral("qt_calendar_yearedit"));
    if (!navigation || !previousButton || !nextButton || !monthButton || !yearButton) {
        return;
    }
    navigation->setAutoFillBackground(false);
    navigation->setBackgroundRole(QPalette::Base);
    if (auto *layout = qobject_cast<QBoxLayout *>(navigation->layout())) {
        // 只移动按钮，不删除 Qt 持有的年份编辑占位和两个弹簧。
        layout->removeWidget(previousButton);
        layout->removeWidget(nextButton);
        layout->addWidget(previousButton);
        layout->addWidget(nextButton);
        for (int index = 0; index < layout->indexOf(monthButton); ++index) {
            layout->setStretch(index, 0);
        }
        // 明确只让年月之后的原生弹簧吸收空白，起始侧不留等分空隙。
        const int trailingSpacer = layout->indexOf(yearButton) + 1;
        if (auto *item = layout->itemAt(trailingSpacer); item && item->spacerItem()) {
            layout->setStretch(trailingSpacer, 1);
        }
        layout->setSpacing(zzCalendarContentGap);
    }
    for (auto *button : {previousButton, nextButton, monthButton, yearButton}) {
        button->setAutoRaise(true);
        button->setProperty("zzFluentSubtle", true);
        button->setAttribute(Qt::WA_Hover);
        button->installEventFilter(this);
    }
}

void ZzCalendarPrivate::refreshVisuals()
{
    hoverFill = q_ptr->palette().color(QPalette::Text);
    hoverFill.setAlpha(20);
    pressedFill = q_ptr->palette().color(QPalette::Text);
    pressedFill.setAlpha(36);
    if (const auto *style = qobject_cast<const ZzFluentStyle *>(q_ptr->style())) {
        const auto snapshot = style->themeSnapshot();
        panelPadding = qRound(snapshot->metric(ZzMetricToken::HorizontalPadding));
        panelRadius = snapshot->metric(ZzMetricToken::CornerRadiusMedium);
        strokeWidth = snapshot->metric(ZzMetricToken::StrokeThin);
        if (snapshot->mode() != ZzThemeMode::HighContrast) {
            hoverFill = snapshot->color(ZzColorToken::ControlFillHover);
            pressedFill = snapshot->color(ZzColorToken::ControlFillPressed);
        }
    }
    dayExtent = std::max(zzCalendarMinimumDayExtent,
        q_ptr->fontMetrics().height() + panelPadding);
    if (auto *layout = q_ptr->layout()) {
        layout->setContentsMargins(panelPadding, panelPadding, panelPadding, panelPadding);
        layout->setSpacing(zzCalendarContentGap);
    }
    QPalette palette = q_ptr->palette();
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        palette.setColor(group, QPalette::AlternateBase, palette.color(group, QPalette::Base));
    }
    if (dateView) {
        dateView->setPalette(palette);
        dateView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        dateView->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }
    QFont headerFont = q_ptr->font();
    headerFont.setWeight(QFont::DemiBold);
    QTextCharFormat headerFormat;
    headerFormat.setFont(headerFont);
    q_ptr->setHeaderTextFormat(headerFormat);
    const auto group = q_ptr->isEnabled() ? QPalette::Active : QPalette::Disabled;
    const auto weekdays = q_ptr->locale().weekdays();
    for (std::size_t index = 0; index < dayTexts.size(); ++index) {
        dayTexts[index] = q_ptr->locale().toString(static_cast<int>(index) + 1);
    }
    for (int day = Qt::Monday; day <= Qt::Sunday; ++day) {
        // 使用实际星期而非固定列号，周日起始和 RTL 仍由 Qt 正确排列。
        const auto weekday = static_cast<Qt::DayOfWeek>(day);
        const bool weekend = !weekdays.contains(weekday);
        workingDays[static_cast<std::size_t>(day - 1)] = !weekend;
        QTextCharFormat format;
        format.setForeground(q_ptr->isEnabled() && weekend
            ? ZzControlAppearancePrivate::accent(palette) : palette.color(group, QPalette::Text));
        q_ptr->setWeekdayTextFormat(weekday, format);
    }
    const int buttonHeight = std::max(zzCalendarButtonExtent,
        QFontMetrics(headerFont).height() + 2 * zzCalendarContentGap);
    for (auto *button : {previousButton, nextButton, monthButton, yearButton}) {
        if (!button) {
            continue;
        }
        button->setFont(headerFont);
        button->setMinimumHeight(buttonHeight);
        button->setPalette(palette);
        if (button == previousButton || button == nextButton) {
            button->setFixedSize(buttonHeight, buttonHeight);
        }
    }
    if (yearEditor) {
        yearEditor->setFont(headerFont);
        yearEditor->setPalette(palette);
    }
    if (navigation) {
        navigation->setPalette(palette);
        navigation->update();
    }
    q_ptr->update();
}

QSize ZzCalendarPrivate::minimumSize() const
{
    const int columns = q_ptr->verticalHeaderFormat() == QCalendarWidget::NoVerticalHeader ? 7 : 8;
    const int rows = q_ptr->horizontalHeaderFormat() == QCalendarWidget::NoHorizontalHeader ? 6 : 7;
    const int navigationHeight = q_ptr->isNavigationBarVisible() && navigation
        ? navigation->sizeHint().height() + zzCalendarContentGap : 0;
    int cellWidth = dayExtent;
    if (dateView && dateView->model()
        && q_ptr->horizontalHeaderFormat() != QCalendarWidget::NoHorizontalHeader) {
        // Qt 根据地区和表头模式生成实际星期文本；长名称不能仅按字体高度估宽。
        const auto *model = dateView->model();
        for (int column = 0; column < model->columnCount(); ++column) {
            const auto index = model->index(0, column);
            const QFont font = index.data(Qt::FontRole).value<QFont>();
            cellWidth = std::max(cellWidth,
                QFontMetrics(font).horizontalAdvance(index.data().toString())
                    + 2 * zzCalendarCellTextMargin);
        }
    }
    return QSize(columns * cellWidth + 2 * panelPadding,
        rows * dayExtent + navigationHeight + 2 * panelPadding);
}

void ZzCalendarPrivate::paintSurface(QPainter *painter) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(q_ptr->palette().color(QPalette::Mid), strokeWidth));
    painter->setBrush(q_ptr->palette().brush(QPalette::Base));
    const qreal inset = strokeWidth / 2.0;
    const QRectF surface = QRectF(q_ptr->rect()).adjusted(inset, inset, -inset, -inset);
    painter->drawRoundedRect(surface, panelRadius, panelRadius);
    painter->restore();
}

void ZzCalendarPrivate::paintNavigationButton(QToolButton *button) const
{
    QStyleOptionToolButton option;
    option.initFrom(button);
    option.font = button->font();
    option.text = button->text();
    option.subControls = QStyle::SC_ToolButton;
    option.state.setFlag(QStyle::State_AutoRaise, true);
    option.state.setFlag(QStyle::State_Sunken, button->isDown());
    option.state.setFlag(QStyle::State_MouseOver, button->underMouse());
    if (button == monthButton) {
        option.features |= QStyleOptionToolButton::HasMenu;
    }
    const bool arrow = button == previousButton || button == nextButton;
    if (arrow) {
        option.text.clear();
    }
    QPainter painter(button);
    q_ptr->style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, button);
    if (arrow) {
        const bool pointsLeft = (button == previousButton) != q_ptr->isRightToLeft();
        const qreal dx = pointsLeft ? zzCalendarChevronExtent : -zzCalendarChevronExtent;
        const QPointF center = QRectF(button->rect()).center();
        QPainterPath path;
        path.moveTo(center + QPointF(dx / 2.0, -zzCalendarChevronExtent));
        path.lineTo(center + QPointF(-dx / 2.0, 0));
        path.lineTo(center + QPointF(dx / 2.0, zzCalendarChevronExtent));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(option.palette.color(button->isEnabled() ? QPalette::Active
            : QPalette::Disabled, QPalette::Text), zzCalendarChevronStroke, Qt::SolidLine,
            Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    }
}

void ZzCalendarPrivate::clearHover()
{
    updateHover({});
}

void ZzCalendarPrivate::updateHover(const QRect &cell)
{
    if (cell == hoveredCellRect) {
        return;
    }
    const QRect dirty = hoveredCellRect.united(cell);
    hoveredCellRect = cell;
    if (hoverViewport != nullptr && !dirty.isEmpty()) {
        hoverViewport->update(dirty.adjusted(-1, -1, 1, 1));
    }
}

bool ZzCalendarPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::Paint
        && (watched == previousButton || watched == nextButton
            || watched == monthButton || watched == yearButton)) {
        if (auto *button = qobject_cast<QToolButton *>(watched)) {
            paintNavigationButton(button);
            return true;
        }
    }
    if (watched != hoverViewport || event == nullptr) {
        return QObject::eventFilter(watched, event);
    }

    auto *viewport = qobject_cast<QWidget *>(watched);
    if (viewport == nullptr) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        if (!hoveredCellRect.isEmpty()) {
            viewport->update(hoveredCellRect);
        }
        break;
    case QEvent::MouseMove: {
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        auto *view = qobject_cast<QTableView *>(viewport->parentWidget());
        if (view == nullptr) {
            clearHover();
            break;
        }
        const QModelIndex index = view->indexAt(mouseEvent->position().toPoint());
        updateHover(index.isValid() ? view->visualRect(index) : QRect());
        break;
    }
    case QEvent::Leave:
    case QEvent::Hide:
    case QEvent::EnabledChange:
        clearHover();
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void ZzCalendarPrivate::paintCell(
    QPainter *painter,
    const QRect &rect,
    QDate date) const
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    Q_ASSERT(date.isValid());
    if (painter == nullptr || !painter->isActive() || !date.isValid()) {
        return;
    }

    const bool withinRange = date >= q_ptr->minimumDate()
        && date <= q_ptr->maximumDate();
    const bool enabled = q_ptr->isEnabled() && withinRange;
    const bool selected = enabled
        && q_ptr->selectionMode() == QCalendarWidget::SingleSelection
        && date == q_ptr->selectedDate();
    const bool today = date == QDate::currentDate();
    const bool adjacentMonth = date.year() != q_ptr->yearShown()
        || date.month() != q_ptr->monthShown();
    const QPalette::ColorGroup activeGroup = q_ptr->isActiveWindow()
        ? QPalette::Active
        : QPalette::Inactive;
    const QPalette::ColorGroup textGroup = enabled && !adjacentMonth
        ? activeGroup
        : QPalette::Disabled;
    const int buttonMargin = q_ptr->style()->pixelMetric(
        QStyle::PM_ButtonMargin,
        nullptr,
        q_ptr);
    const qreal radius = static_cast<qreal>(std::max(2, buttonMargin / 2));
    const qreal inset = std::max(1.0, radius / 2.0);
    const qreal devicePixelRatio = std::max(1.0, q_ptr->devicePixelRatioF());
    const qreal cellStrokeWidth = std::max(strokeWidth, 1.0 / devicePixelRatio);
    const QRectF cellRect = QRectF(rect).adjusted(
        inset,
        inset,
        -inset,
        -inset);
    const qreal diameter = std::max(
        0.0,
        std::min(cellRect.width(), cellRect.height()));
    const QRectF stateRect(
        cellRect.center().x() - diameter / 2.0,
        cellRect.center().y() - diameter / 2.0,
        diameter,
        diameter);
    const bool hovered = !hoveredCellRect.isEmpty()
        && hoveredCellRect == rect;
    const bool pressed = hovered && enabled
        && QApplication::mouseButtons().testFlag(Qt::LeftButton);
    const QColor accent = ZzControlAppearancePrivate::accent(q_ptr->palette());

    painter->save();
    painter->setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing,
        true);
    painter->setFont(q_ptr->font());

    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(ZzControlAppearancePrivate::fill(q_ptr->palette(), hovered, pressed));
        painter->drawEllipse(stateRect);
    } else if (today && enabled) {
        painter->setBrush(hovered ? QBrush(pressed ? pressedFill : hoverFill)
                                 : QBrush(Qt::NoBrush));
        painter->setPen(QPen(
            accent,
            cellStrokeWidth));
        painter->drawEllipse(stateRect);
    } else if (hovered && enabled) {
        const QColor hoverColor = pressed ? pressedFill : hoverFill;
        painter->setPen(Qt::NoPen);
        painter->setBrush(hoverColor);
        painter->drawEllipse(stateRect);
    }

    const QWidget *focusWidget = QApplication::focusWidget();
    const bool hasFocusWithin = q_ptr->hasFocus()
        || (focusWidget != nullptr && q_ptr->isAncestorOf(focusWidget));
    const auto *fluentStyle = qobject_cast<const ZzFluentStyle *>(
        q_ptr->style());
    const bool showFocusVisual = fluentStyle != nullptr
        ? fluentStyle->isFocusVisualVisible(q_ptr)
        : hasFocusWithin;
    if (selected && hasFocusWithin && showFocusVisual) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(
            q_ptr->palette().color(activeGroup, QPalette::HighlightedText),
            cellStrokeWidth));
        painter->drawEllipse(stateRect.adjusted(
            cellStrokeWidth,
            cellStrokeWidth,
            -cellStrokeWidth,
            -cellStrokeWidth));
    }

    const QPalette::ColorRole textRole = selected
        ? QPalette::HighlightedText
        : QPalette::Text;
    const QPalette::ColorGroup colorGroup = selected
        ? activeGroup
        : textGroup;
    QColor textColor = q_ptr->palette().color(colorGroup, textRole);
    if (selected) {
        textColor = ZzControlAppearancePrivate::text(q_ptr->palette());
    } else if (enabled && !adjacentMonth
        && !workingDays[static_cast<std::size_t>(date.dayOfWeek() - 1)]) {
        textColor = accent;
    }
    if (!enabled || adjacentMonth) {
        textColor.setAlpha(150);
    }
    painter->setPen(textColor);
    painter->drawText(
        rect,
        Qt::AlignCenter,
        dayTexts.at(static_cast<std::size_t>(date.day() - 1)));
    painter->restore();
}

} // namespace ZzFluentUI
