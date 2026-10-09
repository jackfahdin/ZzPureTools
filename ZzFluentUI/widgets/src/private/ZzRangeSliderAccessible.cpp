#include "ZzRangeSliderAccessible.h"

#include <algorithm>
#include <array>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtGui/QAccessible>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAccessibleWidget>

namespace ZzFluentUI {
#if QT_CONFIG(accessibility)
namespace {
using ZzSliderHandle = ZzRangeSlider::ZzSliderHandle;

/** @brief 虚拟端点由 Qt 无障碍缓存拥有，不创建额外 QWidget。 */
class ZzAccessibleRangeEndpoint final : public QAccessibleInterface,
    public QAccessibleValueInterface, public QAccessibleActionInterface
{
public:
    ZzAccessibleRangeEndpoint(ZzRangeSlider *slider, ZzSliderHandle handle)
        : slider_(slider), handle_(handle) {}
    bool isValid() const override { return !slider_.isNull(); }
    QObject *object() const override { return nullptr; }
    QWindow *window() const override
    { const auto *root = parent(); return root ? root->window() : nullptr; }
    QAccessibleInterface *parent() const override
    { return slider_ ? QAccessible::queryAccessibleInterface(slider_) : nullptr; }
    QAccessibleInterface *child(int) const override { return nullptr; }
    QAccessibleInterface *childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }
    QAccessible::Role role() const override { return QAccessible::Slider; }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        result.invalid = !slider_;
        result.focusable = slider_ && slider_->focusPolicy() != Qt::NoFocus;
        result.focused = slider_ && slider_->hasFocus() && slider_->activeHandle() == handle_;
        result.disabled = !slider_ || !slider_->isEnabled();
        result.invisible = !slider_ || !slider_->isVisible();
        return result;
    }
    QString text(QAccessible::Text type) const override
    {
        if (!slider_) return {};
        if (type == QAccessible::Value) return slider_->locale().toString(currentValue().toInt());
        if (type == QAccessible::Name) {
            const auto suffix = QCoreApplication::translate("ZzRangeSlider",
                handle_ == ZzSliderHandle::LowerHandle ? "下限" : "上限");
            return slider_->accessibleName().isEmpty() ? suffix
                : slider_->accessibleName() + QStringLiteral(" ") + suffix;
        }
        if (type == QAccessible::Description) return slider_->accessibleDescription();
        return {};
    }
    void setText(QAccessible::Text type, const QString &text) override
    { if (type == QAccessible::Value) setCurrentValue(text); }
    QRect rect() const override
    {
        if (!slider_ || !slider_->isVisible()) return {};
        const bool horizontal = slider_->orientation() == Qt::Horizontal;
        const qint64 span = qint64(slider_->maximum()) - slider_->minimum();
        const int value = handle_ == ZzSliderHandle::LowerHandle
            ? slider_->lowerPosition() : slider_->upperPosition();
        qreal fraction = span ? qreal(qint64(value) - slider_->minimum()) / qreal(span) : 0;
        if (span && (!horizontal || slider_->layoutDirection() == Qt::RightToLeft))
            fraction = 1 - fraction;
        const int axis = qRound(10 + fraction * qMax(0,
            (horizontal ? slider_->width() : slider_->height()) - 20));
        const QPoint center = horizontal ? QPoint(axis, slider_->height() / 2)
                                        : QPoint(slider_->width() / 2, axis);
        return QRect(slider_->mapToGlobal(center - QPoint(10, 10)), QSize(20, 20));
    }
    void *interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface) return static_cast<QAccessibleValueInterface *>(this);
        if (type == QAccessible::ActionInterface) return static_cast<QAccessibleActionInterface *>(this);
        return nullptr;
    }
    QVariant currentValue() const override
    { return slider_ ? QVariant(handle_ == ZzSliderHandle::LowerHandle ? slider_->lowerValue() : slider_->upperValue()) : QVariant(); }
    QVariant minimumValue() const override
    { return slider_ ? QVariant(handle_ == ZzSliderHandle::LowerHandle ? slider_->minimum() : slider_->lowerValue()) : QVariant(); }
    QVariant maximumValue() const override
    { return slider_ ? QVariant(handle_ == ZzSliderHandle::LowerHandle ? slider_->upperValue() : slider_->maximum()) : QVariant(); }
    QVariant minimumStepSize() const override
    { return slider_ ? QVariant(slider_->singleStep()) : QVariant(); }
    void setCurrentValue(const QVariant &value) override
    {
        if (!slider_ || !slider_->isEnabled()) return;
        bool valid = false;
        const qlonglong requested = value.toLongLong(&valid);
        if (!valid) return;
        const int bounded = int(std::clamp<qlonglong>(requested,
            minimumValue().toInt(), maximumValue().toInt()));
        if (handle_ == ZzSliderHandle::LowerHandle) slider_->setLowerValue(bounded);
        else slider_->setUpperValue(bounded);
    }
    QStringList actionNames() const override
    { return {setFocusAction(), increaseAction(), decreaseAction()}; }
    QStringList keyBindingsForAction(const QString &) const override { return {}; }
    void doAction(const QString &action) override
    {
        if (!slider_ || !slider_->isEnabled()) return;
        if (action == setFocusAction()) {
            // 焦点回调可能同步销毁控件及本虚拟接口，之后只使用局部保护。
            QPointer<ZzRangeSlider> slider = slider_;
            const bool lower = handle_ == ZzSliderHandle::LowerHandle;
            slider->setFocus(Qt::OtherFocusReason);
            if (!slider) return;
            const int key = slider->orientation() == Qt::Horizontal
                ? (lower ? Qt::Key_Down : Qt::Key_Up)
                : (lower ? Qt::Key_Left : Qt::Key_Right);
            QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
            QCoreApplication::sendEvent(slider, &event);
        } else if (action == increaseAction() || action == decreaseAction()) {
            const qlonglong delta = action == increaseAction()
                ? qlonglong(slider_->singleStep()) : -qlonglong(slider_->singleStep());
            setCurrentValue(currentValue().toLongLong() + delta);
        }
    }
