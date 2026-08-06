#pragma once

#include <QtCore/QString>

#include <ZzCore/ZzApplicationPaths.h>
#include <ZzCore/ZzQtSettingsStore.h>
#include <ZzCore/ZzTaskExecutor.h>

namespace ZzExample {

/** @brief 实现共享上下文的具体后端所有权。 */
class ZzExampleApplicationContextPrivate final
{
public:
    /** @brief 使用已创建的应用目录构造全部非 UI 服务。 */
    explicit ZzExampleApplicationContextPrivate(
        const ZzCore::ZzApplicationPaths &applicationPaths);

    ZzCore::ZzApplicationPaths paths;
    ZzCore::ZzQtSettingsStore settings;
    ZzCore::ZzTaskExecutor tasks;
    QString platform;
};

} // namespace ZzExample
