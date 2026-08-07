#include <ZzFluentUI/ZzIconAssets.h>

#include <QtCore/QFile>
#include <QtCore/QtResource>

/** @brief 显式拉入静态库中的图标资源对象。 */
void zzInitializeBundledIconAssets()
{
    Q_INIT_RESOURCE(zzfluent_icon_assets);
}

namespace ZzFluentUI {

bool ZzIconAssets::ensureInitialized()
{
    static const bool initialized = [] {
        zzInitializeBundledIconAssets();
        return QFile::exists(
                   QStringLiteral(":/zzfluent/fonts/ZzAwesome.ttf"))
            && QFile::exists(
                QStringLiteral(":/zzfluent/icons/Close.svg"));
    }();
    return initialized;
}

} // namespace ZzFluentUI
