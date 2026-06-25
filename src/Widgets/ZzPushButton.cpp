#include "ZzPureTools/Widgets/ZzPushButton.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include "ZzPureTools/Core/ZzApplication.hpp"
#include "ZzPureTools/Core/ZzTheme.hpp"
#include "ZzPureTools/Style/ZzAnimator.hpp"
#include "ZzPushButtonDelegate.hpp"

ZzPushButton::ZzPushButton(QWidget* parent)
    : QPushButton(parent)
    , m_animator(new ZzAnimator(this))
    , m_delegate(new ZzPushButtonDelegate(m_buttonStyle))
{
    initialize();
}

ZzPushButton::ZzPushButton(const QString& text, QWidget* parent)
    : ZzPushButton(parent)
{
    setText(text);
}

ZzPushButton::~ZzPushButton()
{
    delete m_delegate;
}

void ZzPushButton::setButtonStyle(ZzButtonStyle style)
{
    if (m_buttonStyle == style) {
        return;
    }

    m_buttonStyle = style;
    m_delegate->setButtonStyle(style);
    update();
}

ZzPushButton::ZzButtonStyle ZzPushButton::buttonStyle() const noexcept
{
    return m_buttonStyle;
}

void ZzPushButton::initialize()
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    if (ZzTheme* theme = ZzApplication::instance().theme()) {
        setMinimumHeight(theme->metrics().controlHeight);
        m_animator->setDuration(theme->metrics().animationDurationMs);
    } else {
        setMinimumHeight(32);
    }

    setAttribute(Qt::WA_Hover, true);
    setFlat(true);
    setStyleSheet(QStringLiteral("border: none; background: transparent;"));

    connect(m_animator, &ZzAnimator::updated, this, qOverload<>(&ZzPushButton::update));
    bindTheme();
}

void ZzPushButton::bindTheme()
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        return;
    }

    connect(theme, &ZzTheme::colorsChanged, this, qOverload<>(&ZzPushButton::update));
    connect(theme, &ZzTheme::metricsChanged, this, [this, theme]() {
        setMinimumHeight(theme->metrics().controlHeight);
        m_animator->setDuration(theme->metrics().animationDurationMs);
        update();
    });
}

ZzStyleContext ZzPushButton::buildStyleContext() const
{
    ZzStyleContext context;
    context.theme = ZzApplication::instance().theme();
    context.enabled = isEnabled();
    context.text = text();
    context.hoverProgress = m_animator->hoverProgress();
    context.pressProgress = m_animator->pressProgress();
    context.state = m_animator->resolvedState(context.enabled);
    return context;
}

void ZzPushButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    if (m_delegate == nullptr || ZzApplication::instance().theme() == nullptr) {
        QPushButton::paintEvent(event);
        return;
    }

    QPainter painter(this);
    const QRectF rect = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
    m_delegate->paint(painter, rect, buildStyleContext());
}

void ZzPushButton::enterEvent(QEnterEvent* event)
{
    m_animator->setHovered(true);
    QPushButton::enterEvent(event);
}

void ZzPushButton::leaveEvent(QEvent* event)
{
    m_animator->setHovered(false);
    m_animator->setPressed(false);
    QPushButton::leaveEvent(event);
}

void ZzPushButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_animator->setPressed(true);
    }
    QPushButton::mousePressEvent(event);
}

void ZzPushButton::mouseReleaseEvent(QMouseEvent* event)
{
    m_animator->setPressed(false);
    QPushButton::mouseReleaseEvent(event);
}

void ZzPushButton::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled()) {
            m_animator->reset();
        }
        update();
    }
    QPushButton::changeEvent(event);
}
