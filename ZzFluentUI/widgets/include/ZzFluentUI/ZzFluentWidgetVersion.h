#pragma once

#include <QtCore/QString>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/**
 * @brief 提供 Fluent Widgets 的运行时版本信息。
 */
class ZZ_FLUENT_UI_EXPORT ZzFluentWidgetVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求 QApplication 存在。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzFluentUI
