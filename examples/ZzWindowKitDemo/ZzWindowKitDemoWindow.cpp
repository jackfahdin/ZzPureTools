#include "ZzWindowKitDemoWindow.h"

#include "ZzWindowKitDemoWindowPrivate.h"

ZzWindowKitDemoWindow::ZzWindowKitDemoWindow(QWidget *parent)
    : QMainWindow(parent)
    , d_ptr(std::make_unique<ZzWindowKitDemoWindowPrivate>(this))
{
    d_ptr->initialize();
}

ZzWindowKitDemoWindow::~ZzWindowKitDemoWindow() = default;
