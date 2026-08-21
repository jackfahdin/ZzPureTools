#pragma once

#include <QtWidgets/QWidget>

class ZzNoPimplWorkspaceWidget final : public QWidget
{
private:
    int state_ = 0;
};
