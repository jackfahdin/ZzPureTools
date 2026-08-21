#include <ZzFluentUI/ZzDockPanel.h>

#include <QtCore/QThread>

#include <ZzFluentUI/ZzIconButton.h>

#include "private/ZzDockPanelPrivate.h"

namespace ZzFluentUI {

ZzDockPanel::ZzDockPanel(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
    , d_ptr(std::make_unique<ZzDockPanelPrivate>(this))
{
}

ZzDockPanel::~ZzDockPanel() = default;

void ZzDockPanel::setIconDescriptor(const ZzIconDescriptor &descriptor)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return;
    }
    d_ptr->iconDescriptor = descriptor;
    d_ptr->iconWidget->setIconDescriptor(descriptor);
}

QWidget *ZzDockPanel::takeContentWidget()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return nullptr;
    }
    QWidget *const content = widget();
    if (content == nullptr) {
        return nullptr;
    }
    content->setParent(nullptr);
    return content;
}

} // namespace ZzFluentUI
