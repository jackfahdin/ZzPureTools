#include <ZzPureTools/ZzPureApplication.h>

#include <QtCore/QThread>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

#include <ZzFluentUI/ZzFluentStyle.h>

#include "private/ZzPureApplicationPrivate.h"

namespace ZzPureTools {

ZzPureApplication::ZzPureApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , d_ptr(std::make_unique<ZzPureApplicationPrivate>(this))
{
    QApplication::setStyle(
        new ZzFluentUI::ZzFluentStyle(d_ptr->theme.get()));
}

ZzPureApplication::~ZzPureApplication()
{
    beginShutdown();
    if (auto *fallback = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fallback);
    }
}

ZzCore::ZzResult<ZzApplicationWindow *> ZzPureApplication::createWindow()
{
    return d_ptr->createWindow();
}

qsizetype ZzPureApplication::windowCount() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? static_cast<qsizetype>(d_ptr->windows.size()) : 0;
}

ZzFluentUI::ZzThemeController *ZzPureApplication::themeController()
    const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->theme.get() : nullptr;
}

void ZzPureApplication::beginShutdown() noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() == thread()) {
        d_ptr->beginShutdown();
    }
}

} // namespace ZzPureTools
