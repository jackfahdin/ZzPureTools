#pragma once

#include <cstddef>

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

class QEvent;
class QObject;
class QWidget;

namespace ZzWindowKit {

class ZzSoftwareBackdrop;

/** @brief 实现软件材质层生命周期、缓存和宿主事件处理的私有类。 */
class ZzSoftwareBackdropPrivate final
{
public:
    explicit ZzSoftwareBackdropPrivate(ZzSoftwareBackdrop *q);
    ~ZzSoftwareBackdropPrivate();

    [[nodiscard]] bool attach(QWidget *host);
    void detach();
    [[nodiscard]] bool setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept;
    [[nodiscard]] std::size_t rebuildCount() const noexcept;
    [[nodiscard]] bool eventFilter(QObject *watched, QEvent *event);

private:
    void rebuild();
    void invalidate();
    void showLayer();
    void hideLayer();

    ZzSoftwareBackdrop *const q_ptr;
    QPointer<QWidget> host_;
    QPointer<QWidget> layer_;
    QMetaObject::Connection hostDestroyedConnection_;
    bool enabled_ = false;
    std::size_t rebuildCount_ = 0U;
};

} // namespace ZzWindowKit
