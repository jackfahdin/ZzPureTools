#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtGui/QColor>
#include <QtGui/QIcon>

class QWidget;

namespace ZzFluentUI {

class ZzTabBar;
class ZzTabWidget;

/** @brief 保存一次标签转移所需的完整展示元数据。 */
struct ZzTabTransferSnapshot final
{
    QPointer<QWidget> page;
    QString text;
    QIcon icon;
    QString toolTip;
    QString whatsThis;
    QVariant data;
    QColor textColor;
    int sourceIndex = -1;
    bool enabled = true;
    bool pinned = false;
    bool modified = false;
    bool attention = false;
    bool closeEnabled = true;
};

/** @brief 持有标签容器内部引用并执行同步页面转移事务。 */
class ZzTabWidgetPrivate final
{
public:
    /** @brief 绑定公开对象，不取得 QObject 所有权。 */
    explicit ZzTabWidgetPrivate(ZzTabWidget *q) noexcept;

    /** @brief 捕获指定标签的页面和全部公开展示元数据。 */
    [[nodiscard]] ZzTabTransferSnapshot snapshot(int index) const;

    /** @brief 按页面身份在指定容器恢复标签元数据。 */
    [[nodiscard]] static bool restoreMetadata(
        ZzTabWidget *target,
        int index,
        const ZzTabTransferSnapshot &snapshot);

    /** @brief 执行同容器重排或跨容器同步转移。 */
    bool transferTo(
        ZzTabWidget *target,
        int sourceIndex,
        int targetIndex);

    struct ZzMetadata {
        bool pinned = false;
        bool modified = false;
        bool attention = false;
        bool closeEnabled = true;
        QMetaObject::Connection destroyedConnection;
    };
    [[nodiscard]] ZzMetadata metadata(QWidget *page) const;
    ZzMetadata &ensureMetadata(QWidget *page);
    void removeMetadata(QObject *object);
    void disconnectMetadataObservers() noexcept;
    void normalizePinnedOrder();

    ZzTabWidget *const q_ptr;
    ZzTabBar *tabBar = nullptr;
    QHash<QWidget *, ZzMetadata> metadataByPage;
    bool normalizing = false;
    int transferInsertionDepth = 0;
};

} // namespace ZzFluentUI
