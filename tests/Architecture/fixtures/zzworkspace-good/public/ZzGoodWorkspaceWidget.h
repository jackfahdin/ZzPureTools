#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzGoodWorkspaceWidgetPrivate;

class ZzGoodWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzGoodWorkspaceWidgetPrivate> d_ptr;
};
