#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzNavigationNode.h>

namespace ZzPureTools {

class ZzNavigationModel;

/** @brief 实现导航节点校验、角色读取和动态翻译缓存。 */
class ZzNavigationModelPrivate final
{
public:
    /** @brief 创建绑定公开模型的空私有实现。 */
    explicit ZzNavigationModelPrivate(ZzNavigationModel *model);

    /** @brief 原子校验并替换导航节点。 */
    [[nodiscard]] ZzCore::ZzResult<void> setNodes(
        QList<ZzNavigationNode> nodes);

    /** @brief 按行读取导航节点副本。 */
    [[nodiscard]] ZzCore::ZzResult<ZzNavigationNode> nodeAt(
        qsizetype row) const;

    /** @brief 重建当前 translator 对应的标题缓存。 */
    void refreshTranslations();

    ZzNavigationModel *const q_ptr;
    QList<ZzNavigationNode> nodes;
    QStringList translatedTitles;
};

} // namespace ZzPureTools
