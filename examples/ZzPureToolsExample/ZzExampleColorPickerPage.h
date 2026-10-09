#pragma once
#include <QtWidgets/QWidget>

namespace ZzExample {
/** @brief 独立的颜色选择器演示内容，供工作区页面和交互烟测复用。 */
class ZzExampleColorPickerPage final : public QWidget
{
public:
    explicit ZzExampleColorPickerPage(QWidget *parent = nullptr);
};
} // namespace ZzExample
