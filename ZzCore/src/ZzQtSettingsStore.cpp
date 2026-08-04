#include <ZzCore/ZzQtSettingsStore.h>

#include <utility>

#include <QtCore/QSettings>
#include <QtCore/QtGlobal>

#include <ZzCore/ZzError.h>

#include "private/ZzQtSettingsStorePrivate.h"

namespace ZzCore {
namespace {

template<typename ZzValue>
ZzResult<ZzValue> settingsFailure(
    ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzResult<ZzValue>::failure(
        ZzError(code, std::move(message), std::move(context)));
}

ZzResult<void> settingsStatusResult(const QSettings &settings)
{
    if (settings.status() == QSettings::NoError) {
        return ZzResult<void>::success();
    }
    return settingsFailure<void>(
        ZzErrorCode::Io,
        QStringLiteral("QSettings reported an I/O or format error"),
        settings.fileName());
}

} // namespace

ZzQtSettingsStore::ZzQtSettingsStore(const QString &filePath)
    : d_ptr(std::make_unique<ZzQtSettingsStorePrivate>(filePath))
{
    Q_ASSERT(d_ptr->isValid());
}

ZzQtSettingsStore::~ZzQtSettingsStore()
{
    Q_ASSERT(d_ptr->isOwnerThread());
}

ZzResult<QVariant> ZzQtSettingsStore::read(
    QStringView key,
    const QVariant &defaultValue) const
{
    if (!d_ptr->isOwnerThread()) {
        return settingsFailure<QVariant>(
            ZzErrorCode::InvalidState,
            QStringLiteral("settings store accessed from a non-owner thread"));
    }
    if (!d_ptr->isValid()) {
        return settingsFailure<QVariant>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings file path must not be empty"));
    }
    if (key.isEmpty()) {
        return settingsFailure<QVariant>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings key must not be empty"));
    }

    auto value = d_ptr->settings->value(key.toString(), defaultValue);
    if (d_ptr->settings->status() != QSettings::NoError) {
        return settingsFailure<QVariant>(
            ZzErrorCode::Io,
            QStringLiteral("failed to read settings value"),
            key.toString());
    }
    return ZzResult<QVariant>::success(std::move(value));
}

ZzResult<void> ZzQtSettingsStore::write(
    QStringView key,
    const QVariant &value)
{
    if (!d_ptr->isOwnerThread()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidState,
            QStringLiteral("settings store accessed from a non-owner thread"));
    }
    if (!d_ptr->isValid()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings file path must not be empty"));
    }
    if (key.isEmpty()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings key must not be empty"));
    }

    d_ptr->settings->setValue(key.toString(), value);
    return settingsStatusResult(*d_ptr->settings);
}

ZzResult<void> ZzQtSettingsStore::remove(QStringView key)
{
    if (!d_ptr->isOwnerThread()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidState,
            QStringLiteral("settings store accessed from a non-owner thread"));
    }
    if (!d_ptr->isValid()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings file path must not be empty"));
    }
    if (key.isEmpty()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings key must not be empty"));
    }

    d_ptr->settings->remove(key.toString());
    return settingsStatusResult(*d_ptr->settings);
}

ZzResult<void> ZzQtSettingsStore::sync()
{
    if (!d_ptr->isOwnerThread()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidState,
            QStringLiteral("settings store accessed from a non-owner thread"));
    }
    if (!d_ptr->isValid()) {
        return settingsFailure<void>(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("settings file path must not be empty"));
    }

    d_ptr->settings->sync();
    return settingsStatusResult(*d_ptr->settings);
}

} // namespace ZzCore
