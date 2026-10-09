#include "ZzColorPickerButtonPrivate.h"
#include "ZzWidgetTheme.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorPickerButton.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzScrollArea.h>

namespace ZzFluentUI {
namespace {
/** @brief 绘制带主题边框的 Fluent 弹层，不拥有独立颜色状态。 */
class ColorPickerPopup final : public QWidget
{
public:
    explicit ColorPickerPopup(QWidget *parent)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint), theme(this)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoMouseReplay);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto snapshot = theme.snapshot();
        painter.setBrush(snapshot->color(ZzColorToken::Surface));
        painter.setPen(snapshot->color(ZzColorToken::ControlStroke));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 8, 8);
    }
    void changeEvent(QEvent *event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
            theme.refreshFallback();
            update();
        }
    }
private:
    ZzWidgetTheme theme;
};
} // namespace

ZzColorPickerButtonPrivate::ZzColorPickerButtonPrivate(ZzColorPickerButton *q)
    : QObject(q), q_ptr(q), popup(new ColorPickerPopup(q)),
      scrollArea(new ZzScrollArea(popup)),
      picker(new ZzColorPicker(popup))
{
    q->setFixedSize(68, 32);
    q->setFocusPolicy(Qt::StrongFocus);
    q->setToolButtonStyle(Qt::ToolButtonTextOnly);
    picker->setAppearance(ZzColorPicker::Fluent);
    picker->setAlphaEnabled(true);
    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(1, 1, 1, 1);
    // The popup must be smaller than the picker's minimum on small screens.
    // Keep that minimum on the scroll content, not on the top-level window.
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    scrollArea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(picker);
    scrollArea->setAutoFillBackground(false);
    scrollArea->viewport()->setAutoFillBackground(false);
    picker->setAutoFillBackground(false);
    layout->addWidget(scrollArea);
    qApp->installEventFilter(this);
    refreshText();
    connect(q, &QToolButton::clicked, this, [this] { togglePopup(); });
    connect(picker, &ZzColorPicker::currentColorChanged, q,
            [this](const QColor &color) {
                // Snapshot before emitting: a consumer may delete the whole tree.
                const QColor snapshot = color;
                q_ptr->update();
                Q_EMIT q_ptr->selectedColorChanged(snapshot);
            });
}

void ZzColorPickerButtonPrivate::refreshText()
{
    q_ptr->setAccessibleName(ZzColorPickerButton::tr("选择颜色"));
    popup->setAccessibleName(ZzColorPickerButton::tr("颜色选择器"));
}

void ZzColorPickerButtonPrivate::togglePopup()
{
    if (popup->isVisible()) {
        popup->hide();
        return;
    }
    popup->setLayoutDirection(q_ptr->layoutDirection());
    popup->setPalette(q_ptr->palette());
    QScreen *screen = q_ptr->screen();
    if (!screen) screen = QApplication::primaryScreen();
    const QRect available = screen->availableGeometry();
    const QSize preferred = (picker->sizeHint().expandedTo(picker->minimumSizeHint()) + QSize(2, 2))
            .expandedTo(QSize(360, 0));
    popup->resize(preferred.boundedTo(available.size()));
    popup->layout()->activate();
    // Position from the actual resulting window size, never the resize request.
    const QSize size = popup->size();
    const QRect anchor(q_ptr->mapToGlobal(QPoint()), q_ptr->size());
    int x = q_ptr->isRightToLeft() ? anchor.right() - size.width() + 1 : anchor.left();
    int y = anchor.bottom() + 5;
    if (y + size.height() > available.bottom() + 1) y = anchor.top() - size.height() - 4;
    x = qBound(available.left(), x, available.right() - size.width() + 1);
    y = qBound(available.top(), y, available.bottom() - size.height() + 1);
    popup->move(x, y);
    popup->show();
    picker->setFocus(Qt::PopupFocusReason);
    q_ptr->update();
}

bool ZzColorPickerButtonPrivate::eventFilter(QObject *watched, QEvent *event)
{
    // Native popup grabs may send the closing release to the popup. A fresh
    // anchor press starts a new gesture and must not inherit that suppression.
    if (watched == q_ptr && event->type() == QEvent::MouseButtonPress) {
        suppressAnchorRelease = false;
    }
    if (watched == q_ptr && suppressAnchorRelease && event->type() == QEvent::MouseButtonRelease) {
        suppressAnchorRelease = false;
        return true;
    }
    if (watched == popup && event->type() == QEvent::Hide) {
        q_ptr->update();
        // QWidget::hide may still be traversing its focus chain. Flush only after
        // that stack returns, when a synchronous consumer can safely delete us.
        QTimer::singleShot(0, q_ptr, [button = QPointer<ZzColorPickerButton>(q_ptr)] {
            if (!button || button->colorPicker()->window()->isVisible()) return;
            button->colorPicker()->commitPendingEdits();
            if (button && !button->colorPicker()->window()->isVisible()) {
                button->activateWindow();
                button->setFocus(Qt::PopupFocusReason);
            }
        });
    }
    if (!popup->isVisible()) return false;
    auto *widget = qobject_cast<QWidget *>(watched);
    const bool inside = widget && (widget == popup || popup->isAncestorOf(widget));
    if (event->type() == QEvent::KeyPress && inside
        && QApplication::activePopupWidget() == popup
        && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        popup->hide();
        return true;
    }
    if (event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        const QPoint position = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
        const QRect anchor(q_ptr->mapToGlobal(QPoint()), q_ptr->size());
        // On constrained screens the popup can cover its anchor. A click in
        // visible popup content must edit/scroll, even over the anchor's bounds.
        if (anchor.contains(position)
            && (watched == q_ptr || !popup->frameGeometry().contains(position))) {
            suppressAnchorRelease = true;
            popup->hide();
            return true;
        }
        // A combo popup belonging to the picker keeps its own native lifecycle.
        if (!inside && QApplication::activePopupWidget() == popup) popup->hide();
    }
    return false;
}
} // namespace ZzFluentUI
