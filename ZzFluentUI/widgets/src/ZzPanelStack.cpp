#include <ZzFluentUI/ZzPanelStack.h>

#include <QtCore/QThread>

#include "private/ZzPanelStackPrivate.h"

namespace ZzFluentUI {

ZzPanelStack::ZzPanelStack(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzPanelStackPrivate>(this))
{
}

ZzPanelStack::~ZzPanelStack() = default;

int ZzPanelStack::panelCount() const noexcept
{
    return static_cast<int>(d_ptr->panels.size());
}

int ZzPanelStack::visiblePanelCount() const noexcept
{
    return static_cast<int>(d_ptr->visiblePanels().size());
}

QList<QWidget *> ZzPanelStack::panels() const
{
    return d_ptr->allPanels();
}

QList<QWidget *> ZzPanelStack::visiblePanels() const
{
    return d_ptr->visiblePanels();
}

bool ZzPanelStack::areHeadersVisible() const noexcept
{
    return d_ptr->areHeadersVisible();
}

void ZzPanelStack::setHeadersVisible(bool visible)
{
    d_ptr->setHeadersVisible(visible);
}

bool ZzPanelStack::addPanel(
    QWidget *content,
    const QString &title,
    const ZzIconDescriptor &icon)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->addPanel(content, title, icon);
}

QWidget *ZzPanelStack::takePanel(QWidget *content)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return nullptr;
    }
    return d_ptr->takePanel(content);
}

bool ZzPanelStack::movePanel(QWidget *content, int targetIndex)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->movePanel(content, targetIndex);
}

bool ZzPanelStack::setPanelVisible(QWidget *content, bool visible)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->setPanelVisible(content, visible);
}

bool ZzPanelStack::isPanelVisible(QWidget *content) const
{
    return d_ptr->isPanelVisible(content);
}

bool ZzPanelStack::setCurrentPanel(QWidget *content)
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        && d_ptr->setCurrentPanel(content);
}

QWidget *ZzPanelStack::currentPanel() const noexcept
{
    return d_ptr->currentPanel.data();
}

bool ZzPanelStack::setPanelTitle(
    QWidget *content,
    const QString &title)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->setPanelTitle(content, title);
}

QString ZzPanelStack::panelTitle(QWidget *content) const
{
    return d_ptr->panelTitle(content);
}

bool ZzPanelStack::setPanelIconDescriptor(
    QWidget *content,
    const ZzIconDescriptor &icon)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()) {
        return false;
    }
    return d_ptr->setPanelIconDescriptor(content, icon);
}

QList<int> ZzPanelStack::panelSizes() const
{
    return d_ptr->panelSizes();
}

bool ZzPanelStack::setPanelSizes(const QList<int> &sizes)
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        && d_ptr->setPanelSizes(sizes);
}

} // namespace ZzFluentUI
