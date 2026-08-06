#pragma once

#include <QtCore/QString>

#include "ZzExampleDataPageKind.h"

class QAbstractItemModel;
class QLabel;
class QVBoxLayout;
class QWidget;

namespace ZzExample {

class ZzExampleDataPage;

/** @brief 实现三类数据页的自适应标准 Item View 控件树。 */
class ZzExampleDataPagePrivate final
{
public:
    /** @brief 保存非空 View 观察指针。 */
    explicit ZzExampleDataPagePrivate(ZzExampleDataPage *page);

    /** @brief 创建命令区并按种类绑定注入模型。 */
    void initialize(
        ZzExampleDataPageKind kind,
        const QString &title,
        QAbstractItemModel *model);

    /** @brief 更新页面底部的 Presenter 状态文本。 */
    void setStatusText(const QString &text);

    /** @brief 创建列表视图。 */
    void buildList(
        QVBoxLayout *layout,
        QAbstractItemModel *model,
        QWidget *parent);

    /** @brief 创建可排序表格视图。 */
    void buildTable(
        QVBoxLayout *layout,
        QAbstractItemModel *model,
        QWidget *parent);

    /** @brief 创建可展开树形视图。 */
    void buildTree(
        QVBoxLayout *layout,
        QAbstractItemModel *model,
        QWidget *parent);

    ZzExampleDataPage *q_ptr = nullptr;
    QLabel *statusLabel = nullptr;
};

} // namespace ZzExample
