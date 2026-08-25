#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QSize>

#include <ZzFluentUI/ZzCommandBar.h>

class QAction;
class QMenu;
class QToolBar;
class QToolButton;

namespace ZzFluentUI {

class ZzCommandBar;

/** @brief 保存命令栏固定控件、非拥有 action 和布局宽度缓存。 */
class ZzCommandBarPrivate final
{
public:
    struct ZzCommandBarActionRecord {
        QPointer<QAction> action;
        QMetaObject::Connection destroyedConnection;
        QMetaObject::Connection changedConnection;
    };

    /** @brief 创建固定工具栏、更多按钮和菜单。 */
    explicit ZzCommandBarPrivate(ZzCommandBar *q);

    /** @brief 在公开对象删除其拥有的 action 前断开所有观察。 */
    ~ZzCommandBarPrivate();

    /** @brief 插入一个非拥有 action 并建立唯一的 changed/destroyed 观察。 */
    bool insertAction(QList<ZzCommandBarActionRecord> *group,
                      int index,
                      QAction *action);

    /** @brief 移除 action 的观察记录和所有展示关联。 */
    bool removeAction(QAction *action);

    /** @brief 返回指定组中仍有效 action 的逻辑副本。 */
    [[nodiscard]] QList<QAction *> actions(
        const QList<ZzCommandBarActionRecord> &group) const;

    /** @brief 标记 action 宽度缓存失效并按当前宽度重建展示。 */
    void invalidateWidths();

    /** @brief 使用缓存宽度选择展示并只迁移同一 QAction。 */
    void rebuildPresentation();

    /** @brief 清理已析构 action 留下的失效记录。 */
    void removeDestroyedAction(QObject *object);

    struct ZzPresentation {
        bool compact = false;
        bool hasOverflow = false;
        int visiblePrimaryCount = 0;
        int visibleSecondaryCount = 0;
    };

    /** @brief 返回当前宽度与策略对应的展示结果。 */
    [[nodiscard]] ZzPresentation calculatePresentation(int width);

    /** @brief 按逻辑分组顺序将 action 在工具栏与菜单间移动。 */
    void moveActionsWithoutCloning(const ZzPresentation &presentation);

    /** @brief 切换主工具栏内 action 的文字与图标展示。 */
    void applyToolButtonStyle(bool compact);

    /** @brief 同步更多按钮可见性、菜单与无障碍文本。 */
    void updateMoreButton(bool hasOverflow);

    /** @brief 返回 action 在指定按钮展示样式下的缓存逻辑宽度。 */
    [[nodiscard]] int actionWidth(QAction *action, bool compact);

    /** @brief 返回当前两个逻辑组是否已包含 action。 */
    [[nodiscard]] bool containsAction(const QAction *action) const;

    /** @brief 返回当前工具栏分隔线的逻辑宽度。 */
    [[nodiscard]] int separatorWidth() const;

    ZzCommandBar *const q_ptr;
    QToolBar *toolBar = nullptr;
    QToolButton *moreButton = nullptr;
    QMenu *moreMenu = nullptr;
    QAction *separatorAction = nullptr;
    QAction *overflowSeparatorAction = nullptr;
    QList<ZzCommandBarActionRecord> primaryRecords;
    QList<ZzCommandBarActionRecord> secondaryRecords;
    QList<QSize> expandedWidths;
    QList<QSize> compactWidths;
    ZzCommandBarDisplayMode displayMode = ZzCommandBarDisplayMode::Auto;
    bool widthsDirty = true;
    bool rebuilding = false;
};

} // namespace ZzFluentUI
