#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QSize>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

#include "ZzStyleCache.h"

#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeChangeKind.h>

namespace ZzFluentUI {

class ZzFluentStyle;
class ZzThemeController;
class ZzThemeSnapshot;

/** @brief 持有主题快照、非拥有控制器引用和 Widgets 私有缓存。 */
class ZzFluentStylePrivate final
{
public:
    /** @brief 绑定控制器并用首个快照初始化固定视觉槽。 */
    ZzFluentStylePrivate(
        ZzFluentStyle *q,
        ZzThemeController *controller);

    /** @brief 在 GUI 线程执行有界 SVG 资源渲染和缓存。 */
    [[nodiscard]] QPixmap iconPixmap(
        const ZzIconDescriptor &descriptor,
        QSize logicalSize,
        qreal devicePixelRatio,
        QColor color,
        Qt::LayoutDirection direction);

    /** @brief 同步新快照并按变更分类刷新绘制或几何。 */
    void applySnapshot(ZzThemeChangeKinds changes);

    ZzFluentStyle *const q_ptr;
    QPointer<ZzThemeController> controller;
    std::shared_ptr<const ZzThemeSnapshot> snapshot;
    quint64 iconRevision = 0;
    ZzStyleCache cache{4 * 1024 * 1024};
};

} // namespace ZzFluentUI
