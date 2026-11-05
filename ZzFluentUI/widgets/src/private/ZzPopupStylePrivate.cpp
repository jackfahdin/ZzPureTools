#include "ZzPopupStylePrivate.h"

#include <QtCore/QVariant>
#include <QtGui/QGuiApplication>
#include <QtGui/QAction>
#include <QtGui/QScreen>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>

namespace ZzFluentUI {
namespace {
constexpr auto zzPopupBackgroundProperty = "_zzFluentPopupOriginalBackground";
constexpr int zzPopupGap = 3;
constexpr int zzTranslucentFlag = 1;
constexpr int zzNoBackgroundFlag = 2;

bool zzIsStandardPopup(const QWidget *widget)
{
    return widget && widget->windowType() == Qt::Popup
        && (qobject_cast<const QMenu *>(widget)
            || (qobject_cast<const QComboBox *>(widget->parentWidget())
                // A public view() lookup here would lazily create the real
                // container while polishing an unrelated custom popup. Use
                // Qt's meta-object identity without depending on private ABI.
                && widget->inherits("QComboBoxPrivateContainer")));
}
} // namespace

void ZzPopupStylePrivate::polish(QWidget *widget, bool enabled)
{
    if (!widget) {
        return;
    }
    const QVariant original = widget->property(zzPopupBackgroundProperty);
    if (!enabled) {
        if (original.isValid()) {
            const int flags = original.toInt();
            widget->setAttribute(Qt::WA_TranslucentBackground, (flags & zzTranslucentFlag) != 0);
            // Enabling translucency also enables NoSystemBackground. Restore
            // both independently when the caller switches away from Fluent.
            widget->setAttribute(Qt::WA_NoSystemBackground, (flags & zzNoBackgroundFlag) != 0);
            widget->setProperty(zzPopupBackgroundProperty, QVariant());
        }
        return;
    }
    if (!zzIsStandardPopup(widget) || original.isValid()) {
        return;
    }
    const int flags = (widget->testAttribute(Qt::WA_TranslucentBackground) ? zzTranslucentFlag : 0)
        | (widget->testAttribute(Qt::WA_NoSystemBackground) ? zzNoBackgroundFlag : 0);
    widget->setProperty(zzPopupBackgroundProperty, flags);
    widget->setAttribute(Qt::WA_TranslucentBackground);
}

QRect ZzPopupStylePrivate::comboAnchorRect(const QRect &rect)
{
    // Qt positions below at bottom() and above at top(), both inclusive.
    // Expand by gap + 1 so neither direction shares a pixel with the control.
    return rect.isEmpty() ? QRect() : rect.adjusted(0, -zzPopupGap - 1, 0, zzPopupGap + 1);
}

void ZzPopupStylePrivate::alignMenuBarPopup(QWidget *widget)
{
    auto *menu = qobject_cast<QMenu *>(widget);
    if (!menu || QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        return;
    }
    QMenuBar *bar = nullptr;
    // addMenu(QMenu*) associates the action without transferring ownership.
    for (QObject *object : menu->menuAction()->associatedObjects()) {
        auto *candidate = qobject_cast<QMenuBar *>(object);
        if (candidate && candidate->isVisible() && !candidate->isNativeMenuBar()
            && candidate->activeAction() == menu->menuAction() && candidate->style() == menu->style()) {
            bar = candidate;
            break;
        }
    }
    if (!bar) {
        return;
    }
    const QRect item = bar->actionGeometry(menu->menuAction());
    if (item.isEmpty() || !menu->screen()) {
        return;
    }
    const bool rtl = bar->isRightToLeft();
    const int edge = bar->mapToGlobal(rtl ? item.topRight() : item.topLeft()).x()
        + (rtl ? -zzMenuBarItemHorizontalInset : zzMenuBarItemHorizontalInset);
    // QMenu has selected the screen at the popup point. A spanning menu bar
    // may belong to a different screen, so its screen must not override Qt.
    const QRect available = menu->screen()->availableGeometry();
    const int left = rtl ? edge - menu->width() + 1 : edge;
    // Qt has already chosen the height, vertical placement and screen fit.
    // Clamp the adjusted X once, never offset the previous popup position.
    const int lastLeft = qMax(available.left(), available.right() - menu->width() + 1);
    menu->move(qBound(available.left(), left, lastLeft), menu->y());
}

} // namespace ZzFluentUI
