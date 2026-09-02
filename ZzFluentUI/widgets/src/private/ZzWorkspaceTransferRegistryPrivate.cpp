#include "ZzWorkspaceTransferRegistryPrivate.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QRandomGenerator>
#include <QtCore/QThread>
#include <QtCore/QObject>
#include <cstring>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzFluentUI {

ZzWorkspaceTransferRegistryPrivate::Clock
    ZzWorkspaceTransferRegistryPrivate::clock_ = {};

namespace {
ZzCore::ZzError error(ZzCore::ZzErrorCode code, const char *message)
{
    return ZzCore::ZzError(code, QString::fromLatin1(message));
}
}

ZzWorkspaceTransferRegistryPrivate *
ZzWorkspaceTransferRegistryPrivate::instance()
{
    auto *app = QCoreApplication::instance();
    if (app == nullptr || QThread::currentThread() != app->thread()) {
        return nullptr;
    }
    const auto children = app->findChildren<ZzWorkspaceTransferRegistryPrivate *>(
        QStringLiteral("zzWorkspaceTransferRegistry"));
    if (!children.isEmpty()) {
        return children.constFirst();
    }
    auto *registry = new ZzWorkspaceTransferRegistryPrivate(app);
    registry->setObjectName(QStringLiteral("zzWorkspaceTransferRegistry"));
    return registry;
}

ZzWorkspaceTransferRegistryPrivate::ZzWorkspaceTransferRegistryPrivate(QObject *parent)
    : QObject(parent)
{
}

std::chrono::steady_clock::time_point
ZzWorkspaceTransferRegistryPrivate::now() const
{
    return clock_ ? clock_() : std::chrono::steady_clock::now();
}

ZzCore::ZzResult<QByteArray> ZzWorkspaceTransferRegistryPrivate::publish(
    ZzSplitWorkspace *source,
    ZzTabWidget *sourceTabs,
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    const ZzWorkspacePageId &pageId,
    QWidget *page)
{
    auto *app = QCoreApplication::instance();
    if (app == nullptr || QThread::currentThread() != app->thread()) {
        return ZzCore::ZzResult<QByteArray>::failure(
            error(ZzCore::ZzErrorCode::InvalidState, "GUI 线程不可用"));
    }
    if (sourceTabs == nullptr || page == nullptr || sourceIndex < 0
        || sourceTabs->widget(sourceIndex) != page) {
        return ZzCore::ZzResult<QByteArray>::failure(
            error(ZzCore::ZzErrorCode::InvalidArgument, "来源页面无效"));
    }

    QByteArray randomToken(16, Qt::Uninitialized);
    for (int offset = 0; offset < randomToken.size(); offset += 4) {
        const quint32 word = QRandomGenerator::global()->generate();
        std::memcpy(randomToken.data() + offset, &word, sizeof(word));
    }
    QByteArray payload;
    payload.reserve(18);
    payload.append(char(0));
    payload.append(char(2));
    payload.append(randomToken);
    records_.insert(payload, ZzWorkspaceTransferRecordPrivate{
        QPointer<ZzSplitWorkspace>(source), QPointer<ZzTabWidget>(sourceTabs),
        sourceGroup, sourceIndex, pageId, QPointer<QWidget>(page),
        now() + std::chrono::seconds(5)});
    QObject::connect(page, &QObject::destroyed, this, [this, payload] {
        invalidate(payload);
    });
    if (source != nullptr) {
        QObject::connect(source, &QObject::destroyed, this,
                         [this, payload] { invalidate(payload); });
    }
    return ZzCore::ZzResult<QByteArray>::success(std::move(payload));
}

ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>
ZzWorkspaceTransferRegistryPrivate::lookup(
    const QByteArray &token, ZzSplitWorkspace *target, bool remove)
{
    for (auto it = records_.begin(); it != records_.end();) {
        if (it->deadline <= now()) {
            it = records_.erase(it);
        } else {
            ++it;
        }
    }
    if (token.size() != 18 || token.at(0) != 0 || token.at(1) != 2) {
        return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(
            error(ZzCore::ZzErrorCode::InvalidArgument, "令牌格式无效"));
    }
    auto it = records_.find(token);
    if (it == records_.end() || it->page.isNull() || it->sourceTabs.isNull()
        || (target != nullptr && QThread::currentThread() != target->thread())) {
        return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(
            error(ZzCore::ZzErrorCode::InvalidState, "令牌不可用"));
    }
    if (it->sourceIndex < 0 || it->sourceTabs->widget(it->sourceIndex) != it->page
        || (it->sourceWorkspace != nullptr
            && it->sourceWorkspace->pageId(it->page) != it->pageId)) {
        records_.erase(it);
        return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(
            error(ZzCore::ZzErrorCode::InvalidState, "来源页面已变化"));
    }
    auto record = it.value();
    if (remove) {
        records_.erase(it);
    }
    return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::success(
        std::move(record));
}

ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>
ZzWorkspaceTransferRegistryPrivate::inspect(const QByteArray &token,
                                            ZzSplitWorkspace *target) noexcept
{
    return lookup(token, target, false);
}

ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>
ZzWorkspaceTransferRegistryPrivate::consume(const QByteArray &token,
                                            ZzSplitWorkspace *target) noexcept
{
    return lookup(token, target, true);
}

void ZzWorkspaceTransferRegistryPrivate::invalidateWorkspace(
    ZzSplitWorkspace *workspace) noexcept
{
    if (workspace == nullptr) {
        return;
    }
    for (auto it = records_.begin(); it != records_.end();) {
        if (it->sourceWorkspace == workspace) {
            it = records_.erase(it);
        } else {
            ++it;
        }
    }
}

void ZzWorkspaceTransferRegistryPrivate::invalidate(const QByteArray &token) noexcept
{
    records_.remove(token);
}

void ZzWorkspaceTransferRegistryPrivate::setClockForTesting(Clock clock)
{
    clock_ = std::move(clock);
}

void ZzWorkspaceTransferRegistryPrivate::resetClockForTesting()
{
    clock_ = {};
}

qsizetype ZzWorkspaceTransferRegistryPrivate::size() noexcept
{
    for (auto it = records_.begin(); it != records_.end();) {
        if (it->deadline <= now()) it = records_.erase(it); else ++it;
    }
    return records_.size();
}

} // namespace ZzFluentUI
