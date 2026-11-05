#include "ZzBreadcrumbBarPrivate.h"

#include <QtGui/QFont>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOptionToolButton>
#include <QtCore/QScopedValueRollback>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>

namespace ZzFluentUI {

namespace {

constexpr auto zzBreadcrumbIndexProperty = "zzBreadcrumbIndex";
constexpr auto zzBreadcrumbCurrentProperty = "zzBreadcrumbCurrent";
constexpr int zzBreadcrumbSpacing = 4;
constexpr int zzBreadcrumbGlyphSize = 12;
constexpr int zzBreadcrumbButtonPadding = 12;

// Keep the full label for accessibility and menus; only the painted label is
// elided when the last component alone is wider than the available space.
class ZzBreadcrumbItemButton final : public QToolButton
{
public:
    explicit ZzBreadcrumbItemButton(QWidget *parent) : QToolButton(parent) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionToolButton option;
        initStyleOption(&option);
        option.text = fontMetrics().elidedText(option.text, Qt::ElideRight,
            qMax(0, width() - zzBreadcrumbButtonPadding));
        QStylePainter painter(this);
        painter.drawComplexControl(QStyle::CC_ToolButton, option);
    }
};

} // namespace

ZzBreadcrumbBarPrivate::ZzBreadcrumbBarPrivate(ZzBreadcrumbBar *q)
    : q_ptr(q)
    , layout(new QHBoxLayout(q))
{
    Q_ASSERT(q_ptr != nullptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(zzBreadcrumbSpacing);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    overflow = new QToolButton(q);
    overflow->setObjectName(QStringLiteral("zzBreadcrumbOverflow"));
    overflow->setProperty(zzBreadcrumbIndexProperty, -1);
    overflow->setAutoRaise(true);
    overflow->setFocusPolicy(Qt::StrongFocus);
    overflow->setPopupMode(QToolButton::InstantPopup);
    overflow->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    overflow->installEventFilter(q);
    overflowSeparator = new QLabel(q);
    overflowSeparator->setAlignment(Qt::AlignCenter);
    overflowSeparator->setAttribute(Qt::WA_TransparentForMouseEvents);
    overflow->hide();
    overflowSeparator->hide();
    updateAppearance();
}

void ZzBreadcrumbBarPrivate::rebuild()
{
    buttons.clear();
    separators.clear();
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget(); widget && widget != overflow
            && widget != overflowSeparator) {
            // Navigation receivers may rebuild while a button handles input.
            // Retire it immediately, but let the active event unwind first.
            QObject::disconnect(widget, nullptr, q_ptr, nullptr);
            widget->setProperty(zzBreadcrumbIndexProperty, -1);
            widget->removeEventFilter(q_ptr);
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    layout->addWidget(overflow);
    layout->addWidget(overflowSeparator);
    hiddenCount = -1;

    for (qsizetype visual = 0; visual < items.size(); ++visual) {
        const int logicalIndex = static_cast<int>(visual);
        auto *button = new ZzBreadcrumbItemButton(q_ptr);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setText(items.at(logicalIndex));
        button->setToolTip(items.at(logicalIndex));
        button->setAccessibleName(items.at(logicalIndex));
        button->setProperty(zzBreadcrumbIndexProperty, logicalIndex);
        button->setFocusPolicy(Qt::StrongFocus);
        button->installEventFilter(q_ptr);
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QObject::connect(
            button,
            &QToolButton::clicked,
            q_ptr,
            [q = q_ptr, logicalIndex] {
                // A receiver may delete the bar. State setters already refresh
                // presentation; do not touch the bar after notifying clients.
                Q_EMIT q->indexRequested(logicalIndex);
            });
        layout->addWidget(button, 1);
        buttons.append(button);

        if (visual + 1 < items.size()) {
            auto *separator = new QLabel(q_ptr);
            separator->setAlignment(Qt::AlignCenter);
            separator->setAttribute(Qt::WA_TransparentForMouseEvents);
            separator->setAccessibleName(QString());
            layout->addWidget(separator);
            separators.append(separator);
        }
    }
    updateAppearance();
    updateCurrentState();
}

void ZzBreadcrumbBarPrivate::updateCurrentState()
{
    for (QToolButton *button : buttons) {
        const bool current =
            button->property(zzBreadcrumbIndexProperty).toInt()
            == currentIndex;
        button->setProperty(zzBreadcrumbCurrentProperty, current);
        QFont presentationFont = q_ptr->font();
        presentationFont.setBold(current);
        button->setFont(presentationFont);
    }
    updateOverflow();
}

void ZzBreadcrumbBarPrivate::updateAppearance()
{
    const QColor color = q_ptr->palette().color(q_ptr->isEnabled()
        ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
    overflow->setIcon(ZzSegoeIconFont::icon(ZzSegoeIcon::More, color));
    overflow->setAccessibleName(ZzBreadcrumbBar::tr("显示隐藏的路径"));
    overflow->setToolTip(overflow->accessibleName());
    const auto glyph = q_ptr->isRightToLeft() ? ZzSegoeIcon::ChevronLeft : ZzSegoeIcon::ChevronRight;
    QList<QLabel *> labels = separators;
    labels.append(overflowSeparator);
    for (QLabel *label : labels) {
        label->setFont(ZzSegoeIconFont::font(zzBreadcrumbGlyphSize));
        label->setText(QString(QChar(static_cast<char16_t>(glyph))));
    }
}

void ZzBreadcrumbBarPrivate::updateOverflow()
{
    if (updating) {
        return;
    }
    QScopedValueRollback<bool> guard(updating, true);
    const int count = static_cast<int>(buttons.size());
    const int available = q_ptr->contentsRect().width();
    const int separatorWidth = overflowSeparator->sizeHint().width();
    const int gap = separatorWidth + 2 * zzBreadcrumbSpacing;
    int total = qMax(0, count - 1) * gap;
    for (auto *button : buttons) {
        total += button->sizeHint().width();
    }
    int hidden = 0;
    if (count > 1 && total > available) {
        hidden = count - 1;
        int used = overflow->sizeHint().width() + gap + buttons.last()->sizeHint().width();
        while (hidden > 0 && used + gap + buttons.at(hidden - 1)->sizeHint().width() <= available) {
            --hidden;
            used += gap + buttons.at(hidden)->sizeHint().width();
        }
        // At extremely narrow widths preserve menu access to every component.
        if (available < overflow->sizeHint().width() + gap + zzBreadcrumbButtonPadding) {
            hidden = count;
        }
    }
    QWidget *focused = QApplication::focusWidget();
    bool moveFocus = focused == overflow && hidden == 0;
    for (int index = 0; index < count; ++index) {
        if (focused == buttons.at(index) && index < hidden) {
            moveFocus = true;
        }
        buttons.at(index)->setVisible(index >= hidden);
    }
    overflow->setVisible(hidden > 0);
    overflowSeparator->setVisible(hidden > 0 && hidden < count);
    for (int index = 0; index < separators.size(); ++index) {
        separators.at(index)->setVisible(index >= hidden);
    }
    if (hidden != hiddenCount) {
        if (QMenu *oldMenu = overflow->menu()) {
            for (QAction *action : oldMenu->actions()) {
                QObject::disconnect(action, nullptr, q_ptr, nullptr);
            }
            oldMenu->hide();
            oldMenu->deleteLater();
        }
        auto *menu = new QMenu(overflow);
        for (int index = 0; index < hidden; ++index) {
            auto *action = menu->addAction(items.at(index));
            action->setData(index);
            QObject::connect(action, &QAction::triggered, q_ptr, [q = q_ptr, index] {
                Q_EMIT q->indexRequested(index);
            });
        }
        overflow->setMenu(menu);
        hiddenCount = hidden;
    }
    layout->invalidate();
    layout->activate();
    if (moveFocus) {
        if (hidden > 0) {
            overflow->setFocus(Qt::OtherFocusReason);
        } else if (!buttons.isEmpty()) {
            buttons.first()->setFocus(Qt::OtherFocusReason);
        }
    }
}

} // namespace ZzFluentUI
