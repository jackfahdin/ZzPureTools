#pragma once

#include <memory>

#include <QtCore/QObject>
#include <ZzCore/ZzResult.h>
#include <ZzPureTools/ZzPureToolsExport.h>

class QMainWindow;

namespace ZzFluentUI {
class ZzFluentTitleBar;
class ZzThemeController;
}
namespace ZzWindowKit {
class ZzWindowAgent;
}

namespace ZzPureTools {

class ZzAuxiliaryWindowChromePrivate;

/**
 * @brief 为设置、关于等辅助窗口装配统一标题栏和 WindowKit 代理。
 *
 * GUI 线程专用。调用者独占本对象；窗口、主题和业务内容仍由调用者拥有。
 * 标题与图标跟随 QWidget 属性，主题跟随传入控制器。销毁本对象会释放代理
 * 和标题栏，但不销毁宿主；宿主也可以先销毁。装配期间不要替换 menuWidget。
 * 模态、尺寸和 WA_DeleteOnClose 策略由调用者决定。
 * 自绘系统按钮仅显示关闭；具有 NativeSystemButtons 能力的平台（macOS）
 * 保留原生按钮及平台行为，不承诺隐藏原生最小化和缩放按钮。
 */
class ZZ_PURE_TOOLS_EXPORT ZzAuxiliaryWindowChrome final : public QObject
{
    Q_DISABLE_COPY_MOVE(ZzAuxiliaryWindowChrome)

public:
    /** @brief 先释放 WindowKit 代理，再释放仍存活的标题栏。 */
    ~ZzAuxiliaryWindowChrome() override;

    /**
     * @brief 为未显示、无 menuWidget 的顶层窗口装配 chrome。
     * @param window 非空、同 GUI 线程的顶层 QMainWindow。
     * @param theme 非空、同 GUI 线程的应用主题控制器观察值。
     * @return 独占装配对象，或参数、状态、线程及 WindowKit 错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<ZzAuxiliaryWindowChrome>>
    create(QMainWindow *window, ZzFluentUI::ZzThemeController *theme);

    /** @brief 返回标题栏观察指针；宿主销毁后为空。 */
    [[nodiscard]] ZzFluentUI::ZzFluentTitleBar *titleBar() const noexcept;

    /** @brief 返回装配对象独占的 WindowKit 代理观察指针；宿主销毁后为空。 */
    [[nodiscard]] ZzWindowKit::ZzWindowAgent *windowAgent() const noexcept;

private:
    /** @brief 创建尚未绑定宿主的装配对象。 */
    ZzAuxiliaryWindowChrome();

    std::unique_ptr<ZzAuxiliaryWindowChromePrivate> d_ptr;
};

} // namespace ZzPureTools
