#include <ZzFluentUI/ZzToolTip.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QScreen>
#include <QtGui/QTextLayout>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <QtCore/QBasicTimer>
#include <QtCore/QPointer>
#include <QtCore/QTimerEvent>
#include <QtCore/QtMath>

namespace ZzFluentUI {

namespace {

constexpr int kTextMarginH = 10;
constexpr int kTextMarginV = 6;
constexpr int kHideDelayMs = 10000;
constexpr int kMaximumTextWidth = 480;
constexpr qreal kCornerRadius = 4.0;

/** @brief ZzToolTip 的单实例弹出窗口；颜色全部即时取自 palette()。 */
class ZzToolTipLabel final : public QWidget
{
public:
    ZzToolTipLabel(const QString &text, const QPoint &globalPos, QWidget *parent)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint
                  | Qt::WindowDoesNotAcceptFocus | Qt::BypassGraphicsProxyWidget)
    {
        setObjectName(QStringLiteral("ZzToolTipLabel"));
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        reuse(text, globalPos, parent);
    }

    void reuse(const QString &text, const QPoint &globalPos, QWidget *parent)
    {
        // Reparent the window too: a QPointer alone does not transfer ownership.
        qApp->removeEventFilter(this);
        if (parentWidget() != parent)
            setParent(parent, windowFlags());
        parent_ = parent;
        text_ = text;
        setAccessibleName(text);
        QFont textFont = QApplication::font("QTipLabel");
        textFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
            textFont.styleStrategy() | QFont::NoSubpixelAntialias));
        setFont(textFont);
        layoutText(globalPos);
        hideTimer_.start(kHideDelayMs, this);
        placeNear(globalPos);
        show();
        update();
        qApp->installEventFilter(this);
    }

    void retire()
    {
        // Deferred deletion must not leave a filter/timer affecting the next tip.
        qApp->removeEventFilter(this);
        hideTimer_.stop();
        close();
        deleteLater();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
        painter.setBrush(palette().color(QPalette::ToolTipBase));
        painter.drawRoundedRect(frame, kCornerRadius, kCornerRadius);
        painter.setPen(palette().color(QPalette::ToolTipText));
        painter.setClipRect(rect().adjusted(kTextMarginH, kTextMarginV,
                                           -kTextMarginH, -kTextMarginV));
        textLayout_.draw(&painter, QPointF(kTextMarginH, kTextMarginV));
    }

    void timerEvent(QTimerEvent *event) override
    {
        if (event->timerId() == hideTimer_.timerId()) {
            hideTimer_.stop();
            ZzToolTip::hideText();
        }
    }

    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::PaletteChange) {
            update();
        }
        QWidget::changeEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
            ZzToolTip::hideText();
            break;
        case QEvent::KeyPress: {
            const auto key = static_cast<QKeyEvent *>(event)->key();
            if (key < Qt::Key_Shift || key > Qt::Key_ScrollLock)
                ZzToolTip::hideText();
            break;
        }
        case QEvent::Leave:
            if (watched == parent_)
                ZzToolTip::hideText();
            break;
        case QEvent::Destroy:
            if (watched == parent_)
                ZzToolTip::hideText();
            break;
        case QEvent::Hide: {
            const auto *widget = qobject_cast<QWidget *>(watched);
            if (widget && parent_
                && (widget == parent_ || widget->isAncestorOf(parent_)))
                ZzToolTip::hideText();
            break;
        }
        default:
            break;
        }
        return false;
    }

private:
    static QRect screenBounds(const QPoint &globalPos)
    {
        if (QScreen *screen = QGuiApplication::screenAt(globalPos))
            return screen->availableGeometry();
        if (QScreen *primary = QGuiApplication::primaryScreen())
            return primary->availableGeometry();
        return {};
    }

    void layoutText(const QPoint &globalPos)
    {
        const QRect bounds = screenBounds(globalPos);
        const int horizontalPadding = 2 * kTextMarginH + 2;
        const int verticalPadding = 2 * kTextMarginV + 2;
        const int maximumWidth = bounds.isValid()
            ? qMax(1, qMin(kMaximumTextWidth, bounds.width() - horizontalPadding))
            : kMaximumTextWidth;
        const qreal maximumHeight = bounds.isValid()
            ? qMax(1, bounds.height() - verticalPadding) : kMaximumTextWidth;
        // QTextLayout uses a line separator for explicit line breaks.
        QString layoutText = text_;
        layoutText.replace(QLatin1Char('\n'), QChar::LineSeparator);
        textLayout_.setText(layoutText);
        textLayout_.setFont(font());
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textLayout_.setTextOption(option);
        textLayout_.beginLayout();
        qreal height = 0;
        qreal width = 0;
        while (height < maximumHeight) {
            QTextLine line = textLayout_.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(maximumWidth);
            line.setPosition(QPointF(0, height));
            height += line.height();
            width = qMax(width, line.naturalTextWidth());
        }
        textLayout_.endLayout();
        QSize size(qCeil(qMin(width, qreal(maximumWidth))) + horizontalPadding,
                   qCeil(qMin(height, maximumHeight)) + verticalPadding);
        if (bounds.isValid())
            size = size.boundedTo(bounds.size());
        resize(size);
    }

    void placeNear(const QPoint &globalPos)
    {
        const QRect bounds = screenBounds(globalPos);

        QPoint pos = globalPos + QPoint(2, 16);
        if (bounds.isValid()) {
            if (pos.x() + width() > bounds.right())
                pos.setX(bounds.right() - width() + 1);
            if (pos.y() + height() > bounds.bottom())
                pos.setY(globalPos.y() - height() - 2);
            pos.setX(qBound(bounds.left(), pos.x(), bounds.right() - width() + 1));
            pos.setY(qBound(bounds.top(), pos.y(), bounds.bottom() - height() + 1));
        }
        move(pos);
    }

    QString text_;
    QTextLayout textLayout_;
    QPointer<QWidget> parent_;
    QBasicTimer hideTimer_;
};

QPointer<ZzToolTipLabel> tipInstance;

} // namespace

void ZzToolTip::showText(const QPoint &globalPos, const QString &text, QWidget *parent)
{
    if (text.isEmpty() || parent == nullptr) {
        hideText();
        return;
    }
    if (tipInstance)
        tipInstance->reuse(text, globalPos, parent);
    else
        tipInstance = new ZzToolTipLabel(text, globalPos, parent);
}

void ZzToolTip::hideText()
{
    if (tipInstance) {
        tipInstance->retire();
        tipInstance.clear();
    }
}

bool ZzToolTip::isVisible()
{
    return tipInstance != nullptr && tipInstance->isVisible();
}

} // namespace ZzFluentUI
