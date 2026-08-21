#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzDependencyWorkspaceWidgetPrivate;

class ZzDependencyWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzDependencyWorkspaceWidgetPrivate> d_ptr;
};
