#pragma once

#include <QtCore/QString>
#include <QtGui/QFont>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/**
 * @brief 管理随组件发布的 ZzAwesome 应用字体。
 *
 * 字体只允许在应用 GUI 线程注册。首次成功注册后，后续调用仅返回
 * 已缓存的注册结果，不会重复读取字体资源。
 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzIconFont final
{
public:
    ZzIconFont() = delete;

    /**
     * @brief 确保内嵌字体已注册到当前 Qt 应用。
     * @return 字体存在、注册成功且 family 匹配时返回 true。
     * @pre 已创建 QGuiApplication，且调用发生在应用 GUI 线程。
     */
    [[nodiscard]] static bool ensureRegistered();

    /**
     * @brief 返回内嵌字体的稳定 family 名称。
     * @return 始终为 ZzAwesome。
     */
    [[nodiscard]] static QString familyName();

    /**
     * @brief 创建使用内嵌字体的 QFont。
     * @param pixelSize 大于零时设置字体像素尺寸；否则保留 Qt 默认尺寸。
     * @return 注册成功时返回 ZzAwesome 字体，失败时返回默认 QFont。
     */
    [[nodiscard]] static QFont font(int pixelSize = -1);
};

} // namespace ZzFluentUI
