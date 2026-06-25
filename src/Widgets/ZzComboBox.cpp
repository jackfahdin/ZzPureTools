#include "ZzPureTools/Widgets/ZzComboBox.hpp"

#include <QAbstractItemView>
#include <QListView>
#include <QPainter>
#include <QPaintEvent>

#include "ZzPureTools/Core/ZzApplication.hpp"
#include "ZzPureTools/Core/ZzTheme.hpp"
#include "ZzPureTools/Style/ZzAnimator.hpp"
#include "ZzComboBoxDelegate.hpp"

ZzComboBox::ZzComboBox(QWidget* parent)
    : QComboBox(parent)
    , m_animator(new ZzAnimator(this))
    , m_delegate(new ZzComboBoxDelegate())
{
    initialize();
}

ZzComboBox::~ZzComboBox()
{
    delete m_delegate;
}

void ZzComboBox::initialize()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (ZzTheme* theme = ZzApplication::instance().theme()) {
        setMinimumHeight(theme->metrics().controlHeight);
        m_animator->setDuration(theme->metrics().animationDurationMs);
    } else {
        setMinimumHeight(32);
    }

    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);

    auto* listView = new QListView(this);
    listView->setFrameShape(QFrame::NoFrame);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setView(listView);

    m_itemDelegate = new ZzComboBoxItemDelegate(listView);
    listView->setItemDelegate(m_itemDelegate);

    connect(m_animator, &ZzAnimator::updated, this, qOverload<>(&ZzComboBox::update));
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
    connect(theme, &ZzTheme::metricsChanged, this, [this, theme]() {
        setMinimumHeight(theme->metrics().controlHeight);
        m_animator->setDuration(theme->metrics().animationDurationMs);
        refreshPopupStyle();
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

ZzStyleContext ZzComboBox::buildStyleContext() const
{
    ZzStyleContext context;
    context.theme = ZzApplication::instance().theme();
    context.enabled = isEnabled();
    context.text = currentText();
    context.popupVisible = m_popupVisible;
    context.hoverProgress = m_animator->hoverProgress();
    context.pressProgress = m_animator->pressProgress();
    context.state = m_animator->resolvedState(context.enabled);
    return context;
}

void ZzComboBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    if (m_delegate == nullptr || ZzApplication::instance().theme() == nullptr) {
        QComboBox::paintEvent(event);
        return;
    }

    QPainter painter(this);
    const QRectF rect = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
    m_delegate->paint(painter, rect, buildStyleContext());
}

void ZzComboBox::showPopup()
{
    m_popupVisible = true;
    update();
    QComboBox::showPopup();
}

void ZzComboBox::hidePopup()
{
    m_popupVisible = false;
    update();
    QComboBox::hidePopup();
}

void ZzComboBox::enterEvent(QEnterEvent* event)
{
    m_animator->setHovered(true);
    QComboBox::enterEvent(event);
}

void ZzComboBox::leaveEvent(QEvent* event)
{
    m_animator->setHovered(false);
    QComboBox::leaveEvent(event);
}

void ZzComboBox::resizeEvent(QResizeEvent* event)
{
    QComboBox::resizeEvent(event);
    if (view() != nullptr && view()->window() != nullptr) {
        view()->window()->setFixedWidth(width());
    }
}

void ZzComboBox::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled()) {
            m_animator->reset();
        }
        update();
    }
    QComboBox::changeEvent(event);
}
