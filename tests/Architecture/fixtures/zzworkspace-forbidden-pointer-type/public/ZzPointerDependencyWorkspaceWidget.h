#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzPointerDependencyWorkspaceWidgetPrivate;

class ZzPointerDependencyWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzPointerDependencyWorkspaceWidgetPrivate> d_ptr;
};
