#pragma once

#include <memory>
#include <QtCore/QPointer>

namespace ZzFluentUI {
class ZzFluentTitleBar;
}
namespace ZzWindowKit {
class ZzWindowAgent;
}
namespace ZzPureTools {

/** @brief 保存辅助窗口 chrome 的独占代理与 Qt 子树观察指针。 */
class ZzAuxiliaryWindowChromePrivate final
{
public:
    QPointer<ZzFluentUI::ZzFluentTitleBar> titleBar;
    std::unique_ptr<ZzWindowKit::ZzWindowAgent> agent;
};

} // namespace ZzPureTools
