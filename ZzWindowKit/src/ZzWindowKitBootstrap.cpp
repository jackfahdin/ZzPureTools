#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/Qt>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzWindowKit {

ZzCore::ZzResult<void> ZzWindowKitBootstrap::prepare()
{
    if (QCoreApplication::instance() != nullptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral(
                "window kit bootstrap must run before application creation")));
    }

    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    return ZzCore::ZzResult<void>::success();
}

} // namespace ZzWindowKit
