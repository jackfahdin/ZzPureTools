#pragma once

#include <memory>

#include <QtWidgets/QTabBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QKeyEvent;
class QPaintEvent;
class QResizeEvent;

namespace ZzFluentUI {

class ZzPivotPrivate;

/**
 * @brief 提供不拥有页面的轻量 Fluent 页面级导航。
 *
 * Pivot 复用 QTabBar 的 currentIndex、count、currentChanged、方向键、
 * 助记键、滚动按钮和可访问性语义。需要拥有页面、关闭、跨容器转移或撕离时，
 * 应使用 ZzTabWidget，而不是把业务页面交给本类管理。
 */
class ZZ_FLUENT_UI_EXPORT ZzPivot final : public QTabBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPivot)

public:
    /**
     * @brief 创建空的顶部页面枢轴。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzPivot(QWidget *parent = nullptr);

    /** @brief 销毁固定指示条动画和私有主题状态。 */
    ~ZzPivot() override;

    /**
     * @brief 在末尾增加文字项。
     * @param text 展示文字，可包含 Qt 助记符。
     * @return 新项索引。
     */
    int addItem(const QString &text);

    /**
     * @brief 在指定位置插入文字项。
     * @param index 请求位置，具体边界处理沿用 QTabBar。
     * @param text 展示文字，可包含 Qt 助记符。
     * @return 实际插入索引。
     */
    int insertItem(int index, const QString &text);

    /**
     * @brief 移除指定项，不拥有或删除任何页面。
     * @param index 待移除索引；非法值不产生变化。
     */
    void removeItem(int index);

    /**
     * @brief 返回指定项文字。
     * @param index 项索引。
     * @return 展示文字；非法索引返回空字符串。
     */
    [[nodiscard]] QString itemText(int index) const;

    /**
     * @brief 设置指定项文字，重复值不触发布局刷新。
     * @param index 项索引；非法值不产生变化。
     * @param text 新展示文字。
     */
    void setItemText(int index, const QString &text);

Q_SIGNALS:
    /**
     * @brief 项数量实际变化后发出。
     * @param count 新项数量。
     */
    void itemCountChanged(int count);

protected:
    /** @brief 补充 Home/End 可用项导航，并保留 QTabBar 其余键盘语义。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 绘制轻量标签、交互表面和动画指示条。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 尺寸变化后同步指示条终态。 */
    void resizeEvent(QResizeEvent *event) override;

    /** @brief 主题、字体或布局方向变化后刷新终态。 */
    void changeEvent(QEvent *event) override;

    /** @brief 标签插入后同步几何并发出数量信号。 */
    void tabInserted(int index) override;

    /** @brief 标签移除后同步几何并发出数量信号。 */
    void tabRemoved(int index) override;

private:
    friend class ZzPivotPrivate;
    std::unique_ptr<ZzPivotPrivate> d_ptr;
};

} // namespace ZzFluentUI
