#pragma once

#include <ZzCore/ZzResult.h>

class QSettings;
namespace ZzPureTools { class ZzPureApplication; }

/** @brief 注册起步页面、窗口外观和主题保存；settings 须存活到应用关闭。 */
[[nodiscard]] ZzCore::ZzResult<void> zzBuildStarter(
    ZzPureTools::ZzPureApplication &application, QSettings &settings);

/** @brief 使用真实页面按钮执行安装消费回归，失败返回非零。 */
[[nodiscard]] int zzRunStarterSmoke(
    ZzPureTools::ZzPureApplication &application, QSettings &settings);
