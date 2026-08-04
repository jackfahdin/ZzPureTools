#pragma once

#include <memory>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageInstance.h>

class QWidget;

/** @brief 创建只包含静态文本与只读状态的演示页面。 */
class ZzDemoPageFactory final
{
public:
    /**
     * @brief 创建 Home 页面。
     * @param pageParent View 必须使用的非空 Qt 父对象。
     * @return 完整页面实例，或页面所有权校验错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<
        ZzPureTools::ZzPageInstance>> createHome(QWidget *pageParent);

    /**
     * @brief 创建 Details 页面。
     * @param pageParent View 必须使用的非空 Qt 父对象。
     * @return 完整页面实例，或页面所有权校验错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<
        ZzPureTools::ZzPageInstance>> createDetails(QWidget *pageParent);
};
