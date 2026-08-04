#pragma once

#include <memory>

#include <QtCore/QMetaObject>
#include <QtGui/QColor>

#include <ZzFluentUI/ZzThemeChangeKind.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace ZzFluentUI {

class ZzThemeController;
class ZzThemeSnapshot;

/** @brief 保存 GUI 线程主题状态并负责完整快照交换。 */
class ZzThemeControllerPrivate final
{
public:
    /** @brief 初始化系统监听和 revision 为零的首个快照。 */
    explicit ZzThemeControllerPrivate(ZzThemeController *q);

    /** @brief 构造完整新快照、交换所有权并发送分类信号。 */
    void rebuild(ZzThemeChangeKinds changes);

    /** @brief 将 System 请求解析为当前 Qt 公开颜色方案。 */
    [[nodiscard]] ZzThemeMode resolveSystemMode() const noexcept;

    ZzThemeController *const q_ptr;
    ZzThemeMode requestedMode = ZzThemeMode::System;
    ZzThemeMode activeMode = ZzThemeMode::Light;
    QColor accent = QColor(QStringLiteral("#0067c0"));
    bool reduceMotion = false;
    quint64 revision = 0;
    std::shared_ptr<const ZzThemeSnapshot> currentSnapshot;
    QMetaObject::Connection colorSchemeConnection;
};

} // namespace ZzFluentUI
