#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QEvent>
#include <QtCore/QIODevice>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzErrorCode.h>

#include <ZzLog/ZzLog.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>
#include <ZzPureTools/ZzWorkspaceWindowCreateOptions.h>
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>

namespace {

[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzPage()
{
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(QStringLiteral("home"));
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    page.factory = [](QWidget *parent) -> ZzCore::ZzResult<std::unique_ptr<
        ZzPureTools::ZzPageInstance>> {
        return ZzPureTools::ZzPageInstance::create(
            parent,
            new QWidget(parent),
            std::make_unique<QObject>(),
            std::make_unique<QObject>());
    };
    return page;
}

[[nodiscard]] bool zzBuildApplication(
    ZzPureTools::ZzPureApplication &application)
{
    ZzPureTools::ZzApplicationBuilder builder;
    const ZzPureTools::ZzNavigationNode node{
        ZzPureTools::ZzRouteId(QStringLiteral("home")),
        QStringLiteral("ZzWorkspaceWindowCoordinatorTest"),
        QStringLiteral("Home"),
        {}};
    return builder.addPage(zzPage())
        && builder.addNavigationNode(node)
        && builder.setInitialRoute(ZzPureTools::ZzRouteId(QStringLiteral("home")))
        && builder.build(application);
}

[[nodiscard]] ZzPureTools::ZzApplicationWindow *zzOnlyWindow(
    ZzPureTools::ZzPureApplication &application)
{
    ZzPureTools::ZzApplicationWindow *result = nullptr;
    for (QWidget *widget : application.topLevelWidgets()) {
        auto *window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(widget);
        if (window == nullptr) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = window;
    }
    return result;
}

[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>>
zzCreateShell(ZzPureTools::ZzApplicationWindow *window)
{
    return ZzPureTools::ZzWorkspaceShell::create(window, nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
}

[[nodiscard]] ZzPureTools::ZzWorkspaceWindowConfiguration zzConfiguration()
{
    ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
    configuration.title = QStringLiteral("Workspace A");
    configuration.minimumSize = QSize(320, 240);
    configuration.maximumSize = QSize(1440, 900);
    configuration.initialGeometry = QRect(-20, -40, 1280, 720);
    return configuration;
}

[[nodiscard]] QByteArray zzMalformedSameDirectionWorkspaceState()
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    const auto writeString = [&stream](QStringView value) {
        stream << static_cast<quint16>(value.size());
        for (const QChar character : value) stream << character.unicode();
    };
    stream << quint8(1) << quint8(Qt::Horizontal) << quint16(2);
    stream << quint8(1) << quint8(Qt::Horizontal) << quint16(2);
    stream << quint8(0);
    writeString(QStringLiteral("group-a"));
    stream << quint8(0);
    writeString(QStringLiteral("group-b"));
    stream << quint16(2) << qint32(1) << qint32(1);
    stream << quint8(0);
    writeString(QStringLiteral("group-c"));
    stream << quint16(2) << qint32(1) << qint32(1);
    writeString(QStringLiteral("group-a"));
    stream << quint16(0);
    Q_ASSERT(stream.status() == QDataStream::Ok);

    QByteArray encoded;
    QDataStream envelope(&encoded, QIODevice::WriteOnly);
    envelope.setVersion(QDataStream::Qt_6_8);
    envelope.writeRawData("ZZSW", 4);
    envelope << quint16(1) << quint16(QDataStream::Qt_6_8)
             << static_cast<quint32>(payload.size());
    envelope.writeRawData(payload.constData(), payload.size());
    envelope.writeRawData(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).constData(),
        32);
    Q_ASSERT(envelope.status() == QDataStream::Ok);
    return encoded;
}

[[nodiscard]] bool zzReadTopologyString(QDataStream &stream)
{
    quint16 length = 0;
    stream >> length;
    if (stream.status() != QDataStream::Ok || length > 256) return false;
    for (quint16 index = 0; index < length; ++index) {
        quint16 codeUnit = 0;
        stream >> codeUnit;
    }
    return stream.status() == QDataStream::Ok;
}

[[nodiscard]] bool zzTopologyFieldOffsets(
    const QByteArray &encoded,
    QList<qsizetype> *pageIndexOffsets,
    QList<qsizetype> *originUuidOffsets,
    QList<qsizetype> *originIndexOffsets = nullptr)
{
    if (pageIndexOffsets == nullptr || originUuidOffsets == nullptr
        || encoded.size() < 44 || encoded.first(4) != QByteArrayLiteral("ZZWT")) {
        return false;
    }
    QDataStream envelope(encoded);
    envelope.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schema = 0;
    quint16 version = 0;
    quint32 payloadLength = 0;
    if (envelope.readRawData(magic, 4) != 4) return false;
    envelope >> schema >> version >> payloadLength;
    if (envelope.status() != QDataStream::Ok
        || schema != 2 || version != quint16(QDataStream::Qt_6_8)
        || payloadLength > quint32(encoded.size() - 44)) {
        return false;
    }
    QByteArray payload(static_cast<qsizetype>(payloadLength), Qt::Uninitialized);
    if (payloadLength > 0
        && envelope.readRawData(payload.data(), static_cast<int>(payloadLength))
            != static_cast<int>(payloadLength)) {
        return false;
    }
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    quint16 windowCount = 0;
    stream >> windowCount;
    if (stream.status() != QDataStream::Ok || windowCount == 0) return false;
    for (quint16 window = 0; window < windowCount; ++window) {
        if (stream.skipRawData(16) != 16
            || !zzReadTopologyString(stream)
            || !zzReadTopologyString(stream)) return false;
        quint8 byte = 0;
        for (int index = 0; index < 6; ++index) stream >> byte;
        for (int index = 0; index < 13; ++index) {
            qint32 value = 0;
            stream >> value;
        }
        quint32 workspaceLength = 0;
        stream >> workspaceLength;
        if (workspaceLength > quint32(payload.size())
            || stream.skipRawData(static_cast<int>(workspaceLength))
                != static_cast<int>(workspaceLength)) return false;
        quint16 pageCount = 0;
        stream >> pageCount;
        if (stream.status() != QDataStream::Ok) return false;
        for (quint16 page = 0; page < pageCount; ++page) {
            if (stream.skipRawData(16) != 16
                || !zzReadTopologyString(stream)
                || stream.skipRawData(16) != 16
                || !zzReadTopologyString(stream)) return false;
            const qint64 offset = stream.device()->pos();
            qint32 order = 0;
            stream >> order;
            pageIndexOffsets->append(static_cast<qsizetype>(12 + offset));
            quint16 originCount = 0;
            stream >> originCount;
            if (stream.status() != QDataStream::Ok) return false;
            for (quint16 origin = 0; origin < originCount; ++origin) {
                const qint64 originOffset = stream.device()->pos();
                if (stream.skipRawData(16) != 16
                    || !zzReadTopologyString(stream)) return false;
                if (originIndexOffsets != nullptr) {
                    originIndexOffsets->append(static_cast<qsizetype>(
                        12 + stream.device()->pos()));
                }
                qint32 originIndex = 0;
                stream >> originIndex;
                originUuidOffsets->append(
                    static_cast<qsizetype>(12 + originOffset));
            }
        }
    }
    return stream.status() == QDataStream::Ok && stream.atEnd();
}

void zzPatchTopologyInt32(
    QByteArray *encoded,
    qsizetype offset,
    qint32 value)
{
    Q_ASSERT(encoded != nullptr);
    Q_ASSERT(offset >= 0 && offset + 4 <= encoded->size());
    (*encoded)[offset] = static_cast<char>((value >> 24) & 0xff);
    (*encoded)[offset + 1] = static_cast<char>((value >> 16) & 0xff);
    (*encoded)[offset + 2] = static_cast<char>((value >> 8) & 0xff);
    (*encoded)[offset + 3] = static_cast<char>(value & 0xff);
}

void zzRefreshTopologyDigest(QByteArray *encoded)
{
    Q_ASSERT(encoded != nullptr && encoded->size() >= 44);
    QDataStream stream(*encoded);
    stream.setVersion(QDataStream::Qt_6_8);
    stream.skipRawData(8);
    quint32 payloadLength = 0;
    stream >> payloadLength;
    const QByteArray payload = encoded->mid(12, static_cast<qsizetype>(payloadLength));
    encoded->replace(12 + static_cast<qsizetype>(payloadLength), 32,
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256));
}

void zzPatchTopologyScreenName(QByteArray *encoded, QStringView name)
{
    Q_ASSERT(encoded != nullptr && encoded->size() >= 44);
    QDataStream envelope(*encoded);
    envelope.setVersion(QDataStream::Qt_6_8);
    envelope.skipRawData(8);
    quint32 payloadLength = 0;
    envelope >> payloadLength;
    QByteArray payload = encoded->mid(12, static_cast<qsizetype>(payloadLength));
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    quint16 windowCount = 0;
    stream >> windowCount;
    Q_ASSERT(windowCount > 0);
    const qint64 uuidBytesSkipped = stream.skipRawData(16);
    Q_ASSERT(uuidBytesSkipped == 16);
    Q_UNUSED(uuidBytesSkipped);
    const bool screenNameRead = zzReadTopologyString(stream);
    Q_ASSERT(screenNameRead);
    Q_UNUSED(screenNameRead);
    const qint64 nameOffset = stream.device()->pos();
    quint16 oldLength = 0;
    stream >> oldLength;
    Q_ASSERT(oldLength <= 256);
    const qint64 nameBytesSkipped =
        stream.skipRawData(static_cast<qint64>(oldLength) * 2);
    Q_ASSERT(nameBytesSkipped == static_cast<qint64>(oldLength) * 2);
    Q_UNUSED(nameBytesSkipped);

    QByteArray replacement;
    QDataStream replacementStream(&replacement, QIODevice::WriteOnly);
    replacementStream.setVersion(QDataStream::Qt_6_8);
    replacementStream << static_cast<quint16>(name.size());
    for (const QChar character : name) replacementStream << character.unicode();
    Q_ASSERT(replacementStream.status() == QDataStream::Ok);
    payload.replace(static_cast<qsizetype>(nameOffset), 2 + oldLength * 2,
        replacement);

    QByteArray result = encoded->left(12);
    const quint32 newLength = static_cast<quint32>(payload.size());
    result[8] = static_cast<char>((newLength >> 24) & 0xff);
    result[9] = static_cast<char>((newLength >> 16) & 0xff);
    result[10] = static_cast<char>((newLength >> 8) & 0xff);
    result[11] = static_cast<char>(newLength & 0xff);
    result.append(payload);
    result.append(QCryptographicHash::hash(payload, QCryptographicHash::Sha256));
    *encoded = std::move(result);
}

void zzPatchTopologyMaximized(QByteArray *encoded, bool maximized)
{
    Q_ASSERT(encoded != nullptr && encoded->size() >= 44);
    QDataStream envelope(*encoded);
    envelope.setVersion(QDataStream::Qt_6_8);
    envelope.skipRawData(8);
    quint32 payloadLength = 0;
    envelope >> payloadLength;
    QByteArray payload = encoded->mid(12, static_cast<qsizetype>(payloadLength));
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    quint16 windowCount = 0;
    stream >> windowCount;
    Q_ASSERT(windowCount > 0);
    const qint64 uuidBytesSkipped = stream.skipRawData(16);
    Q_ASSERT(uuidBytesSkipped == 16);
    Q_UNUSED(uuidBytesSkipped);
    const bool firstScreenNameRead = zzReadTopologyString(stream);
    Q_ASSERT(firstScreenNameRead);
    Q_UNUSED(firstScreenNameRead);
    const bool secondScreenNameRead = zzReadTopologyString(stream);
    Q_ASSERT(secondScreenNameRead);
    Q_UNUSED(secondScreenNameRead);
    quint8 byte = 0;
    for (int index = 0; index < 5; ++index) stream >> byte;
    const qint64 maximizedOffset = stream.device()->pos();
    stream >> byte;
    Q_ASSERT(stream.status() == QDataStream::Ok);
    payload[static_cast<qsizetype>(maximizedOffset)] =
        static_cast<char>(maximized ? 1 : 0);
    QByteArray result = encoded->left(12);
    result.append(payload);
    result.append(QCryptographicHash::hash(payload, QCryptographicHash::Sha256));
    *encoded = std::move(result);
}

class ZzRejectCloseFilter final : public QObject
{
public:
    bool reject = true;

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (reject && event != nullptr && event->type() == QEvent::Close) {
            event->ignore();
            return true;
        }
        return false;
    }
};

class ZzParentChangeActionPage final : public QWidget
{
public:
    std::function<void()> action;
    bool armed = false;

protected:
    bool event(QEvent *event) override
    {
        const bool result = QWidget::event(event);
        if (armed && event != nullptr && event->type() == QEvent::ParentChange) {
            armed = false;
            if (action) action();
        }
        return result;
    }
};

class ZzForeignResolverPage final : public QWidget
{
public:
    ZzForeignResolverPage(
        std::atomic<bool> *destroyed,
        std::atomic<QThread *> *destroyedThread)
        : destroyed_(destroyed)
        , destroyedThread_(destroyedThread)
    {
    }

    ~ZzForeignResolverPage() override
    {
        if (destroyed_ != nullptr) destroyed_->store(true);
        if (destroyedThread_ != nullptr) {
            destroyedThread_->store(QThread::currentThread());
        }
    }

private:
    std::atomic<bool> *const destroyed_;
    std::atomic<QThread *> *const destroyedThread_;
};

} // namespace

