#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzGoodStringWorkspaceWidgetPrivate;

class ZzGoodStringWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzGoodStringWorkspaceWidgetPrivate> d_ptr;
};
