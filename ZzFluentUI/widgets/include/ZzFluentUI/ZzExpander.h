#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;

namespace ZzFluentUI {

class ZzExpanderPrivate;

/**
 * @brief 提供可键盘操作且支持减少动效的折叠内容容器。
 *
 * header 的鼠标点击、Space、Enter 和 Return 只切换本地 UI 展开状态，
 * 不加载业务数据，也不访问业务模型。调用方可监听 expandedChanged() 决定
 * 是否执行其他应用行为。
 *
 * setContentWidget() 接管内容所有权；调用方需要保留内容时，必须先通过
 * takeContentWidget() 取回。
 */
class ZZ_FLUENT_UI_EXPORT ZzExpander final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzExpander)
    Q_PROPERTY(
        QString headerText
        READ headerText
        WRITE setHeaderText
        NOTIFY headerTextChanged)
    Q_PROPERTY(
        bool expanded
        READ isExpanded
        WRITE setExpanded
        NOTIFY expandedChanged)
    Q_PROPERTY(
        QWidget *contentWidget
        READ contentWidget
        WRITE setContentWidget
        NOTIFY contentWidgetChanged)

public:
    /**
     * @brief 创建默认收起且不包含内容的容器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzExpander(QWidget *parent = nullptr);

    /** @brief 销毁容器及仍由容器拥有的内容。 */
    ~ZzExpander() override;

    /** @brief 返回 header 展示文本。 */
    [[nodiscard]] QString headerText() const;

    /**
     * @brief 设置 header 展示文本，重复值不发信号。
     * @param text 新文本。
     */
    void setHeaderText(QString text);

    /** @brief 返回当前逻辑展开状态。 */
    [[nodiscard]] bool isExpanded() const noexcept;

    /**
     * @brief 设置展开状态并从当前可见高度连续过渡。
     * @param expanded 为 true 时展开内容。
     */
    void setExpanded(bool expanded);

    /** @brief 返回当前由容器拥有的内容控件。 */
    [[nodiscard]] QWidget *contentWidget() const noexcept;

    /**
     * @brief 接管并展示新内容，替换时删除旧内容。
     * @param widget 可为空；非空对象会重挂到内部内容宿主。
     */
    void setContentWidget(QWidget *widget);

    /**
     * @brief 解除内容 parent 并把所有权交回调用方。
     * @return 原内容；为空表示当前没有内容。
     */
    [[nodiscard]] QWidget *takeContentWidget();

Q_SIGNALS:
    /** @brief header 文本实际变化后发出。 */
    void headerTextChanged(const QString &text);

    /** @brief 逻辑展开状态实际变化后发出。 */
    void expandedChanged(bool expanded);

    /** @brief 内容所有权实际变化后发出。 */
    void contentWidgetChanged(QWidget *widget);

protected:
    /** @brief 在内容布局请求期间重定向正在运行的展开动画。 */
    bool event(QEvent *event) override;

    /** @brief 在语言、主题、字体或布局方向变化后刷新 header。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzExpanderPrivate> d_ptr;
};

} // namespace ZzFluentUI
