#pragma once

#include <functional>

#include <ZzCore/ZzResult.h>

namespace ZzPureTools {

class ZzApplicationWindow;

/**
 * @brief 在窗口基础展示对象创建后、WindowKit 配置和显示前完成应用壳层装配。
 *
 * 回调只在应用 GUI 线程调用。窗口由 ZzPureApplication 独占，回调不得删除、显示
 * 或转移窗口；失败结果会放弃本次窗口创建。
 */
using ZzWindowSetupCallback = std::function<ZzCore::ZzResult<void>(
    ZzApplicationWindow &window)>;

} // namespace ZzPureTools
