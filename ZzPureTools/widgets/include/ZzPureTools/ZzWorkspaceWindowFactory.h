#pragma once

#include <functional>
#include <memory>

#include <QtCore/QStringView>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzWorkspaceWindowCreateOptions.h>
#include <ZzPureTools/ZzWorkspaceWindowHandle.h>

class QWidget;

namespace ZzPureTools {

/** @brief 按创建选项装配并返回工作区窗口的工厂契约。 */
using ZzWorkspaceWindowFactory = std::function<
    ZzCore::ZzResult<ZzWorkspaceWindowHandle>(
        const ZzWorkspaceWindowCreateOptions &)>;

/** @brief 按路由标识延迟创建一个无父对象工作区页面的工厂契约。 */
using ZzWorkspacePageResolver = std::function<
    ZzCore::ZzResult<std::unique_ptr<QWidget>>(QStringView)>;

} // namespace ZzPureTools
