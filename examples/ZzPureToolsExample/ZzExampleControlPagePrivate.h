#pragma once

#include "ZzExampleControlKind.h"
#include <QtCore/QString>

class QLabel;
class QWidget;
class QVBoxLayout;

namespace ZzExample {
class ZzExampleControlPage;

/** @brief 装配单控件示例及局部演示交互，所有控件由页面拥有。 */
class ZzExampleControlPagePrivate final
{
public:
    /** @brief 保存页面观察指针。 */
    explicit ZzExampleControlPagePrivate(ZzExampleControlPage *page);
    /** @brief 创建统一页面结构，仅实例化指定控件的示例。 */
    void initialize(ZzExampleControlKind kind, const QString &title);
    /** @brief 添加可随宽度换行的同类控件展示区。 */
    QWidget *section(const QString &title);
    /** @brief 构造按钮、图标按钮或工具按钮的变体。 */
    void buildButtons(ZzExampleControlKind kind);
    /** @brief 使用缓存的字体图标生成适配当前主题、禁用态和 DPR 的 QIcon。 */
    void refreshToolIcons();
    /** @brief 构造单选、复选或开关状态。 */
    void buildSelection(ZzExampleControlKind kind);
    /** @brief 构造文本输入或组合框的独立示例。 */
    void buildInput(ZzExampleControlKind kind);
    /** @brief 构造数值输入、日历或滚轮选择示例。 */
    void buildValue(ZzExampleControlKind kind);
    /** @brief 构造滑块或进度示例及实时数值调节。 */
    void buildProgress(ZzExampleControlKind kind);
    /** @brief 构造消息条或信息徽标的语义状态。 */
    void buildFeedback(ZzExampleControlKind kind);

    ZzExampleControlPage *q_ptr = nullptr;
    QWidget *content = nullptr;
    QVBoxLayout *layout = nullptr;
    QLabel *status = nullptr;
};
} // namespace ZzExample
