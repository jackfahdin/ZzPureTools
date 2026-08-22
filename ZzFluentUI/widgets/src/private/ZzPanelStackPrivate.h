#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QtTypes>

#include <ZzFluentUI/ZzIconDescriptor.h>

class QSplitter;
class QWidget;

namespace ZzFluentUI {

class ZzPanelFrame;
class ZzPanelStack;

/** @brief 保存单个固定框架、内容身份和最近一次非零高度。 */
struct ZzPanelRecord final
{
    QPointer<QWidget> content;
    QWidget *identity = nullptr;
    ZzPanelFrame *frame = nullptr;
    QString title;
    ZzIconDescriptor icon;
    int lastNonZeroSize = 160;
    QMetaObject::Connection destroyedConnection;
};

/** @brief 管理固定分割器、面板记录以及显隐和所有权事务。 */
class ZzPanelStackPrivate final
{
public:
    /** @brief 创建固定纵向分割器。 */
    explicit ZzPanelStackPrivate(ZzPanelStack *publicObject);

    /** @brief 断开外部内容销毁观察。 */
    ~ZzPanelStackPrivate();

    /** @brief 校验后接管内容并创建唯一固定框架。 */
    bool addPanel(
        QWidget *content,
        const QString &title,
        const ZzIconDescriptor &icon);

    /** @brief 移除框架并解除内容父对象。 */
    [[nodiscard]] QWidget *takePanel(QWidget *content);

    /** @brief 按稳定顺序返回全部有效内容。 */
    [[nodiscard]] QList<QWidget *> allPanels() const;

    /** @brief 按稳定顺序返回全部逻辑可见内容。 */
    [[nodiscard]] QList<QWidget *> visiblePanels() const;

    /** @brief 重排已注册内容及其固定框架。 */
    bool movePanel(QWidget *content, int targetIndex);

    /** @brief 切换已注册内容的逻辑可见性。 */
    bool setPanelVisible(QWidget *content, bool visible);

    /** @brief 查询已注册内容的逻辑可见性。 */
    [[nodiscard]] bool isPanelVisible(QWidget *content) const;

    /** @brief 切换当前内容，并在需要时恢复其可见性。 */
    bool setCurrentPanel(QWidget *content);

    /** @brief 更新固定框架标题。 */
    bool setPanelTitle(QWidget *content, const QString &title);

    /** @brief 返回固定框架标题。 */
    [[nodiscard]] QString panelTitle(QWidget *content) const;

    /** @brief 更新固定框架的图标描述。 */
    bool setPanelIconDescriptor(
        QWidget *content,
        const ZzIconDescriptor &icon);

    /** @brief 返回当前可见面板的稳定正高度。 */
    [[nodiscard]] QList<int> panelSizes() const;

    /** @brief 校验并提交当前可见面板的稳定正高度。 */
    bool setPanelSizes(const QList<int> &sizes);

    /** @brief 返回内容对应的稳定记录索引。 */
    [[nodiscard]] int indexOf(QWidget *content) const noexcept;

    /** @brief 返回原始身份对应的稳定记录索引。 */
    [[nodiscard]] int indexOfIdentity(QWidget *identity) const noexcept;

    /** @brief 记录分割器当前可见正高度。 */
    void captureVisibleSizes();

    /** @brief 将记录的正高度写回当前可见框架。 */
    void applyRememberedSizes();

    /** @brief 在外部销毁内容后安全移除框架和观察状态。 */
    void removeDestroyedPanel(QWidget *identity);

    /** @brief 选择第一个可见面板，没有时返回 nullptr。 */
    [[nodiscard]] QWidget *firstVisiblePanel() const noexcept;

    /** @brief 更新当前面板并仅在实际变化时发信号。 */
    [[nodiscard]] bool updateCurrentPanel(QWidget *content);

    /** @brief 发布一次当前面板通知，并返回宿主是否仍存活。 */
    [[nodiscard]] bool notifyCurrentPanelChanged(QWidget *content);

    /** @brief 收敛分割器拖动后的正高度并发出稳定列表。 */
    void handleSplitterMoved();

    ZzPanelStack *const q_ptr;
    QSplitter *splitter = nullptr;
    QList<ZzPanelRecord> panels;
    QPointer<QWidget> currentPanel;
    quint64 currentNotificationRevision = 0;
    bool applyingSizes = false;
};

} // namespace ZzFluentUI
