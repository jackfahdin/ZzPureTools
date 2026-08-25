#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class ZzTemplateDependencyWorkspaceWidgetPrivate;

class ZzTemplateDependencyWorkspaceWidget final : public QWidget
{
private:
    std::unique_ptr<ZzTemplateDependencyWorkspaceWidgetPrivate> d_ptr;
};
