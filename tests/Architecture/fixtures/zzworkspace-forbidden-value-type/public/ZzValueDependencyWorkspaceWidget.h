#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzValueDependencyWorkspaceWidgetPrivate;

class ZzValueDependencyWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzValueDependencyWorkspaceWidgetPrivate> d_ptr;
};
