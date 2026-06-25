#include "ZzComboBox.h"

#include <QAbstractItemView>
#include <QListView>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionComboBox>
#include <QStylePainter>

#include "ZzApplication.h"
#include "ZzFluentPainter.h"
#include "ZzTheme.h"
#include "private/ZzComboBoxPrivate.h"

ZzComboBoxItemDelegate::ZzComboBoxItemDelegate(ZzTheme* theme, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_theme(theme)
{
}

void ZzComboBoxItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (m_theme == nullptr) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    ZzControlState state = ZzControlState::Normal;
    if (!option.state.testFlag(QStyle::State_Enabled)) {
        state = ZzControlState::Disabled;
    } else if (option.state.testFlag(QStyle::State_MouseOver)) {
        state = ZzControlState::Hovered;
    }

    const QRectF rect = option.rect;
    ZzFluentPainter::drawPopupItem(
        *painter,
        rect,
        m_theme->colors(),
        m_theme->metrics(),
        state,
        selected);

    painter->setPen(m_theme->textColor(state));
    painter->drawText(
        rect.adjusted(m_theme->metrics().contentPadding, 0, -8, 0),
        Qt::AlignVCenter | Qt::AlignLeft,
        index.data(Qt::DisplayRole).toString());
    painter->restore();
}

QSize ZzComboBoxItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index)
    const int height = m_theme != nullptr ? m_theme->metrics().controlHeight : 32;
    return QSize(option.rect.width(), height);
}

ZzComboBoxPrivate::ZzComboBoxPrivate(ZzComboBox* q)
    : q_ptr(q)
{
}

ZzComboBox::ZzComboBox(QWidget* parent)
    : QComboBox(parent)
    , d_ptr(new ZzComboBoxPrivate(this))
{
    initialize();
}

ZzComboBox::~ZzComboBox()
{
    delete d_ptr;
}

void ZzComboBox::initialize()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(ZzApplication::instance().theme() != nullptr
        ? ZzApplication::instance().theme()->metrics().controlHeight
        : 32);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);

    auto* listView = new QListView(this);
    listView->setFrameShape(QFrame::NoFrame);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setView(listView);

    ZzTheme* theme = ZzApplication::instance().theme();
    d_ptr->itemDelegate = new ZzComboBoxItemDelegate(theme, listView);
    listView->setItemDelegate(d_ptr->itemDelegate);

    bindTheme();
    refreshPopupStyle();
}

void ZzComboBox::bindTheme()
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        return;
    }

    connect(theme, &ZzTheme::colorsChanged, this, [this]() {
        refreshPopupStyle();
        update();
        if (view() != nullptr) {
            view()->viewport()->update();
        }
    });
    connect(theme, &ZzTheme::metricsChanged, this, [this]() {
        setMinimumHeight(theme->metrics().controlHeight);
        update();
    });
}

void ZzComboBox::refreshPopupStyle()
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr || view() == nullptr) {
        return;
    }

    const ZzColorTokens& colors = theme->colors();
    view()->setStyleSheet(QStringLiteral(
        "QAbstractItemView {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: %3px;"
        "  outline: none;"
        "  padding: 4px;"
        "}"
        "QAbstractItemView::item {"
        "  min-height: %4px;"
        "}")
                              .arg(colors.popupBackground.name(QColor::HexArgb))
                              .arg(colors.popupBorder.name(QColor::HexArgb))
                              .arg(theme->metrics().controlRadius + 2)
                              .arg(theme->metrics().controlHeight));
}

ZzControlState ZzComboBox::controlState() const
{
    if (!isEnabled()) {
        return ZzControlState::Disabled;
    }
    if (d_ptr->popupVisible) {
        return ZzControlState::Pressed;
    }
    if (d_ptr->hovered) {
        return ZzControlState::Hovered;
    }
    return ZzControlState::Normal;
}

void ZzComboBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        QComboBox::paintEvent(event);
        return;
    }

    QPainter painter(this);
    const QRectF rect = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
    ZzFluentPainter::drawComboSurface(
        painter,
        rect,
        theme->colors(),
        theme->metrics(),
        controlState(),
        d_ptr->popupVisible);

    const QRect textRect = QRect(
        theme->metrics().contentPadding,
        0,
        width() - theme->metrics().contentPadding - 28,
        height());
    painter.setPen(theme->textColor(controlState()));
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, currentText());
}

void ZzComboBox::showPopup()
{
    d_ptr->popupVisible = true;
    update();
    QComboBox::showPopup();
}

void ZzComboBox::hidePopup()
{
    d_ptr->popupVisible = false;
    update();
    QComboBox::hidePopup();
}

void ZzComboBox::enterEvent(QEnterEvent* event)
{
    d_ptr->hovered = true;
    update();
    QComboBox::enterEvent(event);
}

void ZzComboBox::leaveEvent(QEvent* event)
{
    d_ptr->hovered = false;
    update();
    QComboBox::leaveEvent(event);
}

void ZzComboBox::resizeEvent(QResizeEvent* event)
{
    QComboBox::resizeEvent(event);
    if (view() != nullptr && view()->window() != nullptr) {
        view()->window()->setFixedWidth(width());
    }
}
