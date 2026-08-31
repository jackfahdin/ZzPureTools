#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzPanelStackPrivate;

/**
 * @brief 使用单一纵向分割器承载多个可独立显隐和调整高度的面板。
 *
 * addPanel() 只接管无父对象内容；takePanel() 解除父子关系并归还内容。
 * 每个已注册内容只创建一个固定框架，显隐和重排不会重建内部对象。
 */
class ZZ_FLUENT_UI_EXPORT ZzPanelStack final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPanelStack)

public:
    /**
     * @brief 创建空的多面板堆栈。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzPanelStack(QWidget *parent = nullptr);

    /** @brief 销毁固定分割器、面板框架及仍被接管的内容。 */
    ~ZzPanelStack() override;

    /** @brief 返回已注册面板总数。 */
    [[nodiscard]] int panelCount() const noexcept;

    /** @brief 返回当前逻辑可见的面板数量。 */
    [[nodiscard]] int visiblePanelCount() const noexcept;

    /** @brief 按稳定布局顺序返回全部已注册内容。 */
    [[nodiscard]] QList<QWidget *> panels() const;

    /** @brief 按稳定布局顺序返回逻辑可见内容。 */
    [[nodiscard]] QList<QWidget *> visiblePanels() const;

    /** @brief 返回固定面板标题头是否可见。 */
    [[nodiscard]] bool areHeadersVisible() const noexcept;

    /**
     * @brief 设置所有固定面板标题头的可见性。
     *
     * 标题和关闭意图仍保留在记录中，隐藏时只改变展示层并释放头部占用空间。
     * @param visible 为 false 时隐藏标题、图标和关闭按钮。
     */
    void setHeadersVisible(bool visible);

    /**
     * @brief 接管无父对象内容并创建固定标题框架。
     * @param content 必须非空、位于控件线程且没有 QObject 父对象。
     * @param title 面板标题，可为空。
     * @param icon 可为空的统一图标描述。
     * @return 完整接管成功时返回 true；失败时不改变原所有权。
     */
    bool addPanel(
        QWidget *content,
        const QString &title,
        const ZzIconDescriptor &icon = {});

    /**
     * @brief 移除已注册面板并归还内容所有权。
     * @param content 已注册的内容对象。
     * @return 已解除父对象的内容；未注册或同步销毁时返回 nullptr。
     */
    [[nodiscard]] QWidget *takePanel(QWidget *content);

    /**
     * @brief 将面板移动到指定稳定索引。
     * @param content 已注册的内容对象。
     * @param targetIndex 范围为 0 至 panelCount() - 1 的最终索引。
     * @return 参数有效且布局保持一致时返回 true。
     */
    bool movePanel(QWidget *content, int targetIndex);

    /**
     * @brief 设置面板逻辑可见性并保留最近非零高度。
     * @param content 已注册的内容对象。
     * @param visible 是否显示固定框架。
     * @return 面板存在时返回 true；重复设置不会发信号。
     */
    bool setPanelVisible(QWidget *content, bool visible);

    /**
     * @brief 查询面板逻辑可见性。
     * @param content 待查询内容对象。
     * @return 已注册且固定框架未隐藏时返回 true。
     */
    [[nodiscard]] bool isPanelVisible(QWidget *content) const;

    /**
     * @brief 将已注册面板设为当前面板，并在需要时恢复可见。
     * @param content 已注册的内容对象。
     * @return 面板存在且操作成功时返回 true。
     */
    bool setCurrentPanel(QWidget *content);

    /** @brief 返回最近聚焦的可见面板；没有可见面板时返回 nullptr。 */
    [[nodiscard]] QWidget *currentPanel() const noexcept;

    /**
     * @brief 更新已注册面板标题。
     * @param content 已注册的内容对象。
     * @param title 新标题，可为空。
     * @return 面板存在时返回 true。
     */
    bool setPanelTitle(QWidget *content, const QString &title);

    /**
     * @brief 返回已注册面板标题。
     * @param content 待查询内容对象。
     * @return 当前标题；未注册时返回空字符串。
     */
    [[nodiscard]] QString panelTitle(QWidget *content) const;

    /**
     * @brief 更新已注册面板的统一图标描述。
     * @param content 已注册的内容对象。
     * @param icon 新图标描述，可为空。
     * @return 面板存在时返回 true。
     */
    bool setPanelIconDescriptor(
        QWidget *content,
        const ZzIconDescriptor &icon);

    /** @brief 按可见面板顺序返回最近一次有效的正高度列表。 */
    [[nodiscard]] QList<int> panelSizes() const;

    /**
     * @brief 为当前可见面板设置正高度列表。
     * @param sizes 数量必须等于 visiblePanelCount()，且每项大于零。
     * @return 参数完整有效时返回 true；失败不会改变已有尺寸。
     */
    bool setPanelSizes(const QList<int> &sizes);

Q_SIGNALS:
    /** @brief 面板逻辑可见性实际变化后发出。 */
    void panelVisibilityChanged(QWidget *content, bool visible);

    /** @brief 当前面板实际变化后发出。 */
    void currentPanelChanged(QWidget *content);

    /** @brief 面板顺序实际变化后发出。 */
    void panelMoved(QWidget *content, int index);

    /** @brief 用户点击标题关闭按钮时发出；组件不自行隐藏或删除内容。 */
    void panelCloseRequested(QWidget *content);

    /** @brief 可见面板的最近有效高度发生变化后发出。 */
    void panelSizesChanged(const QList<int> &sizes);

private:
    friend class ZzPanelStackPrivate;
    std::unique_ptr<ZzPanelStackPrivate> d_ptr;
};

} // namespace ZzFluentUI
