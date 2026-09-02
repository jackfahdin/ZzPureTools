#include "ZzWorkspaceTransferRegistryPrivate.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QRandomGenerator>
#include <QtCore/QThread>
#include <QtCore/QObject>
#include <QtCore/QEvent>
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
    sourceTabs->installEventFilter(this);
    pageConnections_.insert(payload, QObject::connect(page, &QObject::destroyed, this, [this, payload] {
        invalidate(payload);
    }));
    if (source != nullptr) {
        workspaceConnections_.insert(payload, QObject::connect(source, &QObject::destroyed, this,
                         [this, payload] { invalidate(payload); }));
    }
    return ZzCore::ZzResult<QByteArray>::success(std::move(payload));
}

ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>
ZzWorkspaceTransferRegistryPrivate::lookup(
    const QByteArray &token, ZzSplitWorkspace *target, bool remove)
{
    const auto keys = records_.keys();
    for (const auto &key : keys)
        if (records_.contains(key) && records_.value(key).deadline <= now())
            invalidate(key);
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
        invalidate(it.key());
        return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(
            error(ZzCore::ZzErrorCode::InvalidState, "来源页面已变化"));
    }
    if (remove && it->reserved) {
        return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(error(ZzCore::ZzErrorCode::InvalidState, "令牌已预留"));
    }
    auto record = it.value();
    if (remove) {
        QObject::disconnect(pageConnections_.take(token));
        QObject::disconnect(workspaceConnections_.take(token));
        records_.erase(it);
    }
    return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::success(
        std::move(record));
}

ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> ZzWorkspaceTransferRegistryPrivate::reserve(const QByteArray &token, ZzSplitWorkspace *target)
{
    auto result = lookup(token, target, false);
    if (!result) return result;
    auto it = records_.find(token);
    if (it == records_.end() || it->reserved)
        return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(error(ZzCore::ZzErrorCode::InvalidState, "令牌已预留"));
    it->reserved = true;
    return ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::success(it.value());
}

bool ZzWorkspaceTransferRegistryPrivate::commit(const QByteArray &token) noexcept
{
    auto it = records_.find(token);
    if (it == records_.end() || !it->reserved) return false;
    invalidate(token);
    return true;
}

bool ZzWorkspaceTransferRegistryPrivate::release(const QByteArray &token) noexcept
{
    auto it = records_.find(token);
    if (it == records_.end() || !it->reserved) return false;
    it->reserved = false;
    return true;
}

void ZzWorkspaceTransferRegistryPrivate::sourceTabRemoved(ZzTabWidget *tabs, int index, QWidget *page) noexcept
{
    const auto keys = records_.keys();
    for (const auto &key : keys) {
        if (!records_.contains(key)) continue;
        auto &record = records_[key];
        if (record.sourceTabs != tabs) continue;
        if (record.sourceIndex == index || record.page == page) {
            if (!record.reserved) invalidate(key);
            continue;
        }
        if (record.sourceIndex > index) --record.sourceIndex;
    }
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
    const auto keys = records_.keys();
    for (const auto &key : keys)
        if (records_.contains(key) && records_.value(key).sourceWorkspace == workspace)
            invalidate(key);
}

void ZzWorkspaceTransferRegistryPrivate::invalidate(const QByteArray &token) noexcept
{
    QObject::disconnect(pageConnections_.take(token));
    QObject::disconnect(workspaceConnections_.take(token));
    records_.remove(token);
}

bool ZzWorkspaceTransferRegistryPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && event->type() == QEvent::ChildRemoved) {
        const auto keys = records_.keys();
        for (const auto &key : keys)
            if (records_.contains(key) && records_.value(key).sourceTabs == watched
                && (records_.value(key).page.isNull()
                    || records_.value(key).sourceTabs->indexOf(records_.value(key).page) < 0))
                invalidate(key);
    }
    return QObject::eventFilter(watched, event);
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
    const auto keys = records_.keys();
    for (const auto &key : keys) {
        if (!records_.contains(key)) continue;
        const auto &record = records_.value(key);
        if (record.deadline <= now() || record.sourceTabs.isNull()
            || record.sourceTabs->indexOf(record.page) < 0)
            invalidate(key);
    }
    return records_.size();
}

} // namespace ZzFluentUI
