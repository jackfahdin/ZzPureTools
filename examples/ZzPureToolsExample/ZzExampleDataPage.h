#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include "ZzExampleDataPageKind.h"

class QAbstractItemModel;

namespace ZzExample {

class ZzExampleDataPagePrivate;

/** @brief 绑定注入模型并发出筛选与更新意图的数据页纯 View。 */
class ZzExampleDataPage final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建列表、表格或树形数据展示页。
     * @param kind 页面种类。
     * @param title 页面标题。
     * @param model 非空且生命周期覆盖 View 的展示模型。
     * @param parent 必须由页面宿主提供的 QWidget 父对象。
     */
    explicit ZzExampleDataPage(
        ZzExampleDataPageKind kind,
        const QString &title,
        QAbstractItemModel *model,
        QWidget *parent);

    /** @brief 释放私有状态，控件由 Qt 父子树销毁。 */
    ~ZzExampleDataPage() override;

    /** @brief 禁止复制 QWidget View。 */
    ZzExampleDataPage(const ZzExampleDataPage &) = delete;

    /** @brief 禁止复制赋值 QWidget View。 */
    ZzExampleDataPage &operator=(const ZzExampleDataPage &) = delete;

    /** @brief 禁止移动已经建立 QObject 连接的 View。 */
    ZzExampleDataPage(ZzExampleDataPage &&) = delete;

    /** @brief 禁止移动赋值已经建立 QObject 连接的 View。 */
    ZzExampleDataPage &operator=(ZzExampleDataPage &&) = delete;

    /**
     * @brief 由 Presenter 写入当前筛选结果状态。
     * @param text 已格式化的展示文本。
     */
    void setStatusText(const QString &text);

Q_SIGNALS:
    /** @brief 用户修改筛选文本后发出展示层意图。 */
    void filterRequested(const QString &text);

    /** @brief 用户请求追加一条确定性示例记录。 */
    void appendRequested();

    /** @brief 用户请求恢复初始有界数据集。 */
    void resetRequested();

private:
    friend class ZzExampleDataPagePrivate;
    std::unique_ptr<ZzExampleDataPagePrivate> d_ptr;
};

} // namespace ZzExample
