#include <ZzFluentUI/ZzIconFont.h>

#include <QtCore/QThread>
#include <QtCore/QtResource>
#include <QtGui/QFontDatabase>
#include <QtGui/QGuiApplication>

/** @brief 显式拉入静态库中的图标资源对象。 */
void zzInitializeIconAssets()
{
    Q_INIT_RESOURCE(zzfluent_icon_assets);
}

namespace ZzFluentUI {

bool ZzIconFont::ensureRegistered()
{
    QGuiApplication *const application = qGuiApp;
    Q_ASSERT(application != nullptr);
    Q_ASSERT(application == nullptr
             || QThread::currentThread() == application->thread());
    if (application == nullptr
        || QThread::currentThread() != application->thread()) {
        return false;
    }

    static const bool registered = [] {
        zzInitializeIconAssets();
        const int fontId = QFontDatabase::addApplicationFont(
            QStringLiteral(":/zzfluent/fonts/ZzAwesome.ttf"));
        if (fontId < 0) {
            return false;
        }
        return QFontDatabase::applicationFontFamilies(fontId).contains(
            ZzIconFont::familyName());
    }();
    return registered;
}

QString ZzIconFont::familyName()
{
    return QStringLiteral("ZzAwesome");
}

QFont ZzIconFont::font(int pixelSize)
{
    QFont result;
    if (!ensureRegistered()) {
        return result;
    }
    result.setFamily(familyName());
    if (pixelSize > 0) {
        result.setPixelSize(pixelSize);
    }
    return result;
}

} // namespace ZzFluentUI
