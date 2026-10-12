#pragma once

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace ZzTestSupport {

/**
 * @brief 优先使用当前 Qt minor 的截图，未覆盖的场景继续使用共用基线。
 * @details 更新截图时只写入当前 minor 的子目录，避免覆盖其他 Qt 版本。
 */
inline QString resolveScreenshotBaseline(
    const QString &commonPath,
    bool update = qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1)
{
    const QFileInfo image(commonPath);
    QDir root = image.dir();
    const QString scale = root.dirName();
    root.cdUp();
    const QString version = QStringLiteral("qt-%1.%2")
        .arg(QT_VERSION_MAJOR).arg(QT_VERSION_MINOR);
    if (root.dirName() == version) {
        return commonPath;
    }
    const QString versionDirectory = root.filePath(version + QLatin1Char('/') + scale);
    const QString versionPath = QDir(versionDirectory).filePath(image.fileName());
    if (update) {
        QDir().mkpath(versionDirectory);
        return versionPath;
    }
    return QFileInfo::exists(versionPath) ? versionPath : commonPath;
}

} // namespace ZzTestSupport
