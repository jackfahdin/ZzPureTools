#pragma once

#include <memory>

#include <QtCore/QSize>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtWidgets/QProxyStyle>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

class QStyleOptionComplex;

namespace ZzFluentUI {

class ZzFluentStylePrivate;
class ZzThemeController;

/** @brief 在保留平台基础行为的同时应用 Fluent 主题令牌。 */
class ZZ_FLUENT_UI_EXPORT ZzFluentStyle final : public QProxyStyle
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFluentStyle)

public:
    /**
     * @brief 构造应用级样式。
     * @param controller 非空、非拥有，必须同属 GUI 线程并比样式长寿。
     * @param baseStyle 可为空；非空时所有权交给 QProxyStyle。
     * @pre 已构造 QApplication，且调用发生在 GUI 线程。
     */
    explicit ZzFluentStyle(
        ZzThemeController *controller,
        QStyle *baseStyle = nullptr);

    /** @brief 销毁私有缓存并断开主题传播连接。 */
    ~ZzFluentStyle() override;

    /**
     * @brief 返回当前样式快照 revision。
     * @return 与控制器最近一次信号同步的版本。
     */
    [[nodiscard]] quint64 themeRevision() const noexcept;

    /**
     * @brief 返回当前图标缓存字节成本。
     * @return 不超过内部预算的物理像素字节数。
     */
    [[nodiscard]] int iconCacheBytes() const noexcept;

    /**
     * @brief 从 Qt 资源渲染、着色并缓存指定 DPR 的图标。
     * @param descriptor resourceId 必须以 :/ 开头；按描述决定 RTL 镜像。
     * @param logicalSize 非空的设备无关尺寸。
     * @param devicePixelRatio 设备像素比；无效值按 1.0 处理并量化。
     * @param color 有效的目标颜色。
     * @param direction 当前布局方向。
     * @return 命中或新生成的隐式共享 pixmap；无效输入返回空值。
     * @note 只能在样式所属 GUI 线程调用；缓存 miss 才读取 Qt resource。
     */
    [[nodiscard]] QPixmap iconPixmap(
        const ZzIconDescriptor &descriptor,
        QSize logicalSize,
        qreal devicePixelRatio,
        QColor color,
        Qt::LayoutDirection direction = Qt::LeftToRight);

    /**
     * @brief 映射 Fluent 控件、命令栏与弹出表面尺寸，其余指标委托给平台样式。
     * @param metric Qt 样式尺寸标识。
     * @param option 可选绘制状态。
     * @param widget 可选目标控件。
     * @return 逻辑像素整数值。
     */
    [[nodiscard]] int pixelMetric(
        PixelMetric metric,
        const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr) const override;

    /**
     * @brief 映射减少动效策略，其余提示委托给平台基础样式。
     * @param hint Qt 样式提示标识。
     * @param option 可选绘制状态。
     * @param widget 可选目标控件。
     * @param returnData 可选扩展返回数据。
     * @return Qt 约定的提示整数值。
     */
    [[nodiscard]] int styleHint(
        StyleHint hint,
        const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr,
        QStyleHintReturn *returnData = nullptr) const override;

    /**
     * @brief 返回由当前主题颜色覆盖的完整平台调色板。
     * @return 保留平台角色并覆盖 Fluent 核心颜色的值。
     */
    [[nodiscard]] QPalette standardPalette() const override;

    /**
     * @brief 为输入控件、工具按钮和菜单项提供可缩放的最小尺寸。
     * @param type Qt 内容类型。
     * @param option 可选绘制状态。
     * @param contentsSize 平台根据文本和字体计算的内容尺寸。
     * @param widget 可选目标控件。
     * @return 保留平台测量且满足 Fluent 最小命中尺寸的结果。
     */
    [[nodiscard]] QSize sizeFromContents(
        ContentsType type,
        const QStyleOption *option,
        const QSize &contentsSize,
        const QWidget *widget = nullptr) const override;

    /**
     * @brief 绘制 Fluent 焦点、输入框、命令栏、状态栏和弹出面板原语。
     * @param element Qt primitive 标识。
     * @param option 非拥有绘制状态。
     * @param painter 非拥有绘制目标。
     * @param widget 可选目标控件。
     */
    void drawPrimitive(
        PrimitiveElement element,
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget = nullptr) const override;

    /**
     * @brief 绘制按钮、进度、标签页、工具栏与弹出菜单项。
     * @param element Qt control 标识。
     * @param option 非拥有绘制状态。
     * @param painter 非拥有绘制目标。
     * @param widget 可选目标控件。
     */
    void drawControl(
        ControlElement element,
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget = nullptr) const override;

    /**
     * @brief 绘制滑块、组合框、数值输入框和滚动条。
     * @param control Qt complex control 标识。
     * @param option 非拥有绘制状态。
     * @param painter 非拥有绘制目标。
     * @param widget 可选目标控件。
     */
    void drawComplexControl(
        ComplexControl control,
        const QStyleOptionComplex *option,
        QPainter *painter,
        const QWidget *widget = nullptr) const override;

    /**
     * @brief 返回稳定的 Fluent 子控件区域。
     * @param control Qt complex control 标识。
     * @param option 非拥有绘制状态。
     * @param subControl 待查询的子控件。
     * @param widget 可选目标控件。
     * @return 设备无关逻辑坐标区域。
     */
    [[nodiscard]] QRect subControlRect(
        ComplexControl control,
        const QStyleOptionComplex *option,
        SubControl subControl,
        const QWidget *widget = nullptr) const override;

    /**
     * @brief 使用与绘制相同的稳定矩形执行复合控件命中测试。
     * @param control Qt complex control 标识。
     * @param option 非拥有绘制状态。
     * @param position 控件局部坐标。
     * @param widget 可选目标控件。
     * @return 命中的子控件，未命中返回 SC_None。
     */
    [[nodiscard]] SubControl hitTestComplexControl(
        ComplexControl control,
        const QStyleOptionComplex *option,
        const QPoint &position,
        const QWidget *widget = nullptr) const override;

private:
    std::unique_ptr<ZzFluentStylePrivate> d_ptr;
};

} // namespace ZzFluentUI
