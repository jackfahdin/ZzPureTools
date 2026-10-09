#pragma once

#include <QtWidgets/QComboBox>

namespace ZzFluentUI {
/** @brief 保留原生下拉交互并使用内嵌 ChevronDown 字形的表示选择器。 */
class ZzColorRepresentationCombo final : public QComboBox
{
public:
    /** @brief 创建原生 RGB/HSV 下拉选择器。 */
    explicit ZzColorRepresentationCombo(QWidget *parent);
protected:
    /** @brief 复用主题下拉框面板和文字，仅用原始 TTF 绘制箭头。 */
    void paintEvent(QPaintEvent *) override;
};
} // namespace ZzFluentUI
