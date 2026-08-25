#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzGoodArithmeticWorkspaceWidgetPrivate;

class ZzGoodArithmeticWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzGoodArithmeticWorkspaceWidgetPrivate> d_ptr;
};