/** @brief 验证工作区窗口登记、配置快照和生命周期边界。 */
class ZzWorkspaceWindowCoordinatorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsWorkspaceActivityWithWindowIdentityAndDisconnectsOnUnregister()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *workspace = shell->splitWorkspace();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(workspace != nullptr);
        QVERIFY(coordinator != nullptr);
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration()));
        const auto group = workspace->groupIds().constFirst();
        QCOMPARE(workspace->activeGroupId(), group);
        auto *page = new QWidget;
        auto *otherPage = new QWidget;
        workspace->tabWidget(group)->addTab(page, QStringLiteral("activity"));
        workspace->tabWidget(group)->addTab(otherPage, QStringLiteral("other"));
        QCOMPARE(workspace->tabWidget(group)->count(), 2);
        QCOMPARE(workspace->tabWidget(group)->currentIndex(), 0);
        const auto pageId = workspace->pageId(page);
        QSignalSpy activeSpy(
            coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::activePageChanged);
        QSignalSpy workspaceActiveSpy(
            workspace, &ZzFluentUI::ZzSplitWorkspace::activePageChanged);
        QSignalSpy activitySpy(
            coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::pageActivityChanged);

        workspace->tabWidget(group)->setCurrentIndex(1);
        workspace->tabWidget(group)->setCurrentIndex(0);
        QCOMPARE(workspaceActiveSpy.size(), 2);
        QCOMPARE(activeSpy.size(), 2);
        QCOMPARE(activeSpy.constLast().at(0).value<ZzPureTools::ZzApplicationWindow *>(), window);
        QCOMPARE(activeSpy.constLast().at(1).value<QWidget *>(), page);
        QCOMPARE(activeSpy.constLast().at(2).value<ZzFluentUI::ZzWorkspacePageId>(), pageId);
        workspace->tabWidget(group)->setTabModified(0, true);
        QCOMPARE(activitySpy.size(), 1);
        QCOMPARE(activitySpy.constLast().at(0)
                     .value<ZzPureTools::ZzApplicationWindow *>(), window);
        QCOMPARE(activitySpy.constLast().at(1).value<QWidget *>(), page);
        QCOMPARE(activitySpy.constLast().at(2).value<ZzFluentUI::ZzWorkspacePageId>(), pageId);
        QCOMPARE(activitySpy.constLast().at(3).toBool(), true);
        QCOMPARE(activitySpy.constLast().at(4).toBool(), false);

        QVERIFY(coordinator->unregisterWindow(window));
        workspace->tabWidget(group)->setTabAttention(0, true);
        workspace->tabWidget(group)->setCurrentIndex(-1);
        QCOMPARE(activeSpy.size(), 2);
        QCOMPARE(activitySpy.size(), 1);
        application.beginShutdown();
    }

    void aboutToCloseRejectsReentrantCloseAndUnregister()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        auto shellResult = zzCreateShell(window);
        QVERIFY(window != nullptr);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        int signalCount = 0;
        bool nestedCloseRejected = false;
        bool nestedUnregisterRejected = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *closing,
                const QList<QWidget *> &) {
                ++signalCount;
                if (signalCount != 1) return;
                nestedCloseRejected = !coordinator->closeWindow(closing);
                nestedUnregisterRejected =
                    !coordinator->unregisterWindow(closing);
            });

        const auto closed = coordinator->closeWindow(window);

        QVERIFY(closed);
        QVERIFY(nestedCloseRejected);
        QVERIFY(nestedUnregisterRejected);
        QCOMPARE(signalCount, 1);
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 0; }));
        application.beginShutdown();
    }

    void committedTransferMayDestroySourceWorkspaceSafely()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShellResult = zzCreateShell(targetWindow);
        auto sourceShellResult = zzCreateShell(sourceResult.value());
        QVERIFY(targetShellResult);
        QVERIFY(sourceShellResult);
        auto targetShell = std::move(targetShellResult).value();
        auto sourceShell = std::move(sourceShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *targetWorkspace = targetShell->splitWorkspace();
        auto *sourceWorkspace = sourceShell->splitWorkspace();
        const auto targetGroup = targetWorkspace->groupIds().constFirst();
        const auto sourceGroup = sourceWorkspace->groupIds().constFirst();
        auto *const first = new QWidget;
        auto *const second = new QWidget;
        targetWorkspace->tabWidget(targetGroup)
            ->addTab(first, QStringLiteral("First"));
        targetWorkspace->tabWidget(targetGroup)
            ->addTab(second, QStringLiteral("Second"));
        QVERIFY(targetWorkspace->transferTabToWorkspace(
            targetGroup, 0, sourceWorkspace, sourceGroup));
        QVERIFY(targetWorkspace->transferTabToWorkspace(
            targetGroup, 0, sourceWorkspace, sourceGroup));
        QPointer<ZzFluentUI::ZzSplitWorkspace> guardedSource = sourceWorkspace;
        bool destroyed = false;
        QObject::connect(
            sourceWorkspace->tabWidget(sourceGroup)->fluentTabBar(),
            &QTabBar::currentChanged,
            sourceWorkspace->tabWidget(sourceGroup)->fluentTabBar(),
            [&](int) {
                if (destroyed) return;
                destroyed = true;
                delete sourceWorkspace;
            });

        const auto closed = coordinator->closeWindow(sourceResult.value());

        QVERIFY(closed);
        QVERIFY(!destroyed);
        QVERIFY(!guardedSource.isNull());
        QCOMPARE(targetWorkspace->tabWidget(targetGroup)->indexOf(first), 0);
        QCOMPARE(targetWorkspace->tabWidget(targetGroup)->indexOf(second), 1);
        application.beginShutdown();
    }

    void reclaimDoesNotExposeSourceTabBarSignals()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *page = new QWidget;
        source->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Page"));
        bool exposed = false;
        QPointer<ZzFluentUI::ZzSplitWorkspace> guardedSource(source);
        QObject::connect(source->tabWidget(sourceGroup)->fluentTabBar(),
            &QTabBar::currentChanged,
            source->tabWidget(sourceGroup)->fluentTabBar(),
            [&](int) {
                exposed = true;
                delete source;
            });
        QVERIFY(coordinator->closeWindow(sourceResult.value()));
        QVERIFY(!exposed);
        QVERIFY(!guardedSource.isNull());
        QCOMPARE(target->tabWidget(targetGroup)->indexOf(page), 0);
        application.beginShutdown();
    }

    void reclaimDoesNotExposeTargetTabBarSignals()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *page = new QWidget;
        source->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Page"));
        bool exposed = false;
        QPointer<ZzFluentUI::ZzSplitWorkspace> guardedTarget(target);
        QObject::connect(target->tabWidget(targetGroup)->fluentTabBar(),
            &QTabBar::currentChanged,
            target->tabWidget(targetGroup)->fluentTabBar(),
            [&](int) {
                exposed = true;
                delete target;
            });
        QVERIFY(coordinator->closeWindow(sourceResult.value()));
        QVERIFY(!exposed);
        QVERIFY(!guardedTarget.isNull());
        QCOMPARE(target->tabWidget(targetGroup)->indexOf(page), 0);
        application.beginShutdown();
    }

    void reclaimCommitSignalCannotReflowPage()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *page = new QWidget;
        source->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Page"));
        bool exposed = false;
        QObject::connect(target,
            &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted,
            target,
            [&](ZzFluentUI::ZzSplitWorkspace *,
                const ZzFluentUI::ZzTabGroupId &, int,
                const ZzFluentUI::ZzTabGroupId &, QWidget *moved,
                const ZzFluentUI::ZzWorkspacePageId &,
                ZzFluentUI::ZzWorkspaceDropZone) {
                if (moved == page) {
                    exposed = true;
                    source->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Page"));
                }
            });
        QVERIFY(coordinator->closeWindow(sourceResult.value()));
        QVERIFY(!exposed);
        QCOMPARE(target->tabWidget(targetGroup)->indexOf(page), 0);
        application.beginShutdown();
    }

    void windowAboutToClosePrecedesAcceptedClose()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shell = std::move(zzCreateShell(window)).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration(), true));
        QSignalSpy accepted(window, SIGNAL(closeAccepted()));
        bool visibleAtNotification = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *closing, const QList<QWidget *> &) {
                visibleAtNotification = closing != nullptr && closing->isVisible();
            });
        QVERIFY(coordinator->closeWindow(window));
        QVERIFY(visibleAtNotification);
        QCOMPARE(accepted.count(), 1);
        application.beginShutdown();
    }

    void aboutToCloseRejectsDestroyedTargetAndPreservesPage()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *page = new QWidget;
        source->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Page"));
        QPointer<QWidget> guardedPage = page;
        QPointer<ZzFluentUI::ZzSplitWorkspace> guardedTarget = target;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                if (!guardedTarget.isNull()) delete guardedTarget.data();
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(!result);
        QVERIFY(guardedTarget.isNull());
        QVERIFY(!guardedPage.isNull());
        QCOMPARE(source->tabWidget(sourceGroup)->indexOf(page), 0);
        QCOMPARE(application.windowCount(), 2);
        application.beginShutdown();
    }

    void aboutToCloseBlocksPublicTransferOutOfSource()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *page = new QWidget;
        source->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Page"));
        QVERIFY(source->setPageLayoutKey(page, QStringLiteral("original")));
        bool transferRejected = false;
        bool layoutChangeRejected = false;
        bool sourceSnapshotIntact = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &pages) {
                transferRejected = !source->transferTabToWorkspace(
                    sourceGroup, 0, target, targetGroup);
                layoutChangeRejected = !source->setPageLayoutKey(
                    page, QStringLiteral("changed"));
                sourceSnapshotIntact = pages == QList<QWidget *> {page}
                    && source->tabWidget(sourceGroup)->indexOf(page) == 0
                    && source->pageForId(source->pageId(page)) == page
                    && source->pageLayoutKey(page) == QStringLiteral("original");
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(result);
        QVERIFY(transferRejected);
        QVERIFY(layoutChangeRejected);
        QVERIFY(sourceSnapshotIntact);
        QCOMPARE(target->tabWidget(targetGroup)->indexOf(page), 0);
        application.beginShutdown();
    }

    void aboutToCloseBlocksPublicTabInsertionAndRemoval()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *targetTabs = target->tabWidget(targetGroup);
        auto *sourceTabs = source->tabWidget(sourceGroup);
        auto *existing = new QWidget;
        auto *moving = new QWidget;
        auto *injected = new QWidget;
        targetTabs->addTab(existing, QStringLiteral("Existing"));
        sourceTabs->addTab(moving, QStringLiteral("Moving"));
        bool insertionBlocked = false;
        bool removalBlocked = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                const int sourceCount = sourceTabs->count();
                sourceTabs->removeTab(sourceTabs->indexOf(moving));
                removalBlocked = sourceTabs->count() == sourceCount
                    && sourceTabs->indexOf(moving) == 0;

                const int targetCount = targetTabs->count();
                const int inserted = targetTabs->addTab(
                    injected, QStringLiteral("Injected"));
                insertionBlocked = inserted == -1
                    && targetTabs->count() == targetCount
                    && targetTabs->indexOf(injected) == -1;
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(result);
        QVERIFY(insertionBlocked);
        QVERIFY(removalBlocked);
        QCOMPARE(targetTabs->count(), 2);
        QCOMPARE(targetTabs->widget(0), existing);
        QCOMPARE(targetTabs->widget(1), moving);
        QCOMPARE(targetTabs->indexOf(injected), -1);
        delete injected;
        application.beginShutdown();
    }

    void aboutToCloseBlocksPublicTabVisibilityChanges()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *targetTabs = target->tabWidget(targetGroup);
        auto *sourceTabs = source->tabWidget(sourceGroup);
        auto *targetPage = new QWidget;
        auto *sourcePage = new QWidget;
        targetTabs->addTab(targetPage, QStringLiteral("Target"));
        sourceTabs->addTab(sourcePage, QStringLiteral("Source"));
        sourceTabs->setTabVisible(0, true);
        bool visibilityBlocked = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                sourceTabs->setTabVisible(0, false);
                visibilityBlocked = sourceTabs->isTabVisible(0);
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(result);
        QVERIFY(visibilityBlocked);
        QCOMPARE(targetTabs->indexOf(sourcePage), 1);
        application.beginShutdown();
    }

    void aboutToCloseBlocksDirectTabBarInsertion()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shell = std::move(zzCreateShell(window)).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        auto *workspace = shell->splitWorkspace();
        auto *bar = workspace->tabWidget(workspace->groupIds().constFirst())
                        ->fluentTabBar();
        bool insertionBlocked = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                const int added = bar->addTab(QStringLiteral("bar-only-add"));
                const int inserted = bar->insertTab(
                    0, QStringLiteral("bar-only-insert"));
                insertionBlocked = added == -1 && inserted == -1
                    && bar->count() == 0;
                if (inserted >= 0) {
                    static_cast<QTabBar *>(bar)->removeTab(inserted);
                }
                if (added >= 0) {
                    static_cast<QTabBar *>(bar)->removeTab(added);
                }
            });

        const auto result = coordinator->closeWindow(window);

        QVERIFY(result);
        QVERIFY(insertionBlocked);
        application.beginShutdown();
    }

    void aboutToCloseRestoresBaseClassTabVisibilitySnapshot()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *targetTabs = target->tabWidget(targetGroup);
        auto *sourceTabs = source->tabWidget(sourceGroup);
        auto *page = new QWidget;
        targetTabs->addTab(page, QStringLiteral("Page"));
        targetTabs->setTabToolTip(0, QStringLiteral("page-tip"));
        targetTabs->fluentTabBar()->setTabData(0, QStringLiteral("page-data"));
        QVERIFY(target->transferTabToWorkspace(
            targetGroup, 0, source, sourceGroup));
        const auto pageId = source->pageId(page);
        QVERIFY(sourceTabs->isTabVisible(0));
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                static_cast<QTabWidget *>(sourceTabs)->setTabVisible(0, false);
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(!result);
        QVERIFY(sourceTabs->isTabVisible(0));
        QCOMPARE(sourceTabs->indexOf(page), 0);
        QCOMPARE(targetTabs->indexOf(page), -1);
        QCOMPARE(source->pageForId(pageId), page);
        QCOMPARE(sourceTabs->tabToolTip(0), QStringLiteral("page-tip"));
        QCOMPARE(sourceTabs->fluentTabBar()->tabData(0).toString(),
            QStringLiteral("page-data"));
        QCOMPARE(application.windowCount(), 2);
        application.beginShutdown();
    }

    void aboutToCloseBlocksPublicTabMetadataPinnedAndCurrentChanges()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *targetTabs = target->tabWidget(targetGroup);
        auto *sourceTabs = source->tabWidget(sourceGroup);
        auto *targetFirst = new QWidget;
        auto *targetSecond = new QWidget;
        auto *sourceFirst = new QWidget;
        auto *sourceSecond = new QWidget;
        targetTabs->addTab(targetFirst, QStringLiteral("Target First"));
        targetTabs->addTab(targetSecond, QStringLiteral("Target Second"));
        sourceTabs->addTab(sourceFirst, QStringLiteral("Source First"));
        sourceTabs->addTab(sourceSecond, QStringLiteral("Source Second"));
        sourceTabs->setCurrentWidget(sourceFirst);
        targetTabs->setCurrentWidget(targetFirst);
        bool metadataBlocked = false;
        bool pinnedBlocked = false;
        bool currentBlocked = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                int index = sourceTabs->indexOf(sourceSecond);
                sourceTabs->setPageTitle(sourceSecond, QStringLiteral("Changed"));
                index = sourceTabs->indexOf(sourceSecond);
                sourceTabs->setTabIcon(index, QIcon());
                sourceTabs->setTabToolTip(index, QStringLiteral("changed-tip"));
                sourceTabs->setTabWhatsThis(index, QStringLiteral("changed-what"));
                sourceTabs->setTabEnabled(index, false);
                sourceTabs->setTabModified(index, true);
                sourceTabs->setTabAttention(index, true);
                sourceTabs->setTabCloseEnabled(index, false);
                metadataBlocked = sourceTabs->tabText(index)
                        == QStringLiteral("Source Second")
                    && sourceSecond->windowTitle().isEmpty()
                    && sourceTabs->tabToolTip(index).isEmpty()
                    && sourceTabs->tabWhatsThis(index).isEmpty()
                    && sourceTabs->isTabEnabled(index)
                    && !sourceTabs->isTabModified(index)
                    && !sourceTabs->hasTabAttention(index)
                    && sourceTabs->isTabCloseEnabled(index);

                sourceTabs->setTabPinned(index, true);
                pinnedBlocked = !sourceTabs->isTabPinned(
                    sourceTabs->indexOf(sourceSecond));
                sourceTabs->setCurrentWidget(sourceSecond);
                targetTabs->setCurrentIndex(1);
                currentBlocked = sourceTabs->currentWidget() == sourceFirst
                    && targetTabs->currentWidget() == targetFirst;
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(result);
        QVERIFY(metadataBlocked);
        QVERIFY(pinnedBlocked);
        QVERIFY(currentBlocked);
        QCOMPARE(targetTabs->currentWidget(), sourceSecond);
        QCOMPARE(targetTabs->indexOf(sourceFirst), 2);
        QCOMPARE(targetTabs->indexOf(sourceSecond), 3);
        application.beginShutdown();
    }

    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void aboutToCloseRestoresBaseClassTabMutationSnapshot()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        const auto targetOther = target->splitGroup(targetGroup,
            Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        const auto sourceOther = source->splitGroup(sourceGroup,
            Qt::Vertical, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetOther.has_value());
        QVERIFY(sourceOther.has_value());
        QVERIFY(target->setActiveGroup(targetGroup));
        QVERIFY(source->setActiveGroup(sourceGroup));
        auto *targetTabs = target->tabWidget(targetGroup);
        auto *sourceTabs = source->tabWidget(sourceGroup);
        auto *targetPage = new QWidget;
        auto *sourceFirst = new QWidget;
        auto *sourceSecond = new QWidget;
        auto *injected = new QWidget;
        targetTabs->addTab(targetPage, QStringLiteral("Target"));
        sourceTabs->addTab(sourceFirst, QStringLiteral("Source First"));
        sourceTabs->addTab(sourceSecond, QStringLiteral("Source Second"));
        QPixmap sourceIconPixmap(1, 1);
        sourceIconPixmap.fill(Qt::red);
        sourceTabs->setTabIcon(0, QIcon(sourceIconPixmap));
        sourceTabs->setTabToolTip(0, QStringLiteral("source-tip"));
        sourceTabs->setTabWhatsThis(0, QStringLiteral("source-what"));
        sourceTabs->fluentTabBar()->setTabData(0, QStringLiteral("source-data"));
        sourceTabs->fluentTabBar()->setTabTextColor(0, QColor(Qt::green));
        sourceTabs->setTabEnabled(0, false);
        sourceTabs->setTabPinned(0, true);
        sourceTabs->setTabModified(0, true);
        sourceTabs->setTabAttention(0, true);
        sourceTabs->setTabCloseEnabled(0, false);
        sourceTabs->setPageTitle(sourceFirst, QStringLiteral("Source First"));
        sourceTabs->setCurrentWidget(sourceFirst);
        targetTabs->setCurrentWidget(targetPage);
        QVERIFY(source->setPageLayoutKey(sourceFirst, QStringLiteral("source-first")));
        QVERIFY(source->setPageLayoutKey(sourceSecond, QStringLiteral("source-second")));
        QVERIFY(target->setPageLayoutKey(targetPage, QStringLiteral("target-page")));
        const auto sourceFirstId = source->pageId(sourceFirst);
        const auto sourceSecondId = source->pageId(sourceSecond);
        const auto targetPageId = target->pageId(targetPage);
        ZzFluentUI::ZzWorkspacePageId injectedId;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *, const QList<QWidget *> &) {
                auto *sourceBase = static_cast<QTabWidget *>(sourceTabs);
                auto *targetBase = static_cast<QTabWidget *>(targetTabs);
                sourceBase->removeTab(sourceBase->indexOf(sourceFirst));
                targetBase->insertTab(0, injected, QStringLiteral("Injected"));
                injectedId = target->pageId(injected);
                sourceBase->setTabText(
                    sourceBase->indexOf(sourceSecond), QStringLiteral("Changed"));
                sourceBase->setCurrentIndex(0);
                targetBase->setTabToolTip(
                    targetBase->indexOf(targetPage), QStringLiteral("changed-tip"));
                targetTabs->fluentTabBar()->setTabData(
                    targetTabs->indexOf(targetPage), QStringLiteral("changed-data"));
                targetTabs->fluentTabBar()->setTabTextColor(
                    targetTabs->indexOf(targetPage), QColor(Qt::blue));
                targetBase->setCurrentWidget(injected);
                target->setActiveGroup(*targetOther);
                source->setActiveGroup(*sourceOther);
            });

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(!result);
        if (!sourceOther.has_value() || !targetOther.has_value()) return;
        QCOMPARE(source->groupIds(),
            QList<ZzFluentUI::ZzTabGroupId>({sourceGroup, *sourceOther}));
        QCOMPARE(target->groupIds(),
            QList<ZzFluentUI::ZzTabGroupId>({targetGroup, *targetOther}));
        QCOMPARE(source->activeGroupId(), sourceGroup);
        QCOMPARE(target->activeGroupId(), targetGroup);
        QCOMPARE(source->tabWidget(sourceGroup), sourceTabs);
        QCOMPARE(target->tabWidget(targetGroup), targetTabs);
        QCOMPARE(sourceTabs->count(), 2);
        QCOMPARE(sourceTabs->widget(0), sourceFirst);
        QCOMPARE(sourceTabs->widget(1), sourceSecond);
        QCOMPARE(sourceTabs->currentWidget(), sourceFirst);
        QCOMPARE(targetTabs->count(), 1);
        QCOMPARE(targetTabs->widget(0), targetPage);
        QCOMPARE(targetTabs->currentWidget(), targetPage);
        QCOMPARE(sourceTabs->tabText(0), QStringLiteral("Source First"));
        QCOMPARE(sourceTabs->tabIcon(0).pixmap(1, 1).toImage(),
            QIcon(sourceIconPixmap).pixmap(1, 1).toImage());
        QCOMPARE(sourceTabs->tabToolTip(0), QStringLiteral("source-tip"));
        QCOMPARE(sourceTabs->tabWhatsThis(0), QStringLiteral("source-what"));
        QCOMPARE(sourceTabs->fluentTabBar()->tabData(0).toString(),
            QStringLiteral("source-data"));
        QCOMPARE(sourceTabs->fluentTabBar()->tabTextColor(0), QColor(Qt::green));
        QVERIFY(!sourceTabs->isTabEnabled(0));
        QVERIFY(sourceTabs->isTabPinned(0));
        QVERIFY(sourceTabs->isTabModified(0));
        QVERIFY(sourceTabs->hasTabAttention(0));
        QVERIFY(!sourceTabs->isTabCloseEnabled(0));
        QCOMPARE(sourceFirst->windowTitle(), QStringLiteral("Source First"));
        QCOMPARE(targetTabs->tabToolTip(0), QString());
        QCOMPARE(targetTabs->fluentTabBar()->tabData(0), QVariant());
        QCOMPARE(targetTabs->fluentTabBar()->tabTextColor(0), QColor());
        QCOMPARE(source->pageForId(sourceFirstId), sourceFirst);
        QCOMPARE(source->pageForId(sourceSecondId), sourceSecond);
        QCOMPARE(target->pageForId(targetPageId), targetPage);
        QCOMPARE(source->pageLayoutKey(sourceFirst), QStringLiteral("source-first"));
        QCOMPARE(source->pageLayoutKey(sourceSecond), QStringLiteral("source-second"));
        QCOMPARE(target->pageLayoutKey(targetPage), QStringLiteral("target-page"));
        QCOMPARE(targetTabs->indexOf(injected), -1);
        QVERIFY(injectedId.isValid());
        QCOMPARE(target->pageForId(injectedId), nullptr);
        targetTabs->addTab(injected, QStringLiteral("Reinserted"));
        const auto reinsertedId = target->pageId(injected);
        QVERIFY(reinsertedId.isValid());
        QVERIFY(reinsertedId != injectedId);
        targetTabs->removeTab(targetTabs->indexOf(injected));
        delete injected;
        application.beginShutdown();
    }

    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void rejectedSystemCloseRollsBackAndClearsState()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShell = std::move(zzCreateShell(targetWindow)).value();
        auto sourceShell = std::move(zzCreateShell(sourceResult.value())).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        auto filter = std::make_unique<ZzRejectCloseFilter>();
        sourceResult.value()->installEventFilter(filter.get());
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *page = new QWidget;
        target->tabWidget(targetGroup)->addTab(page, QStringLiteral("Page"));
        QVERIFY(target->transferTabToWorkspace(targetGroup, 0, source, sourceGroup));
        QVERIFY(!sourceResult.value()->close());
        QCOMPARE(source->tabWidget(sourceGroup)->indexOf(page), 0);
        QCOMPARE(target->tabWidget(targetGroup)->indexOf(page), -1);
        auto deny = ZzPureTools::ZzWorkspaceWindowConfigurationPatch{};
        deny.closePolicy = ZzPureTools::ZzWindowClosePolicy::Deny;
        QVERIFY(coordinator->applyConfiguration(sourceResult.value(), deny));
        filter->reject = false;
        QVERIFY(!sourceResult.value()->close());
        auto delegate = ZzPureTools::ZzWorkspaceWindowConfigurationPatch{};
        delegate.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        QVERIFY(coordinator->applyConfiguration(sourceResult.value(), delegate));
        QVERIFY(!sourceResult.value()->close());
        QVERIFY(coordinator->approveDelegatedClose(sourceResult.value()));
        QVERIFY(QTest::qWaitFor([&application] { return application.windowCount() == 1; }));
        application.beginShutdown();
    }

    void nonMonotonicOriginsRestoreExactOrder()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *originWindow = zzOnlyWindow(application);
        auto currentResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(originWindow != nullptr);
        QVERIFY(currentResult);
        auto originShellResult = zzCreateShell(originWindow);
        auto currentShellResult = zzCreateShell(currentResult.value());
        QVERIFY(originShellResult);
        QVERIFY(currentShellResult);
        auto originShell = std::move(originShellResult).value();
        auto currentShell = std::move(currentShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {originWindow, originShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {currentResult.value(), currentShell.get()}, zzConfiguration()));
        auto *origin = originShell->splitWorkspace();
        auto *current = currentShell->splitWorkspace();
        const auto originGroup = origin->groupIds().constFirst();
        const auto currentGroup = current->groupIds().constFirst();
        auto *const first = new QWidget;
        auto *const second = new QWidget;
        auto *const third = new QWidget;
        origin->tabWidget(originGroup)->addTab(first, QStringLiteral("A"));
        origin->tabWidget(originGroup)->addTab(second, QStringLiteral("B"));
        origin->tabWidget(originGroup)->addTab(third, QStringLiteral("C"));
        QVERIFY(origin->transferTabToWorkspace(
            originGroup, 1, current, currentGroup));
        QVERIFY(origin->transferTabToWorkspace(
            originGroup, 0, current, currentGroup));

        QVERIFY(coordinator->closeWindow(currentResult.value()));

        auto *const tabs = origin->tabWidget(originGroup);
        QCOMPARE(tabs->widget(0), first);
        QCOMPARE(tabs->widget(1), second);
        QCOMPARE(tabs->widget(2), third);
        application.beginShutdown();
    }

    void thirdPageFailureRestoresExactCurrentOrder()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *firstOriginWindow = zzOnlyWindow(application);
        auto currentResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto thirdOriginResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(firstOriginWindow != nullptr);
        QVERIFY(currentResult);
        QVERIFY(thirdOriginResult);
        auto firstOriginShellResult = zzCreateShell(firstOriginWindow);
        auto currentShellResult = zzCreateShell(currentResult.value());
        auto thirdOriginShellResult = zzCreateShell(thirdOriginResult.value());
        QVERIFY(firstOriginShellResult);
        QVERIFY(currentShellResult);
        QVERIFY(thirdOriginShellResult);
        auto firstOriginShell = std::move(firstOriginShellResult).value();
        auto currentShell = std::move(currentShellResult).value();
        auto thirdOriginShell = std::move(thirdOriginShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {firstOriginWindow, firstOriginShell.get()},
            zzConfiguration(),
            true));
        QVERIFY(coordinator->registerWindow(
            {currentResult.value(), currentShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {thirdOriginResult.value(), thirdOriginShell.get()},
            zzConfiguration()));
        auto *firstOrigin = firstOriginShell->splitWorkspace();
        auto *current = currentShell->splitWorkspace();
        auto *thirdOrigin = thirdOriginShell->splitWorkspace();
        firstOrigin->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        current->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        thirdOrigin->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        const auto firstOriginGroup = firstOrigin->groupIds().constFirst();
        const auto currentGroup = current->groupIds().constFirst();
        const auto thirdRoot = thirdOrigin->groupIds().constFirst();
        const auto thirdGroup = thirdOrigin->splitGroup(
            thirdRoot, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(thirdGroup.has_value());
        if (!thirdGroup.has_value()) return;
        auto *const first = new QWidget;
        auto *const second = new ZzParentChangeActionPage;
        auto *const third = new QWidget;
        firstOrigin->tabWidget(firstOriginGroup)
            ->addTab(first, QStringLiteral("First"));
        firstOrigin->tabWidget(firstOriginGroup)
            ->addTab(second, QStringLiteral("Second"));
        thirdOrigin->tabWidget(*thirdGroup)
            ->addTab(third, QStringLiteral("Third"));
        QVERIFY(firstOrigin->transferTabToWorkspace(
            firstOriginGroup, 0, current, currentGroup));
        QVERIFY(firstOrigin->transferTabToWorkspace(
            firstOriginGroup, 0, current, currentGroup));
        QVERIFY(thirdOrigin->transferTabToWorkspace(
            *thirdGroup, 0, current, currentGroup));
        second->action = [thirdOrigin, thirdGroup] {
            QVERIFY(thirdOrigin->removeEmptyGroup(*thirdGroup));
        };
        second->armed = true;

        const auto failed = coordinator->closeWindow(currentResult.value());

        QVERIFY(!failed);
        auto *const tabs = current->tabWidget(currentGroup);
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->widget(0), first);
        QCOMPARE(tabs->widget(1), second);
        QCOMPARE(tabs->widget(2), third);
        QCOMPARE(application.windowCount(), 3);
        application.beginShutdown();
    }

    void reclaimDoesNotSuppressUnrelatedTransferOrClose()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *firstWindow = zzOnlyWindow(application);
        auto closingResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto unrelatedOriginResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto unrelatedCurrentResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(firstWindow != nullptr);
        QVERIFY(closingResult);
        QVERIFY(unrelatedOriginResult);
        QVERIFY(unrelatedCurrentResult);
        auto firstShellResult = zzCreateShell(firstWindow);
        auto closingShellResult = zzCreateShell(closingResult.value());
        auto unrelatedOriginShellResult =
            zzCreateShell(unrelatedOriginResult.value());
        auto unrelatedCurrentShellResult =
            zzCreateShell(unrelatedCurrentResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(closingShellResult);
        QVERIFY(unrelatedOriginShellResult);
        QVERIFY(unrelatedCurrentShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto closingShell = std::move(closingShellResult).value();
        auto unrelatedOriginShell =
            std::move(unrelatedOriginShellResult).value();
        auto unrelatedCurrentShell =
            std::move(unrelatedCurrentShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {firstWindow, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {closingResult.value(), closingShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {unrelatedOriginResult.value(), unrelatedOriginShell.get()},
            zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {unrelatedCurrentResult.value(), unrelatedCurrentShell.get()},
            zzConfiguration()));
        auto *first = firstShell->splitWorkspace();
        auto *closing = closingShell->splitWorkspace();
        auto *unrelatedOrigin = unrelatedOriginShell->splitWorkspace();
        auto *unrelatedCurrent = unrelatedCurrentShell->splitWorkspace();
        first->setEmptyGroupPolicy(ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        closing->setEmptyGroupPolicy(ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        unrelatedOrigin->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        unrelatedCurrent->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        const auto firstGroup = first->groupIds().constFirst();
        const auto closingGroup = closing->groupIds().constFirst();
        const auto unrelatedOriginGroup =
            unrelatedOrigin->groupIds().constFirst();
        const auto unrelatedCurrentGroup =
            unrelatedCurrent->groupIds().constFirst();
        auto *const closingPage = new QWidget;
        auto *const unrelatedPage = new QWidget;
        first->tabWidget(firstGroup)
            ->addTab(closingPage, QStringLiteral("Closing"));
        unrelatedOrigin->tabWidget(unrelatedOriginGroup)
            ->addTab(unrelatedPage, QStringLiteral("Unrelated"));
        QVERIFY(first->transferTabToWorkspace(
            firstGroup, 0, closing, closingGroup));
        bool nested = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator,
            [&](ZzPureTools::ZzApplicationWindow *window,
                const QList<QWidget *> &) {
                if (nested || window != closingResult.value()) return;
                nested = true;
                QVERIFY(unrelatedOrigin->transferTabToWorkspace(
                    unrelatedOriginGroup,
                    0,
                    unrelatedCurrent,
                    unrelatedCurrentGroup));
                QVERIFY(coordinator->closeWindow(
                    unrelatedCurrentResult.value()));
            });

        QVERIFY(closingResult.value()->close());

        QVERIFY(nested);
        QCOMPARE(unrelatedOrigin->tabWidget(unrelatedOriginGroup)
                     ->indexOf(unrelatedPage),
            0);
        QVERIFY(QTest::qWaitFor([&application] {
            return application.windowCount() == 2;
        }));
        application.beginShutdown();
    }

    void rejectedInternalCloseRollsBackAndCannotBypassDeny()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        auto targetShellResult = zzCreateShell(targetWindow);
        auto sourceShellResult = zzCreateShell(sourceResult.value());
        QVERIFY(targetShellResult);
        QVERIFY(sourceShellResult);
        auto targetShell = std::move(targetShellResult).value();
        auto sourceShell = std::move(sourceShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        target->setEmptyGroupPolicy(ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        source->setEmptyGroupPolicy(ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        auto *const page = new QWidget;
        target->tabWidget(targetGroup)->addTab(page, QStringLiteral("Page"));
        QVERIFY(target->transferTabToWorkspace(
            targetGroup, 0, source, sourceGroup));
        ZzRejectCloseFilter filter;
        sourceResult.value()->installEventFilter(&filter);

        const auto rejected = coordinator->closeWindow(sourceResult.value());

        QVERIFY(!rejected);
        QCOMPARE(source->tabWidget(sourceGroup)->indexOf(page), 0);
        QCOMPARE(target->tabWidget(targetGroup)->indexOf(page), -1);
        auto deny = ZzPureTools::ZzWorkspaceWindowConfigurationPatch{};
        deny.closePolicy = ZzPureTools::ZzWindowClosePolicy::Deny;
        QVERIFY(coordinator->applyConfiguration(sourceResult.value(), deny));
        filter.reject = false;
        QVERIFY(!sourceResult.value()->close());
        QCOMPARE(source->tabWidget(sourceGroup)->indexOf(page), 0);
        QCOMPARE(application.windowCount(), 2);
        application.beginShutdown();
    }

    void closePolicyChangeInvalidatesDelegateRequest()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, configuration, true));
        QSignalSpy requests(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::
                windowCloseApprovalRequested);
        QVERIFY(!window->close());
        QCOMPARE(requests.size(), 1);
        ZzPureTools::ZzWorkspaceWindowConfigurationPatch patch;
        patch.closePolicy = ZzPureTools::ZzWindowClosePolicy::Deny;
        QVERIFY(coordinator->applyConfiguration(window, patch));
        patch.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        QVERIFY(coordinator->applyConfiguration(window, patch));

        QVERIFY(!coordinator->approveDelegatedClose(window));
        QCOMPARE(application.windowCount(), 1);
        QVERIFY(!window->close());
        QCOMPARE(requests.size(), 2);
        application.beginShutdown();
    }

    void failedReclaimAuditContainsNoCommitSuccess()
    {
        QVERIFY(!ZzLog::isInitialized());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString logPath = directory.filePath(QStringLiteral("failed.log"));
        ZzLog::ZzLogConfig logConfiguration;
        logConfiguration.console.enabled = false;
        logConfiguration.file.enabled = true;
        logConfiguration.file.async = false;
        logConfiguration.file.path =
            QFileInfo(logPath).filesystemAbsoluteFilePath();
        logConfiguration.file.pattern = "%v";
        QVERIFY(ZzLog::initialize(logConfiguration));
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *targetWindow = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto secondOriginResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(sourceResult);
        QVERIFY(secondOriginResult);
        auto targetShellResult = zzCreateShell(targetWindow);
        auto sourceShellResult = zzCreateShell(sourceResult.value());
        auto secondOriginShellResult = zzCreateShell(secondOriginResult.value());
        QVERIFY(targetShellResult);
        QVERIFY(sourceShellResult);
        QVERIFY(secondOriginShellResult);
        auto targetShell = std::move(targetShellResult).value();
        auto sourceShell = std::move(sourceShellResult).value();
        auto secondOriginShell = std::move(secondOriginShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {targetWindow, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {secondOriginResult.value(), secondOriginShell.get()},
            zzConfiguration()));
        auto *target = targetShell->splitWorkspace();
        auto *source = sourceShell->splitWorkspace();
        auto *secondOrigin = secondOriginShell->splitWorkspace();
        secondOrigin->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        const auto targetGroup = target->groupIds().constFirst();
        const auto sourceGroup = source->groupIds().constFirst();
        const auto secondOriginRoot = secondOrigin->groupIds().constFirst();
        const auto secondOriginGroup = secondOrigin->splitGroup(secondOriginRoot,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondOriginGroup.has_value());
        if (!secondOriginGroup.has_value()) return;
        auto *const first = new ZzParentChangeActionPage;
        auto *const second = new QWidget;
        target->tabWidget(targetGroup)->addTab(first, QStringLiteral("First"));
        secondOrigin->tabWidget(*secondOriginGroup)
            ->addTab(second, QStringLiteral("Second"));
        QVERIFY(target->transferTabToWorkspace(
            targetGroup, 0, source, sourceGroup));
        QVERIFY(secondOrigin->transferTabToWorkspace(
            *secondOriginGroup, 0, source, sourceGroup));
        first->action = [secondOrigin, secondOriginGroup] {
            QVERIFY(secondOrigin->removeEmptyGroup(*secondOriginGroup));
        };
        first->armed = true;

        QVERIFY(!coordinator->closeWindow(sourceResult.value()));
        QCOMPARE(source->tabWidget(sourceGroup)->count(), 2);
        QCOMPARE(source->tabWidget(sourceGroup)->widget(0), first);
        QCOMPARE(source->tabWidget(sourceGroup)->widget(1), second);
        QVERIFY(ZzLog::flushAndWait(std::chrono::seconds(2)));
        application.beginShutdown();
        ZzLog::shutdown();

        QFile logFile(logPath);
        QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString contents = QString::fromUtf8(logFile.readAll());
        bool sawRollback = false;
        for (const auto &line :
            contents.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            if (!line.startsWith(QStringLiteral("page.reclaim "))) continue;
            QVERIFY(!line.contains(QStringLiteral(
                "phase=commit")));
            sawRollback = sawRollback
                || line.contains(QStringLiteral("phase=rollback"));
        }
        QVERIFY(sawRollback);
    }

    void stackedClosingHonorsPoliciesAndRejectsReentry()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto created = ZzPureTools::ZzWorkspaceShell::create(window);
        QVERIFY(created);
        auto shell = std::move(created).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Deny;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, configuration, true));
        QVERIFY(!window->close());
        QCOMPARE(application.windowCount(), 1);
        ZzPureTools::ZzWorkspaceWindowConfigurationPatch patch;
        patch.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        QVERIFY(coordinator->applyConfiguration(window, patch));
        QSignalSpy approvals(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowCloseApprovalRequested);
        QVERIFY(!window->close());
        QVERIFY(!window->close());
        QCOMPARE(approvals.count(), 1);
        bool closeRejected = false;
        bool unregisterRejected = false;
        QObject::connect(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose,
            coordinator, [&](ZzPureTools::ZzApplicationWindow *closing) {
                closeRejected = !coordinator->closeWindow(closing);
                unregisterRejected = !coordinator->unregisterWindow(closing);
            });
        QVERIFY(coordinator->approveDelegatedClose(window));
        QVERIFY(closeRejected);
        QVERIFY(unregisterRejected);
        QVERIFY(QTest::qWaitFor([&] { return application.windowCount() == 0; }));
        QVERIFY(!coordinator->approveDelegatedClose(window));
        application.beginShutdown();
    }

    void denyPolicyIgnoresSystemCloseEvent()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Deny;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, configuration, true));

        QVERIFY(!window->close());
        QCoreApplication::processEvents();

        QCOMPARE(application.windowCount(), 1);
        QVERIFY(coordinator->configuration(window));
        application.beginShutdown();
    }

    void delegatePolicyRequestsApprovalOnlyOnce()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, configuration, true));
        QSignalSpy approvalRequests(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::
                windowCloseApprovalRequested);

        QVERIFY(!window->close());
        QVERIFY(!window->close());
        QCoreApplication::processEvents();

        QCOMPARE(approvalRequests.count(), 1);
        QCOMPARE(application.windowCount(), 1);
        application.beginShutdown();
    }

    void delegatedApprovalConsumesPendingRequestOnlyOnce()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(
            coordinator->registerWindow({window, shell.get()}, configuration));

        QVERIFY(!window->close());
        QVERIFY(coordinator->approveDelegatedClose(window));
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 0; }));
        QVERIFY(!coordinator->approveDelegatedClose(window));
        application.beginShutdown();
    }

    void delegatedApprovalCannotBypassDenyPolicy()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Deny;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(
            coordinator->registerWindow({window, shell.get()}, configuration));

        QVERIFY(!coordinator->approveDelegatedClose(window));
        QVERIFY(!window->close());
        QCOMPARE(application.windowCount(), 1);
        application.beginShutdown();
    }

    void delegatedApprovalRejectsForeignThreadWithoutConsumingToken()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto configuration = zzConfiguration();
        configuration.closePolicy = ZzPureTools::ZzWindowClosePolicy::Delegate;
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, configuration, true));
        QVERIFY(!window->close());
        bool rejected = false;
        ZzCore::ZzErrorCode errorCode = ZzCore::ZzErrorCode::None;
        std::thread worker([&] {
            const auto result = coordinator->approveDelegatedClose(window);
            rejected = !result;
            if (!result) {
                errorCode = result.error().code();
            }
        });
        worker.join();

        QVERIFY(rejected);
        QCOMPARE(errorCode, ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(coordinator->approveDelegatedClose(window));
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 0; }));
        application.beginShutdown();
    }

    void allowSystemCloseReclaimsBeforeAcceptedClose()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        auto *const page = new QWidget;
        const auto firstGroup =
            firstShell->splitWorkspace()->groupIds().constFirst();
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        firstShell->splitWorkspace()
            ->tabWidget(firstGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(firstShell->splitWorkspace()->transferTabToWorkspace(
            firstGroup, 0, secondShell->splitWorkspace(), secondGroup));

        secondResult.value()->close();

        QCOMPARE(
            firstShell->splitWorkspace()->tabWidget(firstGroup)->indexOf(page),
            0);
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 1; }));
        application.beginShutdown();
    }

    void closingChainedWindowsReclaimsInReverseOriginOrder()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto thirdResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        QVERIFY(thirdResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        auto thirdShellResult = zzCreateShell(thirdResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        QVERIFY(thirdShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto thirdShell = std::move(thirdShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {thirdResult.value(), thirdShell.get()}, zzConfiguration()));
        auto *const page = new QWidget;
        const auto firstGroup =
            firstShell->splitWorkspace()->groupIds().constFirst();
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        const auto thirdGroup =
            thirdShell->splitWorkspace()->groupIds().constFirst();
        firstShell->splitWorkspace()
            ->tabWidget(firstGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(firstShell->splitWorkspace()->transferTabToWorkspace(
            firstGroup, 0, secondShell->splitWorkspace(), secondGroup));
        QVERIFY(secondShell->splitWorkspace()->transferTabToWorkspace(
            secondGroup, 0, thirdShell->splitWorkspace(), thirdGroup));

        QVERIFY(coordinator->closeWindow(thirdResult.value()));

        QCOMPARE(secondShell->splitWorkspace()
                     ->tabWidget(secondGroup)
                     ->indexOf(page),
            0);
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 2; }));

        QVERIFY(coordinator->closeWindow(secondResult.value()));

        QCOMPARE(
            firstShell->splitWorkspace()->tabWidget(firstGroup)->indexOf(page),
            0);
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 1; }));
        application.beginShutdown();
    }

    void reclaimRestoresOriginalSourceIndex()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(first != nullptr);
        QVERIFY(secondResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        const auto firstGroup =
            firstShell->splitWorkspace()->groupIds().constFirst();
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        auto *const before = new QWidget;
        auto *const moving = new QWidget;
        auto *const after = new QWidget;
        auto *const firstTabs =
            firstShell->splitWorkspace()->tabWidget(firstGroup);
        firstTabs->addTab(before, QStringLiteral("Before"));
        firstTabs->addTab(moving, QStringLiteral("Moving"));
        firstTabs->addTab(after, QStringLiteral("After"));
        QVERIFY(firstShell->splitWorkspace()->transferTabToWorkspace(
            firstGroup, 1, secondShell->splitWorkspace(), secondGroup));

        QVERIFY(coordinator->closeWindow(secondResult.value()));

        QCOMPARE(firstTabs->widget(0), before);
        QCOMPARE(firstTabs->widget(1), moving);
        QCOMPARE(firstTabs->widget(2), after);
        application.beginShutdown();
    }

    void returningToKnownOriginTruncatesHistory()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto thirdResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(first != nullptr);
        QVERIFY(secondResult);
        QVERIFY(thirdResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        auto thirdShellResult = zzCreateShell(thirdResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        QVERIFY(thirdShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto thirdShell = std::move(thirdShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {thirdResult.value(), thirdShell.get()}, zzConfiguration()));
        const auto firstGroup =
            firstShell->splitWorkspace()->groupIds().constFirst();
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        const auto thirdGroup =
            thirdShell->splitWorkspace()->groupIds().constFirst();
        auto *const page = new QWidget;
        firstShell->splitWorkspace()
            ->tabWidget(firstGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(firstShell->splitWorkspace()->transferTabToWorkspace(
            firstGroup, 0, secondShell->splitWorkspace(), secondGroup));
        QVERIFY(secondShell->splitWorkspace()->transferTabToWorkspace(
            secondGroup, 0, thirdShell->splitWorkspace(), thirdGroup));
        QVERIFY(thirdShell->splitWorkspace()->transferTabToWorkspace(
            thirdGroup, 0, secondShell->splitWorkspace(), secondGroup));

        QVERIFY(coordinator->closeWindow(secondResult.value()));

        QCOMPARE(
            firstShell->splitWorkspace()->tabWidget(firstGroup)->indexOf(page),
            0);
        QCOMPARE(
            thirdShell->splitWorkspace()->tabWidget(thirdGroup)->indexOf(page),
            -1);
        application.beginShutdown();
    }

    void missingOriginGroupUsesTargetActiveGroup()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(first != nullptr);
        QVERIFY(secondResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        auto *firstWorkspace = firstShell->splitWorkspace();
        firstWorkspace->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        const auto activeGroup = firstWorkspace->groupIds().constFirst();
        const auto originGroup = firstWorkspace->splitGroup(
            activeGroup, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(originGroup.has_value());
        if (!originGroup.has_value()) return;
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        auto *const page = new QWidget;
        firstWorkspace->tabWidget(*originGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(firstWorkspace->transferTabToWorkspace(
            *originGroup, 0, secondShell->splitWorkspace(), secondGroup));
        QVERIFY(firstWorkspace->removeEmptyGroup(*originGroup));
        QVERIFY(firstWorkspace->setActiveGroup(activeGroup));

        QVERIFY(coordinator->closeWindow(secondResult.value()));

        QCOMPARE(firstWorkspace->tabWidget(activeGroup)->indexOf(page), 0);
        application.beginShutdown();
    }

    void destroyedOriginFallsBackToPrimaryWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *primary = zzOnlyWindow(application);
        auto originResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto currentResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(primary != nullptr);
        QVERIFY(originResult);
        QVERIFY(currentResult);
        auto primaryShellResult = zzCreateShell(primary);
        auto originShellResult = zzCreateShell(originResult.value());
        auto currentShellResult = zzCreateShell(currentResult.value());
        QVERIFY(primaryShellResult);
        QVERIFY(originShellResult);
        QVERIFY(currentShellResult);
        auto primaryShell = std::move(primaryShellResult).value();
        auto originShell = std::move(originShellResult).value();
        auto currentShell = std::move(currentShellResult).value();
        primaryShell->splitWorkspace()->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        originShell->splitWorkspace()->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        currentShell->splitWorkspace()->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {primary, primaryShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {originResult.value(), originShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {currentResult.value(), currentShell.get()}, zzConfiguration()));
        const auto primaryGroup =
            primaryShell->splitWorkspace()->groupIds().constFirst();
        const auto originGroup =
            originShell->splitWorkspace()->groupIds().constFirst();
        const auto currentGroup =
            currentShell->splitWorkspace()->groupIds().constFirst();
        auto *const page = new QWidget;
        originShell->splitWorkspace()
            ->tabWidget(originGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(originShell->splitWorkspace()->transferTabToWorkspace(
            originGroup, 0, currentShell->splitWorkspace(), currentGroup));
        originResult.value()->close();
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 2; }));

        QVERIFY(coordinator->closeWindow(currentResult.value()));

        QCOMPARE(primaryShell->splitWorkspace()
                     ->tabWidget(primaryGroup)
                     ->indexOf(page),
            0);
        application.beginShutdown();
    }

    void destroyedOriginAndPrimaryFallBackToFirstWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *primary = zzOnlyWindow(application);
        auto originResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto fallbackResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto currentResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(primary != nullptr);
        QVERIFY(originResult);
        QVERIFY(fallbackResult);
        QVERIFY(currentResult);
        auto primaryShellResult = zzCreateShell(primary);
        auto originShellResult = zzCreateShell(originResult.value());
        auto fallbackShellResult = zzCreateShell(fallbackResult.value());
        auto currentShellResult = zzCreateShell(currentResult.value());
        QVERIFY(primaryShellResult);
        QVERIFY(originShellResult);
        QVERIFY(fallbackShellResult);
        QVERIFY(currentShellResult);
        auto primaryShell = std::move(primaryShellResult).value();
        auto originShell = std::move(originShellResult).value();
        auto fallbackShell = std::move(fallbackShellResult).value();
        auto currentShell = std::move(currentShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {primary, primaryShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {originResult.value(), originShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {fallbackResult.value(), fallbackShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {currentResult.value(), currentShell.get()}, zzConfiguration()));
        const auto originGroup =
            originShell->splitWorkspace()->groupIds().constFirst();
        const auto fallbackGroup =
            fallbackShell->splitWorkspace()->groupIds().constFirst();
        const auto currentGroup =
            currentShell->splitWorkspace()->groupIds().constFirst();
        auto *const page = new QWidget;
        originShell->splitWorkspace()
            ->tabWidget(originGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(originShell->splitWorkspace()->transferTabToWorkspace(
            originGroup, 0, currentShell->splitWorkspace(), currentGroup));
        originResult.value()->close();
        primary->close();
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 2; }));

        QVERIFY(coordinator->closeWindow(currentResult.value()));

        QCOMPARE(fallbackShell->splitWorkspace()
                     ->tabWidget(fallbackGroup)
                     ->indexOf(page),
            0);
        application.beginShutdown();
    }

    void orphanedPagesPreserveStableOrderAndWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        auto *workspace = shell->splitWorkspace();
        const auto firstGroup = workspace->groupIds().constFirst();
        const auto secondGroup = workspace->splitGroup(
            firstGroup, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroup.has_value());
        if (!secondGroup.has_value()) return;
        auto *const first = new QWidget;
        auto *const second = new QWidget;
        auto *const third = new QWidget;
        workspace->tabWidget(firstGroup)
            ->addTab(first, QStringLiteral("First"));
        workspace->tabWidget(firstGroup)
            ->addTab(second, QStringLiteral("Second"));
        workspace->tabWidget(*secondGroup)
            ->addTab(third, QStringLiteral("Third"));
        QSignalSpy orphaned(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::orphanedPages);

        QVERIFY(coordinator->closeWindow(window));

        QCOMPARE(orphaned.size(), 1);
        QCOMPARE(orphaned.constFirst().constFirst().value<QList<QWidget *>>(),
            QList<QWidget *>({first, second, third}));
        QCOMPARE(application.windowCount(), 1);
        QCOMPARE(workspace->tabWidget(firstGroup)->widget(0), first);
        QCOMPARE(workspace->tabWidget(firstGroup)->widget(1), second);
        QCOMPARE(workspace->tabWidget(*secondGroup)->widget(0), third);
        application.beginShutdown();
    }

    void emptyWindowClosesWithoutOrphanSignal()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        QSignalSpy orphaned(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::orphanedPages);

        QVERIFY(coordinator->closeWindow(window));

        QCOMPARE(orphaned.size(), 0);
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 0; }));
        application.beginShutdown();
    }

    void shutdownDoesNotRunOrdinaryReclaim()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(first != nullptr);
        QVERIFY(secondResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        const auto firstGroup =
            firstShell->splitWorkspace()->groupIds().constFirst();
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        auto *const page = new QWidget;
        firstShell->splitWorkspace()
            ->tabWidget(firstGroup)
            ->addTab(page, QStringLiteral("Page"));
        QVERIFY(firstShell->splitWorkspace()->transferTabToWorkspace(
            firstGroup, 0, secondShell->splitWorkspace(), secondGroup));
        QSignalSpy reclaimed(firstShell->splitWorkspace(),
            &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted);
        QSignalSpy aboutToClose(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose);

        application.beginShutdown();

        QCOMPARE(reclaimed.size(), 0);
        QCOMPARE(aboutToClose.size(), 0);
        application.beginShutdown();
    }

    void layoutKeyConflictIsRejectedBeforeAnyPageMoves()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *target = zzOnlyWindow(application);
        auto sourceResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(target != nullptr);
        QVERIFY(sourceResult);
        auto targetShellResult = zzCreateShell(target);
        auto sourceShellResult = zzCreateShell(sourceResult.value());
        QVERIFY(targetShellResult);
        QVERIFY(sourceShellResult);
        auto targetShell = std::move(targetShellResult).value();
        auto sourceShell = std::move(sourceShellResult).value();
        sourceShell->splitWorkspace()->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {target, targetShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {sourceResult.value(), sourceShell.get()}, zzConfiguration()));
        auto *targetWorkspace = targetShell->splitWorkspace();
        auto *sourceWorkspace = sourceShell->splitWorkspace();
        const auto targetGroup = targetWorkspace->groupIds().constFirst();
        const auto sourceGroup = sourceWorkspace->groupIds().constFirst();
        auto *const first = new QWidget;
        auto *const second = new QWidget;
        targetWorkspace->tabWidget(targetGroup)
            ->addTab(first, QStringLiteral("First"));
        targetWorkspace->tabWidget(targetGroup)
            ->addTab(second, QStringLiteral("Second"));
        QVERIFY(targetWorkspace->setPageLayoutKey(
            first, QStringLiteral("first-key")));
        QVERIFY(targetWorkspace->setPageLayoutKey(
            second, QStringLiteral("conflicting-key")));
        QVERIFY(targetWorkspace->transferTabToWorkspace(
            targetGroup, 0, sourceWorkspace, sourceGroup));
        QVERIFY(targetWorkspace->transferTabToWorkspace(
            targetGroup, 0, sourceWorkspace, sourceGroup));
        auto *const conflict = new QWidget;
        targetWorkspace->tabWidget(targetGroup)
            ->addTab(conflict, QStringLiteral("Conflict"));
        QVERIFY(targetWorkspace->setPageLayoutKey(
            conflict, QStringLiteral("conflicting-key")));
        QSignalSpy committed(targetWorkspace,
            &ZzFluentUI::ZzSplitWorkspace::tabTransferCommitted);
        QSignalSpy aboutToClose(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowAboutToClose);

        const auto result = coordinator->closeWindow(sourceResult.value());

        QVERIFY(!result);
        QCOMPARE(committed.size(), 0);
        QCOMPARE(aboutToClose.size(), 0);
        QCOMPARE(sourceWorkspace->tabWidget(sourceGroup)->widget(0), first);
        QCOMPARE(sourceWorkspace->tabWidget(sourceGroup)->widget(1), second);
        QCOMPARE(
            sourceWorkspace->pageLayoutKey(first), QStringLiteral("first-key"));
        QCOMPARE(sourceWorkspace->pageLayoutKey(second),
            QStringLiteral("conflicting-key"));
        QCOMPARE(application.windowCount(), 2);
        application.beginShutdown();
    }

    void secondPageFailureRollsBackIdentityMetadataAndOrder()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *firstWindow = zzOnlyWindow(application);
        auto currentResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        auto secondOriginResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(firstWindow != nullptr);
        QVERIFY(currentResult);
        QVERIFY(secondOriginResult);
        auto firstShellResult = zzCreateShell(firstWindow);
        auto currentShellResult = zzCreateShell(currentResult.value());
        auto secondOriginShellResult =
            zzCreateShell(secondOriginResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(currentShellResult);
        QVERIFY(secondOriginShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto currentShell = std::move(currentShellResult).value();
        auto secondOriginShell = std::move(secondOriginShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {firstWindow, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {currentResult.value(), currentShell.get()}, zzConfiguration()));
        QVERIFY(coordinator->registerWindow(
            {secondOriginResult.value(), secondOriginShell.get()},
            zzConfiguration()));
        auto *firstWorkspace = firstShell->splitWorkspace();
        auto *currentWorkspace = currentShell->splitWorkspace();
        auto *secondOriginWorkspace = secondOriginShell->splitWorkspace();
        secondOriginWorkspace->setEmptyGroupPolicy(
            ZzFluentUI::ZzEmptyGroupPolicy::Keep);
        const auto firstGroup = firstWorkspace->groupIds().constFirst();
        const auto currentGroup = currentWorkspace->groupIds().constFirst();
        const auto secondOriginRoot =
            secondOriginWorkspace->groupIds().constFirst();
        const auto secondOriginGroup =
            secondOriginWorkspace->splitGroup(secondOriginRoot,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondOriginGroup.has_value());
        if (!secondOriginGroup.has_value()) return;
        auto *const first = new ZzParentChangeActionPage;
        auto *const second = new QWidget;
        auto *firstTabs = firstWorkspace->tabWidget(firstGroup);
        firstTabs->addTab(first, QStringLiteral("First"));
        firstTabs->setTabToolTip(0, QStringLiteral("first-tip"));
        firstTabs->setTabWhatsThis(0, QStringLiteral("first-what"));
        firstTabs->fluentTabBar()->setTabData(0, QStringLiteral("first-data"));
        firstTabs->setTabPinned(0, true);
        firstTabs->setTabModified(0, true);
        firstTabs->setTabAttention(0, true);
        firstTabs->setTabCloseEnabled(0, false);
        secondOriginWorkspace->tabWidget(*secondOriginGroup)
            ->addTab(second, QStringLiteral("Second"));
        QVERIFY(firstWorkspace->setPageLayoutKey(
            first, QStringLiteral("first-key")));
        QVERIFY(secondOriginWorkspace->setPageLayoutKey(
            second, QStringLiteral("second-key")));
        const auto firstId = firstWorkspace->pageId(first);
        const auto secondId = secondOriginWorkspace->pageId(second);
        QVERIFY(firstWorkspace->transferTabToWorkspace(
            firstGroup, 0, currentWorkspace, currentGroup));
        QVERIFY(secondOriginWorkspace->transferTabToWorkspace(
            *secondOriginGroup, 0, currentWorkspace, currentGroup));
        bool invalidatedSecondTarget = false;
        first->action = [&] {
            invalidatedSecondTarget = true;
            QVERIFY(secondOriginWorkspace->removeEmptyGroup(
                *secondOriginGroup));
        };
        first->armed = true;

        const auto failed = coordinator->closeWindow(currentResult.value());

        QVERIFY(!failed);
        QVERIFY(invalidatedSecondTarget);
        auto *currentTabs = currentWorkspace->tabWidget(currentGroup);
        QCOMPARE(currentTabs->count(), 2);
        QCOMPARE(currentTabs->widget(0), first);
        QCOMPARE(currentTabs->widget(1), second);
        QCOMPARE(currentWorkspace->pageForId(firstId), first);
        QCOMPARE(currentWorkspace->pageForId(secondId), second);
        QCOMPARE(currentWorkspace->pageLayoutKey(first),
            QStringLiteral("first-key"));
        QCOMPARE(currentWorkspace->pageLayoutKey(second),
            QStringLiteral("second-key"));
        QCOMPARE(currentTabs->tabText(0), QStringLiteral("First"));
        QCOMPARE(currentTabs->tabToolTip(0), QStringLiteral("first-tip"));
        QCOMPARE(currentTabs->tabWhatsThis(0), QStringLiteral("first-what"));
        QCOMPARE(currentTabs->fluentTabBar()->tabData(0).toString(),
            QStringLiteral("first-data"));
        QVERIFY(currentTabs->isTabPinned(0));
        QVERIFY(currentTabs->isTabModified(0));
        QVERIFY(currentTabs->hasTabAttention(0));
        QVERIFY(!currentTabs->isTabCloseEnabled(0));
        QCOMPARE(application.windowCount(), 3);

        const auto restoredSecondOrigin =
            secondOriginWorkspace->splitGroup(secondOriginRoot,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After,
                *secondOriginGroup);
        QVERIFY(restoredSecondOrigin.has_value());
        if (!restoredSecondOrigin.has_value()) return;
        QVERIFY(coordinator->closeWindow(currentResult.value()));
        QCOMPARE(firstWorkspace->tabWidget(firstGroup)->indexOf(first), 0);
        QCOMPARE(secondOriginWorkspace->tabWidget(*secondOriginGroup)
                     ->indexOf(second),
            0);
        application.beginShutdown();
    }

    void actualOperationsWriteStableAuditEvents()
    {
        QVERIFY(!ZzLog::isInitialized());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString logPath =
            directory.filePath(QStringLiteral("operations.log"));
        ZzLog::ZzLogConfig logConfiguration;
        logConfiguration.console.enabled = false;
        logConfiguration.file.enabled = true;
        logConfiguration.file.async = false;
        logConfiguration.file.path =
            QFileInfo(logPath).filesystemAbsoluteFilePath();
        logConfiguration.file.pattern = "%v";
        const auto initialized = ZzLog::initialize(logConfiguration);
        QVERIFY(initialized);

        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *sourceWindow = zzOnlyWindow(application);
        QVERIFY(sourceWindow != nullptr);
        auto sourceShellResult = zzCreateShell(sourceWindow);
        QVERIFY(sourceShellResult);
        auto sourceShell = std::move(sourceShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        auto sourceConfiguration = zzConfiguration();
        sourceConfiguration.title = QStringLiteral("Sensitive Workspace Title");
        QVERIFY(coordinator->registerWindow(
            {sourceWindow, sourceShell.get()}, sourceConfiguration, true));
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> shells;
        QPointer<ZzPureTools::ZzApplicationWindow> targetWindow;
        coordinator->setWindowFactory([&application, &shells, &targetWindow](
                                          const auto &) {
            auto created = application.createWindow(
                ZzPureTools::ZzApplicationWindowVisibility::Deferred);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::
                    failure(created.error());
            }
            targetWindow = created.value();
            auto shellResult =
                ZzPureTools::ZzWorkspaceShell::create(targetWindow.data(), nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
            if (!shellResult) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::
                    failure(shellResult.error());
            }
            auto shell = std::move(shellResult).value();
            auto *const observer = shell.get();
            shells.push_back(std::move(shell));
            return ZzCore::ZzResult<
                ZzPureTools::ZzWorkspaceWindowHandle>::success({targetWindow,
                observer});
        });
        auto *sourceWorkspace = sourceShell->splitWorkspace();
        const auto sourceGroup = sourceWorkspace->groupIds().constFirst();
        auto *const page = new QWidget;
        sourceWorkspace->tabWidget(sourceGroup)
            ->addTab(page, QStringLiteral("Sensitive Page Title"));

        QVERIFY(coordinator->tearOff(sourceWorkspace, sourceGroup, 0));
        QVERIFY(!targetWindow.isNull());
        QVERIFY(coordinator->closeWindow(targetWindow.data()));
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 1; }));
        QVERIFY(ZzLog::flushAndWait(std::chrono::seconds(2)));
        application.beginShutdown();
        ZzLog::shutdown();

        QFile logFile(logPath);
        QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString contents = QString::fromUtf8(logFile.readAll());
        QVERIFY(contents.contains(QStringLiteral("window.create ")));
        QVERIFY(contents.contains(QStringLiteral("window.tear_off ")));
        QVERIFY(contents.contains(QStringLiteral("page.transfer ")));
        QVERIFY(contents.contains(QStringLiteral("page.reclaim ")));
        QVERIFY(!contents.contains(QStringLiteral("layout.restore")));
        QVERIFY(!contents.contains(QStringLiteral("Sensitive")));
        QVERIFY(!contents.contains(QStringLiteral("0x")));
        const QRegularExpression allowedLine(QStringLiteral(
            "^(?:window\\.(?:create|tear_off)|page\\.(?:transfer|reclaim))"
            "(?: (?:window_id|page_id|source_window_id|target_window_id)="
            "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})+"
            " phase=[a-z_]+ elapsed_us=[0-9]+ result=(?:success|failure)$"));
        const auto lines =
            contents.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY(lines.size() >= 4);
        for (const auto &line : lines) {
            QVERIFY2(allowedLine.match(line).hasMatch(),
                qPrintable(
                    QStringLiteral("unexpected log line: %1").arg(line)));
        }
    }

    void failedTopologyRestoreWritesOneSanitizedAuditEvent()
    {
        QVERIFY(!ZzLog::isInitialized());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString logPath =
            directory.filePath(QStringLiteral("failed-topology-restore.log"));
        ZzLog::ZzLogConfig logConfiguration;
        logConfiguration.console.enabled = false;
        logConfiguration.file.enabled = true;
        logConfiguration.file.async = false;
        logConfiguration.file.path =
            QFileInfo(logPath).filesystemAbsoluteFilePath();
        logConfiguration.file.pattern = "%v";
        QVERIFY(ZzLog::initialize(logConfiguration));

        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *const coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);
        if (coordinator == nullptr) return;
        const auto restored = coordinator->restoreTopology(
            QByteArrayLiteral("SensitiveFailureLayoutKey"),
            [](QStringView) {
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(!restored);
        QVERIFY(ZzLog::flushAndWait(std::chrono::seconds(2)));
        application.beginShutdown();
        ZzLog::shutdown();

        QFile logFile(logPath);
        QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString contents = QString::fromUtf8(logFile.readAll());
        const auto lines = contents.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QStringList restoreLines;
        for (const auto &line : lines) {
            if (line.startsWith(QStringLiteral("layout.restore "))) {
                restoreLines.append(line);
            }
        }
        QCOMPARE(restoreLines.size(), 1);
        const QRegularExpression expectedLine(QStringLiteral(
            "^layout\\.restore operation_id="
            "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
            " phase=complete elapsed_us=[0-9]+ result=failure$"));
        QVERIFY(expectedLine.match(restoreLines.constFirst()).hasMatch());
        QVERIFY(!contents.contains(QStringLiteral("SensitiveFailureLayoutKey")));
        QVERIFY(!contents.contains(QStringLiteral("0x")));
    }

    void successfulTopologyRestoreWritesOneSanitizedAuditEvent()
    {
        QVERIFY(!ZzLog::isInitialized());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString logPath =
            directory.filePath(QStringLiteral("successful-topology-restore.log"));
        ZzLog::ZzLogConfig logConfiguration;
        logConfiguration.console.enabled = false;
        logConfiguration.file.enabled = true;
        logConfiguration.file.async = false;
        logConfiguration.file.path =
            QFileInfo(logPath).filesystemAbsoluteFilePath();
        logConfiguration.file.pattern = "%v";
        QVERIFY(ZzLog::initialize(logConfiguration));

        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *const sourceWindow = zzOnlyWindow(application);
        QVERIFY(sourceWindow != nullptr);
        auto sourceShellResult = zzCreateShell(sourceWindow);
        QVERIFY(sourceShellResult);
        auto sourceShell = std::move(sourceShellResult).value();
        sourceWindow->setCentralWidget(sourceShell->workspaceWidget());
        auto *const coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);
        if (coordinator == nullptr) return;
        auto configuration = zzConfiguration();
        configuration.title = QStringLiteral("Sensitive Restore Window Title");
        QVERIFY(coordinator->registerWindow(
            {sourceWindow, sourceShell.get()}, configuration, true));
        auto *const sourceWorkspace = sourceShell->splitWorkspace();
        const auto sourceGroup = sourceWorkspace->activeGroupId();
        auto *const sourcePage = new QWidget;
        sourcePage->setWindowTitle(QStringLiteral("Sensitive Saved Page Title"));
        sourceWorkspace->tabWidget(sourceGroup)->addTab(
            sourcePage, sourcePage->windowTitle());
        QVERIFY(sourceWorkspace->setPageLayoutKey(
            sourcePage, QStringLiteral("SensitiveSuccessLayoutKey")));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        sourceWorkspace->tabWidget(sourceGroup)->removeTab(0);
        delete sourcePage;
        QVERIFY(coordinator->unregisterWindow(sourceWindow));

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> restoredShells;
        coordinator->setWindowFactory(
            [&application, &restoredShells](const auto &options) {
                auto created = application.createWindow(options.visibility);
                if (!created) {
                    return ZzCore::ZzResult<
                        ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                            created.error());
                }
                auto shellResult = zzCreateShell(created.value());
                if (!shellResult) {
                    return ZzCore::ZzResult<
                        ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                            shellResult.error());
                }
                auto shell = std::move(shellResult).value();
                created.value()->setCentralWidget(shell->workspaceWidget());
                ZzPureTools::ZzWorkspaceWindowHandle handle{
                    created.value(), shell.get()};
                restoredShells.push_back(std::move(shell));
                return ZzCore::ZzResult<
                    ZzPureTools::ZzWorkspaceWindowHandle>::success(handle);
            });
        const auto restored = coordinator->restoreTopology(
            saved.value(), [](QStringView) {
                auto page = std::make_unique<QWidget>();
                page->setWindowTitle(
                    QStringLiteral("Sensitive Resolved Page Title"));
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(page));
            });
        QVERIFY(restored);
        QVERIFY(ZzLog::flushAndWait(std::chrono::seconds(2)));
        application.beginShutdown();
        ZzLog::shutdown();

        QFile logFile(logPath);
        QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString contents = QString::fromUtf8(logFile.readAll());
        const auto lines = contents.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QStringList restoreLines;
        for (const auto &line : lines) {
            if (line.startsWith(QStringLiteral("layout.restore "))) {
                restoreLines.append(line);
            }
        }
        QCOMPARE(restoreLines.size(), 1);
        const QRegularExpression expectedLine(QStringLiteral(
            "^layout\\.restore operation_id="
            "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
            " phase=complete elapsed_us=[0-9]+ result=success$"));
        QVERIFY(expectedLine.match(restoreLines.constFirst()).hasMatch());
        QVERIFY(!contents.contains(QStringLiteral("Sensitive")));
        QVERIFY(!contents.contains(QStringLiteral("0x")));
    }

    void operationsDoNotInitializeLogging()
    {
        QVERIFY(!ZzLog::isInitialized());
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(first != nullptr);
        QVERIFY(secondResult);
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(secondResult.value());
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        const auto restoreFailure = coordinator->restoreTopology(
            QByteArrayLiteral("invalid topology"),
            [](QStringView) {
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(!restoreFailure);
        QVERIFY(!ZzLog::isInitialized());
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondResult.value(), secondShell.get()}, zzConfiguration()));
        const auto firstGroup =
            firstShell->splitWorkspace()->groupIds().constFirst();
        const auto secondGroup =
            secondShell->splitWorkspace()->groupIds().constFirst();
        firstShell->splitWorkspace()
            ->tabWidget(firstGroup)
            ->addTab(new QWidget, QStringLiteral("Page"));
        QVERIFY(firstShell->splitWorkspace()->transferTabToWorkspace(
            firstGroup, 0, secondShell->splitWorkspace(), secondGroup));
        QVERIFY(coordinator->closeWindow(secondResult.value()));

        QVERIFY(!ZzLog::isInitialized());
        application.beginShutdown();
    }

    void registrationStoresConfigurationSnapshot()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);
        QCOMPARE(coordinator, application.workspaceWindowCoordinator());
        auto configuration = zzConfiguration();

        const auto registered = coordinator->registerWindow(
            {window, shell.get()}, configuration, true);
        configuration.title = QStringLiteral("mutated after registration");
        const auto stored = coordinator->configuration(window);

        QVERIFY(registered);
        QVERIFY(stored);
        QCOMPARE(stored.value().title, QStringLiteral("Workspace A"));
        QCOMPARE(stored.value().minimumSize, QSize(320, 240));
        QCOMPARE(stored.value().maximumSize, QSize(1440, 900));
        QCOMPARE(stored.value().initialGeometry, QRect(-20, -40, 1280, 720));
        application.beginShutdown();
    }

    void registrationValidatesConfigurationWithoutMutation()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);
        struct ZzInvalidConfigurationCase final
        {
            ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
        };
        auto negativeSize = zzConfiguration();
        negativeSize.minimumSize = QSize(-1, 240);
        auto invertedBounds = zzConfiguration();
        invertedBounds.minimumSize = QSize(800, 600);
        invertedBounds.maximumSize = QSize(640, 480);
        auto negativeMaximumSize = zzConfiguration();
        negativeMaximumSize.maximumSize = QSize(1440, -1);
        auto negativeGeometry = zzConfiguration();
        negativeGeometry.initialGeometry = QRect(20, 40, -1, 720);
        auto invalidTitleMode = zzConfiguration();
        invalidTitleMode.titleMode =
            // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
            static_cast<ZzPureTools::ZzWorkspaceTitleMode>(42);
        auto invalidClosePolicy = zzConfiguration();
        invalidClosePolicy.closePolicy =
            // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
            static_cast<ZzPureTools::ZzWindowClosePolicy>(42);
        const std::vector<ZzInvalidConfigurationCase> invalidCases{
            {negativeSize},
            {invertedBounds},
            {negativeMaximumSize},
            {negativeGeometry},
            {invalidTitleMode},
            {invalidClosePolicy}};

        for (const auto &testCase : invalidCases) {
            const auto rejected = coordinator->registerWindow(
                {window, shell.get()}, testCase.configuration);
            const auto missing = coordinator->configuration(window);
            QVERIFY(!rejected);
            QCOMPARE(
                rejected.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
            QVERIFY(!missing);
            QCOMPARE(missing.error().code(), ZzCore::ZzErrorCode::NotFound);
        }

        const ZzPureTools::ZzWorkspaceWindowConfiguration defaults;
        const auto registered = coordinator->registerWindow(
            {window, shell.get()}, defaults);
        const auto stored = coordinator->configuration(window);

        QVERIFY(registered);
        QVERIFY(stored);
        QCOMPARE(stored.value().minimumSize, QSize());
        QCOMPARE(stored.value().maximumSize, QSize());
        QCOMPARE(stored.value().initialGeometry, QRect());
        application.beginShutdown();
    }

    void registrationRejectsDuplicateWindowAndShell()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto *second = secondResult.value();
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(second);
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({first, firstShell.get()}, zzConfiguration()));

        const auto duplicateWindow = coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration());
        const auto duplicateShell = coordinator->registerWindow(
            {second, firstShell.get()}, zzConfiguration());

        QVERIFY(!duplicateWindow);
        QCOMPARE(duplicateWindow.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!duplicateShell);
        QCOMPARE(duplicateShell.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!coordinator->configuration(second));
        Q_UNUSED(secondShell);
        application.beginShutdown();
    }

    void registrationAllowsOnlyOnePrimaryWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto *second = secondResult.value();
        auto firstShellResult = zzCreateShell(first);
        auto secondShellResult = zzCreateShell(second);
        QVERIFY(firstShellResult);
        QVERIFY(secondShellResult);
        auto firstShell = std::move(firstShellResult).value();
        auto secondShell = std::move(secondShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {first, firstShell.get()}, zzConfiguration(), true));

        const auto rejected = coordinator->registerWindow(
            {second, secondShell.get()}, zzConfiguration(), true);
        const auto accepted = coordinator->registerWindow(
            {second, secondShell.get()}, zzConfiguration());

        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(accepted);
        application.beginShutdown();
    }

    void registrationRejectsWrongHostAndForeignThread()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondResult);
        auto *second = secondResult.value();
        auto foreignShellResult = zzCreateShell(first);
        QVERIFY(foreignShellResult);
        auto foreignShell = std::move(foreignShellResult).value();
        auto ownShellResult = zzCreateShell(second);
        QVERIFY(ownShellResult);
        auto ownShell = std::move(ownShellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();

        const auto wrongHost = coordinator->registerWindow(
            {second, foreignShell.get()}, zzConfiguration());
        bool rejectedInWorker = false;
        ZzCore::ZzErrorCode workerCode = ZzCore::ZzErrorCode::None;
        std::thread worker([&] {
            const auto result = coordinator->registerWindow(
                {second, ownShell.get()}, zzConfiguration());
            rejectedInWorker = !result;
            if (!result) {
                workerCode = result.error().code();
            }
        });
        worker.join();

        QVERIFY(!wrongHost);
        QCOMPARE(wrongHost.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(rejectedInWorker);
        QCOMPARE(workerCode, ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!coordinator->configuration(second));
        application.beginShutdown();
    }

    void destroyedWindowOrShellUnregistersItsRecord()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));

        shell.reset();
        QCoreApplication::processEvents();
        const auto afterShellDestroyed = coordinator->configuration(window);

        QVERIFY(!afterShellDestroyed);
        QCOMPARE(afterShellDestroyed.error().code(), ZzCore::ZzErrorCode::NotFound);
        application.beginShutdown();
    }

    void destroyedWindowUnregistersItsRecordBeforeApplicationErase()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));
        auto *destroyedIdentity = window;
        QPointer<ZzPureTools::ZzApplicationWindow> destroyedWindow(window);

        QVERIFY(window->close());
        QVERIFY(QTest::qWaitFor(
            [&application] { return application.windowCount() == 0; }));
        const auto afterWindowDestroyed = coordinator->configuration(
            destroyedIdentity);

        QVERIFY(destroyedWindow.isNull());
        QVERIFY(!afterWindowDestroyed);
        QCOMPARE(
            afterWindowDestroyed.error().code(),
            ZzCore::ZzErrorCode::NotFound);
        application.beginShutdown();
    }

    void unregisterOnlyRemovesCoordinatorState()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));

        const auto unregistered = coordinator->unregisterWindow(window);
        const auto missing = coordinator->configuration(window);

        QVERIFY(unregistered);
        QVERIFY(window != nullptr);
        QCOMPARE(application.windowCount(), 1);
        QVERIFY(!missing);
        QCOMPARE(missing.error().code(), ZzCore::ZzErrorCode::NotFound);
        application.beginShutdown();
    }

    void createWindowRequiresFactoryAndAppliesConfiguration()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;

        const auto missingFactory = coordinator->createWindow();
        QVERIFY(!missingFactory);
        QCOMPARE(missingFactory.error().code(), ZzCore::ZzErrorCode::InvalidState);

        coordinator->setWindowFactory([&application, &createdShells](const auto &) {
            auto result = application.createWindow(ZzPureTools::ZzApplicationWindowVisibility::Deferred);
            if (!result) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(result.error());
            }
            auto *window = result.value();
            auto createdShell = ZzPureTools::ZzWorkspaceShell::create(window, nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
            if (!createdShell) {
                window->close();
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(createdShell.error());
            }
            auto newShell = std::move(createdShell).value();
            auto *workspace = newShell->workspaceWidget();
            window->setCentralWidget(workspace);
            auto *shellObserver = newShell.get();
            createdShells.push_back(std::move(newShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success({window, shellObserver});
        });
        ZzPureTools::ZzWorkspaceWindowCreateOptions options;
        options.configurationSource = ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
        options.sourceWindow = source;
        options.visibility = ZzPureTools::ZzApplicationWindowVisibility::Deferred;
        options.configuration.title = QStringLiteral("Child");
        options.configuration.minimumSize = QSize(500, 400);
        options.configuration.alwaysOnTop = true;
        const auto created = coordinator->createWindow(options);
        QVERIFY(created);
        QVERIFY(created.value().window != nullptr);
        QCOMPARE(created.value().window->windowTitle(), QStringLiteral("Child"));
        QVERIFY(!created.value().window->isVisible());
        QVERIFY(coordinator->configuration(created.value().window));
        const auto createdConfiguration = coordinator->configuration(created.value().window);
        QCOMPARE(createdConfiguration.value().minimumSize, QSize(500, 400));
        QVERIFY(createdConfiguration.value().alwaysOnTop);
        source->setWindowTitle(QStringLiteral("Mutated source"));
        QCOMPARE(created.value().window->windowTitle(), QStringLiteral("Child"));
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void deferredFactoryWindowIsHiddenBeforeCoordinatorCommit()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;
        QPointer<ZzPureTools::ZzApplicationWindow> createdWindow;
        coordinator->setWindowFactory([&application, &createdShells, &createdWindow](const auto &) {
            auto created = application.createWindow(
                ZzPureTools::ZzApplicationWindowVisibility::Visible);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            createdWindow = created.value();
            auto createdShell = ZzPureTools::ZzWorkspaceShell::create(createdWindow.data(), nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    createdShell.error());
            }
            auto shellValue = std::move(createdShell).value();
            createdWindow->setCentralWidget(shellValue->workspaceWidget());
            auto *shellObserver = shellValue.get();
            createdShells.push_back(std::move(shellValue));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                {createdWindow, shellObserver});
        });
        ZzPureTools::ZzWorkspaceWindowCreateOptions options;
        options.visibility = ZzPureTools::ZzApplicationWindowVisibility::Deferred;
        const auto created = coordinator->createWindow(options);

        QVERIFY(created);
        QVERIFY(createdWindow);
        QVERIFY(!createdWindow->isVisible());
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void emitsWindowLifecycleSignalsOnlyAfterCommit()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        QSignalSpy createdSpy(
            coordinator, &ZzPureTools::ZzWorkspaceWindowCoordinator::windowCreated);
        QSignalSpy configurationSpy(
            coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::windowConfigurationChanged);
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;
        coordinator->setWindowFactory([&application, &createdShells](const auto &options) {
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto createdShell = ZzPureTools::ZzWorkspaceShell::create(created.value(), nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    createdShell.error());
            }
            auto shellValue = std::move(createdShell).value();
            created.value()->setCentralWidget(shellValue->workspaceWidget());
            auto *shellObserver = shellValue.get();
            createdShells.push_back(std::move(shellValue));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                {created.value(), shellObserver});
        });
        ZzPureTools::ZzWorkspaceWindowCreateOptions options;
        options.visibility = ZzPureTools::ZzApplicationWindowVisibility::Deferred;
        options.configuration.title = QStringLiteral("Committed child");
        const auto created = coordinator->createWindow(options);
        QVERIFY(created);
        QCOMPARE(createdSpy.size(), 1);
        QCOMPARE(createdSpy.at(0).at(0).value<ZzPureTools::ZzApplicationWindow *>(),
                 created.value().window.data());
        QCOMPARE(configurationSpy.size(), 1);
        QCOMPARE(configurationSpy.at(0).at(0).value<ZzPureTools::ZzApplicationWindow *>(),
                 created.value().window.data());
        QCOMPARE(configurationSpy.at(0).at(1)
                     .value<ZzPureTools::ZzWorkspaceWindowConfiguration>().title,
                 QStringLiteral("Committed child"));

        ZzPureTools::ZzWorkspaceWindowConfigurationPatch patch;
        patch.title = QStringLiteral("Updated");
        QVERIFY(coordinator->applyConfiguration(source, patch));
        QCOMPARE(configurationSpy.size(), 2);
        QCOMPARE(configurationSpy.at(1).at(0).value<ZzPureTools::ZzApplicationWindow *>(),
                 source);
        QCOMPARE(configurationSpy.at(1).at(1)
                     .value<ZzPureTools::ZzWorkspaceWindowConfiguration>().title,
                 QStringLiteral("Updated"));
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void registeredWorkspaceSignalTearsOffTheSamePageTransactionally()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        source->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        auto *workspace = shell->splitWorkspace();
        const auto sourceGroup = workspace->groupIds().constFirst();
        auto *const page = new QWidget;
        workspace->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Tear off"));

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;
        QPointer<ZzPureTools::ZzApplicationWindow> targetWindow;
        ZzPureTools::ZzApplicationWindowVisibility factoryVisibility =
            ZzPureTools::ZzApplicationWindowVisibility::Visible;
        coordinator->setWindowFactory(
            [&application, &createdShells, &targetWindow, &factoryVisibility](
                const ZzPureTools::ZzWorkspaceWindowCreateOptions &options) {
                factoryVisibility = options.visibility;
                auto created = application.createWindow(options.visibility);
                if (!created) {
                    return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                        created.error());
                }
                targetWindow = created.value();
                auto targetShellResult = ZzPureTools::ZzWorkspaceShell::create(
                    targetWindow.data(), nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
                if (!targetShellResult) {
                    return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                        targetShellResult.error());
                }
                auto targetShell = std::move(targetShellResult).value();
                targetWindow->setCentralWidget(targetShell->workspaceWidget());
                auto *shellObserver = targetShell.get();
                createdShells.push_back(std::move(targetShell));
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                    {targetWindow, shellObserver});
            });

        Q_EMIT workspace->tabTearOffRequested(
            sourceGroup, 0, workspace->pageId(page), QPoint(100, 100), QSize(420, 300));

        QCOMPARE(factoryVisibility, ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(targetWindow != nullptr);
        QVERIFY(targetWindow->isVisible());
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), -1);
        QCOMPARE(createdShells.front()->splitWorkspace()->tabWidget(
            createdShells.front()->splitWorkspace()->groupIds().constFirst())->indexOf(page), 0);
        QVERIFY(createdShells.front()->splitWorkspace()->isAncestorOf(page));
        const auto targetConfiguration = coordinator->configuration(targetWindow.data());
        QVERIFY(targetConfiguration);
        QCOMPARE(targetConfiguration.value().title, QStringLiteral("Workspace A"));

        const auto failed = coordinator->tearOff(workspace, sourceGroup, 9);
        QVERIFY(!failed);
        QCOMPARE(failed.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), -1);
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void factoryAndTransferFailuresLeaveSourceAndCoordinatorUnchanged()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *source = zzOnlyWindow(application);
        QVERIFY(source != nullptr);
        auto shellResult = zzCreateShell(source);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        source->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({source, shell.get()}, zzConfiguration(), true));
        auto *workspace = shell->splitWorkspace();
        const auto sourceGroup = workspace->groupIds().constFirst();
        auto *const page = new QWidget;
        workspace->tabWidget(sourceGroup)->addTab(page, QStringLiteral("Source"));

        coordinator->setWindowFactory([](const auto &) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success({});
        });
        const auto invalidHandle = coordinator->createWindow();
        QVERIFY(!invalidHandle);
        QCOMPARE(invalidHandle.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(application.windowCount(), 1);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), 0);

        coordinator->setWindowFactory([](const auto &) {
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                ZzCore::ZzError(ZzCore::ZzErrorCode::Unknown,
                    QStringLiteral("factory failure")));
        });
        const auto factoryFailure = coordinator->createWindow();
        QVERIFY(!factoryFailure);
        QCOMPARE(factoryFailure.error().code(), ZzCore::ZzErrorCode::Unknown);
        QCOMPARE(application.windowCount(), 1);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), 0);

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> createdShells;
        QPointer<ZzPureTools::ZzApplicationWindow> stagedWindow;
        coordinator->setWindowFactory([&application, &createdShells, &stagedWindow](const auto &) {
            auto created = application.createWindow(ZzPureTools::ZzApplicationWindowVisibility::Deferred);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(created.error());
            }
            stagedWindow = created.value();
            auto createdShell = ZzPureTools::ZzWorkspaceShell::create(stagedWindow.data(), nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(createdShell.error());
            }
            auto targetShell = std::move(createdShell).value();
            stagedWindow->setCentralWidget(targetShell->workspaceWidget());
            auto *shellObserver = targetShell.get();
            createdShells.push_back(std::move(targetShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                {stagedWindow, shellObserver});
        });
        const auto failedTransfer = coordinator->tearOff(workspace, sourceGroup, 3);
        QVERIFY(!failedTransfer);
        QCOMPARE(failedTransfer.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(workspace->tabWidget(sourceGroup)->indexOf(page), 0);
        QVERIFY(QTest::qWaitFor([&application] { return application.windowCount() == 1; }));
        QVERIFY(coordinator->configuration(source));
        createdShells.clear();
        shell.reset();
        application.beginShutdown();
    }

    void factoryExceptionReturnsUnknownWithoutCoordinatorMutation()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));
        coordinator->setWindowFactory([](const auto &) -> ZzCore::ZzResult<
            ZzPureTools::ZzWorkspaceWindowHandle> {
            throw std::runtime_error("factory exception");
        });

        const auto created = coordinator->createWindow();

        QVERIFY(!created);
        QCOMPARE(created.error().code(), ZzCore::ZzErrorCode::Unknown);
        QCOMPARE(application.windowCount(), 1);
        QVERIFY(coordinator->configuration(window));
        shell.reset();
        application.beginShutdown();
    }

    void tearOffRejectsForeignThreadAndShutdownBeforeStateAccess()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, zzConfiguration()));
        auto *workspace = shell->splitWorkspace();
        const auto group = workspace->groupIds().constFirst();
        ZzCore::ZzErrorCode foreignCode = ZzCore::ZzErrorCode::None;
        bool foreignFailed = false;
        std::thread worker([&] {
            const auto result = coordinator->tearOff(workspace, group, 0);
            foreignFailed = !result;
            if (!result) {
                foreignCode = result.error().code();
            }
        });
        worker.join();
        QVERIFY(foreignFailed);
        QCOMPARE(foreignCode, ZzCore::ZzErrorCode::InvalidState);

        application.beginShutdown();
        const auto afterShutdown = coordinator->tearOff(nullptr, {}, 0);
        QVERIFY(!afterShutdown);
        QCOMPARE(afterShutdown.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void applyConfigurationPreflightsAlwaysOnTopBeforeChangingRealSurfaces()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QPixmap iconPixmap(1, 1);
        iconPixmap.fill(Qt::red);
        const QIcon iconBefore(iconPixmap);
        window->setWindowIcon(iconBefore);
        window->setMaximumSize(QSize(1200, 900));
        window->setMinimumSize(QSize(320, 240));
        window->setGeometry(QRect(40, 50, 640, 480));
        shell->setApplicationTitle(QStringLiteral("Before"));
        shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Application);
        QVERIFY(shell->setAlwaysOnTop(false));
        ZzPureTools::ZzWorkspaceWindowConfiguration before;
        before.title = shell->applicationTitle();
        before.icon = window->windowIcon();
        before.titleMode = shell->titleMode();
        before.alwaysOnTop = shell->isAlwaysOnTop();
        before.minimumSize = window->minimumSize();
        before.maximumSize = window->maximumSize();
        before.initialGeometry = window->geometry();
        QVERIFY(coordinator->registerWindow({window, shell.get()}, before));

        ZzPureTools::ZzWorkspaceWindowConfigurationPatch patch;
        QPixmap changedIconPixmap(1, 1);
        changedIconPixmap.fill(Qt::blue);
        patch.icon = QIcon(changedIconPixmap);
        patch.minimumSize = QSize(400, 300);
        patch.maximumSize = QSize(1000, 800);
        patch.initialGeometry = QRect(120, 130, 700, 500);
        patch.title = QStringLiteral("Changed");
        patch.titleMode = ZzPureTools::ZzWorkspaceTitleMode::Custom;
        patch.alwaysOnTop = true;
        bool callbackEntered = false;
        ZzCore::ZzErrorCode callbackError = ZzCore::ZzErrorCode::None;
        auto *tabs = shell->splitWorkspace()->tabWidget(
            shell->splitWorkspace()->activeGroupId());
        const QMetaObject::Connection connection = QObject::connect(
            tabs, &QTabWidget::currentChanged, window, [&](int) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                const auto applied = coordinator->applyConfiguration(window, patch);
                QVERIFY(!applied);
                callbackError = applied.error().code();
            });

        const auto integrated = shell->integrateApplicationNavigation(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("navigation")),
            QStringLiteral("Navigation"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            QStringLiteral("Pages"));
        QObject::disconnect(connection);

        QVERIFY(callbackEntered);
        QCOMPARE(callbackError, ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(integrated);
        QCOMPARE(window->geometry(), before.initialGeometry);
        QCOMPARE(window->minimumSize(), before.minimumSize);
        QCOMPARE(window->maximumSize(), before.maximumSize);
        QCOMPARE(window->windowIcon().cacheKey(), before.icon.cacheKey());
        QCOMPARE(shell->applicationTitle(), before.title);
        QCOMPARE(shell->titleMode(), before.titleMode);
        QCOMPARE(shell->isAlwaysOnTop(), before.alwaysOnTop);
        const auto stored = coordinator->configuration(window);
        QVERIFY(stored);
        QCOMPARE(stored.value().initialGeometry, before.initialGeometry);
        QCOMPARE(stored.value().minimumSize, before.minimumSize);
        QCOMPARE(stored.value().maximumSize, before.maximumSize);
        QCOMPARE(stored.value().icon.cacheKey(), before.icon.cacheKey());
        QCOMPARE(stored.value().title, before.title);
        QCOMPARE(stored.value().titleMode, before.titleMode);
        QCOMPARE(stored.value().alwaysOnTop, before.alwaysOnTop);
        shell.reset();
        application.beginShutdown();
    }

    void topologyPersistenceRejectsEmptyStateAndMissingPageKeys()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));

        const auto rejected = coordinator->restoreTopology(QByteArray(), {});
        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        auto *const page = new QWidget;
        const auto group = shell->splitWorkspace()->activeGroupId();
        shell->splitWorkspace()->tabWidget(group)->addTab(
            page, QStringLiteral("missing-key"));

        const auto missingKey = coordinator->saveTopology();
        QVERIFY(!missingKey);
        QCOMPARE(missingKey.error().code(), ZzCore::ZzErrorCode::InvalidState);
        int factoryCalls = 0;
        int resolverCalls = 0;
        const qsizetype windowCount = application.windowCount();
        const qsizetype widgetCount = QApplication::allWidgets().size();
        coordinator->setWindowFactory([&factoryCalls](const auto &) {
            ++factoryCalls;
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                ZzCore::ZzError(ZzCore::ZzErrorCode::Backend,
                    QStringLiteral("factory must not be called")));
        });
        const auto nonEmpty = coordinator->restoreTopology(
            QByteArrayLiteral("invalid"), [&resolverCalls](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(!nonEmpty);
        QCOMPARE(nonEmpty.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(factoryCalls, 0);
        QCOMPARE(resolverCalls, 0);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);
        application.beginShutdown();
    }

    void topologyPersistenceRoundTripsDeferredWindowsAndPages()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *firstWindow = zzOnlyWindow(application);
        QVERIFY(firstWindow != nullptr);
        auto firstShellResult = zzCreateShell(firstWindow);
        QVERIFY(firstShellResult);
        auto firstShell = std::move(firstShellResult).value();
        firstWindow->setCentralWidget(firstShell->workspaceWidget());
        auto secondWindowResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondWindowResult);
        auto secondShellResult = zzCreateShell(secondWindowResult.value());
        QVERIFY(secondShellResult);
        auto secondShell = std::move(secondShellResult).value();
        secondWindowResult.value()->setCentralWidget(secondShell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {firstWindow, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondWindowResult.value(), secondShell.get()}, zzConfiguration()));

        auto *firstPage = new QWidget;
        auto *secondPage = new QWidget;
        const auto firstGroup = firstShell->splitWorkspace()->activeGroupId();
        const auto secondGroup = secondShell->splitWorkspace()->activeGroupId();
        firstShell->splitWorkspace()->tabWidget(firstGroup)->addTab(
            firstPage, QStringLiteral("first"));
        secondShell->splitWorkspace()->tabWidget(secondGroup)->addTab(
            secondPage, QStringLiteral("second"));
        QVERIFY(firstShell->splitWorkspace()->setPageLayoutKey(
            firstPage, QStringLiteral("page/first")));
        QVERIFY(secondShell->splitWorkspace()->setPageLayoutKey(
            secondPage, QStringLiteral("page/second")));
        const auto firstPageId = firstShell->splitWorkspace()->pageId(firstPage);
        const auto secondPageId = secondShell->splitWorkspace()->pageId(secondPage);
        QVERIFY(firstPageId.isValid());
        QVERIFY(secondPageId.isValid());
        QVERIFY(firstShell->splitWorkspace()->splitGroup(
            firstGroup, Qt::Vertical, ZzFluentUI::ZzSplitPlacement::After)
                    .has_value());
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        QVERIFY(!saved.value().isEmpty());

        firstShell->splitWorkspace()->tabWidget(firstGroup)->removeTab(0);
        secondShell->splitWorkspace()->tabWidget(secondGroup)->removeTab(0);
        delete firstPage;
        delete secondPage;
        QVERIFY(coordinator->unregisterWindow(firstWindow));
        QVERIFY(coordinator->unregisterWindow(secondWindowResult.value()));

        int factoryCalls = 0;
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> failedShells;
        coordinator->setWindowFactory([&application, &factoryCalls, &failedShells](const auto &options) {
            if (++factoryCalls == 2) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    ZzCore::ZzError(ZzCore::ZzErrorCode::Backend,
                        QStringLiteral("second window failure")));
            }
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto shellResult = zzCreateShell(created.value());
            if (!shellResult) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    shellResult.error());
            }
            auto shell = std::move(shellResult).value();
            created.value()->setCentralWidget(shell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), shell.get()};
            failedShells.push_back(std::move(shell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        const auto factoryFailure = coordinator->restoreTopology(
            saved.value(), [](QStringView) {
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(!factoryFailure);
        QCOMPARE(application.windowCount(), qsizetype(2));

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> restoredShells;
        coordinator->setWindowFactory([&application, &restoredShells](const auto &options) {
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto shellResult = zzCreateShell(created.value());
            if (!shellResult) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    shellResult.error());
            }
            auto shell = std::move(shellResult).value();
            created.value()->setCentralWidget(shell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), shell.get()};
            restoredShells.push_back(std::move(shell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        QHash<QString, QWidget *> restoredPages;
        const auto restored = coordinator->restoreTopology(
            saved.value(), [&restoredPages](QStringView key) {
                auto page = std::make_unique<QWidget>();
                restoredPages.insert(key.toString(), page.get());
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(page));
            });
        QVERIFY(restored);
        QCOMPARE(restoredShells.size(), std::size_t(2));
        QCOMPARE(application.windowCount(), qsizetype(4));
        auto *const firstRestoredWorkspace = restoredShells.at(0)->splitWorkspace();
        auto *const secondRestoredWorkspace = restoredShells.at(1)->splitWorkspace();
        auto *const restoredFirstPage = restoredPages.value(QStringLiteral("page/first"));
        auto *const restoredSecondPage = restoredPages.value(QStringLiteral("page/second"));
        QVERIFY(restoredFirstPage != nullptr);
        QVERIFY(restoredSecondPage != nullptr);
        if (restoredFirstPage == nullptr || restoredSecondPage == nullptr) return;
        QCOMPARE(firstRestoredWorkspace->pageForId(firstPageId), restoredFirstPage);
        QCOMPARE(secondRestoredWorkspace->pageForId(secondPageId), restoredSecondPage);
        QVERIFY(restoredFirstPage->parent() != nullptr);
        QVERIFY(restoredSecondPage->parent() != nullptr);
        QCOMPARE(firstRestoredWorkspace->pageLayoutKey(restoredFirstPage),
            QStringLiteral("page/first"));
        QCOMPARE(secondRestoredWorkspace->pageLayoutKey(restoredSecondPage),
            QStringLiteral("page/second"));
        auto *const firstTabs = firstRestoredWorkspace->tabWidget(firstGroup);
        auto *const secondTabs = secondRestoredWorkspace->tabWidget(secondGroup);
        QVERIFY(firstTabs != nullptr);
        QVERIFY(secondTabs != nullptr);
        QCOMPARE(firstTabs->indexOf(restoredFirstPage), 0);
        QCOMPARE(secondTabs->indexOf(restoredSecondPage), 0);
        application.beginShutdown();
    }

    void topologyRestoreReusesRegisteredEmptyPrimaryWithoutExtraWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *const primaryWindow = zzOnlyWindow(application);
        QVERIFY(primaryWindow != nullptr);
        auto primaryShellResult = zzCreateShell(primaryWindow);
        QVERIFY(primaryShellResult);
        auto primaryShell = std::move(primaryShellResult).value();
        primaryWindow->setCentralWidget(primaryShell->workspaceWidget());
        auto secondWindowResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondWindowResult);
        QPointer<ZzPureTools::ZzApplicationWindow> secondWindow =
            secondWindowResult.value();
        auto secondShellResult = zzCreateShell(secondWindow.data());
        QVERIFY(secondShellResult);
        auto secondShell = std::move(secondShellResult).value();
        secondWindow->setCentralWidget(secondShell->workspaceWidget());
        auto *const coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {primaryWindow, primaryShell.get()}, zzConfiguration(), true));
        auto secondConfiguration = zzConfiguration();
        secondConfiguration.title = QStringLiteral("Second archive window");
        QVERIFY(coordinator->registerWindow(
            {secondWindow.data(), secondShell.get()}, secondConfiguration));

        auto *const primaryWorkspace = primaryShell->splitWorkspace();
        auto *const secondWorkspace = secondShell->splitWorkspace();
        const auto primaryGroup = primaryWorkspace->activeGroupId();
        const auto secondGroup = secondWorkspace->activeGroupId();
        auto *const primaryPage = new QWidget;
        auto *const secondPage = new QWidget;
        primaryWorkspace->tabWidget(primaryGroup)->addTab(
            primaryPage, QStringLiteral("Primary"));
        secondWorkspace->tabWidget(secondGroup)->addTab(
            secondPage, QStringLiteral("Second"));
        QVERIFY(primaryWorkspace->setPageLayoutKey(
            primaryPage, QStringLiteral("restore/primary")));
        QVERIFY(secondWorkspace->setPageLayoutKey(
            secondPage, QStringLiteral("restore/second")));
        primaryWindow->setGeometry(QRect(20, 20, 640, 480));
        secondWindow->setGeometry(QRect(80, 80, 600, 440));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);

        primaryWorkspace->tabWidget(primaryGroup)->removeTab(0);
        secondWorkspace->tabWidget(secondGroup)->removeTab(0);
        delete primaryPage;
        delete secondPage;
        QVERIFY(coordinator->unregisterWindow(secondWindow.data()));
        secondShell.reset();
        QVERIFY(secondWindow->close());
        QTRY_COMPARE(application.windowCount(), qsizetype(1));
        QVERIFY(!secondWindow);

        int factoryCalls = 0;
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> restoredShells;
        coordinator->setWindowFactory(
            [&application, &factoryCalls, &restoredShells](const auto &options) {
                ++factoryCalls;
                auto created = application.createWindow(options.visibility);
                if (!created) {
                    return ZzCore::ZzResult<
                        ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                            created.error());
                }
                auto shellResult = zzCreateShell(created.value());
                if (!shellResult) {
                    return ZzCore::ZzResult<
                        ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                            shellResult.error());
                }
                auto shell = std::move(shellResult).value();
                created.value()->setCentralWidget(shell->workspaceWidget());
                const ZzPureTools::ZzWorkspaceWindowHandle handle{
                    created.value(), shell.get()};
                restoredShells.push_back(std::move(shell));
                return ZzCore::ZzResult<
                    ZzPureTools::ZzWorkspaceWindowHandle>::success(handle);
            });
        QHash<QString, QWidget *> restoredPages;
        const auto restored = coordinator->restoreTopology(
            saved.value(), [&restoredPages](QStringView key) {
                auto page = std::make_unique<QWidget>();
                restoredPages.insert(key.toString(), page.get());
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(page));
            });

        QVERIFY2(restored,
            restored ? "" : qPrintable(restored.error().technicalMessage()));
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(application.windowCount(), qsizetype(2));
        QCOMPARE(restoredShells.size(), std::size_t(1));
        QVERIFY(restoredPages.contains(QStringLiteral("restore/primary")));
        QVERIFY(restoredPages.contains(QStringLiteral("restore/second")));
        QVERIFY(coordinator->configuration(primaryWindow));
        const auto savedAgain = coordinator->saveTopology();
        QVERIFY(savedAgain);
        QCOMPARE(savedAgain.value(), saved.value());
        application.beginShutdown();
    }

    void topologyRestoreRejectsResolverInvalidatingRegisteredWindow()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *const primaryWindow = zzOnlyWindow(application);
        QVERIFY(primaryWindow != nullptr);
        auto primaryShellResult = zzCreateShell(primaryWindow);
        QVERIFY(primaryShellResult);
        auto primaryShell = std::move(primaryShellResult).value();
        primaryWindow->setCentralWidget(primaryShell->workspaceWidget());
        auto *const coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {primaryWindow, primaryShell.get()}, zzConfiguration(), true));

        auto *const workspace = primaryShell->splitWorkspace();
        const auto group = workspace->activeGroupId();
        auto *const page = new QWidget;
        workspace->tabWidget(group)->addTab(page, QStringLiteral("Page"));
        QVERIFY(workspace->setPageLayoutKey(page,
            QStringLiteral("restore/invalidated")));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        workspace->tabWidget(group)->removeTab(0);
        delete page;

        bool resolverCalled = false;
        const auto restored = coordinator->restoreTopology(
            saved.value(), [&resolverCalled, coordinator, primaryWindow](QStringView) {
                resolverCalled = true;
                const auto unregistered = coordinator->unregisterWindow(primaryWindow);
                Q_ASSERT(unregistered);
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });

        QVERIFY(resolverCalled);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        const auto restoredConfiguration = coordinator->configuration(primaryWindow);
        QVERIFY(restoredConfiguration);
        QCOMPARE(restoredConfiguration.value().title,
            zzConfiguration().title);
        QCOMPARE(application.windowCount(), qsizetype(1));
        application.beginShutdown();
    }

    void topologyRestoreRejectsRawAndOriginMismatchesBeforeFactories()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *firstWindow = zzOnlyWindow(application);
        QVERIFY(firstWindow != nullptr);
        auto firstShellResult = zzCreateShell(firstWindow);
        QVERIFY(firstShellResult);
        auto firstShell = std::move(firstShellResult).value();
        firstWindow->setCentralWidget(firstShell->workspaceWidget());
        auto secondWindowResult = application.createWindow(
            ZzPureTools::ZzApplicationWindowVisibility::Deferred);
        QVERIFY(secondWindowResult);
        auto secondShellResult = zzCreateShell(secondWindowResult.value());
        QVERIFY(secondShellResult);
        auto secondShell = std::move(secondShellResult).value();
        secondWindowResult.value()->setCentralWidget(secondShell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {firstWindow, firstShell.get()}, zzConfiguration(), true));
        QVERIFY(coordinator->registerWindow(
            {secondWindowResult.value(), secondShell.get()}, zzConfiguration()));
        auto *const firstWorkspace = firstShell->splitWorkspace();
        auto *const secondWorkspace = secondShell->splitWorkspace();
        const auto firstGroup = firstWorkspace->activeGroupId();
        const auto secondGroup = secondWorkspace->activeGroupId();
        auto *moved = new QWidget;
        auto *resident = new QWidget;
        firstWorkspace->tabWidget(firstGroup)->addTab(
            moved, QStringLiteral("moved"));
        secondWorkspace->tabWidget(secondGroup)->addTab(
            resident, QStringLiteral("resident"));
        QVERIFY(firstWorkspace->setPageLayoutKey(moved, QStringLiteral("moved")));
        QVERIFY(secondWorkspace->setPageLayoutKey(
            resident, QStringLiteral("resident")));
        QVERIFY(firstWorkspace->transferTabToWorkspace(
            firstGroup, 0, secondWorkspace, secondGroup));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        auto pageIndexOffsets = QList<qsizetype> {};
        auto originUuidOffsets = QList<qsizetype> {};
        auto originIndexOffsets = QList<qsizetype> {};
        QVERIFY(zzTopologyFieldOffsets(
            saved.value(), &pageIndexOffsets, &originUuidOffsets,
            &originIndexOffsets));
        QVERIFY(pageIndexOffsets.size() >= 2);
        QVERIFY(!originUuidOffsets.isEmpty());
        QVERIFY(!originIndexOffsets.isEmpty());
        secondWorkspace->tabWidget(secondGroup)->removeTab(1);
        secondWorkspace->tabWidget(secondGroup)->removeTab(0);
        delete moved;
        delete resident;
        QVERIFY(coordinator->unregisterWindow(firstWindow));
        QVERIFY(coordinator->unregisterWindow(secondWindowResult.value()));
        const qsizetype windowCount = application.windowCount();
        const qsizetype widgetCount = QApplication::allWidgets().size();

        const auto expectRejectedWithoutCallbacks =
            [&](const QByteArray &encoded) {
                int factoryCalls = 0;
                int resolverCalls = 0;
                coordinator->setWindowFactory([&factoryCalls](const auto &) {
                    ++factoryCalls;
                    return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                        ZzCore::ZzError(ZzCore::ZzErrorCode::Backend,
                        QStringLiteral("factory must not be called")));
                });
                const auto result = coordinator->restoreTopology(
                    encoded, [&resolverCalls](QStringView) {
                        ++resolverCalls;
                        return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                            std::make_unique<QWidget>());
                    });
                QVERIFY(!result);
                QCOMPARE(factoryCalls, 0);
                QCOMPARE(resolverCalls, 0);
                QCOMPARE(application.windowCount(), windowCount);
                QCOMPARE(QApplication::allWidgets().size(), widgetCount);
            };

        QByteArray invalidOriginIndex = saved.value();
        zzPatchTopologyInt32(
            &invalidOriginIndex, originIndexOffsets.front(), 4097);
        zzRefreshTopologyDigest(&invalidOriginIndex);
        expectRejectedWithoutCallbacks(invalidOriginIndex);

        QByteArray invalidOrder = saved.value();
        zzPatchTopologyInt32(&invalidOrder, pageIndexOffsets.at(0), 1);
        zzPatchTopologyInt32(&invalidOrder, pageIndexOffsets.at(1), 0);
        zzRefreshTopologyDigest(&invalidOrder);
        expectRejectedWithoutCallbacks(invalidOrder);

        expectRejectedWithoutCallbacks(zzMalformedSameDirectionWorkspaceState());

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> restoredShells;
        coordinator->setWindowFactory([&application, &restoredShells](const auto &options) {
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto shellResult = zzCreateShell(created.value());
            if (!shellResult) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    shellResult.error());
            }
            auto shell = std::move(shellResult).value();
            created.value()->setCentralWidget(shell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), shell.get()};
            restoredShells.push_back(std::move(shell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        QByteArray unknownHistoricalWindow = saved.value();
        const QByteArray unknownWindowBytes = QUuid::createUuid().toRfc4122();
        QVERIFY(unknownWindowBytes.size() == 16);
        unknownHistoricalWindow.replace(
            originUuidOffsets.front(), 16, unknownWindowBytes);
        zzRefreshTopologyDigest(&unknownHistoricalWindow);
        QHash<QString, QWidget *> restoredPages;
        const auto restored = coordinator->restoreTopology(
            unknownHistoricalWindow, [&restoredPages](QStringView key) {
                auto page = std::make_unique<QWidget>();
                restoredPages.insert(key.toString(), page.get());
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(page));
            });
        QVERIFY(restored);
        QVERIFY(restoredPages.contains(QStringLiteral("moved")));

        application.beginShutdown();
    }

    void topologyRestoreRejectsResolverResultsWithoutLeakingObjects()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        window->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        const auto group = shell->splitWorkspace()->activeGroupId();
        auto *const page = new QWidget;
        auto *const secondPage = new QWidget;
        shell->splitWorkspace()->tabWidget(group)->addTab(
            page, QStringLiteral("resolver"));
        shell->splitWorkspace()->tabWidget(group)->addTab(
            secondPage, QStringLiteral("resolver-second"));
        QVERIFY(shell->splitWorkspace()->setPageLayoutKey(
            page, QStringLiteral("resolver/page")));
        QVERIFY(shell->splitWorkspace()->setPageLayoutKey(
            secondPage, QStringLiteral("resolver/second-page")));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        shell->splitWorkspace()->tabWidget(group)->removeTab(0);
        shell->splitWorkspace()->tabWidget(group)->removeTab(0);
        delete page;
        delete secondPage;
        QVERIFY(coordinator->unregisterWindow(window));
        const qsizetype windowCount = application.windowCount();
        const qsizetype widgetCount = QApplication::allWidgets().size();

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> stagedShells;
        int factoryCalls = 0;
        coordinator->setWindowFactory([&application, &stagedShells, &factoryCalls](
                                          const auto &options) {
            ++factoryCalls;
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto createdShell = zzCreateShell(created.value());
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    createdShell.error());
            }
            auto ownedShell = std::move(createdShell).value();
            created.value()->setCentralWidget(ownedShell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), ownedShell.get()};
            stagedShells.push_back(std::move(ownedShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });

        int resolverCalls = 0;
        const auto resolverFailure = coordinator->restoreTopology(
            saved.value(), [&resolverCalls](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                    ZzCore::ZzError(ZzCore::ZzErrorCode::Backend,
                        QStringLiteral("resolver failed")));
            });
        QVERIFY(!resolverFailure);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 1);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);

        factoryCalls = 0;
        resolverCalls = 0;
        const auto resolverEmpty = coordinator->restoreTopology(
            saved.value(), [&resolverCalls](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::unique_ptr<QWidget> {});
            });
        QVERIFY(!resolverEmpty);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 1);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);

        factoryCalls = 0;
        resolverCalls = 0;
        auto *const originalWorkspaceRoot = shell->workspaceWidget();
        QVERIFY(originalWorkspaceRoot != nullptr);
        bool parentedDestroyed = false;
        QObject::connect(originalWorkspaceRoot, &QObject::destroyed,
            window, [&parentedDestroyed] { parentedDestroyed = true; });
        const auto resolverParented = coordinator->restoreTopology(
            saved.value(), [originalWorkspaceRoot, &resolverCalls](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::unique_ptr<QWidget>(originalWorkspaceRoot));
            });
        QVERIFY(!resolverParented);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 1);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);
        QVERIFY(!parentedDestroyed);
        QCOMPARE(shell->workspaceWidget(), originalWorkspaceRoot);

        factoryCalls = 0;
        resolverCalls = 0;
        QWidget *firstResolvedPage = nullptr;
        const auto duplicateResolver = coordinator->restoreTopology(
            saved.value(), [&resolverCalls, &firstResolvedPage](QStringView) {
                ++resolverCalls;
                if (resolverCalls == 1) {
                    auto resolvedPage = std::make_unique<QWidget>();
                    firstResolvedPage = resolvedPage.get();
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::move(resolvedPage));
                }
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::unique_ptr<QWidget>(firstResolvedPage));
            });
        QVERIFY(!duplicateResolver);
        QCOMPARE(duplicateResolver.error().code(),
            ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 2);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);

        factoryCalls = 0;
        resolverCalls = 0;
        QThread foreignThread;
        QObject foreignContext;
        foreignContext.moveToThread(&foreignThread);
        foreignThread.start();
        std::atomic<bool> foreignDestroyed{false};
        std::atomic<QThread *> foreignDestroyedThread{nullptr};
        QPointer<QWidget> foreignPage;
        QVERIFY(QMetaObject::invokeMethod(
            &foreignContext,
            [&foreignPage, &foreignDestroyed, &foreignDestroyedThread] {
                auto *const resolvedPage = new ZzForeignResolverPage(
                    &foreignDestroyed, &foreignDestroyedThread);
                foreignPage = resolvedPage;
            },
            Qt::BlockingQueuedConnection));
        QVERIFY(!foreignPage.isNull());
        const auto foreignResolver = coordinator->restoreTopology(
            saved.value(), [&resolverCalls, &foreignPage](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::unique_ptr<QWidget>(foreignPage.data()));
            });
        QVERIFY(!foreignResolver);
        QCOMPARE(foreignResolver.error().code(),
            ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 1);
        QCOMPARE(application.windowCount(), windowCount);
        QTRY_VERIFY(foreignDestroyed.load());
        QVERIFY(foreignPage.isNull());
        QCOMPARE(foreignDestroyedThread.load(), &foreignThread);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);
        QVERIFY(QMetaObject::invokeMethod(
            &foreignContext,
            [&foreignContext] {
                foreignContext.moveToThread(QCoreApplication::instance()->thread());
            },
            Qt::BlockingQueuedConnection));
        foreignThread.quit();
        QVERIFY(foreignThread.wait(2000));

        application.beginShutdown();
    }

    void topologyRestoreCleansFactoryHandleRejectedByRegistration()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        window->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        QVERIFY(coordinator->unregisterWindow(window));
        const qsizetype windowCount = application.windowCount();
        const qsizetype widgetCount = QApplication::allWidgets().size();

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> stagedShells;
        int factoryCalls = 0;
        coordinator->setWindowFactory([&application, &stagedShells, &factoryCalls](
                                          const auto &options) {
            ++factoryCalls;
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto createdShell = zzCreateShell(created.value());
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    createdShell.error());
            }
            auto ownedShell = std::move(createdShell).value();
            delete ownedShell->splitWorkspace();
            created.value()->setCentralWidget(ownedShell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), ownedShell.get()};
            stagedShells.push_back(std::move(ownedShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        int resolverCalls = 0;
        const auto rejected = coordinator->restoreTopology(
            saved.value(), [&resolverCalls](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(!rejected);
        QCOMPARE(rejected.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 0);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);
        application.beginShutdown();
    }

    void topologyRestoreFallsBackToPrimaryAndConvergesGeometry()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        window->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        const auto group = shell->splitWorkspace()->activeGroupId();
        auto *const page = new QWidget;
        shell->splitWorkspace()->tabWidget(group)->addTab(
            page, QStringLiteral("screen"));
        QVERIFY(shell->splitWorkspace()->setPageLayoutKey(
            page, QStringLiteral("screen/page")));
        window->setGeometry(QRect(-100000, -100000, 8000, 8000));
        window->show();
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        QByteArray missingScreen = saved.value();
        zzPatchTopologyScreenName(&missingScreen,
            QStringLiteral("screen-that-does-not-exist"));
        shell->splitWorkspace()->tabWidget(group)->removeTab(0);
        delete page;
        QVERIFY(coordinator->unregisterWindow(window));
        const qsizetype windowCount = application.windowCount();

        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> restoredShells;
        coordinator->setWindowFactory([&application, &restoredShells](const auto &options) {
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto createdShell = zzCreateShell(created.value());
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    createdShell.error());
            }
            auto ownedShell = std::move(createdShell).value();
            created.value()->setCentralWidget(ownedShell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), ownedShell.get()};
            restoredShells.push_back(std::move(ownedShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        auto *restoredPage = static_cast<QWidget *>(nullptr);
        const auto restored = coordinator->restoreTopology(
            missingScreen, [&restoredPage](QStringView) {
                auto resolvedPage = std::make_unique<QWidget>();
                restoredPage = resolvedPage.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(resolvedPage));
            });
        QVERIFY(restored);
        QCOMPARE(application.windowCount(), windowCount + 1);
        QVERIFY(restoredPage != nullptr);
        QVERIFY(restoredShells.size() == 1);
        auto *const restoredWindow = restoredShells.front()
                                        ->workspaceWidget()->window();
        QVERIFY(restoredWindow != nullptr);
        if (restoredWindow == nullptr) return;
        QVERIFY(restoredWindow->isVisible());
        const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
        const QRect actual = restoredWindow->geometry();
        QVERIFY(available.contains(actual.topLeft()));
        QVERIFY(available.contains(actual.center()));
        QVERIFY(actual.width() >= 160);
        QVERIFY(actual.height() >= 150);
        application.beginShutdown();
    }

    void topologyRestoreValidatesMaximizedState()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        window->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, zzConfiguration(), true));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        QVERIFY(coordinator->unregisterWindow(window));
        QByteArray maximized = saved.value();
        zzPatchTopologyMaximized(&maximized, true);

        const qsizetype windowCount = application.windowCount();
        const qsizetype widgetCount = QApplication::allWidgets().size();
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> restoredShells;
        coordinator->setWindowFactory([&application, &restoredShells](const auto &options) {
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto restoredShellResult = zzCreateShell(created.value());
            if (!restoredShellResult) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    restoredShellResult.error());
            }
            auto ownedShell = std::move(restoredShellResult).value();
            created.value()->setCentralWidget(ownedShell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), ownedShell.get()};
            restoredShells.push_back(std::move(ownedShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        const auto restored = coordinator->restoreTopology(
            maximized, [](QStringView) {
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                    ZzCore::ZzError(ZzCore::ZzErrorCode::Backend,
                        QStringLiteral("resolver must not be called")));
            });
        if (restored) {
            QVERIFY(restoredShells.size() == 1);
            auto *const restoredWindow = restoredShells.front()
                                             ->workspaceWidget()->window();
            QVERIFY(restoredWindow != nullptr);
            if (restoredWindow == nullptr) return;
            QVERIFY(restoredWindow->isMaximized());
        } else {
            QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::Unsupported);
            QCOMPARE(application.windowCount(), windowCount);
            QCOMPARE(QApplication::allWidgets().size(), widgetCount);
        }
        application.beginShutdown();
    }

    void topologyRestoreRejectsMinimumSizeThatCannotFitScreen()
    {
        auto &application = zzApplication();
        QVERIFY(zzBuildApplication(application));
        auto *window = zzOnlyWindow(application);
        QVERIFY(window != nullptr);
        auto shellResult = zzCreateShell(window);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        window->setCentralWidget(shell->workspaceWidget());
        auto *coordinator = application.workspaceWindowCoordinator();
        ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
        configuration.minimumSize = QSize(10000, 10000);
        configuration.maximumSize = QSize(10000, 10000);
        configuration.initialGeometry = QRect(-100000, -100000, 10000, 10000);
        QVERIFY(coordinator->registerWindow(
            {window, shell.get()}, configuration, true));
        const auto saved = coordinator->saveTopology();
        QVERIFY(saved);
        QVERIFY(coordinator->unregisterWindow(window));
        const qsizetype windowCount = application.windowCount();
        const qsizetype widgetCount = QApplication::allWidgets().size();
        int factoryCalls = 0;
        int resolverCalls = 0;
        std::vector<std::unique_ptr<ZzPureTools::ZzWorkspaceShell>> stagedShells;
        coordinator->setWindowFactory([&application, &stagedShells, &factoryCalls](const auto &options) {
            ++factoryCalls;
            auto created = application.createWindow(options.visibility);
            if (!created) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    created.error());
            }
            auto createdShell = zzCreateShell(created.value());
            if (!createdShell) {
                return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                    createdShell.error());
            }
            auto ownedShell = std::move(createdShell).value();
            created.value()->setCentralWidget(ownedShell->workspaceWidget());
            ZzPureTools::ZzWorkspaceWindowHandle handle{
                created.value(), ownedShell.get()};
            stagedShells.push_back(std::move(ownedShell));
            return ZzCore::ZzResult<ZzPureTools::ZzWorkspaceWindowHandle>::success(
                handle);
        });
        const auto rejected = coordinator->restoreTopology(
            saved.value(), [&resolverCalls](QStringView) {
                ++resolverCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        QVERIFY(!rejected);
        QCOMPARE(factoryCalls, 1);
        QCOMPARE(resolverCalls, 0);
        QCOMPARE(application.windowCount(), windowCount);
        QCOMPARE(QApplication::allWidgets().size(), widgetCount);
        application.beginShutdown();
    }
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return 1;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzWorkspaceWindowCoordinatorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzWorkspaceWindowCoordinatorTest.moc"