private:
    QPointer<ZzRangeSlider> slider_;
    ZzSliderHandle handle_;
};

/** @brief 父接口管理两个虚拟子接口的缓存寿命，避免控件销毁后留下端点。 */
class ZzAccessibleRangeSlider final : public QAccessibleWidget
{
public:
    explicit ZzAccessibleRangeSlider(ZzRangeSlider *slider)
        : QAccessibleWidget(slider, QAccessible::Grouping), slider_(slider)
    {
        ids_[0] = QAccessible::registerAccessibleInterface(new ZzAccessibleRangeEndpoint(slider, ZzSliderHandle::LowerHandle));
        ids_[1] = QAccessible::registerAccessibleInterface(new ZzAccessibleRangeEndpoint(slider, ZzSliderHandle::UpperHandle));
    }
    ~ZzAccessibleRangeSlider() override
    { for (auto id : ids_) QAccessible::deleteAccessibleInterface(id); }
    int childCount() const override { return 2; }
    QAccessibleInterface *child(int index) const override
    { return index >= 0 && index < 2 ? QAccessible::accessibleInterface(ids_[size_t(index)]) : nullptr; }
    int indexOfChild(const QAccessibleInterface *item) const override
    { return item == child(0) ? 0 : item == child(1) ? 1 : -1; }
    QAccessibleInterface *focusChild() const override
    { return slider_ && slider_->hasFocus() ? child(slider_->activeHandle() == ZzSliderHandle::UpperHandle ? 1 : 0) : nullptr; }
    QAccessibleInterface *childAt(int x, int y) const override
    {
        const int first = slider_ && slider_->activeHandle() == ZzSliderHandle::UpperHandle ? 1 : 0;
        for (int index : {first, 1 - first})
            if (auto *item = child(index); item && item->rect().contains(x, y)) return item;
        return nullptr;
    }
private:
    QPointer<ZzRangeSlider> slider_;
    std::array<QAccessible::Id, 2> ids_{};
};

QAccessibleInterface *zzRangeFactory(const QString &, QObject *object)
{
    if (auto *slider = qobject_cast<ZzRangeSlider *>(object)) return new ZzAccessibleRangeSlider(slider);
    return nullptr;
}
} // namespace
#endif

void ZzRangeSliderAccessible::install()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] { QAccessible::installFactory(zzRangeFactory); return true; }();
    Q_UNUSED(installed);
#endif
}

void ZzRangeSliderAccessible::notifyValueChanged(ZzRangeSlider *slider, ZzRangeSlider::ZzSliderHandle handle, int value)
{
#if QT_CONFIG(accessibility)
    if (!QAccessible::isActive()) return;
    auto *root = QAccessible::queryAccessibleInterface(slider);
    if (auto *endpoint = root ? root->child(handle == ZzSliderHandle::LowerHandle ? 0 : 1) : nullptr) {
        QAccessibleValueChangeEvent event(endpoint, value);
        QAccessible::updateAccessibility(&event);
    }
#else
    Q_UNUSED(slider); Q_UNUSED(handle); Q_UNUSED(value);
#endif
}

void ZzRangeSliderAccessible::notifyFocusChanged(ZzRangeSlider *slider)
{
#if QT_CONFIG(accessibility)
    if (!QAccessible::isActive() || !slider->hasFocus()) return;
    auto *root = QAccessible::queryAccessibleInterface(slider);
    if (auto *endpoint = root ? root->focusChild() : nullptr) {
        QAccessibleEvent event(endpoint, QAccessible::Focus);
        QAccessible::updateAccessibility(&event);
    }
#else
    Q_UNUSED(slider);
#endif
}
} // namespace ZzFluentUI
