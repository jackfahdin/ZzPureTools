#include "ZzExampleSystemPage.h"

#include "ZzExampleSystemPagePrivate.h"

namespace ZzExample {

ZzExampleSystemPage::ZzExampleSystemPage(
    ZzExampleSystemPageKind kind,
    const QString &title,
    QAbstractItemModel *model,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleSystemPagePrivate>(this))
{
    d_ptr->initialize(kind, title.trimmed(), model);
}

ZzExampleSystemPage::~ZzExampleSystemPage() = default;

void ZzExampleSystemPage::setSettingsSnapshot(
    int themeMode,
    int logLevel,
    bool reducedMotion,
    bool activityDockVisible)
{
    d_ptr->setSettingsSnapshot(
        themeMode, logLevel, reducedMotion, activityDockVisible);
}

void ZzExampleSystemPage::setStatusText(const QString &text)
{
    d_ptr->setStatusText(text);
}

} // namespace ZzExample
