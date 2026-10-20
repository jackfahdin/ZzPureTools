#include <ZzFluentUI/ZzToolTip.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <QtCore/QBasicTimer>
#include <QtCore/QPointer>
#include <QtCore/QTimerEvent>

namespace ZzFluentUI {

namespace {

constexpr int kTextMarginH = 10;
constexpr int kTextMarginV = 6;
constexpr int kHideDelayMs = 10000;

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
        setFont(QApplication::font("QTipLabel"));
        qApp->installEventFilter(this);
        reuse(text, globalPos, parent);
    }

    void reuse(const QString &text, const QPoint &globalPos, QWidget *parent)
    {
        parent_ = parent;
        text_ = text;
        hideTimer_.start(kHideDelayMs, this);
        adjustSize();
        placeNear(globalPos);
        show();
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        return { fm.horizontalAdvance(text_) + 2 * kTextMarginH + 2,
                 fm.height() + 2 * kTextMarginV + 2 };
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
        painter.setBrush(palette().color(QPalette::ToolTipBase));
        painter.drawRoundedRect(frame, 4.0, 4.0);
        painter.setPen(palette().color(QPalette::ToolTipText));
        painter.drawText(rect().adjusted(kTextMarginH, kTextMarginV,
                                         -kTextMarginH, -kTextMarginV),
                         Qt::AlignLeft | Qt::AlignVCenter, text_);
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
        default:
            break;
        }
        return false;
    }

private:
    void placeNear(const QPoint &globalPos)
    {
        QRect bounds;
        if (QScreen *screen = QGuiApplication::screenAt(globalPos))
            bounds = screen->availableGeometry();
        else if (QScreen *primary = QGuiApplication::primaryScreen())
            bounds = primary->availableGeometry();

        QPoint pos = globalPos + QPoint(2, 16);
        if (bounds.isValid()) {
            if (pos.x() + width() > bounds.right())
                pos.setX(bounds.right() - width() + 1);
            if (pos.y() + height() > bounds.bottom())
                pos.setY(globalPos.y() - height() - 2);
            pos.setX(qMax(pos.x(), bounds.left()));
            pos.setY(qMax(pos.y(), bounds.top()));
        }
        move(pos);
    }

    QString text_;
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
        tipInstance->close();
        tipInstance->deleteLater();
        tipInstance.clear();
    }
}

bool ZzToolTip::isVisible()
{
    return tipInstance != nullptr && tipInstance->isVisible();
}

} // namespace ZzFluentUI
