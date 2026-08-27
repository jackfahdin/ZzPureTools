#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QAbstractAnimation>
#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLayout>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QVBoxLayout>

#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzError.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSidePaneMode.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspaceActivityId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

namespace {

constexpr qsizetype zzWorkspaceMaximumLayoutSize = qsizetype{1024} * 1024;

struct ZzTestSideLayoutEntry final
{
    ZzTestSideLayoutEntry(
        QString entryId,
        ZzFluentUI::ZzActivityArea entryArea,
        qint32 entryOrder)
        : id(std::move(entryId))
        , area(static_cast<quint8>(entryArea))
        , order(entryOrder)
    {
    }

    ZzTestSideLayoutEntry(
        QString entryId,
        quint8 encodedArea,
        qint32 entryOrder)
        : id(std::move(entryId))
        , area(encodedArea)
        , order(entryOrder)
    {
    }

    QString id;
    quint8 area = static_cast<quint8>(
        ZzFluentUI::ZzActivityArea::LeftPrimary);
    qint32 order = 0;
};

struct ZzTestVersionTwoLayout final
{
    QByteArray qtState;
    quint8 leftCollapsed = 0;
    qint32 leftWidth = 280;
    QString leftCurrent;
    QStringList leftVisible;
    QList<qint32> leftSizes;
    quint8 rightCollapsed = 0;
    qint32 rightWidth = 280;
    QString rightCurrent;
    QStringList rightVisible;
    QList<qint32> rightSizes;
    QVector<ZzTestSideLayoutEntry> sideEntries;
    QByteArray splitState;
    quint8 bottomCollapsed = 1;
    qint32 bottomHeight = 240;
    QString bottomCurrent;
    quint8 titleMode = 0;
};

/** @brief 在组件接管对象后释放测试夹具持有的临时所有权。 */
template <typename T>
void zzReleaseAfterAdoption(std::unique_ptr<T> &object) noexcept
{
    [[maybe_unused]] T *const adoptedObject = object.release();
}

struct ZzShellFixture final
{
    QMainWindow host;
    ZzFluentUI::ZzFluentTitleBar titleBar{&host};
    std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;

    ZzShellFixture()
    {
        auto result = ZzPureTools::ZzWorkspaceShell::create(
            &host, &titleBar);
        Q_ASSERT(result);
        shell = std::move(result).value();
    }
};

class ZzShowEventFilter final : public QObject
{
public:
    std::function<void()> shown;
    std::function<void()> hidden;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event != nullptr
            && (event->type() == QEvent::Show
                || event->type() == QEvent::ShowToParent)
            && shown) {
            shown();
        }
        if (event != nullptr
            && (event->type() == QEvent::Hide
                || event->type() == QEvent::HideToParent)
            && hidden) {
            hidden();
        }
        return QObject::eventFilter(watched, event);
    }
};

class ZzDeferredVisibilityAuditWidget final : public QWidget
{
public:
    explicit ZzDeferredVisibilityAuditWidget(
        ZzFluentUI::ZzPanelStack *stack)
        : stack_(stack)
        , stackDescendantsBefore_(stack != nullptr
              ? stack->findChildren<QWidget *>().size() : -1)
    {
    }

    bool hiddenBeforeAdoption = false;
    bool hiddenAfterAdoptionStarted = false;

    void setVisible(bool visible) override
    {
        if (!visible && parentWidget() == nullptr) {
            const qsizetype descendants = stack_ != nullptr
                ? stack_->findChildren<QWidget *>().size() : -1;
            hiddenBeforeAdoption = descendants == stackDescendantsBefore_;
            hiddenAfterAdoptionStarted = descendants > stackDescendantsBefore_;
        }
        QWidget::setVisible(visible);
    }

private:
    QPointer<ZzFluentUI::ZzPanelStack> stack_;
    qsizetype stackDescendantsBefore_ = -1;
};

class ZzHideResistantWidget final : public QWidget
{
public:
    void setVisible(bool visible) override
    {
        if (!visible) {
            return;
        }
        QWidget::setVisible(true);
    }
};

class ZzReparentOnHideWidget final : public QWidget
{
public:
    explicit ZzReparentOnHideWidget(QWidget *targetParent)
        : targetParent_(targetParent)
    {
    }

    void setVisible(bool visible) override
    {
        if (!visible && !reparented_ && parentWidget() == nullptr
            && targetParent_ != nullptr) {
            reparented_ = true;
            setParent(targetParent_);
        }
        QWidget::setVisible(visible);
    }

private:
    QPointer<QWidget> targetParent_;
    bool reparented_ = false;
};

/** @brief 首次被移出捕获 frame 时同步重新挂回，用于覆盖 ParentChange 重入。 */
class ZzReattachingOwner final : public QWidget
{
public:
    ZzReattachingOwner(QWidget *frame, bool *reattached)
        : QWidget(frame)
        , frame_(frame)
        , reattached_(reattached)
    {
    }

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event != nullptr && event->type() == QEvent::ParentChange
            && parentWidget() == nullptr && frame_ != nullptr
            && reattached_ != nullptr && !*reattached_) {
            *reattached_ = true;
            setParent(frame_);
        }
        return handled;
    }

private:
    QPointer<QWidget> frame_;
    bool *reattached_ = nullptr;
};

/** @brief 首次离开捕获 frame 时排队回挂，用于覆盖 DeferredDelete 前重入。 */
class ZzQueuedReattachingOwner final : public QWidget
{
public:
    ZzQueuedReattachingOwner(
        QWidget *frame,
        QObject *reattachContext,
        bool *reattachQueued)
        : QWidget(frame)
        , frame_(frame)
        , reattachContext_(reattachContext)
        , reattachQueued_(reattachQueued)
    {
    }

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event == nullptr || event->type() != QEvent::ParentChange
            || parentWidget() == frame_ || frame_ == nullptr
            || reattachContext_ == nullptr || reattachQueued_ == nullptr
            || *reattachQueued_) {
            return handled;
        }

        const QPointer<QWidget> ownerGuard(this);
        const QPointer<QWidget> frameGuard(frame_);
        *reattachQueued_ = QMetaObject::invokeMethod(
            reattachContext_,
            [ownerGuard, frameGuard] {
                if (ownerGuard != nullptr && frameGuard != nullptr) {
                    ownerGuard->setParent(frameGuard);
                }
            },
            Qt::QueuedConnection);
        return handled;
    }

private:
    QPointer<QWidget> frame_;
    QPointer<QObject> reattachContext_;
    bool *reattachQueued_ = nullptr;
};

[[nodiscard]] ZzPureTools::ZzWorkspacePanelId zzPanelId(
    const char *value)
{
    return ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(value));
}

[[nodiscard]] ZzPureTools::ZzWorkspaceActivityId zzActivityId(
    const char *value)
{
    return ZzPureTools::ZzWorkspaceActivityId(QString::fromLatin1(value));
}

[[nodiscard]] ZzFluentUI::ZzIconDescriptor zzActivityIcon()
{
    return ZzFluentUI::ZzIconDescriptor::fromFontIcon(
        ZzFluentUI::ZzFontIcon::Gear);
}

[[nodiscard]] ZzFluentUI::ZzIconDescriptor zzIcon()
{
    return {};
}

/** @brief 按显示标题查找 Activity 源模型中的稳定测试索引。 */
[[nodiscard]] QModelIndex zzActivityIndex(
    QAbstractItemModel *model,
    const QString &title)
{
    if (model == nullptr) {
        return {};
    }
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(Qt::DisplayRole).toString() == title) {
            return index;
        }
    }
    return {};
}

[[nodiscard]] QByteArray zzTestWorkspaceEnvelope(
    quint16 schemaVersion,
    const QByteArray &payload)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    if (stream.writeRawData("ZZWS", 4) != 4) {
        return {};
    }
    stream << schemaVersion
           << static_cast<quint16>(QDataStream::Qt_6_8)
           << static_cast<quint32>(payload.size());
    if (stream.writeRawData(payload.constData(), payload.size())
            != payload.size()
        || stream.status() != QDataStream::Ok) {
        return {};
    }
    result.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return result;
}

[[nodiscard]] QByteArray zzVersionOneLayout(
    const QByteArray &qtState,
    bool leftCollapsed,
    qint32 leftWidth,
    bool rightCollapsed,
    qint32 rightWidth,
    const QString &leftCurrent,
    const QString &rightCurrent,
    const QVector<ZzTestSideLayoutEntry> &sideEntries,
    qint32 currentTabIndex,
    ZzPureTools::ZzWorkspaceTitleMode titleMode)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    stream << qtState << leftCollapsed << leftWidth
           << rightCollapsed << rightWidth
           << leftCurrent << rightCurrent
           << static_cast<quint32>(sideEntries.size());
    for (const ZzTestSideLayoutEntry &entry : sideEntries) {
        stream << entry.id << entry.area << entry.order;
    }
    stream << currentTabIndex << static_cast<quint8>(titleMode);
    return stream.status() == QDataStream::Ok
        ? zzTestWorkspaceEnvelope(1, payload) : QByteArray{};
}

void zzWriteTestWorkspaceString(QDataStream &stream, const QString &value)
{
    stream << static_cast<quint16>(value.size());
    for (const QChar character : value) {
        stream << character.unicode();
    }
}

void zzWriteTestNestedSplitNode(
    QDataStream &stream,
    int depth,
    int maximumDepth,
    int *nextGroup)
{
    if (depth >= maximumDepth) {
        stream << quint8(0);
        zzWriteTestWorkspaceString(
            stream, QStringLiteral("group-%1").arg((*nextGroup)++));
        return;
    }
    const auto orientation = depth % 2 == 0
        ? Qt::Vertical : Qt::Horizontal;
    stream << quint8(1) << static_cast<quint8>(orientation) << quint16(2);
    zzWriteTestNestedSplitNode(
        stream, depth + 1, maximumDepth, nextGroup);
    stream << quint8(0);
    zzWriteTestWorkspaceString(
        stream, QStringLiteral("group-%1").arg((*nextGroup)++));
    stream << quint16(2) << qint32(1) << qint32(1);
}

[[nodiscard]] QByteArray zzTestSplitLayout(
    int groupCount,
    int maximumDepth = 0)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    int nextGroup = 0;
    if (maximumDepth > 0) {
        zzWriteTestNestedSplitNode(stream, 1, maximumDepth, &nextGroup);
    } else if (groupCount <= 1) {
        stream << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        nextGroup = 1;
    } else {
        stream << quint8(1) << quint8(Qt::Horizontal)
               << static_cast<quint16>(groupCount);
        for (int index = 0; index < groupCount; ++index) {
            stream << quint8(0);
            zzWriteTestWorkspaceString(
                stream, QStringLiteral("group-%1").arg(index));
        }
        stream << static_cast<quint16>(groupCount);
        for (int index = 0; index < groupCount; ++index) {
            stream << qint32(1);
        }
        nextGroup = groupCount;
    }
    Q_ASSERT(nextGroup > 0);
    zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
    stream << quint16(0);

    QByteArray result;
    QDataStream envelope(&result, QIODevice::WriteOnly);
    envelope.setVersion(QDataStream::Qt_6_8);
    envelope.writeRawData("ZZSW", 4);
    envelope << quint16(1)
             << static_cast<quint16>(QDataStream::Qt_6_8)
             << static_cast<quint32>(payload.size());
    envelope.writeRawData(payload.constData(), payload.size());
    result.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return result;
}

[[nodiscard]] QByteArray zzMalformedTestSplitLayout(const QString &mutation)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    if (mutation == QStringLiteral("adjacent-orientation")) {
        stream << quint8(1) << quint8(Qt::Horizontal) << quint16(2)
               << quint8(1) << quint8(Qt::Horizontal) << quint16(2)
               << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        stream << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-1"));
        stream << quint16(2) << qint32(1) << qint32(1)
               << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-2"));
        stream << quint16(2) << qint32(1) << qint32(1);
    } else if (mutation == QStringLiteral("trimmed-group")) {
        stream << quint8(1) << quint8(Qt::Horizontal) << quint16(2)
               << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral("group"));
        stream << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral(" group "));
        stream << quint16(2) << qint32(1) << qint32(1);
    } else {
        stream << quint8(0);
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
    }
    zzWriteTestWorkspaceString(
        stream,
        mutation == QStringLiteral("trimmed-group")
            ? QStringLiteral("group") : QStringLiteral("group-0"));
    if (mutation == QStringLiteral("empty-key")) {
        stream << quint16(1);
        zzWriteTestWorkspaceString(stream, QStringLiteral("   "));
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        stream << qint32(0) << quint8(1);
    } else if (mutation == QStringLiteral("trimmed-key")) {
        stream << quint16(2);
        zzWriteTestWorkspaceString(stream, QStringLiteral("key"));
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        stream << qint32(0) << quint8(1);
        zzWriteTestWorkspaceString(stream, QStringLiteral(" key "));
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        stream << qint32(1) << quint8(0);
    } else if (mutation == QStringLiteral("duplicate-order")) {
        stream << quint16(2);
        zzWriteTestWorkspaceString(stream, QStringLiteral("first"));
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        stream << qint32(0) << quint8(1);
        zzWriteTestWorkspaceString(stream, QStringLiteral("second"));
        zzWriteTestWorkspaceString(stream, QStringLiteral("group-0"));
        stream << qint32(0) << quint8(0);
    } else {
        stream << quint16(0);
    }

    QByteArray result;
    QDataStream envelope(&result, QIODevice::WriteOnly);
    envelope.setVersion(QDataStream::Qt_6_8);
    envelope.writeRawData("ZZSW", 4);
    envelope << quint16(1)
             << static_cast<quint16>(QDataStream::Qt_6_8)
             << static_cast<quint32>(payload.size());
    envelope.writeRawData(payload.constData(), payload.size());
    result.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return result;
}

[[nodiscard]] QByteArray zzVersionTwoLayout(
    const ZzTestVersionTwoLayout &layout)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    stream << layout.qtState << layout.leftCollapsed << layout.leftWidth
           << layout.leftCurrent
           << static_cast<quint32>(layout.leftVisible.size());
    for (const QString &id : layout.leftVisible) {
        stream << id;
    }
    stream << static_cast<quint32>(layout.leftSizes.size());
    for (const qint32 size : layout.leftSizes) {
        stream << size;
    }
    stream << layout.rightCollapsed << layout.rightWidth
           << layout.rightCurrent
           << static_cast<quint32>(layout.rightVisible.size());
    for (const QString &id : layout.rightVisible) {
        stream << id;
    }
    stream << static_cast<quint32>(layout.rightSizes.size());
    for (const qint32 size : layout.rightSizes) {
        stream << size;
    }
    stream << static_cast<quint32>(layout.sideEntries.size());
    for (const ZzTestSideLayoutEntry &entry : layout.sideEntries) {
        stream << entry.id << entry.area << entry.order;
    }
    stream << layout.splitState << layout.bottomCollapsed
           << layout.bottomHeight << layout.bottomCurrent << layout.titleMode;
    return stream.status() == QDataStream::Ok
        ? zzTestWorkspaceEnvelope(2, payload) : QByteArray{};
}

[[nodiscard]] QByteArray zzReplaceQtState(
    const QByteArray &layout,
    const QByteArray &replacement)
{
    QByteArray headerPayload;
    QDataStream outer(layout);
    outer.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schemaVersion = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (outer.readRawData(magic, 4) != 4) {
        return {};
    }
    outer >> schemaVersion >> streamVersion >> payloadLength;
    headerPayload.resize(static_cast<qsizetype>(payloadLength));
    if (outer.readRawData(
            headerPayload.data(), static_cast<int>(payloadLength))
        != static_cast<int>(payloadLength)) {
        return {};
    }

    QByteArray originalQtState;
    QDataStream payloadIn(headerPayload);
    payloadIn.setVersion(QDataStream::Qt_6_8);
    payloadIn >> originalQtState;
    const qsizetype qtStateFieldSize = payloadIn.device()->pos();
    QByteArray newPayload;
    QDataStream payloadOut(&newPayload, QIODevice::WriteOnly);
    payloadOut.setVersion(QDataStream::Qt_6_8);
    payloadOut << replacement;
    newPayload.append(headerPayload.sliced(qtStateFieldSize));

    QByteArray result;
    QDataStream resultOut(&result, QIODevice::WriteOnly);
    resultOut.setVersion(QDataStream::Qt_6_8);
    if (resultOut.writeRawData("ZZWS", 4) != 4) {
        return {};
    }
    resultOut << schemaVersion << streamVersion
              << static_cast<quint32>(newPayload.size());
    if (resultOut.writeRawData(newPayload.constData(), newPayload.size())
        != newPayload.size()) {
        return {};
    }
    result.append(QCryptographicHash::hash(
        newPayload, QCryptographicHash::Sha256));
    return result;
}

[[nodiscard]] QByteArray zzLayoutWithSideEntries(
    const QByteArray &layout,
    quint32 sideCount,
    const QString &tailDuplicateId = {})
{
    QDataStream outer(layout);
    outer.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schemaVersion = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (outer.readRawData(magic, 4) != 4) {
        return {};
    }
    outer >> schemaVersion >> streamVersion >> payloadLength;
    QByteArray payload(static_cast<qsizetype>(payloadLength), Qt::Uninitialized);
    if (outer.readRawData(payload.data(), static_cast<int>(payloadLength))
        != static_cast<int>(payloadLength)) {
        return {};
    }

    ZzTestVersionTwoLayout decoded;
    quint32 originalSideCount = 0;
    quint32 visibleCount = 0;
    quint32 sizeCount = 0;
    QDataStream payloadIn(payload);
    payloadIn.setVersion(QDataStream::Qt_6_8);
    payloadIn >> decoded.qtState >> decoded.leftCollapsed >> decoded.leftWidth
              >> decoded.leftCurrent >> visibleCount;
    for (quint32 index = 0; index < visibleCount; ++index) {
        QString id;
        payloadIn >> id;
        decoded.leftVisible.append(std::move(id));
    }
    payloadIn >> sizeCount;
    for (quint32 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        payloadIn >> size;
        decoded.leftSizes.append(size);
    }
    payloadIn >> decoded.rightCollapsed >> decoded.rightWidth
              >> decoded.rightCurrent >> visibleCount;
    for (quint32 index = 0; index < visibleCount; ++index) {
        QString id;
        payloadIn >> id;
        decoded.rightVisible.append(std::move(id));
    }
    payloadIn >> sizeCount;
    for (quint32 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        payloadIn >> size;
        decoded.rightSizes.append(size);
    }
    payloadIn >> originalSideCount;
    for (quint32 index = 0; index < originalSideCount; ++index) {
        QString id;
        quint8 area = 0;
        qint32 order = 0;
        payloadIn >> id >> area >> order;
    }
    payloadIn >> decoded.splitState >> decoded.bottomCollapsed
              >> decoded.bottomHeight >> decoded.bottomCurrent
              >> decoded.titleMode;
    if (payloadIn.status() != QDataStream::Ok || !payloadIn.atEnd()) {
        return {};
    }

    decoded.sideEntries.clear();
    decoded.sideEntries.reserve(static_cast<qsizetype>(sideCount));
    for (quint32 index = 0; index < sideCount; ++index) {
        const QString id = index + 1 == sideCount && !tailDuplicateId.isEmpty()
            ? tailDuplicateId
            : QStringLiteral("side-%1").arg(index, 4, 10, QLatin1Char('0'));
        decoded.sideEntries.append({
            id, ZzFluentUI::ZzActivityArea::LeftPrimary,
            static_cast<qint32>(index)});
    }
    return schemaVersion == 2
            && streamVersion == static_cast<quint16>(QDataStream::Qt_6_8)
        ? zzVersionTwoLayout(decoded) : QByteArray{};
}

[[nodiscard]] QByteArray zzMutatedByte(
    QByteArray state,
    qsizetype offset,
    char value)
{
    Q_ASSERT(offset >= 0 && offset < state.size());
    state[offset] = value;
    return state;
}

class ZzParentChangeWidget final : public QWidget
{
public:
    std::function<void()> parentChanged;

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event != nullptr && event->type() == QEvent::ParentChange
            && parentWidget() != nullptr && parentChanged) {
            parentChanged();
        }
        return handled;
    }
};

class ZzParentRemovedWidget final : public QWidget
{
public:
    std::function<void()> parentRemoved;

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event != nullptr && event->type() == QEvent::ParentChange
            && parentWidget() == nullptr && parentRemoved) {
            parentRemoved();
        }
        return handled;
    }
};

/** @brief 在销毁 QObject 子对象前模拟 QMainWindow 已释放内部布局的阶段。 */
class ZzLayoutTornDownMainWindow final : public QMainWindow
{
public:
    ~ZzLayoutTornDownMainWindow() override
    {
        delete layout();
    }
};

} // namespace

/** @brief 验证 Workspace Shell 的装配、所有权、标题和布局事务。 */
class ZzWorkspaceShellTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void validatesFactoryInputsAndThreadsBeforeAllocation()
    {
        auto nullHost = ZzPureTools::ZzWorkspaceShell::create(nullptr);
        QVERIFY(!nullHost);
        QCOMPARE(nullHost.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        QMainWindow outer;
        QMainWindow nested(&outer);
        auto nestedHost = ZzPureTools::ZzWorkspaceShell::create(&nested);
        QVERIFY(!nestedHost);
        QCOMPARE(
            nestedHost.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        QMainWindow host;
        QMainWindow otherHost;
        ZzFluentUI::ZzFluentTitleBar foreignTitleBar(&otherHost);
        auto foreignTitle = ZzPureTools::ZzWorkspaceShell::create(
            &host, &foreignTitleBar);
        QVERIFY(!foreignTitle);
        QCOMPARE(
            foreignTitle.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        bool crossThreadRejected = false;
        ZzCore::ZzErrorCode crossThreadCode = ZzCore::ZzErrorCode::None;
        std::thread worker([&] {
            auto result = ZzPureTools::ZzWorkspaceShell::create(&host);
            crossThreadRejected = !result;
            if (!result) {
                crossThreadCode = result.error().code();
            }
        });
        worker.join();
        QVERIFY(crossThreadRejected);
        QCOMPARE(crossThreadCode, ZzCore::ZzErrorCode::InvalidState);
    }

    void createsWorkspaceWithoutReplacingTheHostCentralWidget()
    {
        QMainWindow host;
        QWidget existing;
        host.setCentralWidget(&existing);
        auto result = ZzPureTools::ZzWorkspaceShell::create(&host);
        QVERIFY(result);
        auto shell = std::move(result).value();

        QCOMPARE(host.centralWidget(), &existing);
        QVERIFY(shell->workspaceWidget() != nullptr);
        QCOMPARE(shell->workspaceWidget()->parentWidget(), &host);
        QVERIFY(shell->tabWidget() != nullptr);
        QVERIFY(shell->commandPalette() != nullptr);
        QVERIFY(shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left) != nullptr);
        QVERIFY(shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right) != nullptr);

        host.takeCentralWidget();
    }

    void createsSplitWorkspaceAndBottomPane()
    {
        ZzShellFixture fixture;
        auto *const splitWorkspace = fixture.shell->splitWorkspace();
        auto *const bottomPane = fixture.shell->bottomPane();

        QVERIFY(splitWorkspace != nullptr);
        QVERIFY(bottomPane != nullptr);
        QCOMPARE(
            fixture.shell->tabWidget(),
            splitWorkspace->tabWidget(splitWorkspace->activeGroupId()));
        QCOMPARE(splitWorkspace->parentWidget(), bottomPane->parentWidget());
        QWidget *const centerHost = splitWorkspace->parentWidget();
        QVERIFY(centerHost != nullptr);
        QCOMPARE(centerHost->parentWidget(), fixture.shell->workspaceWidget());
        auto *const centerLayout = qobject_cast<QVBoxLayout *>(
            centerHost->layout());
        QVERIFY(centerLayout != nullptr);
        QCOMPARE(centerLayout->count(), 2);
        QCOMPARE(centerLayout->itemAt(0)->widget(), splitWorkspace);
        QCOMPARE(centerLayout->itemAt(1)->widget(), bottomPane);
        QCOMPARE(centerLayout->stretch(0), 1);
        QCOMPARE(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)->mode(),
            ZzFluentUI::ZzSidePaneMode::Single);
        QCOMPARE(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right)->mode(),
            ZzFluentUI::ZzSidePaneMode::Single);
    }

    void sideUsesSingleModeAndShowsOnlyCurrentPanel()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QWidget *const secondRaw = second.get();
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);

        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("single-first"), QStringLiteral("Single first"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("single-second"), QStringLiteral("Single second"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary, second.get()));
        zzReleaseAfterAdoption(second);

        QCOMPARE(pane->mode(), ZzFluentUI::ZzSidePaneMode::Single);
        QVERIFY(!bar->isMultiActiveEnabled());
        QCOMPARE(
            pane->panelStack()->panels(),
            QList<QWidget *>({firstRaw, secondRaw}));
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({secondRaw}));
        QCOMPARE(pane->currentWidget(), secondRaw);
        const QModelIndex current = zzActivityIndex(
            bar->model(), QStringLiteral("Single second"));
        QVERIFY(current.isValid());
        QCOMPARE(bar->currentSourceIndex(), current);
        QCOMPARE(bar->activeSourceIndexes(), QList<QModelIndex>({current}));
    }

    void reactivatingCurrentPanelCollapsesButKeepsCurrentIndicator()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("collapse-current"), QStringLiteral("Collapse current"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        const QModelIndex current = zzActivityIndex(
            bar->model(), QStringLiteral("Collapse current"));

        Q_EMIT bar->collapseRequested(current);

        QVERIFY(pane->isCollapsed());
        QVERIFY(pane->isHidden());
        QCOMPARE(pane->width(), 0);
        QCOMPARE(pane->currentWidget(), contentRaw);
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({contentRaw}));
        QCOMPARE(bar->currentSourceIndex(), current);
        QCOMPARE(bar->activeSourceIndexes(), QList<QModelIndex>({current}));

        Q_EMIT bar->activationRequested(current);
        QVERIFY(!pane->isCollapsed());
        QVERIFY(!pane->isHidden());
        QCOMPARE(pane->currentWidget(), contentRaw);
        QCOMPARE(bar->currentSourceIndex(), current);
    }

    void switchingPanelHidesPreviousWithoutDestroyingEitherContent()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QPointer<QWidget> firstGuard(first.get());
        QPointer<QWidget> secondGuard(second.get());
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("switch-first"), QStringLiteral("Switch first"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("switch-second"), QStringLiteral("Switch second"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        const QModelIndex firstIndex = zzActivityIndex(
            bar->model(), QStringLiteral("Switch first"));

        Q_EMIT bar->activationRequested(firstIndex);

        QVERIFY(firstGuard != nullptr);
        QVERIFY(secondGuard != nullptr);
        QCOMPARE(pane->pageCount(), 2);
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({firstGuard.data()}));
        QCOMPARE(pane->currentWidget(), firstGuard.data());
        QCOMPARE(bar->currentSourceIndex(), firstIndex);
        QCOMPARE(
            bar->activeSourceIndexes(), QList<QModelIndex>({firstIndex}));
    }

    void movingCurrentAcrossSidesTransfersExpandedCurrent()
    {
        ZzShellFixture fixture;
        auto leftFallback = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        auto rightCurrent = std::make_unique<QWidget>();
        QWidget *const leftFallbackRaw = leftFallback.get();
        QWidget *const movedRaw = moved.get();
        QWidget *const rightCurrentRaw = rightCurrent.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("move-fallback"), QStringLiteral("Move fallback"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
            leftFallback.get()));
        zzReleaseAfterAdoption(leftFallback);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("move-current"), QStringLiteral("Move current"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("move-target-current"),
            QStringLiteral("Move target current"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, rightCurrent.get()));
        zzReleaseAfterAdoption(rightCurrent);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        const QModelIndex movedIndex = zzActivityIndex(
            leftBar->model(), QStringLiteral("Move current"));

        Q_EMIT leftBar->moveRequested(
            movedIndex, ZzFluentUI::ZzActivityArea::RightSecondary, 0);

        QCOMPARE(leftPane->currentWidget(), leftFallbackRaw);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({leftFallbackRaw}));
        QVERIFY(!leftPane->isCollapsed());
        QCOMPARE(rightPane->currentWidget(), movedRaw);
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QVERIFY(!rightPane->isCollapsed());
        QVERIFY(rightCurrentRaw != nullptr);
        QVERIFY(rightPane->isAncestorOf(rightCurrentRaw));
        const QModelIndex movedAfter = zzActivityIndex(
            rightBar->model(), QStringLiteral("Move current"));
        const QModelIndex fallbackAfter = zzActivityIndex(
            leftBar->model(), QStringLiteral("Move fallback"));
        QCOMPARE(rightBar->currentSourceIndex(), movedAfter);
        QCOMPARE(
            rightBar->activeSourceIndexes(), QList<QModelIndex>({movedAfter}));
        QCOMPARE(leftBar->currentSourceIndex(), fallbackAfter);
        QCOMPARE(
            leftBar->activeSourceIndexes(), QList<QModelIndex>({fallbackAfter}));
    }

    void movingLastRightEntryRemovesBarPaneWidthAndHitTarget()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right-only"), QStringLiteral("Right only"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, content.get()));
        zzReleaseAfterAdoption(content);
        QWidget *const root = fixture.shell->workspaceWidget();
        fixture.host.setCentralWidget(root);
        fixture.host.resize(1000, 640);
        fixture.host.show();
        QCoreApplication::processEvents();
        auto *const center = fixture.shell->splitWorkspace()->parentWidget();
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        const QPoint rightEdge(root->width() - 1, root->height() / 2);
        QWidget *const hitBefore = root->childAt(rightEdge);
        QVERIFY(hitBefore == rightBar || rightBar->isAncestorOf(hitBefore));
        QVERIFY(center->geometry().right() < root->rect().right());
        const QModelIndex onlyIndex = zzActivityIndex(
            rightBar->model(), QStringLiteral("Right only"));

        Q_EMIT rightBar->moveRequested(
            onlyIndex, ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
        QCoreApplication::processEvents();

        QVERIFY(rightBar->isHidden());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightPane->isHidden());
        QCOMPARE(rightPane->width(), 0);
        QCOMPARE(center->geometry().right(), root->rect().right());
        QWidget *const hitAfter = root->childAt(rightEdge);
        QVERIFY(hitAfter != rightBar);
        QVERIFY(hitAfter == nullptr || !rightBar->isAncestorOf(hitAfter));
        QVERIFY(hitAfter != rightPane);
        QVERIFY(hitAfter == nullptr || !rightPane->isAncestorOf(hitAfter));
    }

    void movingEntryBackRestoresOnlyRightBarUntilActivation()
    {
        ZzShellFixture fixture;
        auto current = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        QWidget *const currentRaw = current.get();
        QWidget *const movedRaw = moved.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("stay-left"), QStringLiteral("Stay left"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, current.get()));
        zzReleaseAfterAdoption(current);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("move-right-collapsed"),
            QStringLiteral("Move right collapsed"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->showPanel(zzPanelId("stay-left"), true));
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        const QModelIndex movedIndex = zzActivityIndex(
            leftBar->model(), QStringLiteral("Move right collapsed"));

        Q_EMIT leftBar->moveRequested(
            movedIndex, ZzFluentUI::ZzActivityArea::RightPrimary, 0);

        QCOMPARE(leftPane->currentWidget(), currentRaw);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({currentRaw}));
        QVERIFY(!rightBar->isHidden());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightPane->isHidden());
        QCOMPARE(rightPane->width(), 0);
        QVERIFY(rightPane->visibleWidgets().isEmpty());
        QVERIFY(!rightBar->currentSourceIndex().isValid());
        QVERIFY(rightBar->activeSourceIndexes().isEmpty());
        QVERIFY(rightPane->isAncestorOf(movedRaw));

        const QModelIndex movedAfter = zzActivityIndex(
            rightBar->model(), QStringLiteral("Move right collapsed"));
        Q_EMIT rightBar->activationRequested(movedAfter);
        QVERIFY(!rightPane->isCollapsed());
        QVERIFY(!rightPane->isHidden());
        QCOMPARE(rightPane->currentWidget(), movedRaw);
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QCOMPARE(rightBar->currentSourceIndex(), movedAfter);
    }

    void fixedLeftActionKeepsBarButNeverKeepsEmptyPaneWidth()
    {
        ZzShellFixture fixture;
        QAction settingsAction(QStringLiteral("Settings"));
        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("fixed-settings"), QStringLiteral("Fixed settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction));
        auto content = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("fixed-temporary"), QStringLiteral("Fixed temporary"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);

        auto taken = fixture.shell->takePanel(zzPanelId("fixed-temporary"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());

        QVERIFY(!bar->isHidden());
        QCOMPARE(bar->model()->rowCount(), 1);
        QVERIFY(!bar->currentSourceIndex().isValid());
        QVERIFY(bar->activeSourceIndexes().isEmpty());
        QVERIFY(pane->isCollapsed());
        QVERIFY(pane->isHidden());
        QCOMPARE(pane->width(), 0);
        QVERIFY(pane->visibleWidgets().isEmpty());
    }

    void failedMoveRestoresCurrentExpandedOwnershipAndEdgeVisibility()
    {
        ZzShellFixture fixture;
        auto fallback = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        QWidget *const fallbackRaw = fallback.get();
        QWidget *const movedRaw = moved.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("rollback-fallback"),
            QStringLiteral("Rollback fallback"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, fallback.get()));
        zzReleaseAfterAdoption(fallback);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("rollback-moved"), QStringLiteral("Rollback moved"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QAbstractItemModel *const model = leftBar->model();
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QVERIFY(rightPane->visibleWidgets().isEmpty());
        QVERIFY(rightPane->isHidden());
        QVERIFY(rightBar->isHidden());
        QPointer<QWidget> movedOwner(movedRaw->parentWidget());
        QPointer<QWidget> fallbackOwner(fallbackRaw->parentWidget());
        bool callbackEntered = false;
        ZzShowEventFilter finalShowFilter;
        finalShowFilter.shown = [&] {
                if (!callbackEntered) {
                    callbackEntered = true;
                    rightBar->setCurrentSourceIndex({});
                }
            };
        rightPane->installEventFilter(&finalShowFilter);
        const QModelIndex movedIndex = zzActivityIndex(
            model, QStringLiteral("Rollback moved"));

        Q_EMIT leftBar->moveRequested(
            movedIndex, ZzFluentUI::ZzActivityArea::RightSecondary, 0);

        QVERIFY(callbackEntered);
        QVERIFY(movedOwner.isNull());
        QVERIFY(!fallbackOwner.isNull());
        QVERIFY(movedRaw->parentWidget() != nullptr);
        QCOMPARE(fallbackRaw->parentWidget(), fallbackOwner.data());
        QVERIFY(leftPane->panelStack()->isAncestorOf(movedRaw));
        QVERIFY(leftPane->panelStack()->isAncestorOf(fallbackRaw));
        QCOMPARE(leftPane->currentWidget(), movedRaw);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QVERIFY(!leftPane->isCollapsed());
        QVERIFY(rightPane->currentWidget() == nullptr);
        QVERIFY(rightPane->visibleWidgets().isEmpty());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightPane->isHidden());
        QVERIFY(!leftBar->isHidden());
        QVERIFY(rightBar->isHidden());
        const QModelIndex movedAfter = zzActivityIndex(
            model, QStringLiteral("Rollback moved"));
        QCOMPARE(
            movedAfter.data(static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);
        QCOMPARE(leftBar->currentSourceIndex(), movedAfter);
        QVERIFY(!rightBar->currentSourceIndex().isValid());
    }

    void activityMoveRollbackRestoresFinalBarStateAfterReentry()
    {
        ZzShellFixture fixture;
        auto fallback = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("rollback-fallback"),
            QStringLiteral("Rollback fallback"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, fallback.get()));
        zzReleaseAfterAdoption(fallback);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("rollback-moved"), QStringLiteral("Rollback moved"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        bool targetCallbackEntered = false;
        ZzShowEventFilter targetShowFilter;
        targetShowFilter.shown = [&] {
            if (!targetCallbackEntered) {
                targetCallbackEntered = true;
                rightBar->setCurrentSourceIndex({});
            }
        };
        bool rollbackCallbackEntered = false;
        targetShowFilter.hidden = [&] {
            if (targetCallbackEntered && !rollbackCallbackEntered) {
                rollbackCallbackEntered = true;
                leftBar->setCurrentSourceIndex({});
            }
        };
        rightPane->installEventFilter(&targetShowFilter);

        Q_EMIT leftBar->moveRequested(
            zzActivityIndex(leftBar->model(), QStringLiteral("Rollback moved")),
            ZzFluentUI::ZzActivityArea::RightPrimary, 0);

        QVERIFY(targetCallbackEntered);
        QVERIFY(rollbackCallbackEntered);
        const QModelIndex movedAfter = zzActivityIndex(
            leftBar->model(), QStringLiteral("Rollback moved"));
        QCOMPARE(leftPane->currentWidget(), movedRaw);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QCOMPARE(leftBar->currentSourceIndex(), movedAfter);
        QCOMPARE(leftBar->activeSourceIndexes(), QList<QModelIndex>({movedAfter}));
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightPane->isHidden());
        QVERIFY(rightBar->isHidden());
    }

    void activityMoveRollbackRestoresMovedRegistrationAfterReentry()
    {
        ZzShellFixture fixture;
        auto fallback = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("rollback-fallback"),
            QStringLiteral("Rollback fallback"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, fallback.get()));
        zzReleaseAfterAdoption(fallback);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("rollback-moved"), QStringLiteral("Rollback moved"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        bool targetCallbackEntered = false;
        bool rollbackCallbackEntered = false;
        ZzShowEventFilter targetShowFilter;
        targetShowFilter.shown = [&] {
            if (!targetCallbackEntered) {
                targetCallbackEntered = true;
                rightBar->setCurrentSourceIndex({});
            }
        };
        rightPane->installEventFilter(&targetShowFilter);
        QObject::connect(
            leftPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (targetCallbackEntered && current == movedRaw
                    && !rollbackCallbackEntered) {
                    rollbackCallbackEntered = true;
                    movedRaw->setParent(nullptr);
                }
            });

        Q_EMIT leftBar->moveRequested(
            zzActivityIndex(leftBar->model(), QStringLiteral("Rollback moved")),
            ZzFluentUI::ZzActivityArea::RightPrimary, 0);

        QVERIFY(targetCallbackEntered);
        QVERIFY(rollbackCallbackEntered);
        QCOMPARE(leftPane->currentWidget(), movedRaw);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QVERIFY(leftPane->isAncestorOf(movedRaw));
        QVERIFY(leftPane->panelStack()->isAncestorOf(movedRaw));
        const QModelIndex movedAfter = zzActivityIndex(
            leftBar->model(), QStringLiteral("Rollback moved"));
        QVERIFY(movedAfter.isValid());
        QCOMPARE(leftBar->currentSourceIndex(), movedAfter);
    }

    void sideShowPanelRollsBackSynchronousVisibilityMutation()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QWidget *const secondRaw = second.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        bool callbackEntered = false;
        QObject::connect(
            bar, &ZzFluentUI::ZzActivityBar::currentSourceIndexChanged,
            fixture.shell.get(), [&](const QModelIndex &current) {
                if (current == zzActivityIndex(
                            bar->model(), QStringLiteral("First"))
                    && !callbackEntered) {
                    callbackEntered = true;
                    QVERIFY(pane->setWidgetVisible(firstRaw, false));
                }
            });

        const auto shown = fixture.shell->showPanel(zzPanelId("first"), true);

        QVERIFY(callbackEntered);
        QVERIFY(!shown);
        QCOMPARE(shown.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(pane->currentWidget(), secondRaw);
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({secondRaw}));
        QVERIFY(!pane->isCollapsed());
        const QModelIndex secondIndex = zzActivityIndex(
            bar->model(), QStringLiteral("Second"));
        QCOMPARE(bar->currentSourceIndex(), secondIndex);
        QCOMPARE(bar->activeSourceIndexes(), QList<QModelIndex>({secondIndex}));
    }

    void sideHidePanelRollsBackSynchronousBarMutation()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        bool callbackEntered = false;
        ZzShowEventFilter hideFilter;
        hideFilter.hidden = [&] {
            if (!callbackEntered) {
                callbackEntered = true;
                bar->setCurrentSourceIndex({});
            }
        };
        pane->installEventFilter(&hideFilter);

        const auto hidden = fixture.shell->showPanel(zzPanelId("side"), false);

        QVERIFY(callbackEntered);
        QVERIFY(!hidden);
        QCOMPARE(hidden.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(pane->currentWidget(), contentRaw);
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({contentRaw}));
        QVERIFY(!pane->isCollapsed());
        QVERIFY(!pane->isHidden());
        const QModelIndex sideIndex = zzActivityIndex(
            bar->model(), QStringLiteral("Side"));
        QCOMPARE(bar->currentSourceIndex(), sideIndex);
        QCOMPARE(bar->activeSourceIndexes(), QList<QModelIndex>({sideIndex}));
    }

    void sideShowPanelStopsWhenCurrentWidgetSignalDestroysShell()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);

        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            &fixture.host, [&](QWidget *current) {
                if (callbackEntered || current != firstRaw) {
                    return;
                }
                callbackEntered = true;
                fixture.shell.reset();
            });

        const auto activated = fixture.shell->showPanel(
            zzPanelId("first"), true);

        QVERIFY(callbackEntered);
        QVERIFY(!activated);
        QCOMPARE(activated.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(fixture.shell == nullptr);
        QVERIFY(workspaceGuard.isNull());
    }

    void sideShowPanelStopsWhenCollapsedSignalDestroysShell()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::collapsedChanged,
            &fixture.host, [&](bool collapsed) {
                if (callbackEntered || !collapsed) {
                    return;
                }
                callbackEntered = true;
                fixture.shell.reset();
            });

        const auto collapsed = fixture.shell->showPanel(
            zzPanelId("side"), false);

        QVERIFY(callbackEntered);
        QVERIFY(!collapsed);
        QCOMPARE(collapsed.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(fixture.shell == nullptr);
        QVERIFY(workspaceGuard.isNull());
    }

    void sidePanelRegistrationStopsWhenCurrentWidgetSignalDestroysShell()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            &fixture.host, [&](QWidget *current) {
                if (callbackEntered || current != contentRaw) {
                    return;
                }
                callbackEntered = true;
                fixture.shell.reset();
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get());
        [[maybe_unused]] QWidget *const adoptedContent = content.release();

        QVERIFY(callbackEntered);
        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(fixture.shell == nullptr);
        QVERIFY(workspaceGuard.isNull());
    }

    void hidesEmptySideEdgesAndRestoresOnlyTheOccupiedEdge()
    {
        ZzShellFixture fixture;
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);

        QVERIFY(leftPane->isCollapsed());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(leftBar->isHidden());
        QVERIFY(rightBar->isHidden());

        auto content = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left"), QStringLiteral("Left"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(!leftPane->isCollapsed());
        QVERIFY(!leftBar->isHidden());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightBar->isHidden());

        auto taken = fixture.shell->takePanel(zzPanelId("left"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QVERIFY(leftPane->isCollapsed());
        QVERIFY(leftBar->isHidden());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightBar->isHidden());
    }

    void registersFixedActivityWithoutSelectionOrDragFlags()
    {
        ZzShellFixture fixture;
        QAction settingsAction(QStringLiteral("Settings"));
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);

        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
            &settingsAction));
        QCOMPARE(bar->model()->rowCount(), 1);
        QCOMPARE(
            bar->model()->flags(bar->model()->index(0, 0)),
            Qt::ItemFlags(Qt::ItemIsEnabled));
        QVERIFY(!bar->isHidden());
        QVERIFY(!bar->currentSourceIndex().isValid());

        auto panel = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("sessions"), QStringLiteral("Sessions"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, panel.get()));
        zzReleaseAfterAdoption(panel);

        QCOMPARE(bar->model()->rowCount(), 2);
        const QModelIndex sideIndex = bar->model()->index(0, 0);
        const QModelIndex actionIndex = bar->model()->index(1, 0);
        QCOMPARE(sideIndex.data(Qt::DisplayRole).toString(), QStringLiteral("Sessions"));
        QCOMPARE(actionIndex.data(Qt::DisplayRole).toString(), QStringLiteral("Settings"));
        QCOMPARE(
            sideIndex.flags(),
            Qt::ItemFlags(
                Qt::ItemIsEnabled | Qt::ItemIsSelectable
                | Qt::ItemIsDragEnabled));
        QCOMPARE(
            actionIndex.flags(), Qt::ItemFlags(Qt::ItemIsEnabled));
        QCOMPARE(bar->currentSourceIndex(), sideIndex);
    }

    void fixedActivityTracksActionEnabledAndTriggeredState()
    {
        ZzShellFixture fixture;
        QAction settingsAction(QStringLiteral("Settings"));
        QSignalSpy triggeredSpy(&settingsAction, &QAction::triggered);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction));
        const QModelIndex index = bar->model()->index(0, 0);

        settingsAction.setEnabled(false);
        QTRY_COMPARE(index.flags(), Qt::NoItemFlags);
        settingsAction.setEnabled(true);
        QTRY_COMPARE(index.flags(), Qt::ItemFlags(Qt::ItemIsEnabled));

        settingsAction.trigger();
        QCOMPARE(triggeredSpy.count(), 1);
        Q_EMIT bar->activationRequested(index);
        QCOMPARE(triggeredSpy.count(), 2);
        QVERIFY(!bar->currentSourceIndex().isValid());
    }

    void fixedActivityPublishesActionChangeReceivedDuringInsertion()
    {
        ZzShellFixture fixture;
        QAction settingsAction(QStringLiteral("Settings"));
        auto *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QSignalSpy changedSpy(model, &QAbstractItemModel::dataChanged);
        QObject::connect(
            model, &QAbstractItemModel::rowsInserted,
            &settingsAction,
            [&settingsAction](const QModelIndex &, int, int) {
                settingsAction.setEnabled(false);
            });

        const auto registered = fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction);

        QVERIFY(registered);
        QCOMPARE(model->flags(model->index(0, 0)), Qt::NoItemFlags);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(
            changedSpy.at(0).at(0).value<QModelIndex>(), model->index(0, 0));
    }

    void fixedActivityRegistrationStopsWhenAboutToInsertDestroysShell()
    {
        ZzShellFixture fixture;
        QAction settingsAction(QStringLiteral("Settings"));
        auto *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        QObject::connect(
            model, &QAbstractItemModel::rowsAboutToBeInserted,
            &settingsAction,
            [&fixture](const QModelIndex &, int, int) {
                fixture.shell.reset();
            });

        const auto registered = fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction);

        QVERIFY(!registered);
        QVERIFY(fixture.shell == nullptr);
        QVERIFY(workspaceGuard.isNull());
    }

    void fixedActivityRegistrationStopsWhenAboutToInsertDestroysHost()
    {
        auto host = std::make_unique<QMainWindow>();
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        QAction settingsAction(QStringLiteral("Settings"));
        auto *const model = shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QPointer<QAbstractItemModel> modelGuard(model);
        QObject::connect(
            model, &QAbstractItemModel::rowsAboutToBeInserted,
            &settingsAction,
            [&host](const QModelIndex &, int, int) {
                host.reset();
            });

        const auto registered = shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction);

        QVERIFY(!registered);
        QVERIFY(host == nullptr);
        QVERIFY(modelGuard != nullptr);
        QCOMPARE(modelGuard->rowCount(), 0);
        QCOMPARE(shell->workspaceWidget(), nullptr);
    }

    void fixedActivityRegistrationStopsWhenBarShowDestroysShell()
    {
        ZzShellFixture fixture;
        QAction settingsAction(QStringLiteral("Settings"));
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        ZzShowEventFilter filter;
        bool callbackEntered = false;
        filter.shown = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            fixture.shell.reset();
        };
        bar->installEventFilter(&filter);

        const auto registered = fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction);

        QVERIFY(callbackEntered);
        QVERIFY(!registered);
        QVERIFY(fixture.shell == nullptr);
        QTRY_VERIFY(workspaceGuard.isNull());
    }

    void fixedActivityRegistrationStopsWhenBarShowDestroysAction()
    {
        ZzShellFixture fixture;
        auto *settingsAction = new QAction(QStringLiteral("Settings"));
        QPointer<QAction> actionGuard(settingsAction);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        ZzShowEventFilter filter;
        bool callbackEntered = false;
        filter.shown = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            delete settingsAction;
        };
        bar->installEventFilter(&filter);

        const auto registered = fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            settingsAction);

        QVERIFY(callbackEntered);
        QVERIFY(!registered);
        QVERIFY(actionGuard.isNull());
        QCOMPARE(bar->model()->rowCount(), 0);
        QVERIFY(bar->isHidden());
    }

    void sameActionUpdatesEveryRegisteredFixedActivity()
    {
        ZzShellFixture fixture;
        QAction sharedAction(QStringLiteral("Shared"));
        auto *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &sharedAction));
        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("account"), QStringLiteral("Account"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::RightSecondary,
            &sharedAction));
        QSignalSpy changedSpy(model, &QAbstractItemModel::dataChanged);

        sharedAction.setEnabled(false);

        QCOMPARE(changedSpy.count(), 2);
        QList<int> changedRows;
        for (const QList<QVariant> &arguments : changedSpy) {
            changedRows.append(arguments.at(0).value<QModelIndex>().row());
        }
        std::sort(changedRows.begin(), changedRows.end());
        QCOMPARE(changedRows, QList<int>({0, 1}));
        QCOMPARE(model->flags(model->index(0, 0)), Qt::NoItemFlags);
        QCOMPARE(model->flags(model->index(1, 0)), Qt::NoItemFlags);
    }

    void destroyedFixedActionRemovesOnlyItsActivityRow()
    {
        ZzShellFixture fixture;
        auto panel = std::make_unique<QWidget>();
        QWidget *const panelIdentity = panel.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("sessions"), QStringLiteral("Sessions"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, panel.get()));
        zzReleaseAfterAdoption(panel);
        auto *const action = new QAction(QStringLiteral("Settings"));
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            action));
        const QPersistentModelIndex currentBefore(bar->currentSourceIndex());

        delete action;

        QTRY_COMPARE(bar->model()->rowCount(), 1);
        QCOMPARE(bar->model()->index(0, 0).data().toString(), QStringLiteral("Sessions"));
        QCOMPARE(bar->currentSourceIndex(), QModelIndex(currentBefore));
        QCOMPARE(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                ->currentWidget(),
            panelIdentity);
    }

    void rejectsDuplicateIdsAcrossPanelAndActionDomains()
    {
        ZzShellFixture actionFirst;
        QAction action(QStringLiteral("Shared"));
        QVERIFY(actionFirst.shell->registerFixedActivityAction(
            zzActivityId(" shared "), QStringLiteral("Shared"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &action));

        QWidget side;
        QWidget bottom;
        QWidget dock;
        QVERIFY(!actionFirst.shell->registerSidePanel(
            zzPanelId("shared"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, &side));
        QVERIFY(!actionFirst.shell->registerBottomPanel(
            zzPanelId("shared"), QStringLiteral("Bottom"), zzIcon(), &bottom));
        QVERIFY(!actionFirst.shell->registerDockPanel(
            zzPanelId("shared"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, &dock));
        QCOMPARE(side.parent(), nullptr);
        QCOMPARE(bottom.parent(), nullptr);
        QCOMPARE(dock.parent(), nullptr);

        ZzShellFixture panelFirst;
        auto content = std::make_unique<QWidget>();
        QVERIFY(panelFirst.shell->registerSidePanel(
            zzPanelId("shared"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);
        QAction duplicateAction(QStringLiteral("Duplicate"));
        const auto duplicate = panelFirst.shell->registerFixedActivityAction(
            zzActivityId(" shared "), QStringLiteral("Duplicate"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::RightSecondary,
            &duplicateAction);
        QVERIFY(!duplicate);
        QCOMPARE(duplicate.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
    }

    void rejectsForeignThreadFixedAction()
    {
        ZzShellFixture fixture;
        QThread foreignThread;
        auto *const action = new QAction(QStringLiteral("Settings"));
        action->moveToThread(&foreignThread);
        foreignThread.start();

        const auto registered = fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            action);

        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(
            fixture.shell->activityBar(ZzFluentUI::ZzSidePaneEdge::Left)
                ->model()->rowCount(),
            0);
        QVERIFY(QMetaObject::invokeMethod(
            action, [action] { delete action; }, Qt::BlockingQueuedConnection));
        foreignThread.quit();
        QVERIFY(foreignThread.wait());
    }

    void fixedActionNeverAppearsInSavedLayout()
    {
        ZzShellFixture fixture;
        const auto before = fixture.shell->saveLayout();
        QVERIFY(before);
        QAction settingsAction(QStringLiteral("Settings"));
        QVERIFY(fixture.shell->registerFixedActivityAction(
            zzActivityId("settings"), QStringLiteral("Settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction));

        const auto after = fixture.shell->saveLayout();

        QVERIFY(after);
        QCOMPARE(after.value(), before.value());
    }

    void keepsEagerSideRegistrationVisible()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);

        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("sessions"), QStringLiteral("Sessions"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        QCOMPARE(pane->panelStack()->panels(), QList<QWidget *>({contentRaw}));
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({contentRaw}));
        QCOMPARE(pane->currentWidget(), contentRaw);
        QVERIFY(!pane->isCollapsed());
        QCOMPARE(bar->model()->rowCount(), 1);
        const QModelIndex index = bar->model()->index(0, 0);
        QVERIFY(bar->activeSourceIndexes().contains(index));
        QCOMPARE(bar->currentSourceIndex(), index);
    }

    void registersDeferredSidePanelWithoutCreatingContent()
    {
        ZzShellFixture fixture;
        int calls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("sessions"), QStringLiteral("Sessions"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&calls] {
                ++calls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));

        QCOMPARE(calls, 0);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QCOMPARE(leftBar->model()->rowCount(), 1);
        QVERIFY(leftPane->panelStack()->panels().isEmpty());
        QVERIFY(leftPane->isCollapsed());
        QVERIFY(leftBar->currentSourceIndex().isValid() == false);
        QVERIFY(leftBar->activeSourceIndexes().isEmpty());
        QVERIFY(!leftBar->isHidden());
    }

    void materializesDeferredSidePanelOnlyOnce()
    {
        ZzShellFixture fixture;
        int calls = 0;
        QWidget *created = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("sessions"), QStringLiteral("Sessions"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&calls, &created] {
                ++calls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("deferredSessions"));
                created = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        QVERIFY(fixture.shell->showPanel(zzPanelId("sessions"), false));
        QCOMPARE(calls, 0);
        QVERIFY(fixture.shell->showPanel(zzPanelId("sessions"), true));
        QCOMPARE(calls, 1);
        QVERIFY(created != nullptr);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QCOMPARE(pane->panelStack()->panels(), QList<QWidget *>({created}));
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({created}));
        QCOMPARE(pane->currentWidget(), created);
        QVERIFY(!pane->isCollapsed());
        const QModelIndex index = bar->model()->index(0, 0);
        QCOMPARE(bar->currentSourceIndex(), index);
        QVERIFY(bar->activeSourceIndexes().contains(index));

        QVERIFY(fixture.shell->showPanel(zzPanelId("sessions"), false));
        QVERIFY(fixture.shell->showPanel(zzPanelId("sessions"), true));
        QVERIFY(fixture.shell->showPanel(zzPanelId("sessions"), true));
        QCOMPARE(calls, 1);
    }

    void deferredSidePanelMaterializationStopsWhenCurrentWidgetSignalDestroysShell()
    {
        ZzShellFixture fixture;
        QWidget *created = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("Deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&created] {
                auto content = std::make_unique<QWidget>();
                created = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            &fixture.host, [&](QWidget *current) {
                if (callbackEntered || current != created) {
                    return;
                }
                callbackEntered = true;
                fixture.shell.reset();
            });

        const auto materialized = fixture.shell->showPanel(
            zzPanelId("deferred"), true);

        QVERIFY(callbackEntered);
        QVERIFY(!materialized);
        QCOMPARE(materialized.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(fixture.shell == nullptr);
        QVERIFY(workspaceGuard.isNull());
    }

    void deferredSidePanelFactoryStopsWhenFactoryDestroysShell()
    {
        ZzShellFixture fixture;
        int calls = 0;
        int destroyed = 0;
        QPointer<QWidget> createdGuard;
        QPointer<QWidget> workspaceGuard(fixture.shell->workspaceWidget());
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("factory-destroys-shell"),
            QStringLiteral("Factory destroys shell"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&] {
                ++calls;
                auto content = std::make_unique<QWidget>();
                createdGuard = content.get();
                QObject::connect(content.get(), &QObject::destroyed,
                    &fixture.host, [&destroyed] { ++destroyed; });
                fixture.shell.reset();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        const auto materialized = fixture.shell->showPanel(
            zzPanelId("factory-destroys-shell"), true);

        QCOMPARE(calls, 1);
        QVERIFY(!materialized);
        QCOMPARE(materialized.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(fixture.shell == nullptr);
        QVERIFY(workspaceGuard.isNull());
        QVERIFY(createdGuard.isNull());
        QCOMPARE(destroyed, 1);
    }

    void activityActivationMaterializesDeferredSidePanel()
    {
        ZzShellFixture fixture;
        int calls = 0;
        QWidget *created = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("files"), QStringLiteral("Files"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&calls, &created] {
                ++calls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("deferredFiles"));
                created = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        const QModelIndex index = bar->model()->index(0, 0);

        Q_EMIT bar->activationRequested(index);
        QCOMPARE(calls, 1);
        QVERIFY(created != nullptr);
        QCOMPARE(pane->currentWidget(), created);
        QVERIFY(pane->visibleWidgets().contains(created));
        QVERIFY(bar->activeSourceIndexes().contains(index));
        Q_EMIT bar->activationRequested(index);
        QCOMPARE(calls, 1);
    }

    void deferredFactoryFailureIsAtomic_data()
    {
        QTest::addColumn<int>("failureKind");
        QTest::newRow("zz-error") << 0;
        QTest::newRow("success-null") << 1;
        QTest::newRow("parented") << 2;
        QTest::newRow("wrong-thread") << 3;
        QTest::newRow("standard-exception") << 4;
        QTest::newRow("unknown-exception") << 5;
    }

    void deferredFactoryFailureIsAtomic()
    {
        QFETCH(int, failureKind);
        ZzShellFixture fixture;
        QWidget foreignParent;
        QPointer<QWidget> parentedContent;
        QPointer<QWidget> wrongThreadContent;
        QThread workerThread;
        auto *const worker = new QObject;
        worker->moveToThread(&workerThread);
        QObject::connect(
            &workerThread, &QThread::finished,
            worker, &QObject::deleteLater);
        if (failureKind == 3) {
            workerThread.start();
            QVERIFY(QMetaObject::invokeMethod(
                worker,
                [&wrongThreadContent] {
                    wrongThreadContent = new QWidget;
                },
                Qt::BlockingQueuedConnection));
            QVERIFY(wrongThreadContent != nullptr);
        }

        int calls = 0;
        QWidget *created = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("Deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&]() -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
                ++calls;
                if (calls > 1) {
                    auto content = std::make_unique<QWidget>();
                    content->setObjectName(QStringLiteral("deferredRetry"));
                    created = content.get();
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::move(content));
                }
                switch (failureKind) {
                case 0:
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                        ZzCore::ZzError(
                            ZzCore::ZzErrorCode::Backend,
                            QStringLiteral("factory backend failure")));
                case 1:
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        nullptr);
                case 2: {
                    auto content = std::make_unique<QWidget>(&foreignParent);
                    parentedContent = content.get();
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::move(content));
                }
                case 3:
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::unique_ptr<QWidget>(wrongThreadContent.data()));
                case 4:
                    throw std::runtime_error("deferred standard failure");
                case 5:
                    throw 42;
                default:
                    Q_UNREACHABLE();
                }
            }));

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        const auto saveBefore = fixture.shell->saveLayout();
        QVERIFY(saveBefore);
        const QModelIndex leftCurrentBefore = leftBar->currentSourceIndex();
        const QModelIndex rightCurrentBefore = rightBar->currentSourceIndex();
        const QList<QModelIndex> leftActiveBefore =
            leftBar->activeSourceIndexes();
        const QList<QModelIndex> rightActiveBefore =
            rightBar->activeSourceIndexes();
        const QList<QWidget *> leftPanelsBefore =
            leftPane->panelStack()->panels();
        const QList<QWidget *> rightPanelsBefore =
            rightPane->panelStack()->panels();
        const QList<QWidget *> leftVisibleBefore = leftPane->visibleWidgets();
        const QList<QWidget *> rightVisibleBefore = rightPane->visibleWidgets();
        const QList<int> leftSizesBefore =
            leftPane->panelStack()->panelSizes();
        const QList<int> rightSizesBefore =
            rightPane->panelStack()->panelSizes();
        QWidget *const leftPaneCurrentBefore = leftPane->currentWidget();
        QWidget *const rightPaneCurrentBefore = rightPane->currentWidget();
        const bool leftCollapsedBefore = leftPane->isCollapsed();
        const bool rightCollapsedBefore = rightPane->isCollapsed();

        const auto failed = fixture.shell->showPanel(
            zzPanelId("deferred"), true);
        QVERIFY(!failed);
        QCOMPARE(
            failed.error().code(), failureKind == 0
                ? ZzCore::ZzErrorCode::Backend
                : ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(calls, 1);
        const auto saveAfter = fixture.shell->saveLayout();
        QVERIFY(saveAfter);
        QCOMPARE(saveAfter.value(), saveBefore.value());
        QCOMPARE(leftBar->currentSourceIndex(), leftCurrentBefore);
        QCOMPARE(rightBar->currentSourceIndex(), rightCurrentBefore);
        QCOMPARE(leftBar->activeSourceIndexes(), leftActiveBefore);
        QCOMPARE(rightBar->activeSourceIndexes(), rightActiveBefore);
        QCOMPARE(leftPane->panelStack()->panels(), leftPanelsBefore);
        QCOMPARE(rightPane->panelStack()->panels(), rightPanelsBefore);
        QCOMPARE(leftPane->visibleWidgets(), leftVisibleBefore);
        QCOMPARE(rightPane->visibleWidgets(), rightVisibleBefore);
        QCOMPARE(leftPane->panelStack()->panelSizes(), leftSizesBefore);
        QCOMPARE(rightPane->panelStack()->panelSizes(), rightSizesBefore);
        QCOMPARE(leftPane->currentWidget(), leftPaneCurrentBefore);
        QCOMPARE(rightPane->currentWidget(), rightPaneCurrentBefore);
        QCOMPARE(leftPane->isCollapsed(), leftCollapsedBefore);
        QCOMPARE(rightPane->isCollapsed(), rightCollapsedBefore);
        if (failureKind == 2) {
            QVERIFY(parentedContent != nullptr);
            QCOMPARE(parentedContent->parentWidget(), &foreignParent);
        }

        QVERIFY(fixture.shell->showPanel(zzPanelId("deferred"), true));
        QCOMPARE(calls, 2);
        QVERIFY(created != nullptr);
        QCOMPARE(leftPane->currentWidget(), created);
        QVERIFY(leftPane->visibleWidgets().contains(created));

        if (workerThread.isRunning()) {
            QTRY_VERIFY(wrongThreadContent == nullptr);
            workerThread.quit();
            QVERIFY(workerThread.wait());
        } else {
            delete worker;
        }
    }

    void rejectsReentrantDeferredMaterialization()
    {
        ZzShellFixture fixture;
        int calls = 0;
        bool nestedShowRejected = false;
        bool nestedTakeRejected = false;
        ZzCore::ZzErrorCode nestedShowCode = ZzCore::ZzErrorCode::None;
        ZzCore::ZzErrorCode nestedTakeCode = ZzCore::ZzErrorCode::None;
        QWidget *created = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("reentrant"), QStringLiteral("Reentrant"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&]() -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
                ++calls;
                const auto nestedShow = fixture.shell->showPanel(
                    zzPanelId("reentrant"), true);
                nestedShowRejected = !nestedShow;
                if (!nestedShow) {
                    nestedShowCode = nestedShow.error().code();
                }
                const auto nestedTake = fixture.shell->takePanel(
                    zzPanelId("reentrant"));
                nestedTakeRejected = !nestedTake;
                if (!nestedTake) {
                    nestedTakeCode = nestedTake.error().code();
                }
                auto content = std::make_unique<QWidget>();
                created = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        QVERIFY(fixture.shell->showPanel(zzPanelId("reentrant"), true));
        QCOMPARE(calls, 1);
        QVERIFY(nestedShowRejected);
        QVERIFY(nestedTakeRejected);
        QCOMPARE(nestedShowCode, ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(nestedTakeCode, ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->currentWidget(), created);
        QVERIFY(fixture.shell->showPanel(zzPanelId("reentrant"), true));
        QCOMPARE(calls, 1);
    }

    void takesPendingPanelWithoutShowingIt()
    {
        ZzShellFixture fixture;
        int calls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("pending-take"), QStringLiteral("Pending take"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary,
            [&calls] {
                ++calls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("pendingTakeContent"));
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QSignalSpy visibilityChanged(
            pane->panelStack(),
            &ZzFluentUI::ZzPanelStack::panelVisibilityChanged);
        QSignalSpy currentChanged(
            pane->panelStack(),
            &ZzFluentUI::ZzPanelStack::currentPanelChanged);

        auto taken = fixture.shell->takePanel(zzPanelId("pending-take"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QCOMPARE(calls, 1);
        QVERIFY(returned != nullptr);
        QCOMPARE(returned->objectName(), QStringLiteral("pendingTakeContent"));
        QCOMPARE(returned->parent(), nullptr);
        QVERIFY(!returned->isVisible());
        QCOMPARE(visibilityChanged.count(), 0);
        QCOMPARE(currentChanged.count(), 0);
        QVERIFY(pane->panelStack()->panels().isEmpty());
        QVERIFY(pane->isCollapsed());
        QCOMPARE(bar->model()->rowCount(), 0);
        QVERIFY(bar->isHidden());
    }

    void failedPendingTakePreservesRegistration()
    {
        ZzShellFixture fixture;
        int calls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("pending-retry"), QStringLiteral("Pending retry"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&calls]() -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
                ++calls;
                if (calls == 1) {
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                        ZzCore::ZzError(
                            ZzCore::ZzErrorCode::Backend,
                            QStringLiteral("pending take failed")));
                }
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);

        const auto failed = fixture.shell->takePanel(
            zzPanelId("pending-retry"));
        QVERIFY(!failed);
        QCOMPARE(failed.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(calls, 1);
        QCOMPARE(bar->model()->rowCount(), 1);
        QVERIFY(pane->panelStack()->panels().isEmpty());
        QVERIFY(!bar->isHidden());

        auto retried = fixture.shell->takePanel(zzPanelId("pending-retry"));
        QVERIFY(retried);
        std::unique_ptr<QWidget> returned(retried.value());
        QCOMPARE(calls, 2);
        QVERIFY(returned != nullptr);
        QCOMPARE(returned->parent(), nullptr);
        QCOMPARE(bar->model()->rowCount(), 0);
        QVERIFY(pane->panelStack()->panels().isEmpty());
        QVERIFY(bar->isHidden());
    }

    void updatesBadgeBeforeMaterialization()
    {
        ZzShellFixture fixture;
        int calls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("badged"), QStringLiteral("Badged"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&calls] {
                ++calls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        const QModelIndex index = bar->model()->index(0, 0);

        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("badged"), 7));
        QCOMPARE(index.data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Badge)).toInt(), 7);
        QCOMPARE(calls, 0);
        QVERIFY(fixture.shell->showPanel(zzPanelId("badged"), true));
        QCOMPARE(calls, 1);
        QCOMPARE(index.data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Badge)).toInt(), 7);
    }

    void ordersDeferredPanelsByLogicalActivityRows()
    {
        ZzShellFixture fixture;
        QWidget *pendingPrimary = nullptr;
        QWidget *pendingSecondary = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("pending-primary"), QStringLiteral("Pending primary"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&pendingPrimary] {
                auto content = std::make_unique<QWidget>();
                pendingPrimary = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        auto eagerPrimary = std::make_unique<QWidget>();
        QWidget *const eagerPrimaryRaw = eagerPrimary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("eager-primary"), QStringLiteral("Eager primary"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, eagerPrimary.get()));
        zzReleaseAfterAdoption(eagerPrimary);
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("pending-secondary"),
            QStringLiteral("Pending secondary"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&pendingSecondary] {
                auto content = std::make_unique<QWidget>();
                pendingSecondary = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        auto eagerSecondary = std::make_unique<QWidget>();
        QWidget *const eagerSecondaryRaw = eagerSecondary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("eager-secondary"), QStringLiteral("Eager secondary"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary, eagerSecondary.get()));
        zzReleaseAfterAdoption(eagerSecondary);

        const auto secondaryShown = fixture.shell->showPanel(
            zzPanelId("pending-secondary"), true);
        const QString secondaryError = secondaryShown
            ? QString() : secondaryShown.error().technicalMessage();
        QVERIFY2(secondaryShown,
            qPrintable(secondaryError));
        const auto primaryShown = fixture.shell->showPanel(
            zzPanelId("pending-primary"), true);
        const QString primaryError = primaryShown
            ? QString() : primaryShown.error().technicalMessage();
        QVERIFY2(primaryShown,
            qPrintable(primaryError));
        QCOMPARE(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->panelStack()->panels(),
            QList<QWidget *>({pendingPrimary, eagerPrimaryRaw,
                pendingSecondary, eagerSecondaryRaw}));
    }

    void movesPendingPanelWithoutMaterializingIt()
    {
        ZzShellFixture fixture;
        auto leftReady = std::make_unique<QWidget>();
        QWidget *const leftReadyRaw = leftReady.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-ready"), QStringLiteral("Left ready"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftReady.get()));
        zzReleaseAfterAdoption(leftReady);

        int calls = 0;
        QWidget *pendingContent = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("pending"), QStringLiteral("Pending"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&calls, &pendingContent] {
                ++calls;
                auto content = std::make_unique<QWidget>();
                pendingContent = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        auto rightReady = std::make_unique<QWidget>();
        QWidget *const rightReadyRaw = rightReady.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right-ready"), QStringLiteral("Right ready"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary, rightReady.get()));
        zzReleaseAfterAdoption(rightReady);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QCOMPARE(model->rowCount(), 3);

        Q_EMIT leftBar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::RightSecondary,
            0);

        QCOMPARE(calls, 0);
        QCOMPARE(model->index(1, 0).data().toString(),
            QStringLiteral("Pending"));
        QCOMPARE(model->index(1, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area))
            .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightSecondary);
        QCOMPARE(leftPane->panelStack()->panels(),
            QList<QWidget *>({leftReadyRaw}));
        QCOMPARE(rightPane->panelStack()->panels(),
            QList<QWidget *>({rightReadyRaw}));

        QVERIFY(fixture.shell->showPanel(zzPanelId("pending"), true));
        QCOMPARE(calls, 1);
        QVERIFY(pendingContent != nullptr);
        QCOMPARE(rightPane->panelStack()->panels(),
            QList<QWidget *>({rightReadyRaw, pendingContent}));
        QCOMPARE(leftPane->panelStack()->panels(),
            QList<QWidget *>({leftReadyRaw}));
    }

    void movesMixedPendingAndReadyPanelsConsistently()
    {
        ZzShellFixture fixture;
        auto leftReady = std::make_unique<QWidget>();
        leftReady->setObjectName(QStringLiteral("left-ready"));
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-ready"), QStringLiteral("left-ready"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftReady.get()));
        zzReleaseAfterAdoption(leftReady);

        int leftPendingCalls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("left-pending"), QStringLiteral("left-pending"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&leftPendingCalls] {
                ++leftPendingCalls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("left-pending"));
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        auto rightReady = std::make_unique<QWidget>();
        rightReady->setObjectName(QStringLiteral("right-ready"));
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right-ready"), QStringLiteral("right-ready"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary, rightReady.get()));
        zzReleaseAfterAdoption(rightReady);

        int rightPendingCalls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("right-pending"), QStringLiteral("right-pending"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&rightPendingCalls] {
                ++rightPendingCalls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("right-pending"));
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        const auto rowFor = [model](const QString &title) {
            for (int row = 0; row < model->rowCount(); ++row) {
                if (model->index(row, 0).data().toString() == title) {
                    return row;
                }
            }
            return -1;
        };
        const auto logicalIds = [model](bool left) {
            QStringList ids;
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex index = model->index(row, 0);
                const auto area = index.data(static_cast<int>(
                    ZzFluentUI::ZzActivityItemRole::Area))
                    .value<ZzFluentUI::ZzActivityArea>();
                const bool isLeft = area
                        == ZzFluentUI::ZzActivityArea::LeftPrimary
                    || area == ZzFluentUI::ZzActivityArea::LeftSecondary;
                if (isLeft == left) {
                    ids.append(index.data().toString());
                }
            }
            return ids;
        };
        const auto physicalIds = [](ZzFluentUI::ZzSidePane *pane) {
            QStringList ids;
            for (QWidget *const content : pane->panelStack()->panels()) {
                ids.append(content->objectName());
            }
            return ids;
        };

        Q_EMIT leftBar->moveRequested(
            model->index(rowFor(QStringLiteral("right-pending")), 0),
            ZzFluentUI::ZzActivityArea::RightPrimary, 0);
        QCOMPARE(rightPendingCalls, 0);
        QCOMPARE(logicalIds(false), QStringList({QStringLiteral("right-pending"),
            QStringLiteral("right-ready")}));
        QCOMPARE(physicalIds(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)),
            QStringList({QStringLiteral("right-ready")}));

        Q_EMIT leftBar->moveRequested(
            model->index(rowFor(QStringLiteral("right-ready")), 0),
            ZzFluentUI::ZzActivityArea::LeftSecondary, 0);
        QCOMPARE(logicalIds(true), QStringList({QStringLiteral("left-ready"),
            QStringLiteral("right-ready"), QStringLiteral("left-pending")}));
        QCOMPARE(logicalIds(false),
            QStringList({QStringLiteral("right-pending")}));
        QCOMPARE(physicalIds(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)),
            QStringList({QStringLiteral("left-ready"),
                QStringLiteral("right-ready")}));
        QVERIFY(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->panelStack()->panels().isEmpty());

        QVERIFY(fixture.shell->showPanel(zzPanelId("left-pending"), true));
        QCOMPARE(leftPendingCalls, 1);
        QCOMPARE(physicalIds(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)),
            QStringList({QStringLiteral("left-ready"),
                QStringLiteral("right-ready"),
                QStringLiteral("left-pending")}));

        auto taken = fixture.shell->takePanel(zzPanelId("right-pending"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QCOMPARE(rightPendingCalls, 1);
        QCOMPARE(returned->objectName(), QStringLiteral("right-pending"));
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(logicalIds(true), QStringList({QStringLiteral("left-ready"),
            QStringLiteral("right-ready"), QStringLiteral("left-pending")}));
        QCOMPARE(physicalIds(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)), logicalIds(true));
    }

    void savesPendingPanelsWithoutMaterializingThem()
    {
        ZzShellFixture source;
        int sourcePendingCalls = 0;
        auto sourceLeft = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("left-ready"), QStringLiteral("left-ready"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceLeft.get()));
        zzReleaseAfterAdoption(sourceLeft);
        QVERIFY(source.shell->registerSidePanelFactory(
            zzPanelId("pending-one"), QStringLiteral("pending-one"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&sourcePendingCalls] {
                ++sourcePendingCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));
        auto sourceRight = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("right-ready"), QStringLiteral("right-ready"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceRight.get()));
        zzReleaseAfterAdoption(sourceRight);
        QVERIFY(source.shell->registerSidePanelFactory(
            zzPanelId("pending-two"), QStringLiteral("pending-two"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&sourcePendingCalls] {
                ++sourcePendingCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));

        auto *const sourceBar = source.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const sourceModel = sourceBar->model();
        const auto sourceRowFor = [sourceModel](const QString &title) {
            for (int row = 0; row < sourceModel->rowCount(); ++row) {
                if (sourceModel->index(row, 0).data().toString() == title) {
                    return row;
                }
            }
            return -1;
        };
        Q_EMIT sourceBar->moveRequested(
            sourceModel->index(sourceRowFor(QStringLiteral("pending-one")), 0),
            ZzFluentUI::ZzActivityArea::RightSecondary, 0);
        Q_EMIT sourceBar->moveRequested(
            sourceModel->index(sourceRowFor(QStringLiteral("pending-two")), 0),
            ZzFluentUI::ZzActivityArea::LeftSecondary, 0);
        QCOMPARE(sourcePendingCalls, 0);

        const auto saved = source.shell->saveLayout();
        QVERIFY(saved);
        QCOMPARE(sourcePendingCalls, 0);

        ZzShellFixture target;
        int targetPendingCalls = 0;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("pending-two"), QStringLiteral("pending-two"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary,
            [&targetPendingCalls] {
                ++targetPendingCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));
        auto targetRight = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("right-ready"), QStringLiteral("right-ready"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary, targetRight.get()));
        zzReleaseAfterAdoption(targetRight);
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("pending-one"), QStringLiteral("pending-one"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&targetPendingCalls] {
                ++targetPendingCalls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));
        auto targetLeft = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("left-ready"), QStringLiteral("left-ready"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetLeft.get()));
        zzReleaseAfterAdoption(targetLeft);

        const auto restored = target.shell->restoreLayout(saved.value());
        const QString restoreError = restored
            ? QString{} : restored.error().technicalMessage();
        QVERIFY2(restored, qPrintable(restoreError));
        QCOMPARE(targetPendingCalls, 0);
        QAbstractItemModel *const targetModel = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(targetModel->rowCount(), sourceModel->rowCount());
        const auto idsForArea = [](QAbstractItemModel *model,
                                   ZzFluentUI::ZzActivityArea area) {
            QStringList ids;
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex index = model->index(row, 0);
                if (index.data(static_cast<int>(
                        ZzFluentUI::ZzActivityItemRole::Area))
                        .value<ZzFluentUI::ZzActivityArea>() == area) {
                    ids.append(index.data().toString());
                }
            }
            return ids;
        };
        for (const auto area : {
                 ZzFluentUI::ZzActivityArea::LeftPrimary,
                 ZzFluentUI::ZzActivityArea::LeftSecondary,
                 ZzFluentUI::ZzActivityArea::RightPrimary,
                 ZzFluentUI::ZzActivityArea::RightSecondary}) {
            QCOMPARE(idsForArea(targetModel, area),
                idsForArea(sourceModel, area));
        }
        const auto resaved = target.shell->saveLayout();
        QVERIFY(resaved);
        QCOMPARE(targetPendingCalls, 0);
        QCOMPARE(resaved.value(), saved.value());
    }

    void preservesAdditionalPendingPanelWhenRestoringOlderLayout()
    {
        ZzShellFixture source;
        auto sourceKnown = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("known"), QStringLiteral("known"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceKnown.get()));
        zzReleaseAfterAdoption(sourceKnown);
        const auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        auto targetKnown = std::make_unique<QWidget>();
        QWidget *const targetKnownRaw = targetKnown.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("known"), QStringLiteral("known"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary, targetKnown.get()));
        zzReleaseAfterAdoption(targetKnown);
        int additionalCalls = 0;
        QWidget *additionalContent = nullptr;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("additional"), QStringLiteral("additional"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&additionalCalls, &additionalContent] {
                ++additionalCalls;
                auto content = std::make_unique<QWidget>();
                additionalContent = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        const auto restored = target.shell->restoreLayout(saved.value());
        const QString restoreError = restored
            ? QString{} : restored.error().technicalMessage();
        QVERIFY2(restored, qPrintable(restoreError));
        QCOMPARE(additionalCalls, 0);
        auto *const bar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->index(0, 0).data().toString(),
            QStringLiteral("known"));
        QCOMPARE(model->index(0, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area))
            .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);
        QCOMPARE(model->index(1, 0).data().toString(),
            QStringLiteral("additional"));
        QCOMPARE(model->index(1, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area))
            .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightSecondary);
        QCOMPARE(target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->panelStack()->panels(),
            QList<QWidget *>({targetKnownRaw}));
        QVERIFY(target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)
                    ->panelStack()->panels().isEmpty());
        QVERIFY(target.shell->saveLayout());

        QVERIFY(target.shell->showPanel(zzPanelId("additional"), true));
        QCOMPARE(additionalCalls, 1);
        QVERIFY(additionalContent != nullptr);
        QCOMPARE(target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->panelStack()->panels(),
            QList<QWidget *>({additionalContent}));
    }

    void restoresOnlyDeferredPanelsRequestedVisible()
    {
        ZzShellFixture source;
        for (const auto &registration : {
                 std::pair{"files", ZzFluentUI::ZzActivityArea::LeftPrimary},
                 std::pair{"outline", ZzFluentUI::ZzActivityArea::LeftSecondary},
                 std::pair{"tasks", ZzFluentUI::ZzActivityArea::RightPrimary},
                 std::pair{"search", ZzFluentUI::ZzActivityArea::RightSecondary}}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(source.shell->registerSidePanel(
                zzPanelId(registration.first),
                QString::fromLatin1(registration.first), {},
                registration.second, content.get()));
            zzReleaseAfterAdoption(content);
        }
        QVERIFY(source.shell->showPanel(zzPanelId("outline"), false));
        QVERIFY(source.shell->showPanel(zzPanelId("search"), false));
        auto *const sourceLeft = source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const sourceRight = source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(sourceLeft->panelStack()->setPanelSizes({333}));
        QVERIFY(sourceRight->panelStack()->setPanelSizes({444}));
        const auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        std::array<int, 4> calls{};
        std::array<QWidget *, 4> created{};
        const std::array<const char *, 4> ids = {
            "files", "outline", "tasks", "search"};
        const std::array<ZzFluentUI::ZzActivityArea, 4> areas = {
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            ZzFluentUI::ZzActivityArea::RightPrimary,
            ZzFluentUI::ZzActivityArea::RightSecondary};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            QVERIFY(target.shell->registerSidePanelFactory(
                zzPanelId(ids.at(index)), QString::fromLatin1(ids.at(index)),
                {}, areas.at(index),
                [&, index] {
                    ++calls.at(index);
                    auto content = std::make_unique<QWidget>();
                    content->setObjectName(
                        QString::fromLatin1(ids.at(index)));
                    created.at(index) = content.get();
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::move(content));
                }));
        }

        const auto restored = target.shell->restoreLayout(saved.value());
        const QString restoreError = restored
            ? QString{} : restored.error().technicalMessage();
        QVERIFY2(restored, qPrintable(restoreError));
        const std::array<int, 4> expectedCalls = {0, 1, 0, 1};
        QCOMPARE(calls, expectedCalls);
        auto *const targetLeft = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const targetRight = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QCOMPARE(targetLeft->panelStack()->panels(),
            QList<QWidget *>({created.at(1)}));
        QCOMPARE(targetRight->panelStack()->panels(),
            QList<QWidget *>({created.at(3)}));
        QCOMPARE(targetLeft->visibleWidgets(),
            QList<QWidget *>({created.at(1)}));
        QCOMPARE(targetRight->visibleWidgets(),
            QList<QWidget *>({created.at(3)}));
        QCOMPARE(targetLeft->currentWidget(), created.at(1));
        QCOMPARE(targetRight->currentWidget(), created.at(3));
        QCOMPARE(targetLeft->panelStack()->panelSizes(),
            QList<int>({1}));
        QCOMPARE(targetRight->panelStack()->panelSizes(),
            QList<int>({1}));
        QCOMPARE(targetLeft->isCollapsed(), sourceLeft->isCollapsed());
        QCOMPARE(targetRight->isCollapsed(), sourceRight->isCollapsed());
    }

    void rollsBackNewlyMaterializedPanelsWhenRestoreFails()
    {
        ZzShellFixture source;
        auto sourceFirst = std::make_unique<QWidget>();
        auto sourceSecond = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("first"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceFirst.get()));
        zzReleaseAfterAdoption(sourceFirst);
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("second"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceSecond.get()));
        zzReleaseAfterAdoption(sourceSecond);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        int firstCalls = 0;
        int secondCalls = 0;
        QPointer<QWidget> firstCreated;
        QPointer<QWidget> secondCreated;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("first"), QStringLiteral("first"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [attempt = 0, &firstCalls, &firstCreated]() mutable {
                ++attempt;
                ++firstCalls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(
                    QStringLiteral("first-%1").arg(attempt));
                firstCreated = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("second"), QStringLiteral("second"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [attempt = 0, &secondCalls, &secondCreated]() mutable
                -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
                ++attempt;
                ++secondCalls;
                if (attempt == 1) {
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                        ZzCore::ZzError(
                            ZzCore::ZzErrorCode::Backend,
                            QStringLiteral("second factory failed")));
                }
                auto content = std::make_unique<QWidget>();
                content->setObjectName(
                    QStringLiteral("second-%1").arg(attempt));
                secondCreated = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        QVERIFY(target.shell->setPanelBadge(zzPanelId("first"), 7));

        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const bar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        const QList<QWidget *> leftPanelsBefore =
            leftPane->panelStack()->panels();
        const QList<QWidget *> rightPanelsBefore =
            rightPane->panelStack()->panels();
        const QList<QWidget *> leftVisibleBefore = leftPane->visibleWidgets();
        const QList<QWidget *> rightVisibleBefore = rightPane->visibleWidgets();
        const QList<int> leftSizesBefore = leftPane->panelStack()->panelSizes();
        const QList<int> rightSizesBefore = rightPane->panelStack()->panelSizes();
        QWidget *const leftCurrentBefore = leftPane->currentWidget();
        QWidget *const rightCurrentBefore = rightPane->currentWidget();
        const bool leftCollapsedBefore = leftPane->isCollapsed();
        const bool rightCollapsedBefore = rightPane->isCollapsed();
        const auto originalLayout = target.shell->saveLayout();
        QVERIFY(originalLayout);

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(!restored);
        QCOMPARE(firstCalls, 1);
        QCOMPARE(secondCalls, 1);
        QVERIFY(firstCreated == nullptr);
        QVERIFY(secondCreated == nullptr);
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->index(0, 0).data().toString(),
            QStringLiteral("first"));
        QCOMPARE(model->index(0, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area))
            .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftSecondary);
        QCOMPARE(model->index(0, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Badge)).toInt(), 7);
        QCOMPARE(model->index(1, 0).data().toString(),
            QStringLiteral("second"));
        QCOMPARE(model->index(1, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area))
            .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightSecondary);
        QCOMPARE(leftPane->panelStack()->panels(), leftPanelsBefore);
        QCOMPARE(rightPane->panelStack()->panels(), rightPanelsBefore);
        QCOMPARE(leftPane->visibleWidgets(), leftVisibleBefore);
        QCOMPARE(rightPane->visibleWidgets(), rightVisibleBefore);
        QCOMPARE(leftPane->panelStack()->panelSizes(), leftSizesBefore);
        QCOMPARE(rightPane->panelStack()->panelSizes(), rightSizesBefore);
        QCOMPARE(leftPane->currentWidget(), leftCurrentBefore);
        QCOMPARE(rightPane->currentWidget(), rightCurrentBefore);
        QCOMPARE(leftPane->isCollapsed(), leftCollapsedBefore);
        QCOMPARE(rightPane->isCollapsed(), rightCollapsedBefore);
        const auto layoutAfter = target.shell->saveLayout();
        QVERIFY(layoutAfter);
        QCOMPARE(layoutAfter.value(), originalLayout.value());

        QVERIFY(target.shell->showPanel(zzPanelId("first"), true));
        QCOMPARE(firstCalls, 2);
        QVERIFY(firstCreated != nullptr);
        QCOMPARE(firstCreated->objectName(), QStringLiteral("first-2"));
        QVERIFY(target.shell->showPanel(zzPanelId("second"), true));
        QCOMPARE(secondCalls, 2);
        QVERIFY(secondCreated != nullptr);
        QCOMPARE(secondCreated->objectName(), QStringLiteral("second-2"));
    }

    void rollsBackFirstFactorySideEffectsWhenRestoreFails()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto *const pane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        const int widthBefore = pane->paneWidth();
        const bool collapsedBefore = pane->isCollapsed();
        int calls = 0;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [pane, attempt = 0, &calls]() mutable
                -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
                ++attempt;
                ++calls;
                if (attempt == 1) {
                    pane->setPaneWidth(333);
                    pane->setCollapsed(!pane->isCollapsed());
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                        ZzCore::ZzError(
                            ZzCore::ZzErrorCode::Backend,
                            QStringLiteral("first factory failed")));
                }
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));
        const auto layoutBeforeRestore = target.shell->saveLayout();
        QVERIFY(layoutBeforeRestore);

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(calls, 1);
        QCOMPARE(pane->paneWidth(), widthBefore);
        QCOMPARE(pane->isCollapsed(), collapsedBefore);
        const auto layoutAfter = target.shell->saveLayout();
        QVERIFY(layoutAfter);
        QCOMPARE(layoutAfter.value(), layoutBeforeRestore.value());

        QVERIFY(target.shell->showPanel(zzPanelId("deferred"), true));
        QCOMPARE(calls, 2);
    }

    void hidesVisibleFactoryContentBeforeRestoreAdoption()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto *const stack = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->panelStack();
        int calls = 0;
        ZzDeferredVisibilityAuditWidget *created = nullptr;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [stack, &calls, &created] {
                ++calls;
                auto content =
                    std::make_unique<ZzDeferredVisibilityAuditWidget>(stack);
                content->show();
                created = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        const auto restored = target.shell->restoreLayout(requested.value());
        const QString restoreError = restored
            ? QString{} : restored.error().technicalMessage();
        QVERIFY2(restored, qPrintable(restoreError));
        QCOMPARE(calls, 1);
        QVERIFY(created != nullptr);
        QVERIFY(created->hiddenBeforeAdoption);
        QVERIFY(!created->hiddenAfterAdoptionStarted);
        QCOMPARE(target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->panelStack()->panels(),
            QList<QWidget *>({created}));
    }

    void rejectsFactoryContentThatRemainsVisibleAfterHide()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        int calls = 0;
        QPointer<QWidget> firstCreated;
        QWidget *retryContent = nullptr;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [attempt = 0, &calls, &firstCreated, &retryContent]() mutable {
                ++attempt;
                ++calls;
                if (attempt == 1) {
                    auto content = std::make_unique<ZzHideResistantWidget>();
                    content->show();
                    firstCreated = content.get();
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::move(content));
                }
                auto content = std::make_unique<QWidget>();
                retryContent = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        const auto layoutBefore = target.shell->saveLayout();
        QVERIFY(layoutBefore);

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(calls, 1);
        QVERIFY(firstCreated == nullptr);
        auto *const pane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QVERIFY(pane->panelStack()->panels().isEmpty());
        QVERIFY(pane->isCollapsed());
        const auto layoutAfter = target.shell->saveLayout();
        QVERIFY(layoutAfter);
        QCOMPARE(layoutAfter.value(), layoutBefore.value());

        QVERIFY(target.shell->showPanel(zzPanelId("deferred"), true));
        QCOMPARE(calls, 2);
        QVERIFY(retryContent != nullptr);
        QCOMPARE(pane->panelStack()->panels(),
            QList<QWidget *>({retryContent}));
    }

    void destroysFactoryContentReparentedDuringRestoreHide()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget reparentOwner(&target.host);
        int calls = 0;
        QPointer<QWidget> firstCreated;
        QWidget *retryContent = nullptr;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [attempt = 0, &calls, &firstCreated, &retryContent,
             &reparentOwner]() mutable {
                ++attempt;
                ++calls;
                if (attempt == 1) {
                    auto content = std::make_unique<ZzReparentOnHideWidget>(
                        &reparentOwner);
                    content->show();
                    firstCreated = content.get();
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                        std::move(content));
                }
                auto content = std::make_unique<QWidget>();
                retryContent = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        const auto layoutBefore = target.shell->saveLayout();
        QVERIFY(layoutBefore);

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(calls, 1);
        QVERIFY(firstCreated == nullptr);
        QVERIFY(reparentOwner.findChildren<QWidget *>(
            QString{}, Qt::FindDirectChildrenOnly).isEmpty());
        auto *const pane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QVERIFY(pane->panelStack()->panels().isEmpty());
        const auto layoutAfter = target.shell->saveLayout();
        QVERIFY(layoutAfter);
        QCOMPARE(layoutAfter.value(), layoutBefore.value());

        QVERIFY(target.shell->showPanel(zzPanelId("deferred"), true));
        QCOMPARE(calls, 2);
        QVERIFY(retryContent != nullptr);
        QCOMPARE(pane->panelStack()->panels(),
            QList<QWidget *>({retryContent}));
    }

    void rollsBackNewlyMaterializedPanelsWhenCommitFails()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(420);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        int calls = 0;
        QPointer<QWidget> created;
        QVERIFY(target.shell->registerSidePanelFactory(
            zzPanelId("deferred"), QStringLiteral("deferred"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [attempt = 0, &calls, &created]() mutable {
                ++attempt;
                ++calls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(
                    QStringLiteral("deferred-%1").arg(attempt));
                created = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        const auto originalLayout = target.shell->saveLayout();
        QVERIFY(originalLayout);
        auto *const pane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        const int widthBefore = pane->paneWidth();
        QSignalSpy widthSpy(
            pane, &ZzFluentUI::ZzSidePane::paneWidthChanged);
        QSignalSpy modelResetSpy(
            target.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->model(),
            &QAbstractItemModel::modelReset);
        bool polluted = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::paneWidthChanged,
            target.shell.get(), [pane, &polluted](int width) {
                if (!polluted && width == 420) {
                    polluted = true;
                    pane->setPaneWidth(222);
                }
            });

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(!restored);
        QVERIFY(polluted);
        QCOMPARE(calls, 1);
        QVERIFY(created == nullptr);
        QVERIFY(pane->panelStack()->panels().isEmpty());
        QVERIFY(pane->isCollapsed());
        QCOMPARE(widthSpy.count(), 3);
        QCOMPARE(widthSpy.at(0).at(0).toInt(), 420);
        QCOMPARE(widthSpy.at(1).at(0).toInt(), 222);
        QCOMPARE(widthSpy.at(2).at(0).toInt(), widthBefore);
        QCOMPARE(modelResetSpy.count(), 1);
        const auto afterFailure = target.shell->saveLayout();
        QVERIFY(afterFailure);
        QCOMPARE(afterFailure.value(), originalLayout.value());

        QVERIFY(target.shell->showPanel(zzPanelId("deferred"), true));
        QCOMPARE(calls, 2);
        QVERIFY(created != nullptr);
        QCOMPARE(created->objectName(), QStringLiteral("deferred-2"));
    }

    void auditsMixedPendingReadyLifecycle()
    {
        ZzShellFixture fixture;
        auto leftReady = std::make_unique<QWidget>();
        leftReady->setObjectName(QStringLiteral("left-ready"));
        QWidget *const leftReadyRaw = leftReady.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-ready"), QStringLiteral("left-ready"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftReady.get()));
        zzReleaseAfterAdoption(leftReady);
        int shownCalls = 0;
        QWidget *shownContent = nullptr;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("shown"), QStringLiteral("shown"), {},
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            [&shownCalls, &shownContent] {
                ++shownCalls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("shown"));
                shownContent = content.get();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));
        auto removedReady = std::make_unique<QWidget>();
        removedReady->setObjectName(QStringLiteral("removed-ready"));
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("removed-ready"), QStringLiteral("removed-ready"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary,
            removedReady.get()));
        zzReleaseAfterAdoption(removedReady);
        int pendingCalls = 0;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("pending"), QStringLiteral("pending"), {},
            ZzFluentUI::ZzActivityArea::RightSecondary,
            [&pendingCalls] {
                ++pendingCalls;
                auto content = std::make_unique<QWidget>();
                content->setObjectName(QStringLiteral("pending"));
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("shown"), 9));
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        const auto rowFor = [model](const QString &title) {
            for (int row = 0; row < model->rowCount(); ++row) {
                if (model->index(row, 0).data().toString() == title) {
                    return row;
                }
            }
            return -1;
        };
        Q_EMIT bar->moveRequested(
            model->index(rowFor(QStringLiteral("shown")), 0),
            ZzFluentUI::ZzActivityArea::RightSecondary, 0);
        QCOMPARE(shownCalls, 0);
        QVERIFY(fixture.shell->showPanel(zzPanelId("shown"), true));
        QCOMPARE(shownCalls, 1);
        auto removed = fixture.shell->takePanel(zzPanelId("removed-ready"));
        QVERIFY(removed);
        std::unique_ptr<QWidget> returned(removed.value());
        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        QCOMPARE(pendingCalls, 0);

        Q_EMIT bar->moveRequested(
            model->index(rowFor(QStringLiteral("pending")), 0),
            ZzFluentUI::ZzActivityArea::LeftSecondary, 0);
        QVERIFY(fixture.shell->restoreLayout(saved.value()));

        QStringList logicalIds;
        QSet<QString> logicalUnique;
        for (int row = 0; row < model->rowCount(); ++row) {
            const QString id = model->index(row, 0).data().toString();
            QVERIFY(!logicalUnique.contains(id));
            logicalUnique.insert(id);
            logicalIds.append(id);
        }
        QCOMPARE(logicalIds.size(), 3);
        QCOMPARE(logicalUnique, QSet<QString>({QStringLiteral("left-ready"),
            QStringLiteral("shown"), QStringLiteral("pending")}));
        QCOMPARE(model->index(rowFor(QStringLiteral("shown")), 0).data(
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Badge)).toInt(), 9);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QCOMPARE(leftPane->panelStack()->panels(),
            QList<QWidget *>({leftReadyRaw}));
        QCOMPARE(rightPane->panelStack()->panels(),
            QList<QWidget *>({shownContent}));
        QSet<QWidget *> physicalUnique;
        for (QWidget *const content : leftPane->panelStack()->panels()
                 + rightPane->panelStack()->panels()) {
            QVERIFY(content != nullptr);
            QVERIFY(!physicalUnique.contains(content));
            physicalUnique.insert(content);
        }
        QCOMPARE(physicalUnique.size(), 2);
        QCOMPARE(shownCalls, 1);
        QCOMPARE(pendingCalls, 0);
    }

    void preservesStatefulDeferredFactoryAcrossRetry()
    {
        ZzShellFixture fixture;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("stateful"), QStringLiteral("Stateful"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [attempt = 0]() mutable
                -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
                ++attempt;
                if (attempt == 1) {
                    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
                        ZzCore::ZzError(
                            ZzCore::ZzErrorCode::Backend,
                            QStringLiteral("retry")));
                }
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            }));

        QVERIFY(!fixture.shell->showPanel(zzPanelId("stateful"), true));
        QVERIFY(fixture.shell->showPanel(zzPanelId("stateful"), true));
    }

    void blocksMaterializationDuringDeferredRegistrationSignals()
    {
        ZzShellFixture fixture;
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        ZzShowEventFilter filter;
        bool callbackEntered = false;
        bool nestedShowRejected = false;
        bool nestedTakeRejected = false;
        int calls = 0;
        filter.shown = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            nestedShowRejected = !fixture.shell->showPanel(
                zzPanelId("registering"), true);
            nestedTakeRejected = !fixture.shell->takePanel(
                zzPanelId("registering"));
        };
        bar->installEventFilter(&filter);

        const auto registered = fixture.shell->registerSidePanelFactory(
            zzPanelId("registering"), QStringLiteral("Registering"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            [&calls] {
                ++calls;
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::make_unique<QWidget>());
            });
        bar->removeEventFilter(&filter);

        QVERIFY(registered);
        QVERIFY(callbackEntered);
        QVERIFY(nestedShowRejected);
        QVERIFY(nestedTakeRejected);
        QCOMPARE(calls, 0);
        QVERIFY(fixture.shell->showPanel(zzPanelId("registering"), true));
        QCOMPARE(calls, 1);
    }

    void hidesVisiblePendingPanelBeforeTakeReturns()
    {
        ZzShellFixture fixture;
        QVERIFY(fixture.shell->registerSidePanelFactory(
            zzPanelId("visible"), QStringLiteral("Visible"), {},
            ZzFluentUI::ZzActivityArea::RightPrimary,
            [] {
                auto content = std::make_unique<QWidget>();
                content->show();
                return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                    std::move(content));
            }));

        auto taken = fixture.shell->takePanel(zzPanelId("visible"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> content(taken.value());
        QVERIFY(!content->isVisible());
    }

    void rejectsRegistrationErrorsWithoutTakingContent()
    {
        ZzShellFixture fixture;
        const std::array invalidIds{
            ZzPureTools::ZzWorkspacePanelId(),
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("   "))};
        for (const auto &id : invalidIds) {
            QWidget content;
            auto result = fixture.shell->registerSidePanel(
                id, QStringLiteral("Explorer"), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, &content);
            QVERIFY(!result);
            QCOMPARE(content.parent(), nullptr);
        }

        QWidget emptyTitleContent;
        auto emptyTitle = fixture.shell->registerSidePanel(
            zzPanelId("empty-title"), QStringLiteral("  "), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, &emptyTitleContent);
        QVERIFY(!emptyTitle);
        QCOMPARE(emptyTitleContent.parent(), nullptr);

        auto nullContent = fixture.shell->registerDockPanel(
            zzPanelId("null"), QStringLiteral("Null"), zzIcon(),
            Qt::BottomDockWidgetArea, nullptr);
        QVERIFY(!nullContent);

        QWidget invalidAreaContent;
        auto invalidArea = fixture.shell->registerDockPanel(
            zzPanelId("invalid-area"), QStringLiteral("Invalid"), zzIcon(),
            Qt::NoDockWidgetArea, &invalidAreaContent);
        QVERIFY(!invalidArea);
        QCOMPARE(invalidAreaContent.parent(), nullptr);

        QWidget parent;
        QWidget parentedContent(&parent);
        auto parented = fixture.shell->registerDockPanel(
            zzPanelId("parented"), QStringLiteral("Parented"), zzIcon(),
            Qt::BottomDockWidgetArea, &parentedContent);
        QVERIFY(!parented);
        QCOMPARE(parentedContent.parentWidget(), &parent);
    }

    void enforcesGlobalIdsAndReturnsSideAndDockOwnership()
    {
        ZzShellFixture fixture;
        auto sideContent = std::make_unique<QWidget>();
        QWidget *const sideRaw = sideContent.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("explorer"), QStringLiteral("Explorer"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            sideContent.get()));
        zzReleaseAfterAdoption(sideContent);
        QVERIFY(sideRaw->parentWidget() != nullptr);
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->pageCount(),
            1);

        QWidget duplicateContent;
        auto duplicate = fixture.shell->registerDockPanel(
            zzPanelId("explorer"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &duplicateContent);
        QVERIFY(!duplicate);
        QCOMPARE(duplicateContent.parent(), nullptr);

        auto dockContent = std::make_unique<QWidget>();
        QWidget *const dockRaw = dockContent.get();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("terminal"), QStringLiteral("Terminal"), zzIcon(),
            Qt::BottomDockWidgetArea, dockContent.get()));
        zzReleaseAfterAdoption(dockContent);
        auto *dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:terminal"));
        QVERIFY(dock != nullptr);
        QCOMPARE(dock->widget(), dockRaw);
        QVERIFY(dockRaw->parentWidget() != nullptr);

        auto takenSide = fixture.shell->takePanel(zzPanelId("explorer"));
        QVERIFY(takenSide);
        std::unique_ptr<QWidget> returnedSide(takenSide.value());
        QCOMPARE(returnedSide.get(), sideRaw);
        QCOMPARE(returnedSide->parent(), nullptr);

        auto takenDock = fixture.shell->takePanel(zzPanelId("terminal"));
        QVERIFY(takenDock);
        std::unique_ptr<QWidget> returnedDock(takenDock.value());
        QCOMPARE(returnedDock.get(), dockRaw);
        QCOMPARE(returnedDock->parent(), nullptr);
        QCOMPARE(
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:terminal")),
            nullptr);

        auto missing = fixture.shell->takePanel(zzPanelId("terminal"));
        QVERIFY(!missing);
        QCOMPARE(missing.error().code(), ZzCore::ZzErrorCode::NotFound);
    }

    void registersShowsAndReturnsBottomOwnership()
    {
        ZzShellFixture fixture;
        auto output = std::make_unique<QWidget>();
        QWidget *const outputRaw = output.get();

        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("output"), QStringLiteral("Output"), zzIcon(),
            output.get()));
        zzReleaseAfterAdoption(output);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 1);
        QVERIFY(fixture.shell->showPanel(zzPanelId("output"), true));
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), outputRaw);
        QVERIFY(!fixture.shell->bottomPane()->isCollapsed());
        QVERIFY(fixture.shell->showPanel(zzPanelId("output"), false));
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());

        auto taken = fixture.shell->takePanel(zzPanelId("output"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QCOMPARE(returned.get(), outputRaw);
        QCOMPARE(returned->parent(), nullptr);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 0);
    }

    void rejectsDuplicateIdsAcrossSideBottomAndDockWithoutTakingContent()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QWidget sideToBottom;
        QWidget sideToDock;
        QVERIFY(!fixture.shell->registerBottomPanel(
            zzPanelId("side"), QStringLiteral("Duplicate"), zzIcon(),
            &sideToBottom));
        QVERIFY(!fixture.shell->registerDockPanel(
            zzPanelId("side"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &sideToDock));
        QCOMPARE(sideToBottom.parent(), nullptr);
        QCOMPARE(sideToDock.parent(), nullptr);

        auto bottom = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            bottom.get()));
        zzReleaseAfterAdoption(bottom);
        QWidget bottomToSide;
        QWidget bottomToDock;
        QVERIFY(!fixture.shell->registerSidePanel(
            zzPanelId("bottom"), QStringLiteral("Duplicate"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, &bottomToSide));
        QVERIFY(!fixture.shell->registerDockPanel(
            zzPanelId("bottom"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &bottomToDock));
        QCOMPARE(bottomToSide.parent(), nullptr);
        QCOMPARE(bottomToDock.parent(), nullptr);

        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);
        QWidget dockToSide;
        QWidget dockToBottom;
        QVERIFY(!fixture.shell->registerSidePanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, &dockToSide));
        QVERIFY(!fixture.shell->registerBottomPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            &dockToBottom));
        QCOMPARE(dockToSide.parent(), nullptr);
        QCOMPARE(dockToBottom.parent(), nullptr);
    }

    void reservesBottomIdDuringSynchronousRegistrationSignals()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentChangeWidget>();
        QWidget *const contentRaw = content.get();
        QWidget duplicateContent;
        bool callbackEntered = false;
        bool duplicateRegistrationSucceeded = false;
        bool reentrantTakeSucceeded = false;
        content->parentChanged = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            duplicateRegistrationSucceeded = static_cast<bool>(
                fixture.shell->registerBottomPanel(
                    zzPanelId("bottom"), QStringLiteral("Duplicate"),
                    zzIcon(), &duplicateContent));
            reentrantTakeSucceeded = static_cast<bool>(
                fixture.shell->takePanel(zzPanelId("bottom")));
        };

        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!duplicateRegistrationSucceeded);
        QVERIFY(!reentrantTakeSucceeded);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 1);
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), contentRaw);
    }

    void externalBottomContentDestructionCleansStateAndAllowsIdReuse()
    {
        ZzShellFixture fixture;
        auto *bottom = new QWidget;
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(), bottom));
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 1);

        delete bottom;

        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Replacement"), zzIcon(),
            replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void cleansBottomIdWhenContentIsDestroyedDuringTakeSignals()
    {
        ZzShellFixture fixture;
        auto *bottom = new QWidget;
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(), bottom));
        bool callbackEntered = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(),
            [&](QWidget *) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete bottom;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("bottom"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Replacement"), zzIcon(),
            replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void cleansSideIdWhenContentIsDestroyedDuringTakeSignals()
    {
        ZzShellFixture fixture;
        auto *side = new QWidget;
        QPointer<QWidget> sideGuard(side);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete side;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("side"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(sideGuard.isNull());
        QCOMPARE(model->rowCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void cleansDockIdWhenContentIsDestroyedDuringTakeSignals()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QPointer<QWidget> contentGuard(contentRaw);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        bool callbackEntered = false;
        QObject::connect(
            dock, &QObject::destroyed,
            fixture.shell.get(), [&](QObject *) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete contentRaw;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(contentGuard.isNull());
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::BottomDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void dockTakePreservesContentInjectedDuringParentChange()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto injected = std::make_unique<QWidget>();
        QPointer<QWidget> injectedGuard(injected.get());
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        bool callbackEntered = false;
        contentRaw->parentRemoved = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            dock->setWidget(injected.get());
            zzReleaseAfterAdoption(injected);
        };

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(injectedGuard != nullptr);
        QCOMPARE(injectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedInjected(injectedGuard.data());
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::BottomDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void dockCleanupRetainsIdUntilRepeatedInjectionIsDetached()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<QWidget>();
        QPointer<QWidget> secondInjectedGuard(secondInjected.get());
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            zzReleaseAfterAdoption(firstInjected);
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            zzReleaseAfterAdoption(secondInjected);
        };

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));

        QVERIFY(!interruptedTake);
        QVERIFY(firstInjectedGuard != nullptr);
        QCOMPARE(firstInjectedGuard->parent(), nullptr);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);
        auto duplicate = std::make_unique<QWidget>();
        auto duplicateRegistration = fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, duplicate.get());
        if (duplicateRegistration) {
            zzReleaseAfterAdoption(duplicate);
        }
        QVERIFY(!duplicateRegistration);

        QCoreApplication::processEvents();

        QVERIFY(dockGuard.isNull());
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::BottomDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void shellDestructionPreservesContentWhileDockCleanupIsPending()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<QWidget>();
        QPointer<QWidget> secondInjectedGuard(secondInjected.get());
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            zzReleaseAfterAdoption(firstInjected);
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            zzReleaseAfterAdoption(secondInjected);
        };
        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));
        QVERIFY(!interruptedTake);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);

        fixture.shell.reset();

        QVERIFY(dockGuard.isNull());
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
    }

    void hostDestructionPreservesContentWhileDockCleanupIsPending()
    {
        auto host = std::make_unique<QMainWindow>();
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<QWidget>();
        QPointer<QWidget> secondInjectedGuard(secondInjected.get());
        QVERIFY(shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = host->findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            zzReleaseAfterAdoption(firstInjected);
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            zzReleaseAfterAdoption(secondInjected);
        };
        auto interruptedTake = shell->takePanel(zzPanelId("dock"));
        QVERIFY(!interruptedTake);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);

        host.reset();

        QVERIFY(dockGuard != nullptr);
        QCOMPARE(dockGuard->parent(), nullptr);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dockGuard.data());
        shell.reset();
        QVERIFY(dockGuard.isNull());
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
    }

    void pendingDockCleanupPreservesThirdInjectionDuringDestruction_data()
    {
        QTest::addColumn<bool>("destroyHostFirst");
        QTest::newRow("shell-first") << false;
        QTest::newRow("host-first") << true;
    }

    void pendingDockCleanupPreservesThirdInjectionDuringDestruction()
    {
        QFETCH(bool, destroyHostFirst);
        auto host = std::make_unique<QMainWindow>();
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        QPointer<QWidget> contentGuard(contentRaw);
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const secondInjectedRaw = secondInjected.get();
        QPointer<QWidget> secondInjectedGuard(secondInjectedRaw);
        auto thirdInjected = std::make_unique<QWidget>();
        QPointer<QWidget> thirdInjectedGuard(thirdInjected.get());
        QVERIFY(shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = host->findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            zzReleaseAfterAdoption(firstInjected);
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            zzReleaseAfterAdoption(secondInjected);
        };
        secondInjectedRaw->parentRemoved = [&] {
            dock->setWidget(thirdInjected.get());
            zzReleaseAfterAdoption(thirdInjected);
        };
        auto interruptedTake = shell->takePanel(zzPanelId("dock"));
        QVERIFY(!interruptedTake);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);
        if (destroyHostFirst) {
            host.reset();
            QVERIFY(dockGuard != nullptr);
            QCOMPARE(dockGuard->parent(), nullptr);
        }

        shell.reset();

        QVERIFY(dockGuard.isNull());
        QVERIFY(contentGuard != nullptr);
        QCOMPARE(contentGuard->parent(), nullptr);
        QVERIFY(firstInjectedGuard != nullptr);
        QCOMPARE(firstInjectedGuard->parent(), nullptr);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        QVERIFY(thirdInjectedGuard != nullptr);
        QCOMPARE(thirdInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
        std::unique_ptr<QWidget> preservedThird(thirdInjectedGuard.data());
    }

    void pendingDockCleanupPreservesLongFiniteInjectionChainDuringDestruction_data()
    {
        QTest::addColumn<bool>("destroyHostFirst");
        QTest::newRow("shell-first") << false;
        QTest::newRow("host-first") << true;
    }

    void pendingDockCleanupPreservesLongFiniteInjectionChainDuringDestruction()
    {
        QFETCH(bool, destroyHostFirst);
        constexpr std::size_t injectionCount = 12;
        auto host = std::make_unique<QMainWindow>();
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        QPointer<QWidget> contentGuard(contentRaw);
        std::array<std::unique_ptr<ZzParentRemovedWidget>, injectionCount>
            injected;
        std::array<QPointer<QWidget>, injectionCount> injectedGuards;
        for (std::size_t index = 0; index < injectionCount; ++index) {
            injected[index] = std::make_unique<ZzParentRemovedWidget>();
            injectedGuards[index] = injected[index].get();
        }
        QVERIFY(shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = host->findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(injected.front().get());
            zzReleaseAfterAdoption(injected.front());
        };
        for (std::size_t index = 1; index < injectionCount; ++index) {
            injected[index - 1]->parentRemoved = [&, index] {
                dock->setWidget(injected[index].get());
                zzReleaseAfterAdoption(injected[index]);
            };
        }

        auto interruptedTake = shell->takePanel(zzPanelId("dock"));

        QVERIFY(!interruptedTake);
        QVERIFY(dockGuard != nullptr);
        QVERIFY(dockGuard->widget() != nullptr);
        QCOMPARE(dockGuard->widget()->parent(), dockGuard.data());
        QWidget duplicate;
        QVERIFY(!shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &duplicate));
        QCOMPARE(duplicate.parent(), nullptr);
        if (destroyHostFirst) {
            host.reset();
            QVERIFY(dockGuard != nullptr);
            QCOMPARE(dockGuard->parent(), nullptr);
        }

        shell.reset();

        QVERIFY(dockGuard.isNull());
        QVERIFY(contentGuard != nullptr);
        QCOMPARE(contentGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::array<std::unique_ptr<QWidget>, injectionCount> preservedInjected;
        for (std::size_t index = 0; index < injectionCount; ++index) {
            QVERIFY(injected[index] == nullptr);
            QVERIFY(injectedGuards[index] != nullptr);
            QCOMPARE(injectedGuards[index]->parent(), nullptr);
            preservedInjected[index].reset(injectedGuards[index].data());
        }
    }

    void sideTakeRejectsDestroyedContentAfterActivityRemovalSignals()
    {
        ZzShellFixture fixture;
        auto *side = new QWidget;
        QPointer<QWidget> sideGuard(side);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        bool callbackEntered = false;
        QObject::connect(
            model, &QAbstractItemModel::rowsAboutToBeRemoved,
            fixture.shell.get(), [&] {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete side;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("side"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(sideGuard.isNull());
        QCOMPARE(model->rowCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void sideTakeLeavesThirdPartyOwnerAfterActivityRemovalSignals()
    {
        ZzShellFixture fixture;
        QWidget thirdPartyOwner;
        auto side = std::make_unique<QWidget>();
        QWidget *const sideRaw = side.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        bool callbackEntered = false;
        QObject::connect(
            model, &QAbstractItemModel::rowsAboutToBeRemoved,
            fixture.shell.get(), [&] {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                sideRaw->setParent(&thirdPartyOwner);
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("side"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QCOMPARE(sideRaw->parentWidget(), &thirdPartyOwner);
        QCOMPARE(model->rowCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void rejectsBottomShowDuringRegistrationTransaction()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        bool callbackEntered = false;
        bool reentrantShowSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != contentRaw) {
                    return;
                }
                callbackEntered = true;
                reentrantShowSucceeded = static_cast<bool>(
                    fixture.shell->showPanel(zzPanelId("bottom"), true));
            });

        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!reentrantShowSucceeded);
    }

    void rejectsBottomShowDuringRemovalTransaction()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);
        bool callbackEntered = false;
        bool reentrantShowSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current == contentRaw) {
                    return;
                }
                callbackEntered = true;
                reentrantShowSucceeded = static_cast<bool>(
                    fixture.shell->showPanel(zzPanelId("bottom"), true));
            });

        auto taken = fixture.shell->takePanel(zzPanelId("bottom"));

        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QCOMPARE(returned.get(), contentRaw);
        QVERIFY(callbackEntered);
        QVERIFY(!reentrantShowSucceeded);
    }

    void bottomShowDetectsSynchronousInvalidation_data()
    {
        QTest::addColumn<bool>("takeDuringSignal");
        QTest::newRow("take") << true;
        QTest::newRow("destroy") << false;
    }

    void bottomShowDetectsSynchronousInvalidation()
    {
        QFETCH(bool, takeDuringSignal);
        ZzShellFixture fixture;
        auto *content = new QWidget;
        QPointer<QWidget> contentGuard(content);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(), content));
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        bool callbackEntered = false;
        bool nestedTakeSucceeded = false;
        std::unique_ptr<QWidget> returned;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::collapsedChanged,
            fixture.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                if (takeDuringSignal) {
                    auto nestedTake = fixture.shell->takePanel(
                        zzPanelId("bottom"));
                    nestedTakeSucceeded = static_cast<bool>(nestedTake);
                    if (nestedTake) {
                        returned.reset(nestedTake.value());
                    }
                } else {
                    delete content;
                }
            });

        const auto interruptedShow = fixture.shell->showPanel(
            zzPanelId("bottom"), true);

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedShow);
        QCOMPARE(nestedTakeSucceeded, takeDuringSignal);
        if (takeDuringSignal) {
            QCOMPARE(returned.get(), content);
            QCOMPARE(returned->parent(), nullptr);
        } else {
            QVERIFY(contentGuard.isNull());
        }
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Replacement"), zzIcon(),
            replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void bottomShowDetectsSameWidgetReregistrationDuringSignal()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        bool callbackEntered = false;
        bool nestedTakeSucceeded = false;
        bool nestedRegistrationSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::collapsedChanged,
            fixture.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                auto nestedTake = fixture.shell->takePanel(
                    zzPanelId("bottom"));
                nestedTakeSucceeded = static_cast<bool>(nestedTake);
                if (!nestedTake) {
                    return;
                }
                std::unique_ptr<QWidget> returned(nestedTake.value());
                auto nestedRegistration = fixture.shell->registerBottomPanel(
                    zzPanelId("bottom"), QStringLiteral("Replacement"),
                    zzIcon(), returned.get());
                nestedRegistrationSucceeded =
                    static_cast<bool>(nestedRegistration);
                if (nestedRegistration) {
                    zzReleaseAfterAdoption(returned);
                }
            });

        const auto interruptedShow = fixture.shell->showPanel(
            zzPanelId("bottom"), true);

        QVERIFY(callbackEntered);
        QVERIFY(nestedTakeSucceeded);
        QVERIFY(nestedRegistrationSucceeded);
        QVERIFY(!interruptedShow);
        auto replacementTake = fixture.shell->takePanel(zzPanelId("bottom"));
        QVERIFY(replacementTake);
        std::unique_ptr<QWidget> returned(replacementTake.value());
        QCOMPARE(returned.get(), contentRaw);
        QCOMPARE(returned->parent(), nullptr);
    }

    void bottomShowStopsBeforeCollapseAfterCurrentWidgetReregistration()
    {
        ZzShellFixture fixture;
        auto target = std::make_unique<QWidget>();
        QWidget *const targetRaw = target.get();
        auto other = std::make_unique<QWidget>();
        QWidget *const otherRaw = other.get();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            target.get()));
        zzReleaseAfterAdoption(target);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("other"), QStringLiteral("Other"), zzIcon(),
            other.get()));
        zzReleaseAfterAdoption(other);
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), otherRaw);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        bool callbackEntered = false;
        bool nestedTakeSucceeded = false;
        bool nestedRegistrationSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != targetRaw) {
                    return;
                }
                callbackEntered = true;
                auto nestedTake = fixture.shell->takePanel(
                    zzPanelId("target"));
                nestedTakeSucceeded = static_cast<bool>(nestedTake);
                if (!nestedTake) {
                    return;
                }
                std::unique_ptr<QWidget> returned(nestedTake.value());
                auto nestedRegistration = fixture.shell->registerBottomPanel(
                    zzPanelId("target"), QStringLiteral("Replacement"),
                    zzIcon(), returned.get());
                nestedRegistrationSucceeded =
                    static_cast<bool>(nestedRegistration);
                if (nestedRegistration) {
                    zzReleaseAfterAdoption(returned);
                }
            });

        const auto interruptedShow = fixture.shell->showPanel(
            zzPanelId("target"), true);

        QVERIFY(callbackEntered);
        QVERIFY(nestedTakeSucceeded);
        QVERIFY(nestedRegistrationSucceeded);
        QVERIFY(!interruptedShow);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        auto replacementTake = fixture.shell->takePanel(zzPanelId("target"));
        QVERIFY(replacementTake);
        std::unique_ptr<QWidget> returned(replacementTake.value());
        QCOMPARE(returned.get(), targetRaw);
    }

    void failedTakePreservesRegisteredSideState()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        QWidget *const sideRaw = side.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);

        std::unique_ptr<QWidget> externallyTakenSide(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->takeWidget(sideRaw));
        QCOMPARE(externallyTakenSide.get(), sideRaw);
        auto failedSideTake = fixture.shell->takePanel(zzPanelId("side"));
        QVERIFY(!failedSideTake);
        QCOMPARE(failedSideTake.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(model->rowCount(), 1);
        QWidget duplicateSide;
        QVERIFY(!fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Duplicate side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, &duplicateSide));
    }

    void failedTakePreservesRegisteredDockState()
    {
        ZzShellFixture fixture;
        auto dockContent = std::make_unique<QWidget>();
        QWidget *const dockRaw = dockContent.get();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, dockContent.get()));
        zzReleaseAfterAdoption(dockContent);
        QPointer<ZzFluentUI::ZzDockPanel> dock =
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        std::unique_ptr<QWidget> externallyTakenDock(
            dock->takeContentWidget());
        QCOMPARE(externallyTakenDock.get(), dockRaw);

        auto failedDockTake = fixture.shell->takePanel(zzPanelId("dock"));
        QVERIFY(!failedDockTake);
        QCOMPARE(failedDockTake.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(dock != nullptr);
        QCOMPARE(dock->widget(), nullptr);
        QWidget duplicateDock;
        QVERIFY(!fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate dock"), zzIcon(),
            Qt::BottomDockWidgetArea, &duplicateDock));
    }

    void externalSideContentDestructionCleansStateAndAllowsIdReuse()
    {
        ZzShellFixture fixture;
        auto *side = new QWidget;
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);

        delete side;

        QCOMPARE(model->rowCount(), 0);
        auto replacementSide = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            replacementSide.get()));
        zzReleaseAfterAdoption(replacementSide);
    }

    void externalDockContentDestructionCleansStateAndAllowsIdReuse()
    {
        ZzShellFixture fixture;
        auto *dockContent = new QWidget;
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, dockContent));
        QPointer<ZzFluentUI::ZzDockPanel> dock =
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);

        delete dockContent;
        auto replacementDock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement dock"), zzIcon(),
            Qt::RightDockWidgetArea, replacementDock.get()));
        zzReleaseAfterAdoption(replacementDock);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(dock.isNull());
        QVERIFY(fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock")) != nullptr);
    }

    void dockDestructionBeforeContentDoesNotUseStaleIdentity()
    {
        ZzShellFixture fixture;
        auto *content = new QWidget;
        QPointer<QWidget> contentGuard(content);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, content));
        QPointer<ZzFluentUI::ZzDockPanel> dock =
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QObject::connect(
            dock, &QObject::destroyed, fixture.shell.get(),
            [contentGuard] {
                if (contentGuard != nullptr) {
                    contentGuard->setParent(nullptr);
                }
            });

        delete dock;

        QVERIFY(dock.isNull());
        QVERIFY(contentGuard != nullptr);
        QCOMPARE(contentGuard->parent(), nullptr);
        delete content;
        QVERIFY(contentGuard.isNull());
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::RightDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void reservesSideIdDuringSynchronousRegistrationSignals()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QWidget duplicateContent;
        bool callbackEntered = false;
        bool duplicateRegistrationSucceeded = false;
        bool reentrantTakeSucceeded = false;
        QObject::connect(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left),
            &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(),
            [&](QWidget *current) {
                if (callbackEntered || current != contentRaw) {
                    return;
                }
                callbackEntered = true;
                duplicateRegistrationSucceeded = static_cast<bool>(
                    fixture.shell->registerSidePanel(
                        zzPanelId("side"), QStringLiteral("Duplicate"),
                        zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                        &duplicateContent));
                reentrantTakeSucceeded = static_cast<bool>(
                    fixture.shell->takePanel(zzPanelId("side")));
            });

        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!duplicateRegistrationSucceeded);
        QVERIFY(!reentrantTakeSucceeded);
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QCOMPARE(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->pageCount(), 1);
    }

    void reservesDockIdDuringSynchronousParentChange()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentChangeWidget>();
        QWidget duplicateContent;
        bool callbackEntered = false;
        bool duplicateRegistrationSucceeded = false;
        bool reentrantTakeSucceeded = false;
        content->parentChanged = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            duplicateRegistrationSucceeded = static_cast<bool>(
                fixture.shell->registerDockPanel(
                    zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
                    Qt::BottomDockWidgetArea, &duplicateContent));
            reentrantTakeSucceeded = static_cast<bool>(
                fixture.shell->takePanel(zzPanelId("dock")));
        };

        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!duplicateRegistrationSucceeded);
        QVERIFY(!reentrantTakeSucceeded);
        QCOMPARE(
            fixture.host.findChildren<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock")).size(),
            1);
    }

    void preservesRegistrationOrderAndUpdatesBadges()
    {
        ZzShellFixture fixture;
        const std::array ids{"one", "two", "three"};
        const std::array titles{"One", "Two", "Three"};
        const std::array areas{
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::RightPrimary,
            ZzFluentUI::ZzActivityArea::LeftSecondary};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(fixture.shell->registerSidePanel(
                zzPanelId(ids.at(index)),
                QString::fromLatin1(titles.at(index)), zzIcon(),
                areas.at(index), content.get()));
            zzReleaseAfterAdoption(content);
        }

        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("One"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("Two"));
        QCOMPARE(model->index(2, 0).data().toString(), QStringLiteral("Three"));

        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("two"), 104));
        QCOMPARE(
            model->index(1, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Badge))
                .toInt(),
            104);
        QWidget unregistered;
        QVERIFY(!fixture.shell->setPanelBadge(zzPanelId("missing"), 1));
        QVERIFY(!fixture.shell->setPanelBadge(zzPanelId("one"), -1));
    }

    void secondaryFirstRegistrationUsesCanonicalSideOrderAndRoundTrips()
    {
        ZzShellFixture fixture;
        auto leftSecondaryOne = std::make_unique<QWidget>();
        auto rightSecondaryOne = std::make_unique<QWidget>();
        auto leftPrimaryOne = std::make_unique<QWidget>();
        auto rightPrimaryOne = std::make_unique<QWidget>();
        auto leftPrimaryTwo = std::make_unique<QWidget>();
        auto leftSecondaryTwo = std::make_unique<QWidget>();
        QWidget *const leftSecondaryOneRaw = leftSecondaryOne.get();
        QWidget *const rightSecondaryOneRaw = rightSecondaryOne.get();
        QWidget *const leftPrimaryOneRaw = leftPrimaryOne.get();
        QWidget *const rightPrimaryOneRaw = rightPrimaryOne.get();
        QWidget *const leftPrimaryTwoRaw = leftPrimaryTwo.get();
        QWidget *const leftSecondaryTwoRaw = leftSecondaryTwo.get();

        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-secondary-one"), QStringLiteral("Left secondary one"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            leftSecondaryOne.get()));
        zzReleaseAfterAdoption(leftSecondaryOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right-secondary-one"), QStringLiteral("Right secondary one"),
            zzIcon(), ZzFluentUI::ZzActivityArea::RightSecondary,
            rightSecondaryOne.get()));
        zzReleaseAfterAdoption(rightSecondaryOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-primary-one"), QStringLiteral("Left primary one"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
            leftPrimaryOne.get()));
        zzReleaseAfterAdoption(leftPrimaryOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right-primary-one"), QStringLiteral("Right primary one"),
            zzIcon(), ZzFluentUI::ZzActivityArea::RightPrimary,
            rightPrimaryOne.get()));
        zzReleaseAfterAdoption(rightPrimaryOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-primary-two"), QStringLiteral("Left primary two"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
            leftPrimaryTwo.get()));
        zzReleaseAfterAdoption(leftPrimaryTwo);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-secondary-two"), QStringLiteral("Left secondary two"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            leftSecondaryTwo.get()));
        zzReleaseAfterAdoption(leftSecondaryTwo);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QCOMPARE(leftPane->panelStack()->panels(),
            QList<QWidget *>({leftPrimaryOneRaw, leftPrimaryTwoRaw,
                leftSecondaryOneRaw, leftSecondaryTwoRaw}));
        QCOMPARE(rightPane->panelStack()->panels(),
            QList<QWidget *>({rightPrimaryOneRaw, rightSecondaryOneRaw}));

        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(model != nullptr);
        QCOMPARE(model, rightBar->model());
        const QList<QString> titles{
            QStringLiteral("Left secondary one"),
            QStringLiteral("Right secondary one"),
            QStringLiteral("Left primary one"),
            QStringLiteral("Right primary one"),
            QStringLiteral("Left primary two"),
            QStringLiteral("Left secondary two")};
        const QList<ZzFluentUI::ZzActivityArea> areas{
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            ZzFluentUI::ZzActivityArea::RightSecondary,
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::RightPrimary,
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::LeftSecondary};
        QCOMPARE(model->rowCount(), titles.size());
        for (int row = 0; row < titles.size(); ++row) {
            QCOMPARE(model->index(row, 0).data().toString(), titles.at(row));
            QCOMPARE(model->index(row, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(), areas.at(row));
        }

        QVERIFY(leftPane->panelStack()->setPanelSizes({444}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({555}));
        QCOMPARE(leftPane->visibleWidgets(),
            QList<QWidget *>({leftSecondaryTwoRaw}));
        QCOMPARE(rightPane->visibleWidgets(),
            QList<QWidget *>({rightPrimaryOneRaw}));
        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        auto targetLeftPrimaryOne = std::make_unique<QWidget>();
        auto targetRightPrimaryOne = std::make_unique<QWidget>();
        auto targetLeftPrimaryTwo = std::make_unique<QWidget>();
        auto targetLeftSecondaryOne = std::make_unique<QWidget>();
        auto targetRightSecondaryOne = std::make_unique<QWidget>();
        auto targetLeftSecondaryTwo = std::make_unique<QWidget>();
        QWidget *const targetLeftPrimaryOneRaw = targetLeftPrimaryOne.get();
        QWidget *const targetRightPrimaryOneRaw = targetRightPrimaryOne.get();
        QWidget *const targetLeftPrimaryTwoRaw = targetLeftPrimaryTwo.get();
        QWidget *const targetLeftSecondaryOneRaw = targetLeftSecondaryOne.get();
        QWidget *const targetRightSecondaryOneRaw = targetRightSecondaryOne.get();
        QWidget *const targetLeftSecondaryTwoRaw = targetLeftSecondaryTwo.get();
        const auto registerTarget = [&target](
                                        const char *id,
                                        const QString &title,
                                        ZzFluentUI::ZzActivityArea area,
                                        std::unique_ptr<QWidget> &content) {
            const bool registered = static_cast<bool>(
                target.shell->registerSidePanel(
                    zzPanelId(id), title, zzIcon(), area, content.get()));
            if (registered) {
                zzReleaseAfterAdoption(content);
            }
            return registered;
        };
        QVERIFY(registerTarget("left-primary-one", QStringLiteral("Left primary one"),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetLeftPrimaryOne));
        QVERIFY(registerTarget("right-primary-one", QStringLiteral("Right primary one"),
            ZzFluentUI::ZzActivityArea::RightPrimary, targetRightPrimaryOne));
        QVERIFY(registerTarget("left-primary-two", QStringLiteral("Left primary two"),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetLeftPrimaryTwo));
        QVERIFY(registerTarget("left-secondary-one", QStringLiteral("Left secondary one"),
            ZzFluentUI::ZzActivityArea::LeftSecondary, targetLeftSecondaryOne));
        QVERIFY(registerTarget("right-secondary-one", QStringLiteral("Right secondary one"),
            ZzFluentUI::ZzActivityArea::RightSecondary, targetRightSecondaryOne));
        QVERIFY(registerTarget("left-secondary-two", QStringLiteral("Left secondary two"),
            ZzFluentUI::ZzActivityArea::LeftSecondary, targetLeftSecondaryTwo));

        QVERIFY(target.shell->restoreLayout(saved.value()));
        auto *const targetLeftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const targetRightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const targetLeftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const targetRightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QCOMPARE(targetLeftPane->panelStack()->panels(),
            QList<QWidget *>({targetLeftPrimaryOneRaw, targetLeftPrimaryTwoRaw,
                targetLeftSecondaryOneRaw, targetLeftSecondaryTwoRaw}));
        QCOMPARE(targetRightPane->panelStack()->panels(),
            QList<QWidget *>({targetRightPrimaryOneRaw,
                targetRightSecondaryOneRaw}));
        QCOMPARE(targetLeftPane->panelStack()->panelSizes(), QList<int>({1}));
        QCOMPARE(targetRightPane->panelStack()->panelSizes(), QList<int>({1}));
        QCOMPARE(targetLeftPane->visibleWidgets(),
            QList<QWidget *>({targetLeftSecondaryTwoRaw}));
        QCOMPARE(targetRightPane->visibleWidgets(),
            QList<QWidget *>({targetRightPrimaryOneRaw}));
        QCOMPARE(leftPane->currentWidget(), leftSecondaryTwoRaw);
        QCOMPARE(rightPane->currentWidget(), rightPrimaryOneRaw);
        QCOMPARE(targetLeftPane->currentWidget(), targetLeftSecondaryTwoRaw);
        QCOMPARE(targetRightPane->currentWidget(), targetRightPrimaryOneRaw);
        QCOMPARE(targetLeftBar->currentSourceIndex().data().toString(),
            QStringLiteral("Left secondary two"));
        QCOMPARE(targetRightBar->currentSourceIndex().data().toString(),
            QStringLiteral("Right primary one"));
        const auto targetActiveTitles = [](ZzFluentUI::ZzActivityBar *bar) {
            QList<QString> activeTitles;
            for (const QModelIndex &index : bar->activeSourceIndexes()) {
                activeTitles.append(index.data().toString());
            }
            return activeTitles;
        };
        QCOMPARE(targetActiveTitles(targetLeftBar),
            QList<QString>({QStringLiteral("Left secondary two")}));
        QCOMPARE(targetActiveTitles(targetRightBar),
            QList<QString>({QStringLiteral("Right primary one")}));
        for (QWidget *const content : targetLeftPane->panelStack()->panels()) {
            QVERIFY(targetLeftPane->isAncestorOf(content));
        }
        for (QWidget *const content : targetRightPane->panelStack()->panels()) {
            QVERIFY(targetRightPane->isAncestorOf(content));
        }
        const auto resaved = target.shell->saveLayout();
        QVERIFY(resaved);
        QVERIFY(target.shell->restoreLayout(resaved.value()));
    }

    void sideRegistrationRejectsNestedThirdPartyOwnerDuringAddWidget()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QWidget thirdPartyOwner(leftPane->panelStack());
        bool registrationReturned = false;
        bool callbackBeforeRegistrationReturned = false;
        int callbackCount = 0;
        QObject::connect(
            leftPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (current != primaryRaw) {
                    return;
                }
                ++callbackCount;
                callbackBeforeRegistrationReturned = !registrationReturned;
                primaryRaw->setParent(&thirdPartyOwner);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        registrationReturned = true;
        if (primaryRaw->parentWidget() == &thirdPartyOwner) {
            zzReleaseAfterAdoption(primary);
        }

        QCOMPARE(callbackCount, 1);
        QVERIFY(callbackBeforeRegistrationReturned);
        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(primaryRaw->parentWidget(), &thirdPartyOwner);
        const auto taken = fixture.shell->takePanel(zzPanelId("primary"));
        QVERIFY(!taken);
        QCOMPARE(taken.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Secondary"));
        QVERIFY(fixture.shell->saveLayout());

        primaryRaw->setParent(nullptr);
        primary.reset(primaryRaw);
    }

    void sideRegistrationDoesNotReclaimThirdPartyContentAfterPanelMove()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto primary = std::make_unique<QWidget>();
        QWidget *const primaryRaw = primary.get();
        QWidget thirdPartyOwner;
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        int callbackCount = 0;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                ++callbackCount;
                QCOMPARE(leftPane->takeWidget(primaryRaw), primaryRaw);
                primaryRaw->setParent(&thirdPartyOwner);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());

        QCOMPARE(callbackCount, 1);
        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(primaryRaw->parentWidget(), &thirdPartyOwner);
        const auto taken = fixture.shell->takePanel(zzPanelId("primary"));
        QVERIFY(!taken);
        QCOMPARE(taken.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QVERIFY(fixture.shell->saveLayout());

        primaryRaw->setParent(nullptr);
    }

    void sideRegistrationDoesNotReclaimNestedThirdPartyContentAfterPanelMove()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QWidget thirdPartyOwner(leftPane->panelStack());
        int callbackCount = 0;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                ++callbackCount;
                primaryRaw->setParent(&thirdPartyOwner);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        if (primaryRaw->parentWidget() == &thirdPartyOwner) {
            zzReleaseAfterAdoption(primary);
        }

        QCOMPARE(callbackCount, 1);
        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(primaryRaw->parentWidget(), &thirdPartyOwner);
        const auto taken = fixture.shell->takePanel(zzPanelId("primary"));
        QVERIFY(!taken);
        QCOMPARE(taken.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Secondary"));
        QVERIFY(fixture.shell->saveLayout());

        primaryRaw->setParent(nullptr);
        primary.reset(primaryRaw);
    }

    void sideRegistrationPreservesThirdPartyOwnerInsideFrameAfterPanelMove()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QPointer<QWidget> primaryGuard(primaryRaw);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> frameOwner;
        QPointer<QWidget> thirdPartyOwner;
        int callbackCount = 0;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                ++callbackCount;
                frameOwner = primaryRaw->parentWidget();
                if (frameOwner == nullptr) {
                    return;
                }
                thirdPartyOwner = new QWidget(frameOwner);
                primaryRaw->setParent(thirdPartyOwner);
                zzReleaseAfterAdoption(primary);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        if (primary != nullptr && primaryGuard != nullptr
            && primaryGuard->parent() != nullptr) {
            zzReleaseAfterAdoption(primary);
        }

        QCOMPARE(callbackCount, 1);
        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(frameOwner != nullptr);
        QCOMPARE(frameOwner->parent(), nullptr);
        QVERIFY(frameOwner->isHidden());
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(primaryGuard->parentWidget(), thirdPartyOwner.data());
        const auto taken = fixture.shell->takePanel(zzPanelId("primary"));
        QVERIFY(!taken);
        QCOMPARE(taken.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Secondary"));
        QVERIFY(fixture.shell->saveLayout());

        QWidget *const survivingPrimary = primaryGuard.data();
        survivingPrimary->setParent(nullptr);
        delete thirdPartyOwner.data();
        primary.reset(survivingPrimary);
        QTRY_VERIFY(frameOwner.isNull());
    }

    void sideRegistrationDoesNotDetachOwnerThatWouldReattachInsideFrame()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QPointer<QWidget> primaryGuard(primaryRaw);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> frameOwner;
        QPointer<ZzReattachingOwner> thirdPartyOwner;
        bool reattached = false;
        int callbackCount = 0;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                ++callbackCount;
                frameOwner = primaryRaw->parentWidget();
                if (frameOwner == nullptr) {
                    return;
                }
                thirdPartyOwner = new ZzReattachingOwner(
                    frameOwner, &reattached);
                primaryRaw->setParent(thirdPartyOwner);
                zzReleaseAfterAdoption(primary);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        if (primary != nullptr && primaryGuard != nullptr
            && primaryGuard->parent() != nullptr) {
            zzReleaseAfterAdoption(primary);
        }

        QCOMPARE(callbackCount, 1);
        QVERIFY(!reattached);
        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(primaryGuard->parentWidget(), thirdPartyOwner.data());
        const auto taken = fixture.shell->takePanel(zzPanelId("primary"));
        QVERIFY(!taken);
        QCOMPARE(taken.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Secondary"));
        QVERIFY(fixture.shell->saveLayout());

        QWidget *const survivingPrimary = primaryGuard.data();
        survivingPrimary->setParent(nullptr);
        delete thirdPartyOwner.data();
        primary.reset(survivingPrimary);
        QTRY_VERIFY(frameOwner.isNull());
    }

    void sideRegistrationEscrowSurvivesQueuedOwnerReattachment()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QPointer<QWidget> primaryGuard(primaryRaw);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> frameOwner;
        QPointer<ZzQueuedReattachingOwner> thirdPartyOwner;
        bool reattachQueued = false;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                frameOwner = primaryRaw->parentWidget();
                if (frameOwner == nullptr) {
                    return;
                }
                thirdPartyOwner = new ZzQueuedReattachingOwner(
                    frameOwner, fixture.shell.get(), &reattachQueued);
                primaryRaw->setParent(thirdPartyOwner);
                zzReleaseAfterAdoption(primary);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        if (primary != nullptr && primaryGuard != nullptr
            && primaryGuard->parent() != nullptr) {
            zzReleaseAfterAdoption(primary);
        }

        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(frameOwner != nullptr);
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(primaryGuard->parentWidget(), thirdPartyOwner.data());
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QVERIFY(fixture.shell->saveLayout());

        QWidget outsideOwner;
        thirdPartyOwner->setParent(&outsideOwner);
        QVERIFY(reattachQueued);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(frameOwner != nullptr);
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(thirdPartyOwner->parentWidget(), frameOwner.data());
        QCOMPARE(primaryGuard->parentWidget(), thirdPartyOwner.data());
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QVERIFY(fixture.shell->saveLayout());

        thirdPartyOwner->setParent(&outsideOwner);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(frameOwner.isNull());
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(thirdPartyOwner->parentWidget(), &outsideOwner);
        QCOMPARE(primaryGuard->parentWidget(), thirdPartyOwner.data());

        QWidget *const survivingPrimary = primaryGuard.data();
        survivingPrimary->setParent(nullptr);
        delete thirdPartyOwner.data();
        primary.reset(survivingPrimary);
    }

    void sideRegistrationEscrowReclaimsFrameAfterOwnerLeaves()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QPointer<QWidget> primaryGuard(primaryRaw);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> frameOwner;
        QPointer<QWidget> thirdPartyOwner;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                frameOwner = primaryRaw->parentWidget();
                if (frameOwner == nullptr) {
                    return;
                }
                thirdPartyOwner = new QWidget(frameOwner);
                primaryRaw->setParent(thirdPartyOwner);
                zzReleaseAfterAdoption(primary);
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        if (primary != nullptr && primaryGuard != nullptr
            && primaryGuard->parent() != nullptr) {
            zzReleaseAfterAdoption(primary);
        }

        QVERIFY(!registered);
        QCOMPARE(registered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(frameOwner != nullptr);
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QVERIFY(fixture.shell->saveLayout());

        QWidget outsideOwner;
        thirdPartyOwner->setParent(&outsideOwner);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(frameOwner.isNull());
        QVERIFY(thirdPartyOwner != nullptr);
        QVERIFY(primaryGuard != nullptr);
        QCOMPARE(thirdPartyOwner->parentWidget(), &outsideOwner);
        QCOMPARE(primaryGuard->parentWidget(), thirdPartyOwner.data());
        QCOMPARE(leftPane->panelStack()->panels(), QList<QWidget *>({secondaryRaw}));
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QVERIFY(fixture.shell->saveLayout());

        QWidget *const survivingPrimary = primaryGuard.data();
        survivingPrimary->setParent(nullptr);
        delete thirdPartyOwner.data();
        primary.reset(survivingPrimary);
    }

    void panelStackTakeDoesNotReturnContentWithQObjectOwner()
    {
        ZzFluentUI::ZzPanelStack stack;
        QObject thirdPartyOwner;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QPointer<QWidget> contentGuard(contentRaw);
        QVERIFY(stack.addPanel(contentRaw, QStringLiteral("Content")));
        zzReleaseAfterAdoption(content);
        contentRaw->QObject::setParent(&thirdPartyOwner);

        QWidget *const taken = stack.takePanel(contentRaw);

        QCOMPARE(taken, nullptr);
        QVERIFY(contentGuard != nullptr);
        QCOMPARE(contentGuard->parent(), &thirdPartyOwner);
        QVERIFY(stack.panels().isEmpty());
        contentRaw->QObject::setParent(nullptr);
        delete contentRaw;
        QVERIFY(contentGuard.isNull());
    }

    void sideRegistrationKeepsFixedOwnerWhenFrameObjectNameChanges()
    {
        ZzShellFixture fixture;
        auto secondary = std::make_unique<QWidget>();
        auto primary = std::make_unique<QWidget>();
        QWidget *const secondaryRaw = secondary.get();
        QWidget *const primaryRaw = primary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("secondary"), QStringLiteral("Secondary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, secondary.get()));
        zzReleaseAfterAdoption(secondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> frameOwner;
        int callbackCount = 0;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (moved != primaryRaw) {
                    return;
                }
                ++callbackCount;
                frameOwner = primaryRaw->parentWidget();
                QVERIFY(frameOwner != nullptr);
                frameOwner->setObjectName(QStringLiteral("renamed-frame"));
            });

        const auto registered = fixture.shell->registerSidePanel(
            zzPanelId("primary"), QStringLiteral("Primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, primary.get());
        const bool registrationSucceeded = static_cast<bool>(registered);
        const QList<QWidget *> stackPanels = leftPane->panelStack()->panels();
        const int modelRows = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount();
        const bool layoutSaved = registrationSucceeded
            && static_cast<bool>(fixture.shell->saveLayout());

        if (registrationSucceeded) {
            zzReleaseAfterAdoption(primary);
        } else if (leftPane->panelStack()->panels().contains(primaryRaw)) {
            QCOMPARE(leftPane->takeWidget(primaryRaw), primaryRaw);
        }

        QCOMPARE(callbackCount, 1);
        QVERIFY(registrationSucceeded);
        QCOMPARE(primaryRaw->parentWidget(), frameOwner.data());
        QCOMPARE(stackPanels, QList<QWidget *>({primaryRaw, secondaryRaw}));
        QCOMPARE(modelRows, 2);
        QVERIFY(layoutSaved);
    }

    void sideRegistrationRollsBackOnlyOuterAfterReentrantRegistration()
    {
        ZzShellFixture fixture;
        auto originalSecondary = std::make_unique<QWidget>();
        auto outerPrimary = std::make_unique<QWidget>();
        auto nestedSecondary = std::make_unique<QWidget>();
        QWidget *const originalSecondaryRaw = originalSecondary.get();
        QWidget *const outerPrimaryRaw = outerPrimary.get();
        QWidget *const nestedSecondaryRaw = nestedSecondary.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("original-secondary"), QStringLiteral("Original secondary"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            originalSecondary.get()));
        zzReleaseAfterAdoption(originalSecondary);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        bool callbackEntered = false;
        bool nestedRegistered = false;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *moved, int) {
                if (callbackEntered || moved != outerPrimaryRaw) {
                    return;
                }
                callbackEntered = true;
                nestedRegistered = static_cast<bool>(
                    fixture.shell->registerSidePanel(
                        zzPanelId("nested-secondary"),
                        QStringLiteral("Nested secondary"), zzIcon(),
                        ZzFluentUI::ZzActivityArea::LeftSecondary,
                        nestedSecondary.get()));
                if (nestedRegistered) {
                    zzReleaseAfterAdoption(nestedSecondary);
                }
            });

        const auto outerRegistered = fixture.shell->registerSidePanel(
            zzPanelId("outer-primary"), QStringLiteral("Outer primary"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, outerPrimary.get());

        QVERIFY(callbackEntered);
        QVERIFY(nestedRegistered);
        QVERIFY(!outerRegistered);
        QCOMPARE(outerRegistered.error().code(), ZzCore::ZzErrorCode::InvalidState);
        const auto outerTaken = fixture.shell->takePanel(zzPanelId("outer-primary"));
        QVERIFY(!outerTaken);
        QCOMPARE(outerTaken.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(leftPane->panelStack()->panels(),
            QList<QWidget *>({originalSecondaryRaw, nestedSecondaryRaw}));
        QVERIFY(leftPane->isAncestorOf(originalSecondaryRaw));
        QVERIFY(leftPane->isAncestorOf(nestedSecondaryRaw));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->index(0, 0).data().toString(),
            QStringLiteral("Original secondary"));
        QCOMPARE(model->index(1, 0).data().toString(),
            QStringLiteral("Nested secondary"));
        QVERIFY(fixture.shell->saveLayout());
    }

    void showsSideAndDockPanelsThroughOneApi()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);

        QVERIFY(fixture.shell->showPanel(zzPanelId("side"), false));
        QVERIFY(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->isCollapsed());
        QVERIFY(fixture.shell->showPanel(zzPanelId("side"), true));
        QVERIFY(!fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->isCollapsed());

        auto *dockPanel = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        QVERIFY(fixture.shell->showPanel(zzPanelId("dock"), false));
        QVERIFY(dockPanel->isHidden());
        QVERIFY(fixture.shell->showPanel(zzPanelId("dock"), true));
        QVERIFY(!dockPanel->isHidden());
    }

    void movesActivityPanelsWithoutLosingStackState()
    {
        ZzShellFixture fixture;
        auto leftOne = std::make_unique<QWidget>();
        auto leftTwo = std::make_unique<QWidget>();
        auto leftSecondary = std::make_unique<QWidget>();
        auto rightOne = std::make_unique<QWidget>();
        QWidget *const leftTwoRaw = leftTwo.get();
        QWidget *const leftSecondaryRaw = leftSecondary.get();
        QWidget *const rightOneRaw = rightOne.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-one"), QStringLiteral("Left one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftOne.get()));
        zzReleaseAfterAdoption(leftOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftTwo.get()));
        zzReleaseAfterAdoption(leftTwo);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-secondary"), QStringLiteral("Left secondary"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            leftSecondary.get()));
        zzReleaseAfterAdoption(leftSecondary);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right-one"), QStringLiteral("Right one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, rightOne.get()));
        zzReleaseAfterAdoption(rightOne);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({333}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({444}));

        Q_EMIT leftBar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            0);
        QCOMPARE(leftPane->visibleWidgets(),
            QList<QWidget *>({leftSecondaryRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({333}));
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Left two"));

        Q_EMIT leftBar->moveRequested(
            model->index(2, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            1);
        QCOMPARE(leftPane->visibleWidgets(),
            QList<QWidget *>({leftSecondaryRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({333}));
        QCOMPARE(
            model->index(1, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);

        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({rightOneRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({444}));
        QCOMPARE(leftPane->visibleWidgets(),
            QList<QWidget *>({leftSecondaryRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({333}));
        QVERIFY(leftTwoRaw->parentWidget() != nullptr);
        QVERIFY(rightPane->isAncestorOf(leftTwoRaw));

        QVERIFY(fixture.shell->showPanel(zzPanelId("left-one"), false));
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({leftSecondaryRaw}));
        const auto leftActive = leftBar->activeSourceIndexes();
        QCOMPARE(leftActive.size(), 1);
        QCOMPARE(leftActive.constFirst().data().toString(),
            QStringLiteral("Left secondary"));
        const auto rightActive = rightBar->activeSourceIndexes();
        QCOMPARE(rightActive.size(), 1);
        QVERIFY(rightActive.contains(model->index(3, 0)));
    }

    void activityMoveAuditScalesBelowQuadraticGrowth()
    {
        const auto measureMove = [](int panelCount) {
            ZzShellFixture fixture;
            for (int index = 0; index < panelCount; ++index) {
                auto content = std::make_unique<QWidget>();
                const auto id = ZzPureTools::ZzWorkspacePanelId(
                    QStringLiteral("audit-%1").arg(index));
                const auto registered = fixture.shell->registerSidePanel(
                    id, id.value(), zzIcon(),
                    ZzFluentUI::ZzActivityArea::LeftPrimary, content.get());
                if (!registered) {
                    return qint64{-1};
                }
                zzReleaseAfterAdoption(content);
            }

            auto *const bar = fixture.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left);
            auto *const pane = fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left);
            QAbstractItemModel *const model = bar->model();
            const auto move = [bar, model, pane](int sourceRow, int targetRow) {
                const QString movedTitle = model->index(sourceRow, 0)
                    .data().toString();
                QWidget *const movedPanel = pane->panelStack()->panels().at(
                    sourceRow);
                QElapsedTimer timer;
                timer.start();
                Q_EMIT bar->moveRequested(
                    model->index(sourceRow, 0),
                    ZzFluentUI::ZzActivityArea::LeftPrimary,
                    targetRow);
                const qint64 elapsed = timer.nsecsElapsed();
                if (model->index(targetRow, 0).data().toString()
                        != movedTitle
                    || model->index(targetRow, 0).data(static_cast<int>(
                           ZzFluentUI::ZzActivityItemRole::Area))
                           .value<ZzFluentUI::ZzActivityArea>()
                        != ZzFluentUI::ZzActivityArea::LeftPrimary
                    || pane->panelStack()->panels().at(targetRow)
                        != movedPanel) {
                    return qint64{-1};
                }
                return elapsed;
            };
            if (move(0, panelCount - 1) <= 0
                || move(panelCount - 1, 0) <= 0) {
                return qint64{-1};
            }

            QList<qint64> samples;
            samples.reserve(5);
            for (int iteration = 0; iteration < 5; ++iteration) {
                const int sourceRow = iteration % 2 == 0 ? 0 : panelCount - 1;
                const int targetRow = iteration % 2 == 0 ? panelCount - 1 : 0;
                const qint64 elapsed = move(sourceRow, targetRow);
                if (elapsed <= 0) {
                    return qint64{-1};
                }
                samples.append(elapsed);
            }
            std::sort(samples.begin(), samples.end());
            return samples.at(samples.size() / 2);
        };

        const qint64 small = measureMove(128);
        const qint64 large = measureMove(512);
        QVERIFY(small > 0);
        QVERIFY(large > 0);
        QVERIFY2(large < small * 10,
            qPrintable(QStringLiteral(
                "Activity move grew from %1 ns to %2 ns")
                    .arg(small)
                    .arg(large)));
    }

    void activityMoveReordersOnlyRequestedPanel()
    {
        ZzShellFixture fixture;
        QList<QWidget *> contents;
        for (int index = 0; index < 6; ++index) {
            auto content = std::make_unique<QWidget>();
            QWidget *const rawContent = content.get();
            const auto id = ZzPureTools::ZzWorkspacePanelId(
                QStringLiteral("single-%1").arg(index));
            QVERIFY(fixture.shell->registerSidePanel(
                id, id.value(), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            zzReleaseAfterAdoption(content);
            contents.append(rawContent);
        }
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(pane->panelStack()->setPanelSizes({606}));
        QList<QPointer<QWidget>> owners;
        QList<QWidget *> rawOwners;
        for (QWidget *content : contents) {
            owners.append(content->parentWidget());
            rawOwners.append(content->parentWidget());
        }
        QList<QPair<QWidget *, int>> moves;
        QObject::connect(pane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&moves](QWidget *content, int index) {
                moves.append({content, index});
            });

        Q_EMIT bar->moveRequested(model->index(0, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary, 5);
        const QList<QPair<QWidget *, int>> movedToEnd = {{contents.at(0), 5}};
        QCOMPARE(moves, movedToEnd);
        QCOMPARE(pane->panelStack()->panels(), QList<QWidget *>({contents.at(1),
            contents.at(2), contents.at(3), contents.at(4), contents.at(5),
            contents.at(0)}));
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({contents.at(5)}));
        QCOMPARE(pane->panelStack()->panelSizes(), QList<int>({606}));
        QCOMPARE(model->index(5, 0).data().toString(), QStringLiteral("single-0"));
        QCOMPARE(model->index(5, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area)).value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);
        for (int row = 0; row < 6; ++row) {
            QCOMPARE(model->index(row, 0).data().toString(),
                QStringLiteral("single-%1").arg((row + 1) % 6));
            QCOMPARE(model->index(row, 0).data(static_cast<int>(
                ZzFluentUI::ZzActivityItemRole::Area)).value<ZzFluentUI::ZzActivityArea>(),
                ZzFluentUI::ZzActivityArea::LeftPrimary);
        }
        QCOMPARE(pane->currentWidget(), contents.at(5));
        QCOMPARE(bar->currentSourceIndex().data().toString(), QStringLiteral("single-5"));
        QCOMPARE(bar->activeSourceIndexes().size(), 1);
        QVERIFY(rightPane->panelStack()->panels().isEmpty());
        QVERIFY(rightPane->visibleWidgets().isEmpty());
        QVERIFY(rightPane->panelStack()->panelSizes().isEmpty());
        QVERIFY(rightPane->currentWidget() == nullptr);
        QVERIFY(!rightBar->currentSourceIndex().isValid());
        QVERIFY(rightBar->activeSourceIndexes().isEmpty());
        for (qsizetype index = 0; index < contents.size(); ++index) {
            QCOMPARE(contents.at(index)->parentWidget(), rawOwners.at(index));
            QVERIFY(owners.at(index) != nullptr);
            QVERIFY(pane->panelStack()->isAncestorOf(contents.at(index)));
            QVERIFY(pane->isAncestorOf(contents.at(index)));
        }

        moves.clear();
        Q_EMIT bar->moveRequested(model->index(5, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
        const QList<QPair<QWidget *, int>> movedToStart = {{contents.at(0), 0}};
        QCOMPARE(moves, movedToStart);
        QCOMPARE(pane->panelStack()->panels(), contents);
        QCOMPARE(pane->panelStack()->panelSizes(), QList<int>({606}));
        for (int row = 0; row < 6; ++row) {
            QCOMPARE(model->index(row, 0).data().toString(),
                QStringLiteral("single-%1").arg(row));
        }

        moves.clear();
        Q_EMIT bar->moveRequested(model->index(0, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
        QVERIFY(moves.isEmpty());
        QCOMPARE(pane->currentWidget(), contents.at(5));
        QCOMPARE(bar->activeSourceIndexes().size(), 1);
        QVERIFY(fixture.shell->saveLayout());
    }

    void activityMoveAcrossSidesReordersOnlyRequestedPanel()
    {
        ZzShellFixture fixture;
        auto moved = std::make_unique<QWidget>();
        auto rightOne = std::make_unique<QWidget>();
        auto rightTwo = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QWidget *const rightOneRaw = rightOne.get();
        QWidget *const rightTwoRaw = rightTwo.get();
        QVERIFY(fixture.shell->registerSidePanel(zzPanelId("cross-moved"),
            QStringLiteral("Cross moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(zzPanelId("cross-right-one"),
            QStringLiteral("Right one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, rightOne.get()));
        zzReleaseAfterAdoption(rightOne);
        QVERIFY(fixture.shell->registerSidePanel(zzPanelId("cross-right-two"),
            QStringLiteral("Right two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, rightTwo.get()));
        zzReleaseAfterAdoption(rightTwo);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(leftPane->panelStack()->setPanelSizes({111}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({333}));
        QPointer<QWidget> rightOneOwner = rightOneRaw->parentWidget();
        QPointer<QWidget> rightTwoOwner = rightTwoRaw->parentWidget();
        QWidget *const rawRightOneOwner = rightOneRaw->parentWidget();
        QWidget *const rawRightTwoOwner = rightTwoRaw->parentWidget();
        QList<QPair<QWidget *, int>> leftMoves;
        QList<QPair<QWidget *, int>> rightMoves;
        QObject::connect(leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&leftMoves](QWidget *content, int index) {
                leftMoves.append({content, index});
            });
        QObject::connect(rightPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&rightMoves](QWidget *content, int index) {
                rightMoves.append({content, index});
            });
        Q_EMIT bar->moveRequested(model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary, 1);
        QVERIFY(leftMoves.isEmpty());
        const QList<QPair<QWidget *, int>> expectedMoves = {{movedRaw, 1}};
        QCOMPARE(rightMoves, expectedMoves);
        QVERIFY(leftPane->panelStack()->panels().isEmpty());
        QCOMPARE(rightPane->panelStack()->panels(),
            QList<QWidget *>({rightOneRaw, movedRaw, rightTwoRaw}));
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({111}));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("Cross moved"));
        QCOMPARE(model->index(1, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area)).value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightPrimary);
        QCOMPARE(rightPane->currentWidget(), movedRaw);
        QVERIFY(leftPane->visibleWidgets().isEmpty());
        QVERIFY(leftPane->panelStack()->panelSizes().isEmpty());
        QVERIFY(leftPane->currentWidget() == nullptr);
        QVERIFY(!bar->currentSourceIndex().isValid());
        QVERIFY(bar->activeSourceIndexes().isEmpty());
        QCOMPARE(rightBar->currentSourceIndex().data().toString(),
            QStringLiteral("Cross moved"));
        QCOMPARE(rightBar->activeSourceIndexes().size(), 1);
        const QStringList titles = {QStringLiteral("Right one"),
            QStringLiteral("Cross moved"), QStringLiteral("Right two")};
        for (int row = 0; row < titles.size(); ++row) {
            QCOMPARE(model->index(row, 0).data().toString(), titles.at(row));
            QCOMPARE(model->index(row, 0).data(static_cast<int>(
                ZzFluentUI::ZzActivityItemRole::Area)).value<ZzFluentUI::ZzActivityArea>(),
                ZzFluentUI::ZzActivityArea::RightPrimary);
        }
        QCOMPARE(rightOneRaw->parentWidget(), rawRightOneOwner);
        QCOMPARE(rightTwoRaw->parentWidget(), rawRightTwoOwner);
        QVERIFY(rightOneOwner != nullptr && rightTwoOwner != nullptr);
        for (QWidget *content : {rightOneRaw, movedRaw, rightTwoRaw}) {
            QVERIFY(content->parentWidget() != nullptr);
            QVERIFY(rightPane->panelStack()->isAncestorOf(content));
            QVERIFY(rightPane->isAncestorOf(content));
        }
        QVERIFY(fixture.shell->saveLayout());
    }

    void activityMoveUsesAreaLocalRowWithinSecondary()
    {
        ZzShellFixture fixture;
        QList<QWidget *> contents;
        const std::array<ZzFluentUI::ZzActivityArea, 4> areas = {
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            ZzFluentUI::ZzActivityArea::LeftSecondary};
        for (int index = 0; index < 4; ++index) {
            auto content = std::make_unique<QWidget>();
            QWidget *const raw = content.get();
            const QString id = QStringLiteral("local-%1").arg(index);
            QVERIFY(fixture.shell->registerSidePanel(
                ZzPureTools::ZzWorkspacePanelId(id), id, zzIcon(),
                areas.at(static_cast<std::size_t>(index)), content.get()));
            zzReleaseAfterAdoption(content);
            contents.append(raw);
        }
        QVERIFY(fixture.shell->showPanel(zzPanelId("local-1"), false));
        QVERIFY(fixture.shell->showPanel(zzPanelId("local-2"), false));
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const model = bar->model();
        QVERIFY(pane->panelStack()->setPanelSizes({202}));

        Q_EMIT bar->moveRequested(model->index(3, 0),
            ZzFluentUI::ZzActivityArea::LeftSecondary, 0);

        QCOMPARE(pane->panelStack()->panels(),
            QList<QWidget *>({contents.at(0), contents.at(1),
                contents.at(3), contents.at(2)}));
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({contents.at(3)}));
        QCOMPARE(pane->panelStack()->panelSizes(), QList<int>({202}));
        QCOMPARE(pane->currentWidget(), contents.at(3));
        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
    }

    void activityMoveUsesAreaLocalRowAcrossSides()
    {
        ZzShellFixture fixture;
        QList<QWidget *> leftContents;
        const std::array<ZzFluentUI::ZzActivityArea, 4> leftAreas = {
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            ZzFluentUI::ZzActivityArea::LeftSecondary};
        for (int index = 0; index < 4; ++index) {
            auto content = std::make_unique<QWidget>();
            QWidget *const raw = content.get();
            const QString id = QStringLiteral("cross-local-%1").arg(index);
            QVERIFY(fixture.shell->registerSidePanel(
                ZzPureTools::ZzWorkspacePanelId(id), id, zzIcon(),
                leftAreas.at(static_cast<std::size_t>(index)), content.get()));
            zzReleaseAfterAdoption(content);
            leftContents.append(raw);
        }
        auto moved = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("cross-local-moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->showPanel(zzPanelId("cross-local-1"), false));
        QVERIFY(fixture.shell->showPanel(zzPanelId("cross-local-2"), false));
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const model = rightBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({202}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({303}));

        Q_EMIT rightBar->moveRequested(model->index(4, 0),
            ZzFluentUI::ZzActivityArea::LeftSecondary, 0);

        QCOMPARE(leftPane->panelStack()->panels(),
            QList<QWidget *>({leftContents.at(0), leftContents.at(1), movedRaw,
                leftContents.at(2), leftContents.at(3)}));
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({303}));
        QCOMPARE(leftPane->currentWidget(), movedRaw);
        QVERIFY(rightPane->panelStack()->panels().isEmpty());
        QVERIFY(rightPane->visibleWidgets().isEmpty());
        QVERIFY(rightPane->panelStack()->panelSizes().isEmpty());
        int movedRow = -1;
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->index(row, 0).data().toString() == QStringLiteral("Moved")) {
                movedRow = row;
                break;
            }
        }
        QVERIFY(movedRow >= 0);
        QCOMPARE(model->index(movedRow, 0).data(static_cast<int>(
            ZzFluentUI::ZzActivityItemRole::Area)).value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftSecondary);
        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
    }

    void keepsActivityMoveResourceBudgetStableAcrossRoundTrips()
    {
        ZzShellFixture fixture;
        auto leftFirst = std::make_unique<QWidget>();
        auto leftSecond = std::make_unique<QWidget>();
        auto right = std::make_unique<QWidget>();
        QWidget *const leftFirstRaw = leftFirst.get();
        QWidget *const leftSecondRaw = leftSecond.get();
        QWidget *const rightRaw = right.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-first"), QStringLiteral("Left first"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftFirst.get()));
        zzReleaseAfterAdoption(leftFirst);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-second"), QStringLiteral("Left second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftSecond.get()));
        zzReleaseAfterAdoption(leftSecond);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, right.get()));
        zzReleaseAfterAdoption(right);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QAbstractItemModel *const leftModel = leftBar->model();
        QAbstractItemModel *const rightModel = rightBar->model();
        QVERIFY(leftModel != nullptr);
        QVERIFY(rightModel != nullptr);
        const auto resources = [&fixture] {
            return std::tuple{
                fixture.host.findChildren<QObject *>().size(),
                fixture.host.findChildren<QTimer *>().size(),
                fixture.host.findChildren<QAbstractAnimation *>().size()};
        };
        const auto baselineResources = resources();
        const QList<QWidget *> baselineLeft = leftPane->visibleWidgets();
        const QList<QWidget *> baselineRight = rightPane->visibleWidgets();
        const QList<QWidget *> baselineLeftPanels =
            leftPane->panelStack()->panels();
        const QList<QWidget *> baselineRightPanels =
            rightPane->panelStack()->panels();
        const int baselineLeftRows = leftModel->rowCount();
        const int baselineRightRows = rightModel->rowCount();
        const auto activityTitles = [&] {
            QStringList titles;
            for (QAbstractItemModel *const model : {leftModel}) {
                for (int row = 0; row < model->rowCount(); ++row) {
                    titles.append(model->index(row, 0).data().toString());
                }
            }
            std::sort(titles.begin(), titles.end());
            return titles;
        };
        const QStringList baselineActivityTitles = activityTitles();
        const QHash<QString, QWidget *> activityContents{
            {QStringLiteral("left-first"), leftFirstRaw},
            {QStringLiteral("left-second"), leftSecondRaw},
            {QStringLiteral("right"), rightRaw}};
        const QHash<QString, QString> activityTitlesById{
            {QStringLiteral("left-first"), QStringLiteral("Left first")},
            {QStringLiteral("left-second"), QStringLiteral("Left second")},
            {QStringLiteral("right"), QStringLiteral("Right")}};
        const QHash<int, QString> activityIdsByBadge{
            {101, QStringLiteral("left-first")},
            {202, QStringLiteral("left-second")},
            {303, QStringLiteral("right")}};
        const QHash<QString, ZzFluentUI::ZzActivityArea> activityAreasById{
            {QStringLiteral("left-first"),
             ZzFluentUI::ZzActivityArea::LeftPrimary},
            {QStringLiteral("left-second"),
             ZzFluentUI::ZzActivityArea::LeftPrimary},
            {QStringLiteral("right"),
             ZzFluentUI::ZzActivityArea::RightPrimary}};
        QVERIFY(fixture.shell->setPanelBadge(
            zzPanelId("left-first"), 101));
        QVERIFY(fixture.shell->setPanelBadge(
            zzPanelId("left-second"), 202));
        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("right"), 303));
        const auto assertActivityIdentity = [&] {
            QSet<QString> seenIds;
            for (QAbstractItemModel *const model : {leftModel}) {
                for (int row = 0; row < model->rowCount(); ++row) {
                    const QModelIndex index = model->index(row, 0);
                    const auto id = activityIdsByBadge.constFind(
                        index.data(static_cast<int>(
                            ZzFluentUI::ZzActivityItemRole::Badge)).toInt());
                    if (id == activityIdsByBadge.cend()
                        || seenIds.contains(id.value())
                        || index.data().toString()
                            != activityTitlesById.value(id.value())
                        || index.data(static_cast<int>(
                               ZzFluentUI::ZzActivityItemRole::Area))
                            .value<ZzFluentUI::ZzActivityArea>()
                            != activityAreasById.value(id.value())
                        || activityContents.value(id.value()) == nullptr) {
                        return false;
                    }
                    seenIds.insert(id.value());
                }
            }
            if (seenIds.size() != activityContents.size()
                || !seenIds.contains(QStringLiteral("left-first"))
                || !seenIds.contains(QStringLiteral("left-second"))
                || !seenIds.contains(QStringLiteral("right"))) {
                return false;
            }
            const auto assertStack = [&](ZzFluentUI::ZzSidePane *pane) {
                if (pane == nullptr || pane->panelStack() == nullptr) {
                    return false;
                }
                for (QWidget *const content : pane->panelStack()->panels()) {
                    const QString id = activityContents.key(
                        content, QString{});
                    if (id.isEmpty() || !pane->isAncestorOf(content)
                        || !pane->panelStack()->isAncestorOf(content)) {
                        return false;
                    }
                }
                return true;
            };
            QSet<QString> stackedIds;
            for (ZzFluentUI::ZzSidePane *const pane : {leftPane, rightPane}) {
                for (QWidget *const content : pane->panelStack()->panels()) {
                    const QString id = activityContents.key(content, QString{});
                    if (id.isEmpty() || stackedIds.contains(id)) {
                        return false;
                    }
                    stackedIds.insert(id);
                }
            }
            return assertStack(leftPane) && assertStack(rightPane)
                && stackedIds == seenIds;
        };
        QVERIFY(assertActivityIdentity());

        for (int iteration = 0; iteration < 1000; ++iteration) {
            Q_EMIT leftBar->moveRequested(
                leftModel->index(0, 0),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1);
            QCOMPARE(leftPane->visibleWidgets(),
                QList<QWidget *>({leftSecondRaw}));
            QCOMPARE(leftPane->panelStack()->panels(),
                QList<QWidget *>({leftSecondRaw, leftFirstRaw}));
            QCOMPARE(rightPane->panelStack()->panels(), baselineRightPanels);
            QVERIFY(leftPane->isAncestorOf(leftFirstRaw));
            QVERIFY(leftPane->isAncestorOf(leftSecondRaw));
            QVERIFY(rightPane->isAncestorOf(rightRaw));
            Q_EMIT leftBar->moveRequested(
                leftModel->index(1, 0),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
            QCOMPARE(leftPane->visibleWidgets(), baselineLeft);
            QCOMPARE(leftPane->panelStack()->panels(), baselineLeftPanels);
            Q_EMIT leftBar->moveRequested(
                leftModel->index(0, 0),
                ZzFluentUI::ZzActivityArea::RightPrimary,
                rightModel->rowCount());
            QCOMPARE(leftPane->visibleWidgets(),
                QList<QWidget *>({leftSecondRaw}));
            QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({rightRaw}));
            QCOMPARE(leftPane->panelStack()->panels(),
                QList<QWidget *>({leftSecondRaw}));
            QCOMPARE(rightPane->panelStack()->panels(),
                QList<QWidget *>({rightRaw, leftFirstRaw}));
            QVERIFY(!leftPane->isAncestorOf(leftFirstRaw));
            QVERIFY(rightPane->isAncestorOf(leftFirstRaw));
            Q_EMIT rightBar->moveRequested(
                rightModel->index(rightModel->rowCount() - 1, 0),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
            QCOMPARE(leftPane->visibleWidgets(), baselineLeft);
            QCOMPARE(rightPane->visibleWidgets(), baselineRight);
            QCOMPARE(leftPane->panelStack()->panels(), baselineLeftPanels);
            QCOMPARE(rightPane->panelStack()->panels(), baselineRightPanels);
            QCOMPARE(activityTitles(), baselineActivityTitles);
        }

        QCOMPARE(resources(), baselineResources);
        QCOMPARE(leftPane->visibleWidgets(), baselineLeft);
        QCOMPARE(rightPane->visibleWidgets(), baselineRight);
        QCOMPARE(leftModel->rowCount(), baselineLeftRows);
        QCOMPARE(rightModel->rowCount(), baselineRightRows);
        QCOMPARE(leftPane->panelStack()->panels(), baselineLeftPanels);
        QCOMPARE(rightPane->panelStack()->panels(), baselineRightPanels);
        QCOMPARE(activityTitles(), baselineActivityTitles);
        QVERIFY(leftPane->isAncestorOf(leftFirstRaw));
        QVERIFY(leftPane->isAncestorOf(leftSecondRaw));
        QVERIFY(rightPane->isAncestorOf(rightRaw));
        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("left-first"), 101));
        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("left-second"), 202));
        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("right"), 303));
        QVERIFY(!fixture.shell->setPanelBadge(zzPanelId("ghost"), 404));
        QVERIFY(fixture.shell->showPanel(zzPanelId("left-first"), true));
        QVERIFY(fixture.shell->showPanel(zzPanelId("left-second"), true));
        QVERIFY(fixture.shell->showPanel(zzPanelId("right"), true));
        QVERIFY(assertActivityIdentity());
    }

    void keepsObjectBudgetStableAcrossRepeatedTransactions()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        auto bottomFirst = std::make_unique<QWidget>();
        auto bottomSecond = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QWidget *const secondRaw = second.get();
        QWidget *const bottomFirstRaw = bottomFirst.get();
        QWidget *const bottomSecondRaw = bottomSecond.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        pane->setPaneWidth(347);
        Q_EMIT bar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary, 0);
        QVERIFY(fixture.shell->showPanel(zzPanelId("first"), true));
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom-first"), QStringLiteral("Bottom first"),
            zzIcon(), bottomFirst.get()));
        zzReleaseAfterAdoption(bottomFirst);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom-second"), QStringLiteral("Bottom second"),
            zzIcon(), bottomSecond.get()));
        zzReleaseAfterAdoption(bottomSecond);
        auto *const bottomPane = fixture.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(420);
        bottomPane->setCollapsed(false);
        QVERIFY(bottomPane->setCurrentWidget(bottomSecondRaw));
        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        const QList<QWidget *> expectedVisible{firstRaw};
        const QList<QWidget *> expectedPanels = pane->panelStack()->panels();
        const QList<QWidget *> mutatedPanels{
            firstRaw, secondRaw};
        const int expectedWidth = pane->paneWidth();
        QWidget *const expectedCurrent = pane->currentWidget();
        const QStringList expectedTitles{
            QStringLiteral("Second"), QStringLiteral("First")};
        QStackedWidget *const bottomStack =
            bottomPane->findChild<QStackedWidget *>();
        QVERIFY(bottomStack != nullptr);
        QTabBar *const bottomTabs = bottomPane->findChild<QTabBar *>();
        QVERIFY(bottomTabs != nullptr);
        const QList<QWidget *> expectedBottomPanels{
            bottomStack->widget(0), bottomStack->widget(1)};
        const int expectedBottomHeight = bottomPane->paneHeight();
        QWidget *const expectedBottomCurrent = bottomPane->currentWidget();
        const auto resources = [&fixture] {
            return std::tuple{
                fixture.host.findChildren<QObject *>().size(),
                fixture.host.findChildren<QTimer *>().size(),
                fixture.host.findChildren<QAbstractAnimation *>().size()};
        };
        const auto baselineResources = resources();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            pane->setPaneWidth(211);
            Q_EMIT bar->moveRequested(
                model->index(0, 0),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1);
            QVERIFY(fixture.shell->showPanel(zzPanelId("second"), true));
            QVERIFY(pane->currentWidget() != expectedCurrent);
            bottomPane->setPaneHeight(240);
            bottomPane->setCollapsed(true);
            QVERIFY(bottomPane->setCurrentWidget(bottomFirstRaw));
            QVERIFY(bottomPane->currentWidget() != expectedBottomCurrent);
            QCOMPARE(pane->paneWidth(), 211);
            QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({secondRaw}));
            QCOMPARE(pane->panelStack()->panels(), mutatedPanels);
            QVERIFY(fixture.shell->restoreLayout(saved.value()));
            QCOMPARE(pane->paneWidth(), expectedWidth);
            QCOMPARE(pane->visibleWidgets(), expectedVisible);
            QCOMPARE(pane->panelStack()->panels(), expectedPanels);
            QCOMPARE(pane->currentWidget(), expectedCurrent);
            QCOMPARE(model->rowCount(), 2);
            QCOMPARE(model->index(0, 0).data().toString(), expectedTitles.at(0));
            QCOMPARE(model->index(1, 0).data().toString(), expectedTitles.at(1));
            QVERIFY(pane->isAncestorOf(firstRaw));
            QVERIFY(pane->isAncestorOf(secondRaw));
            const auto missingBottom = fixture.shell->takePanel(
                zzPanelId("bottom-ghost"));
            QVERIFY(!missingBottom.hasValue());
            QCOMPARE(bottomPane->paneHeight(), expectedBottomHeight);
            QCOMPARE(bottomPane->currentWidget(), expectedBottomCurrent);
            QVERIFY(!bottomPane->isCollapsed());
            QCOMPARE(bottomPane->widgetCount(), 2);
            QCOMPARE(bottomStack->count(), 2);
            QCOMPARE(bottomTabs->tabText(0), QStringLiteral("Bottom first"));
            QCOMPARE(bottomTabs->tabText(1), QStringLiteral("Bottom second"));
            QCOMPARE(bottomStack->widget(0), expectedBottomPanels.at(0));
            QCOMPARE(bottomStack->widget(1), expectedBottomPanels.at(1));
            QVERIFY(bottomPane->isAncestorOf(bottomFirstRaw));
            QVERIFY(bottomPane->isAncestorOf(bottomSecondRaw));
        }

        QCOMPARE(resources(), baselineResources);
        QCOMPARE(bottomPane->widgetCount(), 2);
        QCOMPARE(bottomStack->count(), 2);
        QCOMPARE(bottomStack->widget(0), bottomFirstRaw);
        QCOMPARE(bottomStack->widget(1), bottomSecondRaw);
        QCOMPARE(bottomTabs->tabText(0), QStringLiteral("Bottom first"));
        QCOMPARE(bottomTabs->tabText(1), QStringLiteral("Bottom second"));
        const auto takenBottomFirst = fixture.shell->takePanel(
            zzPanelId("bottom-first"));
        QVERIFY(takenBottomFirst.hasValue());
        QCOMPARE(takenBottomFirst.value(), bottomFirstRaw);
        QVERIFY(bottomFirstRaw->parent() == nullptr);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom-first"), QStringLiteral("Bottom first"),
            zzIcon(), bottomFirstRaw));
        const auto takenBottomSecond = fixture.shell->takePanel(
            zzPanelId("bottom-second"));
        QVERIFY(takenBottomSecond.hasValue());
        QCOMPARE(takenBottomSecond.value(), bottomSecondRaw);
        QVERIFY(bottomSecondRaw->parent() == nullptr);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom-second"), QStringLiteral("Bottom second"),
            zzIcon(), bottomSecondRaw));
    }

    void keepsRestoreFailureResourceBudgetStableAcrossSignalPollution()
    {
        ZzShellFixture splitSource;
        auto *const sourceSplit = splitSource.shell->splitWorkspace();
        QVERIFY(sourceSplit->splitGroup(
            sourceSplit->activeGroupId(), Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("requested")))
                    .has_value());
        const auto splitRequested = splitSource.shell->saveLayout();
        QVERIFY(splitRequested);

        ZzShellFixture splitTarget;
        auto *const targetSplit = splitTarget.shell->splitWorkspace();
        auto splitSentinel = std::make_unique<QWidget>();
        QWidget *const splitSentinelRaw = splitSentinel.get();
        QVERIFY(splitTarget.shell->registerSidePanel(
            zzPanelId("split-sentinel"), QStringLiteral("Split sentinel"),
            zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
            splitSentinel.get()));
        zzReleaseAfterAdoption(splitSentinel);
        auto *const splitPane = splitTarget.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const splitBar = splitTarget.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        const QList<QWidget *> splitBaselinePanels =
            splitPane->panelStack()->panels();
        const QStringList splitBaselineTitles{
            QStringLiteral("Split sentinel")};
        const QByteArray splitBefore = targetSplit->saveLayout();
        const auto splitResources = [&splitTarget] {
            return std::tuple{
                splitTarget.host.findChildren<QObject *>().size(),
                splitTarget.host.findChildren<QTimer *>().size(),
                splitTarget.host.findChildren<QAbstractAnimation *>().size()};
        };
        const auto splitBaselineResources = splitResources();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            bool armed = true;
            const QMetaObject::Connection connection = QObject::connect(
                targetSplit, &ZzFluentUI::ZzSplitWorkspace::layoutChanged,
                splitTarget.shell.get(), [&] {
                    if (!armed) {
                        return;
                    }
                    armed = false;
                    static_cast<void>(targetSplit->splitGroup(
                        targetSplit->groupIds().constFirst(), Qt::Vertical,
                        ZzFluentUI::ZzSplitPlacement::After,
                        ZzFluentUI::ZzTabGroupId(
                            QStringLiteral("pollution"))));
                });
            const auto restored = splitTarget.shell->restoreLayout(
                splitRequested.value());
            QObject::disconnect(connection);
            QVERIFY(!restored);
            QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
            QVERIFY(!armed);
            QCOMPARE(targetSplit->saveLayout(), splitBefore);
            QCOMPARE(splitPane->panelStack()->panels(), splitBaselinePanels);
            QCOMPARE(splitBar->model()->rowCount(), 1);
            QCOMPARE(splitBar->model()->index(0, 0).data().toString(),
                splitBaselineTitles.constFirst());
            QVERIFY(splitPane->isAncestorOf(splitSentinelRaw));
            QCOMPARE(splitResources(), splitBaselineResources);
        }
        QCOMPARE(targetSplit->groupIds().size(), 1);
        QVERIFY(splitTarget.shell->setPanelBadge(
            zzPanelId("split-sentinel"), 11));
        QVERIFY(!splitTarget.shell->setPanelBadge(
            zzPanelId("split-ghost"), 11));

        ZzShellFixture sideSource;
        auto sourceSide = std::make_unique<QWidget>();
        QVERIFY(sideSource.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSide.get()));
        zzReleaseAfterAdoption(sourceSide);
        sideSource.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(411);
        const auto sideRequested = sideSource.shell->saveLayout();
        QVERIFY(sideRequested);

        ZzShellFixture sideTarget;
        auto targetSide = std::make_unique<QWidget>();
        QWidget *const targetSideRaw = targetSide.get();
        QVERIFY(sideTarget.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSide.get()));
        zzReleaseAfterAdoption(targetSide);
        auto *const targetSidePane = sideTarget.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const targetSideBar = sideTarget.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        const int sideRows = targetSideBar->model()->rowCount();
        const QList<QWidget *> sideBaselinePanels =
            targetSidePane->panelStack()->panels();
        const QString sideBaselineTitle =
            targetSideBar->model()->index(0, 0).data().toString();
        const auto sideResources = [&sideTarget] {
            return std::tuple{
                sideTarget.host.findChildren<QObject *>().size(),
                sideTarget.host.findChildren<QTimer *>().size(),
                sideTarget.host.findChildren<QAbstractAnimation *>().size()};
        };
        const auto sideBaselineResources = sideResources();
        const int baselineWidth = targetSidePane->paneWidth();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            bool armed = true;
            const QMetaObject::Connection connection = QObject::connect(
                targetSidePane, &ZzFluentUI::ZzSidePane::paneWidthChanged,
                sideTarget.shell.get(), [&](int width) {
                    if (!armed || width != 411) {
                        return;
                    }
                    armed = false;
                    targetSidePane->setPaneWidth(333);
                });
            const auto restored = sideTarget.shell->restoreLayout(
                sideRequested.value());
            QObject::disconnect(connection);
            QVERIFY(!restored);
            QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
            QVERIFY(!armed);
            QCOMPARE(targetSidePane->paneWidth(), baselineWidth);
            QCOMPARE(sideTarget.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->visibleWidgets(),
                QList<QWidget *>({targetSideRaw}));
            QCOMPARE(targetSideBar->model()->rowCount(), sideRows);
            QCOMPARE(targetSideBar->model()->index(0, 0).data().toString(),
                sideBaselineTitle);
            QCOMPARE(targetSidePane->panelStack()->panels(),
                sideBaselinePanels);
            QVERIFY(targetSidePane->isAncestorOf(targetSideRaw));
            QCOMPARE(sideResources(), sideBaselineResources);
        }
        QVERIFY(sideTarget.shell->setPanelBadge(zzPanelId("side"), 7));
        QVERIFY(!sideTarget.shell->setPanelBadge(zzPanelId("ghost"), 7));
        QVERIFY(sideTarget.shell->showPanel(zzPanelId("side"), true));

        ZzShellFixture bottomSource;
        auto sourceBottomFirst = std::make_unique<QWidget>();
        auto sourceBottomSecond = std::make_unique<QWidget>();
        auto sourceDockContent = std::make_unique<QWidget>();
        QWidget *const sourceBottomSecondRaw = sourceBottomSecond.get();
        QVERIFY(bottomSource.shell->registerBottomPanel(
            zzPanelId("bottom-first"), QStringLiteral("Bottom first"),
            zzIcon(), sourceBottomFirst.get()));
        zzReleaseAfterAdoption(sourceBottomFirst);
        QVERIFY(bottomSource.shell->registerBottomPanel(
            zzPanelId("bottom-second"), QStringLiteral("Bottom second"),
            zzIcon(), sourceBottomSecond.get()));
        zzReleaseAfterAdoption(sourceBottomSecond);
        QVERIFY(bottomSource.shell->registerDockPanel(
            zzPanelId("bottom-dock"), QStringLiteral("Bottom dock"),
            zzIcon(), Qt::RightDockWidgetArea, sourceDockContent.get()));
        zzReleaseAfterAdoption(sourceDockContent);
        bottomSource.shell->bottomPane()->setMaximumPaneHeight(800);
        bottomSource.shell->bottomPane()->setPaneHeight(500);
        bottomSource.shell->bottomPane()->setCollapsed(false);
        QVERIFY(bottomSource.shell->bottomPane()->setCurrentWidget(
            sourceBottomSecondRaw));
        const auto bottomRequested = bottomSource.shell->saveLayout();
        QVERIFY(bottomRequested);

        ZzShellFixture bottomTarget;
        auto targetBottomFirst = std::make_unique<QWidget>();
        auto targetBottomSecond = std::make_unique<QWidget>();
        auto targetDockContent = std::make_unique<QWidget>();
        QWidget *const targetBottomFirstRaw = targetBottomFirst.get();
        QWidget *const targetBottomSecondRaw = targetBottomSecond.get();
        QWidget *const targetDockContentRaw = targetDockContent.get();
        QVERIFY(bottomTarget.shell->registerBottomPanel(
            zzPanelId("bottom-first"), QStringLiteral("Bottom first"),
            zzIcon(), targetBottomFirst.get()));
        zzReleaseAfterAdoption(targetBottomFirst);
        QVERIFY(bottomTarget.shell->registerBottomPanel(
            zzPanelId("bottom-second"), QStringLiteral("Bottom second"),
            zzIcon(), targetBottomSecond.get()));
        zzReleaseAfterAdoption(targetBottomSecond);
        QVERIFY(bottomTarget.shell->registerDockPanel(
            zzPanelId("bottom-dock"), QStringLiteral("Bottom dock"),
            zzIcon(), Qt::LeftDockWidgetArea, targetDockContent.get()));
        zzReleaseAfterAdoption(targetDockContent);
        auto *const bottomTargetPane = bottomTarget.shell->bottomPane();
        bottomTargetPane->setMaximumPaneHeight(800);
        bottomTargetPane->setPaneHeight(240);
        bottomTargetPane->setCollapsed(true);
        QVERIFY(bottomTargetPane->setCurrentWidget(targetBottomFirstRaw));
        QStackedWidget *const bottomTargetStack =
            bottomTargetPane->findChild<QStackedWidget *>();
        QVERIFY(bottomTargetStack != nullptr);
        const QList<QWidget *> bottomTargetPanels{
            targetBottomFirstRaw, targetBottomSecondRaw};
        auto *const targetDockPanel = bottomTarget.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:bottom-dock"));
        QVERIFY(targetDockPanel != nullptr);
        const QString targetDockObjectName = targetDockPanel->objectName();
        const Qt::DockWidgetArea targetDockArea =
            bottomTarget.host.dockWidgetArea(targetDockPanel);
        const int bottomTargetHeight = bottomTargetPane->paneHeight();
        const bool bottomTargetCollapsed = bottomTargetPane->isCollapsed();
        QWidget *const bottomTargetCurrent = bottomTargetPane->currentWidget();
        const auto bottomResources = [&bottomTarget] {
            return std::tuple{
                bottomTarget.host.findChildren<QObject *>().size(),
                bottomTarget.host.findChildren<QTimer *>().size(),
                bottomTarget.host.findChildren<QAbstractAnimation *>().size()};
        };
        const auto bottomBaselineResources = bottomResources();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            bool armed = true;
            const QMetaObject::Connection connection = QObject::connect(
                bottomTargetPane,
                &ZzFluentUI::ZzBottomPane::paneHeightChanged,
                bottomTarget.shell.get(), [&](int height) {
                    if (!armed || height != 500) {
                        return;
                    }
                    armed = false;
                    bottomTargetPane->setPaneHeight(333);
                });
            const auto restored = bottomTarget.shell->restoreLayout(
                bottomRequested.value());
            QObject::disconnect(connection);
            QVERIFY(!restored);
            QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
            QVERIFY(!armed);
            QCOMPARE(bottomTargetPane->paneHeight(), bottomTargetHeight);
            QCOMPARE(bottomTargetPane->isCollapsed(), bottomTargetCollapsed);
            QCOMPARE(bottomTargetPane->currentWidget(), bottomTargetCurrent);
            QCOMPARE(bottomTargetPane->widgetCount(), 2);
            QCOMPARE(bottomTargetStack->count(), 2);
            QCOMPARE(bottomTargetStack->widget(0), bottomTargetPanels.at(0));
            QCOMPARE(bottomTargetStack->widget(1), bottomTargetPanels.at(1));
            QVERIFY(bottomTargetPane->isAncestorOf(targetBottomFirstRaw));
            QVERIFY(bottomTargetPane->isAncestorOf(targetBottomSecondRaw));
            QCOMPARE(targetDockPanel->objectName(), targetDockObjectName);
            QCOMPARE(targetDockPanel->widget(), targetDockContentRaw);
            QCOMPARE(targetDockPanel->widget()->parentWidget(), targetDockPanel);
            QCOMPARE(bottomTarget.host.dockWidgetArea(targetDockPanel), targetDockArea);
            QVERIFY(!targetDockPanel->isFloating());
            QVERIFY(!targetDockPanel->isHidden());
            const auto missingBottom = bottomTarget.shell->takePanel(
                zzPanelId("bottom-ghost"));
            QVERIFY(!missingBottom.hasValue());
            QVERIFY(!bottomTarget.shell->showPanel(zzPanelId("bottom-ghost"), true));
            const auto missingDock = bottomTarget.shell->takePanel(
                zzPanelId("dock-ghost"));
            QVERIFY(!missingDock.hasValue());
            QVERIFY(!bottomTarget.shell->showPanel(zzPanelId("dock-ghost"), true));
            QCOMPARE(bottomResources(), bottomBaselineResources);
        }
    }

    void activityMoveStopsWhenAnEarlierPanelIsReparented()
    {
        ZzShellFixture fixture;
        QWidget thirdParty;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        auto third = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QWidget *const secondRaw = second.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("third"), QStringLiteral("Third"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, third.get()));
        zzReleaseAfterAdoption(third);

        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = bar->model();
        auto *const stack = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->panelStack();
        QList<QPair<QWidget *, int>> moveSignals;
        QList<QStringList> resetOrders;
        QObject::connect(
            stack, &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *content, int index) {
                moveSignals.append({content, index});
                if (moveSignals.size() == 1 && content == firstRaw && index == 2) {
                    secondRaw->setParent(&thirdParty);
                }
            });
        QObject::connect(
            model, &QAbstractItemModel::modelReset,
            fixture.shell.get(), [&] {
                QStringList order;
                for (int row = 0; row < model->rowCount(); ++row) {
                    order.append(model->index(row, 0).data().toString());
                }
                resetOrders.append(order);
            });

        Q_EMIT bar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            2);

        QVERIFY(!moveSignals.isEmpty());
        const QPair<QWidget *, int> firstMove(firstRaw, 2);
        QCOMPARE(moveSignals.constFirst(), firstMove);
        for (const QPair<QWidget *, int> &move : moveSignals) {
            QCOMPARE(move.first, firstRaw);
            QVERIFY(move.second == 2 || move.second == 0);
        }
        QCOMPARE(secondRaw->parentWidget(), &thirdParty);
        QVERIFY(!resetOrders.contains(
            {QStringLiteral("Second"), QStringLiteral("Third"),
                QStringLiteral("First")}));
    }

    void rollsBackActivityMoveWhenTargetIsDestroyedSynchronously()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        auto target = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, target.get()));
        zzReleaseAfterAdoption(target);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({456}));
        leftPane->setPaneWidth(337);
        QPointer<ZzFluentUI::ZzSidePane> rightGuard(rightPane);
        bool callbackEntered = false;
        QObject::connect(
            rightPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (current != movedRaw || callbackEntered) {
                    return;
                }
                callbackEntered = true;
                current->setParent(nullptr);
                delete rightGuard.data();
            });

        Q_EMIT leftBar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(callbackEntered);
        QVERIFY(rightGuard.isNull());
        QCOMPARE(
            leftPane->visibleWidgets(),
            QList<QWidget *>({movedRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({456}));
        QCOMPARE(leftPane->paneWidth(), 337);
        QVERIFY(!leftPane->isCollapsed());
        QVERIFY(leftPane->isAncestorOf(movedRaw));
        QCOMPARE(
            model->index(1, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);
        QCOMPARE(leftBar->activeSourceIndexes().size(), 1);
    }

    void activityMoveRollbackDoesNotDetachNonMovedPanelFromLivePane()
    {
        ZzShellFixture fixture;
        auto moved = std::make_unique<QWidget>();
        auto nonMoved = std::make_unique<QWidget>();
        QWidget *const nonMovedRaw = nonMoved.get();
        QVERIFY(fixture.shell->registerSidePanel(zzPanelId("guard-moved"),
            QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(zzPanelId("guard-non-moved"),
            QStringLiteral("Non moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, nonMoved.get()));
        zzReleaseAfterAdoption(nonMoved);
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QWidget> owner;
        QWidget *rawOwner = nullptr;
        QPointer<ZzFluentUI::ZzSidePane> rightGuard(rightPane);
        bool callbackEntered = false;
        QObject::connect(rightPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current == nullptr) {
                    return;
                }
                callbackEntered = true;
                QVERIFY(rightPane->takeWidget(nonMovedRaw) == nonMovedRaw);
                QVERIFY(leftPane->addWidget(nonMovedRaw, QStringLiteral("Non moved")));
                owner = nonMovedRaw->parentWidget();
                rawOwner = owner.data();
                delete rightGuard.data();
            });

        Q_EMIT bar->moveRequested(bar->model()->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary, 0);

        QVERIFY(callbackEntered);
        QVERIFY(rightGuard.isNull());
        QVERIFY(nonMovedRaw != nullptr);
        QVERIFY(owner != nullptr);
        QCOMPARE(nonMovedRaw->parentWidget(), rawOwner);
        QVERIFY(leftPane->panelStack()->panels().contains(nonMovedRaw));
        QVERIFY(leftPane->isAncestorOf(nonMovedRaw));
        QVERIFY(leftPane->panelStack()->isAncestorOf(nonMovedRaw));
        QWidget *const taken = leftPane->takeWidget(nonMovedRaw);
        QCOMPARE(taken, nonMovedRaw);
        delete taken;
    }

    void rollsBackActivityMoveWhenModelResetOverwritesPaneState()
    {
        ZzShellFixture fixture;
        auto moved = std::make_unique<QWidget>();
        auto stayed = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QWidget *const stayedRaw = stayed.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("stayed"), QStringLiteral("Stayed"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, stayed.get()));
        zzReleaseAfterAdoption(stayed);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({123}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({789}));
        bool callbackEntered = false;
        bool overwriteAccepted = false;
        QObject::connect(
            model, &QAbstractItemModel::modelReset,
            fixture.shell.get(), [&] {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                overwriteAccepted = rightPane->setWidgetVisible(movedRaw, false);
            });

        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(callbackEntered);
        QVERIFY(overwriteAccepted);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({movedRaw}));
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({stayedRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({789}));
        QCOMPARE(
            model->index(0, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);
    }

    void cleansActivityMoveWhenContentIsDestroyedSynchronously()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto moved = std::make_unique<QWidget>();
        auto target = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QWidget *const movedRaw = moved.get();
        QWidget *const targetRaw = target.get();
        QPointer<QWidget> movedGuard(movedRaw);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, target.get()));
        zzReleaseAfterAdoption(target);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({456}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({789}));
        bool callbackEntered = false;
        QObject::connect(
            rightPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
            if (current != movedRaw || callbackEntered) {
                return;
            }
            callbackEntered = true;
            delete movedGuard.data();
        });

        Q_EMIT leftBar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(callbackEntered);
        QVERIFY(movedGuard.isNull());
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({firstRaw}));
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({targetRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({789}));
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(leftBar->activeSourceIndexes().size(), 1);
    }

    void cleansActivityMoveWhenSourceIsDestroyedSynchronously()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto moved = std::make_unique<ZzParentChangeWidget>();
        auto target = std::make_unique<QWidget>();
        QPointer<QWidget> firstGuard(first.get());
        ZzParentChangeWidget *const movedRaw = moved.get();
        QWidget *const targetRaw = target.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, target.get()));
        zzReleaseAfterAdoption(target);

        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(rightPane->panelStack()->setPanelSizes({789}));
        QPointer<ZzFluentUI::ZzSidePane> sourceGuard(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left));
        bool armed = true;
        movedRaw->parentChanged = [&] {
            if (!armed) {
                return;
            }
            armed = false;
            delete sourceGuard.data();
        };

        Q_EMIT leftBar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(!armed);
        QVERIFY(sourceGuard.isNull());
        QVERIFY(firstGuard.isNull());
        QCOMPARE(movedRaw->parentWidget(), nullptr);
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({targetRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({789}));
        QCOMPARE(model->rowCount(), 1);
        delete movedRaw;
    }

    void leavesThirdPartyOwnerWhenActivityMoveIsInterceptedSynchronously()
    {
        ZzShellFixture fixture;
        QWidget thirdParty;
        auto first = std::make_unique<QWidget>();
        auto moved = std::make_unique<ZzParentRemovedWidget>();
        auto target = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        ZzParentRemovedWidget *const movedRaw = moved.get();
        QWidget *const targetRaw = target.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, target.get()));
        zzReleaseAfterAdoption(target);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({456}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({789}));
        bool armed = true;
        movedRaw->parentRemoved = [&] {
            if (!armed) {
                return;
            }
            armed = false;
            movedRaw->setParent(&thirdParty);
        };

        Q_EMIT leftBar->moveRequested(
            model->index(1, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(!armed);
        QCOMPARE(movedRaw->parentWidget(), &thirdParty);
        QVERIFY(!leftPane->isAncestorOf(movedRaw));
        QVERIFY(!rightPane->isAncestorOf(movedRaw));
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({firstRaw}));
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({targetRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({789}));
        QCOMPARE(model->rowCount(), 2);
    }

    void rejectsNestedSideRegistrationDuringActivityMove()
    {
        ZzShellFixture fixture;
        auto moved = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const movedRaw = moved.get();
        auto target = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, target.get()));
        zzReleaseAfterAdoption(target);

        auto nested = std::make_unique<QWidget>();
        bool callbackEntered = false;
        bool nestedAccepted = false;
        movedRaw->parentRemoved = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            const auto result = fixture.shell->registerSidePanel(
                zzPanelId("nested"), QStringLiteral("Nested"), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, nested.get());
            nestedAccepted = static_cast<bool>(result);
            if (result) {
                zzReleaseAfterAdoption(nested);
            }
        };

        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(callbackEntered);
        QVERIFY(!nestedAccepted);
        QVERIFY(nested != nullptr);
        QCOMPARE(nested->parentWidget(), nullptr);
        QCOMPARE(model->rowCount(), 2);
        QVERIFY(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->isAncestorOf(movedRaw));
    }

    void activityMoveRejectsPaneDestroyedByPanelMovedSignal()
    {
        ZzShellFixture fixture;
        QWidget thirdParty;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QPointer<QWidget> firstGuard(first.get());
        QPointer<QWidget> secondGuard(second.get());
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);

        QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left));
        bool callbackEntered = false;
        QObject::connect(
            leftPaneGuard->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *, int) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                firstGuard->setParent(&thirdParty);
                secondGuard->setParent(&thirdParty);
                delete leftPaneGuard.data();
            });
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();

        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            1);

        QVERIFY(callbackEntered);
        QVERIFY(leftPaneGuard.isNull());
        QCOMPARE(firstGuard->parentWidget(), &thirdParty);
        QCOMPARE(secondGuard->parentWidget(), &thirdParty);
    }

    void activityMoveRollbackRejectsPaneDestroyedByPanelMovedSignal()
    {
        ZzShellFixture fixture;
        QWidget thirdParty;
        auto moved = std::make_unique<ZzParentRemovedWidget>();
        auto stayed = std::make_unique<QWidget>();
        auto target = std::make_unique<QWidget>();
        ZzParentRemovedWidget *const movedRaw = moved.get();
        QPointer<QWidget> movedGuard(moved.get());
        QPointer<QWidget> stayedGuard(stayed.get());
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("stayed"), QStringLiteral("Stayed"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, stayed.get()));
        zzReleaseAfterAdoption(stayed);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, target.get()));
        zzReleaseAfterAdoption(target);

        QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left));
        QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right));
        bool rollbackArmed = false;
        bool rollbackCallbackEntered = false;
        QObject::connect(
            leftPaneGuard->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            fixture.shell.get(), [&](QWidget *, int) {
                if (!rollbackArmed || rollbackCallbackEntered) {
                    return;
                }
                rollbackCallbackEntered = true;
                movedGuard->setParent(&thirdParty);
                stayedGuard->setParent(&thirdParty);
                delete leftPaneGuard.data();
            });
        movedRaw->parentRemoved = [&] {
            if (rollbackArmed) {
                return;
            }
            rollbackArmed = true;
            delete rightPaneGuard.data();
        };
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();

        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(rollbackArmed);
        QVERIFY(rollbackCallbackEntered);
        QVERIFY(leftPaneGuard.isNull());
        QVERIFY(rightPaneGuard.isNull());
        QCOMPARE(movedGuard->parentWidget(), &thirdParty);
        QCOMPARE(stayedGuard->parentWidget(), &thirdParty);
    }

    void activitySyncRejectsPaneDestroyedByActiveStateSignal()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QPointer<QWidget> contentGuard(content.get());
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left));
        QVERIFY(fixture.shell->showPanel(zzPanelId("side"), false));
        bool callbackEntered = false;
        QObject::connect(
            leftPaneGuard, &ZzFluentUI::ZzSidePane::collapsedChanged,
            fixture.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                delete leftPaneGuard.data();
            });

        const auto hidden = fixture.shell->showPanel(zzPanelId("side"), true);

        QVERIFY(callbackEntered);
        QVERIFY(leftPaneGuard.isNull());
        QVERIFY(contentGuard.isNull());
        QVERIFY(!hidden);
        QCOMPARE(hidden.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void activityMoveKeepsSingleActiveAtDestination()
    {
        ZzShellFixture fixture;
        auto moved = std::make_unique<QWidget>();
        auto stayed = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("stayed"), QStringLiteral("Stayed"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, stayed.get()));
        zzReleaseAfterAdoption(stayed);

        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        Q_EMIT leftBar->moveRequested(
            leftBar->model()->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary, 0);
        QCOMPARE(rightBar->activeSourceIndexes().size(), 1);
        QCOMPARE(rightBar->currentSourceIndex(),
            rightBar->activeSourceIndexes().constFirst());
    }

    void sideKeepsSingleActiveAfterRegistration()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QCOMPARE(bar->activeSourceIndexes().size(), 1);
        QCOMPARE(bar->currentSourceIndex(),
            bar->activeSourceIndexes().constFirst());
    }

    void activityMoveSynchronizesEdgeVisibilityBothDirections()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("only"), QStringLiteral("Only"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QAbstractItemModel *const model = leftBar->model();

        QVERIFY(!leftBar->isHidden());
        QVERIFY(rightBar->isHidden());
        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);
        QVERIFY(leftBar->isHidden());
        QVERIFY(!rightBar->isHidden());

        Q_EMIT rightBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            0);
        QVERIFY(!leftBar->isHidden());
        QVERIFY(rightBar->isHidden());
        QCOMPARE(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
                ->currentWidget(),
            contentRaw);
    }

    void activityMoveRejectsThirdPartyOwnerAfterFinalSizesSignal()
    {
        ZzShellFixture fixture;
        QWidget thirdParty;
        auto moved = std::make_unique<QWidget>();
        auto stayed = std::make_unique<QWidget>();
        QWidget *const movedRaw = moved.get();
        QWidget *const stayedRaw = stayed.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("stayed"), QStringLiteral("Stayed"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, stayed.get()));
        zzReleaseAfterAdoption(stayed);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(leftPane->panelStack()->setPanelSizes({123}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({789}));
        bool callbackEntered = false;
        QObject::connect(
            leftPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != nullptr) {
                    return;
                }
                callbackEntered = true;
                movedRaw->setParent(&thirdParty);
            });

        Q_EMIT leftBar->moveRequested(
            model->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(callbackEntered);
        QCOMPARE(movedRaw->parentWidget(), &thirdParty);
        QVERIFY(!leftPane->isAncestorOf(movedRaw));
        QVERIFY(!rightPane->isAncestorOf(movedRaw));
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>());
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({stayedRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({789}));
        QCOMPARE(model->rowCount(), 1);
    }

    void activityMoveRejectsModelReplacementAfterReset()
    {
        ZzShellFixture fixture;
        auto moved = std::make_unique<QWidget>();
        auto stayed = std::make_unique<QWidget>();
        QPointer<QWidget> movedGuard(moved.get());
        QWidget *const stayedRaw = stayed.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("moved"), QStringLiteral("Moved"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, moved.get()));
        zzReleaseAfterAdoption(moved);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("stayed"), QStringLiteral("Stayed"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, stayed.get()));
        zzReleaseAfterAdoption(stayed);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QPointer<QAbstractItemModel> modelGuard(leftBar->model());
        QVERIFY(leftPane->panelStack()->setPanelSizes({123}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({789}));
        bool callbackEntered = false;
        QObject::connect(
            leftPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != nullptr) {
                    return;
                }
                callbackEntered = true;
                delete modelGuard.data();
            });

        Q_EMIT leftBar->moveRequested(
            modelGuard->index(0, 0),
            ZzFluentUI::ZzActivityArea::RightPrimary,
            0);

        QVERIFY(callbackEntered);
        QVERIFY(modelGuard.isNull());
        QVERIFY(movedGuard != nullptr);
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>());
        QCOMPARE(rightPane->visibleWidgets(), QList<QWidget *>({stayedRaw}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({789}));
        QCOMPARE(movedGuard->parentWidget(), nullptr);
        const auto taken = fixture.shell->takePanel(zzPanelId("moved"));
        QVERIFY(!taken);
        QCOMPARE(taken.error().code(), ZzCore::ZzErrorCode::NotFound);
    }

    void appliesAllFourTitleModesAndTabFallback()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        first->setWindowTitle(QStringLiteral("Document"));
        fixture.shell->tabWidget()->addTab(first.release(), QStringLiteral("First"));
        auto second = std::make_unique<QWidget>();
        fixture.shell->tabWidget()->addTab(
            second.release(), QStringLiteral("Fallback"));

        fixture.shell->setApplicationTitle(QStringLiteral("Pure Tools"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Pure Tools"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Pure Tools"));

        fixture.shell->tabWidget()->setCurrentIndex(0);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Document"));

        fixture.shell->tabWidget()->setCurrentIndex(1);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Fallback"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        QCOMPARE(
            fixture.host.windowTitle(),
            QStringLiteral("Fallback - Pure Tools"));

        fixture.shell->setCustomTitle(QStringLiteral("Workspace A"));
        fixture.shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Custom);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Workspace A"));
        fixture.shell->setCustomTitle({});
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Pure Tools"));
    }

    void refreshesCurrentTabTitlesFromPagePresentationChanges()
    {
        ZzShellFixture fixture;
        auto page = std::make_unique<QWidget>();
        QWidget *const pageRaw = page.get();
        fixture.shell->tabWidget()->addTab(
            page.release(), QStringLiteral("Initial"));
        fixture.shell->setApplicationTitle(QStringLiteral("Pure Tools"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Initial"));
        QSignalSpy presentationSpy(
            fixture.shell->tabWidget(),
            &ZzFluentUI::ZzTabWidget::pagePresentationChanged);

        fixture.shell->tabWidget()->setPageTitle(
            pageRaw, QStringLiteral("Renamed"));

        QCOMPARE(presentationSpy.count(), 1);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Renamed"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        fixture.shell->tabWidget()->setPageTitle(
            pageRaw, QStringLiteral("Final"));
        QCOMPARE(presentationSpy.count(), 2);
        QCOMPARE(
            fixture.host.windowTitle(),
            QStringLiteral("Final - Pure Tools"));
    }

    void followsTheActiveGroupCurrentPageTitle()
    {
        ZzShellFixture fixture;
        auto *const splitWorkspace = fixture.shell->splitWorkspace();
        const ZzFluentUI::ZzTabGroupId firstGroup =
            splitWorkspace->activeGroupId();
        auto *const firstTabs = splitWorkspace->tabWidget(firstGroup);
        auto firstPage = std::make_unique<QWidget>();
        firstPage->setWindowTitle(QStringLiteral("First Window"));
        firstTabs->addTab(firstPage.release(), QStringLiteral("First Tab"));

        const auto secondGroup = splitWorkspace->splitGroup(
            firstGroup, Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroup.has_value());
        const ZzFluentUI::ZzTabGroupId secondGroupId = secondGroup.value_or(
            ZzFluentUI::ZzTabGroupId{});
        auto *const secondTabs = splitWorkspace->tabWidget(secondGroupId);
        auto secondWindowPage = std::make_unique<QWidget>();
        QWidget *const secondWindowRaw = secondWindowPage.get();
        secondWindowPage->setWindowTitle(QStringLiteral("Second Window"));
        secondTabs->addTab(
            secondWindowPage.release(), QStringLiteral("Second Tab"));
        auto secondFallbackPage = std::make_unique<QWidget>();
        QWidget *const secondFallbackRaw = secondFallbackPage.get();
        secondTabs->addTab(
            secondFallbackPage.release(), QStringLiteral("Second Fallback"));

        fixture.shell->setApplicationTitle(QStringLiteral("Pure Tools"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QVERIFY(splitWorkspace->setActiveGroup(secondGroupId));
        QCOMPARE(fixture.shell->tabWidget(), secondTabs);
        secondTabs->setCurrentWidget(secondWindowRaw);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Second Window"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Second Window"));

        secondWindowRaw->setWindowTitle(QStringLiteral("Changed Window"));
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Changed Window"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Changed Window"));
        secondTabs->setCurrentWidget(secondFallbackRaw);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Second Fallback"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Second Fallback"));
        secondTabs->setPageTitle(
            secondFallbackRaw, QStringLiteral("Renamed Fallback"));
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Renamed Fallback"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Renamed Fallback"));

        QVERIFY(splitWorkspace->setActiveGroup(firstGroup));
        QCOMPARE(fixture.shell->tabWidget(), firstTabs);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("First Window"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("First Window"));
    }

    void keepsBothTitleSinksAlignedDuringActiveGroupReentry()
    {
        ZzShellFixture fixture;
        auto *const splitWorkspace = fixture.shell->splitWorkspace();
        const ZzFluentUI::ZzTabGroupId firstGroup =
            splitWorkspace->activeGroupId();
        auto firstPage = std::make_unique<QWidget>();
        firstPage->setWindowTitle(QStringLiteral("First"));
        splitWorkspace->tabWidget(firstGroup)->addTab(
            firstPage.release(), QStringLiteral("First"));
        const auto secondGroup = splitWorkspace->splitGroup(
            firstGroup, Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroup.has_value());
        const ZzFluentUI::ZzTabGroupId secondGroupId = secondGroup.value_or(
            ZzFluentUI::ZzTabGroupId{});
        auto secondPage = std::make_unique<QWidget>();
        secondPage->setWindowTitle(QStringLiteral("Second"));
        splitWorkspace->tabWidget(secondGroupId)->addTab(
            secondPage.release(), QStringLiteral("Second"));
        QVERIFY(splitWorkspace->setActiveGroup(firstGroup));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);

        bool callbackEntered = false;
        bool reactivatedFirstGroup = false;
        QObject::connect(
            &fixture.host, &QWidget::windowTitleChanged,
            fixture.shell.get(),
            [&](const QString &title) {
                if (callbackEntered || title != QStringLiteral("Second")) {
                    return;
                }
                callbackEntered = true;
                reactivatedFirstGroup =
                    splitWorkspace->setActiveGroup(firstGroup);
            });

        QVERIFY(splitWorkspace->setActiveGroup(secondGroupId));

        QVERIFY(callbackEntered);
        QVERIFY(reactivatedFirstGroup);
        QCOMPARE(splitWorkspace->activeGroupId(), firstGroup);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("First"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("First"));
    }

    void appliesAlwaysOnTopRequestsWithoutHidingOrLosingWindowState()
    {
        ZzShellFixture fixture;
        fixture.host.setWindowState(Qt::WindowMaximized);
        fixture.host.show();
        QCoreApplication::processEvents();
        const Qt::WindowStates originalState = fixture.host.windowState();
        QVERIFY(fixture.host.isVisible());

        QVERIFY(fixture.shell->setAlwaysOnTop(true));
        QVERIFY(fixture.host.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(fixture.host.isVisible());
        QCOMPARE(fixture.host.windowState(), originalState);
        QVERIFY(fixture.titleBar.isAlwaysOnTop());

        QVERIFY(QMetaObject::invokeMethod(
            &fixture.titleBar, "alwaysOnTopRequested",
            Qt::DirectConnection, Q_ARG(bool, false)));
        QVERIFY(!fixture.host.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(fixture.host.isVisible());
        QCOMPARE(fixture.host.windowState(), originalState);
        QVERIFY(!fixture.titleBar.isAlwaysOnTop());
    }

    void migratesVersionOneLayoutToVersionTwo()
    {
        ZzShellFixture fixture;
        auto leftOne = std::make_unique<QWidget>();
        auto leftTwo = std::make_unique<QWidget>();
        auto right = std::make_unique<QWidget>();
        auto bottom = std::make_unique<QWidget>();
        QWidget *const leftTwoRaw = leftTwo.get();
        QWidget *const rightRaw = right.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-one"), QStringLiteral("Left one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftOne.get()));
        zzReleaseAfterAdoption(leftOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, leftTwo.get()));
        zzReleaseAfterAdoption(leftTwo);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, right.get()));
        zzReleaseAfterAdoption(right);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            bottom.get()));
        zzReleaseAfterAdoption(bottom);
        QVERIFY(fixture.shell->showPanel(zzPanelId("bottom"), true));

        auto *const rootTabs = fixture.shell->splitWorkspace()->tabWidget(
            fixture.shell->splitWorkspace()->groupIds().constFirst());
        rootTabs->addTab(new QWidget, QStringLiteral("First"));
        rootTabs->addTab(new QWidget, QStringLiteral("Second"));
        rootTabs->setCurrentIndex(0);
        const QByteArray versionOne = zzVersionOneLayout(
            fixture.host.saveState(1), false, 345, true, 456,
            QStringLiteral("left-two"), QStringLiteral("right"),
            {
                {QStringLiteral("left-one"),
                 ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
                {QStringLiteral("left-two"),
                 ZzFluentUI::ZzActivityArea::LeftSecondary, 0},
                {QStringLiteral("right"),
                 ZzFluentUI::ZzActivityArea::RightPrimary, 0},
            },
            1, ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QVERIFY(!versionOne.isEmpty());

        QVERIFY(fixture.shell->restoreLayout(versionOne));
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->visibleWidgets(),
            QList<QWidget *>({leftTwoRaw}));
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Right)->visibleWidgets(),
            QList<QWidget *>({rightRaw}));
        QCOMPARE(rootTabs->currentIndex(), 1);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        QCOMPARE(
            fixture.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);

        const auto savedAgain = fixture.shell->saveLayout();
        QVERIFY(savedAgain);
        QDataStream envelope(savedAgain.value());
        envelope.setVersion(QDataStream::Qt_6_8);
        char magic[4]{};
        quint16 schemaVersion = 0;
        quint16 streamVersion = 0;
        quint32 payloadLength = 0;
        QCOMPARE(envelope.readRawData(magic, 4), 4);
        envelope >> schemaVersion >> streamVersion >> payloadLength;
        QCOMPARE(schemaVersion, quint16(3));
        QCOMPARE(streamVersion,
            static_cast<quint16>(QDataStream::Qt_6_8));
        QVERIFY(payloadLength > 0);
    }

    void failedVersionThreeRestoreRollsBackRuntimeState()
    {
        ZzShellFixture source;
        QAction sourceSettingsAction(QStringLiteral("Fixed settings"));
        QVERIFY(source.shell->registerFixedActivityAction(
            zzActivityId("fixed-settings"), QStringLiteral("Fixed settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &sourceSettingsAction));
        auto sourceContent = std::make_unique<QWidget>();
        auto sourceOther = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("other"), QStringLiteral("Other"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceOther.get()));
        zzReleaseAfterAdoption(sourceOther);
        auto *const sourceLeftPane = source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        sourceLeftPane->setMaximumPaneWidth(800);
        sourceLeftPane->setPaneWidth(700);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);
        QDataStream envelope(requested.value());
        envelope.setVersion(QDataStream::Qt_6_8);
        char magic[4]{};
        quint16 schemaVersion = 0;
        QCOMPARE(envelope.readRawData(magic, 4), 4);
        envelope >> schemaVersion;
        QCOMPARE(schemaVersion, quint16(3));

        ZzShellFixture target;
        QAction settingsAction(QStringLiteral("Fixed settings"));
        QVERIFY(target.shell->registerFixedActivityAction(
            zzActivityId("fixed-settings"), QStringLiteral("Fixed settings"),
            zzActivityIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
            &settingsAction));
        auto targetContent = std::make_unique<QWidget>();
        auto targetOther = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QWidget *const targetOtherRaw = targetOther.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("other"), QStringLiteral("Other"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetOther.get()));
        zzReleaseAfterAdoption(targetOther);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        leftPane->setPaneWidth(100);
        QVERIFY(leftPane->panelStack()->setPanelSizes({456}));
        auto *const leftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QAbstractItemModel *const model = leftBar->model();
        QVERIFY(model != nullptr);
        const QModelIndex fixedBefore = zzActivityIndex(
            model, QStringLiteral("Fixed settings"));
        QVERIFY(fixedBefore.isValid());
        const int fixedRow = fixedBefore.row();
        const QVariant fixedArea = fixedBefore.data(
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));
        const QStringList modelTitles = {model->index(0, 0).data().toString(),
            model->index(1, 0).data().toString(),
            model->index(2, 0).data().toString()};
        const QList<QVariant> modelAreas = {
            model->index(0, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)),
            model->index(1, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)),
            model->index(2, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))};
        const QList<QWidget *> leftPanels = leftPane->panelStack()->panels();
        const QList<QWidget *> leftVisible = leftPane->visibleWidgets();
        QWidget *const leftCurrent = leftPane->currentWidget();
        const bool leftCollapsed = leftPane->isCollapsed();
        const int leftWidth = leftPane->paneWidth();
        const QList<int> leftSizes = leftPane->panelStack()->panelSizes();
        const QList<QWidget *> rightPanels = rightPane->panelStack()->panels();
        const QList<QWidget *> rightVisible = rightPane->visibleWidgets();
        QWidget *const rightCurrent = rightPane->currentWidget();
        const bool rightCollapsed = rightPane->isCollapsed();
        const int rightWidth = rightPane->paneWidth();
        const QList<int> rightSizes = rightPane->panelStack()->panelSizes();
        const QModelIndex leftCurrentIndex = leftBar->currentSourceIndex();
        const QModelIndex rightCurrentIndex = rightBar->currentSourceIndex();
        bool callbackEntered = false;
        QObject::connect(
            leftPane->panelStack(), &ZzFluentUI::ZzPanelStack::panelMoved,
            target.shell.get(), [&](QWidget *panel, int) {
            if (callbackEntered || panel != targetContentRaw) {
                return;
            }
            callbackEntered = true;
            leftPane->setMaximumPaneWidth(100);
        });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!restored);
        QCOMPARE(restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and was rolled back"));
        QVERIFY(callbackEntered);
        QCOMPARE(leftPane->panelStack()->panels(), leftPanels);
        QCOMPARE(leftPane->visibleWidgets(), leftVisible);
        QCOMPARE(leftPane->currentWidget(), leftCurrent);
        QCOMPARE(leftPane->isCollapsed(), leftCollapsed);
        QCOMPARE(leftPane->paneWidth(), leftWidth);
        QCOMPARE(leftPane->panelStack()->panelSizes(), leftSizes);
        QCOMPARE(rightPane->panelStack()->panels(), rightPanels);
        QCOMPARE(rightPane->visibleWidgets(), rightVisible);
        QCOMPARE(rightPane->currentWidget(), rightCurrent);
        QCOMPARE(rightPane->isCollapsed(), rightCollapsed);
        QCOMPARE(rightPane->paneWidth(), rightWidth);
        QCOMPARE(rightPane->panelStack()->panelSizes(), rightSizes);
        QVERIFY(leftPane->isAncestorOf(targetContentRaw));
        QCOMPARE(leftBar->currentSourceIndex(), leftCurrentIndex);
        QCOMPARE(rightBar->currentSourceIndex(), rightCurrentIndex);
        QCOMPARE(model->rowCount(), 3);
        for (int row = 0; row < model->rowCount(); ++row) {
            QCOMPARE(model->index(row, 0).data().toString(), modelTitles.at(row));
            QCOMPARE(model->index(row, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)),
                modelAreas.at(row));
        }
        const QModelIndex fixedAfter = zzActivityIndex(
            model, QStringLiteral("Fixed settings"));
        QVERIFY(fixedAfter.isValid());
        QCOMPARE(fixedAfter.row(), fixedRow);
        QCOMPARE(fixedAfter.data(
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)), fixedArea);
        QCOMPARE(fixedAfter.flags(), Qt::ItemFlags(Qt::ItemIsEnabled));
        QVERIFY(leftPane->panelStack()->panels().contains(targetOtherRaw));
    }

    void boundsVersionOneLayoutDtos_data()
    {
        QTest::addColumn<QString>("mutation");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("baseline") << QStringLiteral("baseline") << true;
        QTest::newRow("current-id-256") << QStringLiteral("current-256") << true;
        QTest::newRow("current-id-257") << QStringLiteral("current-257") << false;
        QTest::newRow("entry-id-256") << QStringLiteral("entry-256") << true;
        QTest::newRow("entry-id-257") << QStringLiteral("entry-257") << false;
        QTest::newRow("order-4095") << QStringLiteral("order-4095") << true;
        QTest::newRow("sparse-order-4096")
            << QStringLiteral("sparse-order-4096") << true;
    }

    void boundsVersionOneLayoutDtos()
    {
        QFETCH(QString, mutation);
        QFETCH(bool, accepted);
        ZzShellFixture fixture;
        QString leftCurrent;
        QVector<ZzTestSideLayoutEntry> entries;
        if (mutation == QStringLiteral("current-256")) {
            leftCurrent = QString(256, QLatin1Char('c'));
        } else if (mutation == QStringLiteral("current-257")) {
            leftCurrent = QString(257, QLatin1Char('c'));
        } else if (mutation == QStringLiteral("entry-256")) {
            entries.append({
                QString(256, QLatin1Char('e')),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0});
        } else if (mutation == QStringLiteral("entry-257")) {
            entries.append({
                QString(257, QLatin1Char('e')),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0});
        } else if (mutation == QStringLiteral("order-4095")) {
            entries.append({
                QStringLiteral("side"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 4095});
        } else if (mutation == QStringLiteral("sparse-order-4096")) {
            entries.append({
                QStringLiteral("side"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 4096});
        }
        const QByteArray encoded = zzVersionOneLayout(
            fixture.host.saveState(1), false, 280, false, 280,
            leftCurrent, {}, entries, -1,
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        QVERIFY(!encoded.isEmpty());

        const auto restored = fixture.shell->restoreLayout(encoded);
        QCOMPARE(bool(restored), accepted);
        if (!restored) {
            QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        }
    }

    void keepsQtMainWindowStateVersionIndependentFromEnvelopeVersion()
    {
        ZzShellFixture fixture;
        const QByteArray before = fixture.host.saveState(1);
        const QByteArray encoded = zzVersionOneLayout(
            fixture.host.saveState(2), false, 280, false, 280,
            {}, {}, {}, -1,
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        QVERIFY(!encoded.isEmpty());

        const auto restored = fixture.shell->restoreLayout(encoded);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(fixture.host.saveState(1), before);
    }

    void restoresCompleteVersionTwoWorkspaceState()
    {
        ZzShellFixture fixture;
        auto leftOne = std::make_unique<QWidget>();
        auto leftTwo = std::make_unique<QWidget>();
        auto right = std::make_unique<QWidget>();
        auto bottomOne = std::make_unique<QWidget>();
        auto bottomTwo = std::make_unique<QWidget>();
        auto dock = std::make_unique<QWidget>();
        QWidget *const leftTwoRaw = leftTwo.get();
        QWidget *const bottomOneRaw = bottomOne.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-one"), QStringLiteral("Left one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftOne.get()));
        zzReleaseAfterAdoption(leftOne);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, leftTwo.get()));
        zzReleaseAfterAdoption(leftTwo);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, right.get()));
        zzReleaseAfterAdoption(right);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom-one"), QStringLiteral("Bottom one"), zzIcon(),
            bottomOne.get()));
        zzReleaseAfterAdoption(bottomOne);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom-two"), QStringLiteral("Bottom two"), zzIcon(),
            bottomTwo.get()));
        zzReleaseAfterAdoption(bottomTwo);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);

        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(leftPane->panelStack()->setPanelSizes({350}));
        QVERIFY(rightPane->panelStack()->setPanelSizes({420}));
        leftPane->setPaneWidth(345);
        rightPane->setPaneWidth(456);
        QVERIFY(fixture.shell->showPanel(zzPanelId("bottom-one"), true));
        fixture.shell->bottomPane()->setPaneHeight(360);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);

        auto *const workspace = fixture.shell->splitWorkspace();
        const auto rootId = workspace->groupIds().constFirst();
        const auto secondId = workspace->splitGroup(
            rootId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("second")));
        QVERIFY(secondId.has_value());
        const ZzFluentUI::ZzTabGroupId secondGroupId = secondId.value_or(
            ZzFluentUI::ZzTabGroupId{});
        auto *const rootTabs = workspace->tabWidget(rootId);
        auto *const secondTabs = workspace->tabWidget(secondGroupId);
        auto *const rootFirst = new QWidget;
        auto *const rootSecond = new QWidget;
        auto *const secondPage = new QWidget;
        rootTabs->addTab(rootFirst, QStringLiteral("Root first"));
        rootTabs->addTab(rootSecond, QStringLiteral("Root second"));
        secondTabs->addTab(secondPage, QStringLiteral("Second"));
        QVERIFY(workspace->setPageLayoutKey(
            rootFirst, QStringLiteral("root-first")));
        QVERIFY(workspace->setPageLayoutKey(
            rootSecond, QStringLiteral("root-second")));
        QVERIFY(workspace->setPageLayoutKey(
            secondPage, QStringLiteral("second-page")));
        rootTabs->setCurrentIndex(1);
        secondTabs->setCurrentIndex(0);
        QVERIFY(workspace->setActiveGroup(secondGroupId));
        const QByteArray splitBefore = workspace->saveLayout();
        QVERIFY(!splitBefore.isEmpty());
        auto *const dockPanel = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);

        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);

        QVERIFY(fixture.shell->showPanel(zzPanelId("left-one"), false));
        leftPane->setPaneWidth(210);
        rightPane->setPaneWidth(220);
        QVERIFY(fixture.shell->showPanel(zzPanelId("bottom-two"), true));
        fixture.shell->bottomPane()->setPaneHeight(210);
        fixture.shell->bottomPane()->setCollapsed(true);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        rootTabs->setCurrentIndex(0);
        QVERIFY(workspace->setActiveGroup(rootId));
        fixture.host.addDockWidget(Qt::RightDockWidgetArea, dockPanel);

        QVERIFY(fixture.shell->restoreLayout(saved.value()));
        QCOMPARE(
            leftPane->visibleWidgets(),
            QList<QWidget *>({leftTwoRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({1}));
        QCOMPARE(rightPane->panelStack()->panelSizes(), QList<int>({1}));
        QCOMPARE(leftPane->paneWidth(), 345);
        QCOMPARE(rightPane->paneWidth(), 456);
        QCOMPARE(workspace->groupIds().size(), 2);
        QCOMPARE(workspace->activeGroupId(), secondGroupId);
        QCOMPARE(workspace->tabWidget(rootId)->currentIndex(), 1);
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), bottomOneRaw);
        QCOMPARE(fixture.shell->bottomPane()->paneHeight(), 360);
        QVERIFY(!fixture.shell->bottomPane()->isCollapsed());
        QCOMPARE(
            fixture.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        QCOMPARE(fixture.host.dockWidgetArea(dockPanel), Qt::LeftDockWidgetArea);
    }

    void boundsVersionTwoLayoutDtos_data()
    {
        QTest::addColumn<QString>("mutation");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("baseline") << QStringLiteral("baseline") << true;
        QTest::newRow("left-visible-32") << QStringLiteral("left-32") << true;
        QTest::newRow("left-visible-33") << QStringLiteral("left-33") << false;
        QTest::newRow("side-entries-4096") << QStringLiteral("side-4096") << true;
        QTest::newRow("side-entries-4097") << QStringLiteral("side-4097") << false;
        QTest::newRow("duplicate-side-id") << QStringLiteral("duplicate-side") << false;
        QTest::newRow("duplicate-visible-id") << QStringLiteral("duplicate-visible") << false;
        QTest::newRow("sizes-count-mismatch") << QStringLiteral("sizes-count") << false;
        QTest::newRow("non-positive-size") << QStringLiteral("size-zero") << false;
        QTest::newRow("oversize-panel-size") << QStringLiteral("size-large") << false;
        QTest::newRow("invalid-area") << QStringLiteral("area") << false;
        QTest::newRow("invalid-title-mode") << QStringLiteral("title") << false;
        QTest::newRow("invalid-collapse-enum") << QStringLiteral("collapse") << false;
        QTest::newRow("oversize-id") << QStringLiteral("id") << false;
        QTest::newRow("split-digest") << QStringLiteral("split-digest") << false;
        QTest::newRow("split-groups-64") << QStringLiteral("groups-64") << true;
        QTest::newRow("split-groups-65") << QStringLiteral("groups-65") << false;
        QTest::newRow("split-depth-16") << QStringLiteral("depth-16") << true;
        QTest::newRow("split-depth-17") << QStringLiteral("depth-17") << false;
        QTest::newRow("split-adjacent-orientation")
            << QStringLiteral("split-adjacent-orientation") << false;
        QTest::newRow("split-trimmed-group")
            << QStringLiteral("split-trimmed-group") << false;
        QTest::newRow("split-empty-key")
            << QStringLiteral("split-empty-key") << false;
        QTest::newRow("split-trimmed-key")
            << QStringLiteral("split-trimmed-key") << false;
        QTest::newRow("split-duplicate-order")
            << QStringLiteral("split-duplicate-order") << false;
        QTest::newRow("current-not-visible")
            << QStringLiteral("current-not-visible") << true;
        QTest::newRow("visible-without-entry")
            << QStringLiteral("visible-without-entry") << false;
        QTest::newRow("visible-on-wrong-edge")
            << QStringLiteral("visible-on-wrong-edge") << false;
    }

    void boundsVersionTwoLayoutDtos()
    {
        QFETCH(QString, mutation);
        QFETCH(bool, accepted);
        ZzShellFixture fixture;
        ZzTestVersionTwoLayout layout;
        layout.qtState = fixture.host.saveState(1);
        layout.splitState = fixture.shell->splitWorkspace()->saveLayout();

        if (mutation == QStringLiteral("left-32")
            || mutation == QStringLiteral("left-33")) {
            const int count = mutation.endsWith(QStringLiteral("32")) ? 32 : 33;
            for (int index = 0; index < count; ++index) {
                const QString id = QStringLiteral("left-%1").arg(index);
                layout.leftVisible.append(id);
                layout.leftSizes.append(index + 1);
                layout.sideEntries.append({
                    id, ZzFluentUI::ZzActivityArea::LeftPrimary, index});
            }
        } else if (mutation == QStringLiteral("side-4096")
                   || mutation == QStringLiteral("side-4097")) {
            const int count = mutation.endsWith(QStringLiteral("4096"))
                ? 4096 : 4097;
            for (int index = 0; index < count; ++index) {
                layout.sideEntries.append({
                    QStringLiteral("side-%1").arg(index, 4, 10, QLatin1Char('0')),
                    ZzFluentUI::ZzActivityArea::LeftPrimary,
                    index});
            }
        } else if (mutation == QStringLiteral("duplicate-side")) {
            layout.sideEntries = {
                {QStringLiteral("same"),
                 ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
                {QStringLiteral("same"),
                 ZzFluentUI::ZzActivityArea::RightPrimary, 0},
            };
        } else if (mutation == QStringLiteral("duplicate-visible")) {
            layout.leftVisible = {QStringLiteral("same"), QStringLiteral("same")};
            layout.leftSizes = {1, 2};
        } else if (mutation == QStringLiteral("sizes-count")) {
            layout.leftVisible = {QStringLiteral("one")};
        } else if (mutation == QStringLiteral("size-zero")) {
            layout.leftVisible = {QStringLiteral("one")};
            layout.leftSizes = {0};
        } else if (mutation == QStringLiteral("size-large")) {
            layout.leftVisible = {QStringLiteral("one")};
            layout.leftSizes = {
                static_cast<qint32>(zzWorkspaceMaximumLayoutSize + 1)};
        } else if (mutation == QStringLiteral("area")) {
            layout.sideEntries = {{
                QStringLiteral("side"),
                quint8{0xff}, 0}};
        } else if (mutation == QStringLiteral("title")) {
            layout.titleMode = 0xff;
        } else if (mutation == QStringLiteral("collapse")) {
            layout.leftCollapsed = 2;
        } else if (mutation == QStringLiteral("id")) {
            layout.leftVisible = {QString(257, QLatin1Char('x'))};
            layout.leftSizes = {1};
        } else if (mutation == QStringLiteral("split-digest")) {
            layout.splitState[layout.splitState.size() - 1] = static_cast<char>(
                layout.splitState.back() ^ 0x5a);
        } else if (mutation == QStringLiteral("groups-64")) {
            layout.splitState = zzTestSplitLayout(64);
        } else if (mutation == QStringLiteral("groups-65")) {
            layout.splitState = zzTestSplitLayout(65);
        } else if (mutation == QStringLiteral("depth-16")) {
            layout.splitState = zzTestSplitLayout(0, 16);
        } else if (mutation == QStringLiteral("depth-17")) {
            layout.splitState = zzTestSplitLayout(0, 17);
        } else if (mutation.startsWith(QStringLiteral("split-"))) {
            layout.splitState = zzMalformedTestSplitLayout(
                mutation.sliced(QStringLiteral("split-").size()));
        } else if (mutation == QStringLiteral("current-not-visible")) {
            layout.leftCurrent = QStringLiteral("side");
            layout.sideEntries = {{
                QStringLiteral("side"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0}};
        } else if (mutation == QStringLiteral("visible-without-entry")) {
            layout.leftVisible = {QStringLiteral("side")};
            layout.leftSizes = {100};
        } else if (mutation == QStringLiteral("visible-on-wrong-edge")) {
            layout.leftVisible = {QStringLiteral("side")};
            layout.leftSizes = {100};
            layout.sideEntries = {{
                QStringLiteral("side"),
                ZzFluentUI::ZzActivityArea::RightPrimary, 0}};
        }
        const QByteArray encoded = zzVersionTwoLayout(layout);
        QVERIFY(!encoded.isEmpty());

        const auto restored = fixture.shell->restoreLayout(encoded);
        QCOMPARE(bool(restored), accepted);
        if (!restored) {
            QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        }
    }

    void boundsVersionTwoSideStateWhenSaving_data()
    {
        QTest::addColumn<int>("visibleCount");
        QTest::addColumn<int>("idLength");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("visible-32") << 32 << 16 << true;
        QTest::newRow("visible-33") << 33 << 16 << true;
        QTest::newRow("id-256") << 1 << 256 << true;
        QTest::newRow("id-257") << 1 << 257 << false;
    }

    void boundsVersionTwoSideStateWhenSaving()
    {
        QFETCH(int, visibleCount);
        QFETCH(int, idLength);
        QFETCH(bool, accepted);
        ZzShellFixture fixture;
        for (int index = 0; index < visibleCount; ++index) {
            const QString id = visibleCount == 1
                ? QString(idLength, QLatin1Char('x'))
                : QStringLiteral("side-%1").arg(index, 11, 10, QLatin1Char('0'));
            auto content = std::make_unique<QWidget>();
            QVERIFY(fixture.shell->registerSidePanel(
                ZzPureTools::ZzWorkspacePanelId(id), id, zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }

        const auto saved = fixture.shell->saveLayout();
        QCOMPARE(bool(saved), accepted);
        if (saved) {
            QDataStream envelope(saved.value());
            envelope.setVersion(QDataStream::Qt_6_8);
            envelope.skipRawData(4);
            quint16 schema = 0;
            envelope >> schema;
            QCOMPARE(schema, quint16(3));
        } else {
            QCOMPARE(saved.error().code(), ZzCore::ZzErrorCode::InvalidState);
        }
    }

    void restoreRejectsNonSubsequenceVisibleOrderBeforeMutation()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QWidget *const firstRaw = first.get();
        QWidget *const secondRaw = second.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);

        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const stack = pane->panelStack();
        pane->setMaximumPaneWidth(800);
        pane->setPaneWidth(321);
        QVERIFY(stack->setPanelSizes({222}));
        QCOMPARE(pane->currentWidget(), secondRaw);

        ZzTestVersionTwoLayout layout;
        layout.qtState = fixture.host.saveState(1);
        layout.leftCollapsed = 0;
        layout.leftWidth = 500;
        layout.leftCurrent = QStringLiteral("second");
        layout.leftVisible = {
            QStringLiteral("second"), QStringLiteral("first")};
        layout.leftSizes = {222, 111};
        layout.sideEntries = {
            {QStringLiteral("first"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("second"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1}};
        layout.splitState = fixture.shell->splitWorkspace()->saveLayout();
        const QByteArray encoded = zzVersionTwoLayout(layout);
        QVERIFY(!encoded.isEmpty());

        QSignalSpy widthSpy(pane, &ZzFluentUI::ZzSidePane::paneWidthChanged);
        QSignalSpy sizesSpy(stack, &ZzFluentUI::ZzPanelStack::panelSizesChanged);
        QSignalSpy currentSpy(pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged);
        const auto restored = fixture.shell->restoreLayout(encoded);

        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(widthSpy.count(), 0);
        QCOMPARE(sizesSpy.count(), 0);
        QCOMPARE(currentSpy.count(), 0);
        QCOMPARE(pane->paneWidth(), 321);
        QCOMPARE(stack->panels(), QList<QWidget *>({firstRaw, secondRaw}));
        QCOMPARE(pane->visibleWidgets(), QList<QWidget *>({secondRaw}));
        QCOMPARE(stack->panelSizes(), QList<int>({222}));
        QCOMPARE(pane->currentWidget(), secondRaw);
    }

    void rejectsInvalidSplitStateWhenSavingWorkspaceLayout()
    {
        ZzShellFixture fixture;
        auto *const workspace = fixture.shell->splitWorkspace();
        auto *const tabs = workspace->tabWidget(
            workspace->groupIds().constFirst());
        QVERIFY(tabs != nullptr);
        for (int index = 0; index < 4097; ++index) {
            auto *const page = new QWidget;
            tabs->addTab(page, QString::number(index));
            QVERIFY(workspace->setPageLayoutKey(
                page, QStringLiteral("page:%1").arg(index)));
        }
        QVERIFY(workspace->saveLayout().isEmpty());

        const auto saved = fixture.shell->saveLayout();

        QVERIFY(!saved);
        QCOMPARE(saved.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void restoreRejectsReentrantSideTransactions()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->setPaneWidth(411);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        rightPane->setPaneWidth(223);
        auto reentrantContent = std::make_unique<QWidget>();
        QWidget thirdPartyOwner;
        bool callbackEntered = false;
        bool nestedRestoreAccepted = false;
        bool registrationAccepted = false;
        bool visibilityAccepted = false;
        bool badgeAccepted = false;
        bool removalAccepted = false;
        QObject::connect(
            rightPane, &ZzFluentUI::ZzSidePane::paneWidthChanged,
            target.shell.get(), [&](int width) {
                if (callbackEntered || width != 411) {
                    return;
                }
                callbackEntered = true;
                nestedRestoreAccepted = bool(
                    target.shell->restoreLayout(requested.value()));
                registrationAccepted = bool(target.shell->registerSidePanel(
                    zzPanelId("reentrant"), QStringLiteral("Reentrant"),
                    zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                    reentrantContent.get()));
                if (registrationAccepted) {
                    zzReleaseAfterAdoption(reentrantContent);
                }
                visibilityAccepted = bool(
                    target.shell->showPanel(zzPanelId("side"), false));
                badgeAccepted = bool(
                    target.shell->setPanelBadge(zzPanelId("side"), 7));
                const QModelIndex sideIndex = leftBar->model()->index(0, 0);
                leftBar->activationRequested(sideIndex);
                leftBar->moveRequested(
                    sideIndex, ZzFluentUI::ZzActivityArea::RightPrimary, 0);
                auto removed = target.shell->takePanel(zzPanelId("side"));
                removalAccepted = bool(removed);
                if (removed) {
                    removed.value()->setParent(&thirdPartyOwner);
                }
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(restored);
        QVERIFY(!nestedRestoreAccepted);
        QVERIFY(!registrationAccepted);
        QVERIFY(!visibilityAccepted);
        QVERIFY(!badgeAccepted);
        QVERIFY(!removalAccepted);
        QCOMPARE(reentrantContent->parentWidget(), nullptr);
        QVERIFY(rightPane->isAncestorOf(targetContentRaw));
        QVERIFY(rightPane->panelStack()->panels().contains(targetContentRaw));
        QVERIFY(leftBar->isHidden());
        QVERIFY(!target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right)->isHidden());
        QCOMPARE(
            leftBar->model()->index(0, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightPrimary);
    }

    void restoreRollsBackIndependentSideAndActivityOrder()
    {
        ZzShellFixture source;
        auto sourceFirst = std::make_unique<QWidget>();
        auto sourceSecond = std::make_unique<QWidget>();
        auto sourceBottom = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceFirst.get()));
        zzReleaseAfterAdoption(sourceFirst);
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSecond.get()));
        zzReleaseAfterAdoption(sourceSecond);
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            sourceBottom.get()));
        zzReleaseAfterAdoption(sourceBottom);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetFirst = std::make_unique<QWidget>();
        auto targetSecond = std::make_unique<QWidget>();
        auto targetBottom = std::make_unique<QWidget>();
        QWidget *const firstRaw = targetFirst.get();
        QWidget *const secondRaw = targetSecond.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetFirst.get()));
        zzReleaseAfterAdoption(targetFirst);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSecond.get()));
        zzReleaseAfterAdoption(targetSecond);
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            targetBottom.get()));
        zzReleaseAfterAdoption(targetBottom);
        auto *const pane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QVERIFY(pane->panelStack()->movePanel(secondRaw, 0));
        QCOMPARE(pane->panelStack()->panels(),
            QList<QWidget *>({secondRaw, firstRaw}));
        QVERIFY(!target.shell->saveLayout());
        target.shell->bottomPane()->setMaximumPaneHeight(300);

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!restored);
        QVERIFY(!restored.error().technicalMessage().contains(
            QStringLiteral("rollback failed")));
        QCOMPARE(pane->panelStack()->panels(),
            QList<QWidget *>({secondRaw, firstRaw}));
        auto *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("First"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("Second"));
    }

    void restoreRejectsMissingNonCurrentBottomMembership()
    {
        ZzShellFixture source;
        auto sourceFirst = std::make_unique<QWidget>();
        auto sourceSecond = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            sourceFirst.get()));
        zzReleaseAfterAdoption(sourceFirst);
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            sourceSecond.get()));
        zzReleaseAfterAdoption(sourceSecond);
        QVERIFY(source.shell->showPanel(zzPanelId("first"), true));
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetFirst = std::make_unique<QWidget>();
        auto targetSecond = std::make_unique<QWidget>();
        QWidget *const firstRaw = targetFirst.get();
        QWidget *const secondRaw = targetSecond.get();
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            targetFirst.get()));
        zzReleaseAfterAdoption(targetFirst);
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            targetSecond.get()));
        zzReleaseAfterAdoption(targetSecond);
        QVERIFY(target.shell->showPanel(zzPanelId("first"), true));
        auto *const pane = target.shell->bottomPane();
        auto *const stack = pane->findChild<QStackedWidget *>();
        QVERIFY(stack != nullptr);
        pane->setMaximumPaneHeight(800);
        pane->setPaneHeight(240);
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                stack->removeWidget(secondRaw);
                QCOMPARE(secondRaw->parentWidget(), stack);
                QCOMPARE(stack->indexOf(secondRaw), -1);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QVERIFY(!restored.error().technicalMessage().contains(
            QStringLiteral("rollback failed")));
        QCOMPARE(pane->widgetCount(), 2);
        QCOMPARE(stack->indexOf(firstRaw), 0);
        QCOMPARE(stack->indexOf(secondRaw), 1);
        QCOMPARE(pane->currentWidget(), firstRaw);
    }

    void migratesVersionOneCurrentInKeyedBranchedSplit()
    {
        ZzShellFixture fixture;
        auto *const workspace = fixture.shell->splitWorkspace();
        const auto initialGroup = workspace->groupIds().constFirst();
        auto *const initialTabs = workspace->tabWidget(initialGroup);
        QVERIFY(initialTabs != nullptr);
        auto *const first = new QWidget;
        auto *const second = new QWidget;
        initialTabs->addTab(first, QStringLiteral("First"));
        initialTabs->addTab(second, QStringLiteral("Second"));
        QVERIFY(workspace->setPageLayoutKey(
            first, QStringLiteral("page:first")));
        QVERIFY(workspace->setPageLayoutKey(
            second, QStringLiteral("page:second")));
        initialTabs->setCurrentIndex(0);
        QVERIFY(workspace->splitGroup(
            initialGroup, Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("second-group")))
                    .has_value());
        const QByteArray versionOne = zzVersionOneLayout(
            fixture.host.saveState(1), false, 280, false, 280,
            {}, {}, {}, 1,
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        QVERIFY(!versionOne.isEmpty());

        QVERIFY(fixture.shell->restoreLayout(versionOne));
        QCOMPARE(workspace->tabWidget(initialGroup), initialTabs);
        QCOMPARE(initialTabs->currentIndex(), 1);
    }

    void restoreRejectsDockOwnerReplacement()
    {
        ZzShellFixture source;
        auto sourceDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        zzReleaseAfterAdoption(sourceDock);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        QWidget thirdPartyOwner;
        ZzShellFixture target;
        auto targetDock = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        zzReleaseAfterAdoption(targetDock);
        auto *const dock = target.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        bool callbackEntered = false;
        QObject::connect(
            dock, &QDockWidget::dockLocationChanged,
            target.shell.get(), [&](Qt::DockWidgetArea area) {
                if (callbackEntered || area != Qt::RightDockWidgetArea) {
                    return;
                }
                callbackEntered = true;
                dock->setParent(&thirdPartyOwner);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QVERIFY(restored.error().technicalMessage().contains(
            QStringLiteral("rollback failed")));
        QCOMPARE(dock->parentWidget(), &thirdPartyOwner);
        QCOMPARE(target.host.dockWidgetArea(dock), Qt::NoDockWidgetArea);
    }

    void restoreRejectsReentrantBottomAndDockTransactions()
    {
        ZzShellFixture source;
        auto sourceBottom = std::make_unique<QWidget>();
        auto sourceDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            sourceBottom.get()));
        zzReleaseAfterAdoption(sourceBottom);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        zzReleaseAfterAdoption(sourceDock);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetBottom = std::make_unique<QWidget>();
        QWidget *const targetBottomRaw = targetBottom.get();
        auto targetDock = std::make_unique<QWidget>();
        QWidget *const targetDockRaw = targetDock.get();
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            targetBottom.get()));
        zzReleaseAfterAdoption(targetBottom);
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        zzReleaseAfterAdoption(targetDock);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        auto extraBottom = std::make_unique<QWidget>();
        auto extraDock = std::make_unique<QWidget>();
        QWidget thirdPartyOwner;
        bool callbackEntered = false;
        bool bottomRegistrationAccepted = false;
        bool dockRegistrationAccepted = false;
        bool bottomVisibilityAccepted = false;
        bool dockVisibilityAccepted = false;
        bool bottomRemovalAccepted = false;
        bool dockRemovalAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                bottomRegistrationAccepted = bool(
                    target.shell->registerBottomPanel(
                        zzPanelId("extra-bottom"),
                        QStringLiteral("Extra bottom"), zzIcon(),
                        extraBottom.get()));
                if (bottomRegistrationAccepted) {
                    zzReleaseAfterAdoption(extraBottom);
                }
                dockRegistrationAccepted = bool(
                    target.shell->registerDockPanel(
                        zzPanelId("extra-dock"),
                        QStringLiteral("Extra dock"), zzIcon(),
                        Qt::BottomDockWidgetArea, extraDock.get()));
                if (dockRegistrationAccepted) {
                    zzReleaseAfterAdoption(extraDock);
                }
                bottomVisibilityAccepted = bool(
                    target.shell->showPanel(zzPanelId("bottom"), false));
                dockVisibilityAccepted = bool(
                    target.shell->showPanel(zzPanelId("dock"), false));
                auto removedBottom = target.shell->takePanel(
                    zzPanelId("bottom"));
                bottomRemovalAccepted = bool(removedBottom);
                if (removedBottom) {
                    removedBottom.value()->setParent(&thirdPartyOwner);
                }
                auto removedDock = target.shell->takePanel(zzPanelId("dock"));
                dockRemovalAccepted = bool(removedDock);
                if (removedDock) {
                    removedDock.value()->setParent(&thirdPartyOwner);
                }
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(restored);
        QVERIFY(!bottomRegistrationAccepted);
        QVERIFY(!dockRegistrationAccepted);
        QVERIFY(!bottomVisibilityAccepted);
        QVERIFY(!dockVisibilityAccepted);
        QVERIFY(!bottomRemovalAccepted);
        QVERIFY(!dockRemovalAccepted);
        QCOMPARE(extraBottom->parentWidget(), nullptr);
        QCOMPARE(extraDock->parentWidget(), nullptr);
        QVERIFY(bottomPane->isAncestorOf(targetBottomRaw));
        auto *const dockPanel = target.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        QCOMPARE(dockPanel->widget(), targetDockRaw);
    }

    void restoreRejectsDestroyedSubsystemBeforeSuccess()
    {
        ZzShellFixture source;
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        QPointer<ZzFluentUI::ZzSplitWorkspace> splitGuard(
            target.shell->splitWorkspace());
        bool callbackEntered = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                delete splitGuard.data();
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(splitGuard.isNull());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void restoreRejectsSideOwnershipLostAfterCommit()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdPartyOwner;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool callbackEntered = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                targetContentRaw->setParent(&thirdPartyOwner);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(targetContentRaw->parentWidget(), &thirdPartyOwner);
        for (const auto edge : {
                 ZzFluentUI::ZzSidePaneEdge::Left,
                 ZzFluentUI::ZzSidePaneEdge::Right}) {
            QVERIFY(!target.shell->sidePane(edge)->panelStack()->panels()
                .contains(targetContentRaw));
        }
        QCOMPARE(
            target.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(),
            0);
        QVERIFY(target.shell->saveLayout());
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void restoreRejectsOwnerPollutionInsideSideFrame()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        QWidget *const frameOwner = targetContentRaw->parentWidget();
        QVERIFY(frameOwner != nullptr);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        QPointer<QWidget> thirdPartyOwner;
        bool callbackEntered = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                thirdPartyOwner = new QWidget(frameOwner);
                targetContentRaw->setParent(thirdPartyOwner);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(thirdPartyOwner != nullptr);
        QCOMPARE(targetContentRaw->parentWidget(), thirdPartyOwner.data());
        QVERIFY(frameOwner->isAncestorOf(targetContentRaw));
    }

    void restoreRejectsUnknownPhysicalSidePanel()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        auto *const sourcePane = source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        sourcePane->setMaximumPaneWidth(800);
        sourcePane->setPaneWidth(410);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const targetPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        targetPane->setMaximumPaneWidth(800);
        targetPane->setPaneWidth(230);
        auto unknown = std::make_unique<QWidget>();
        QWidget *const unknownRaw = unknown.get();
        bool callbackEntered = false;
        bool injectionAccepted = false;
        bool hidden = false;
        QObject::connect(
            targetPane, &ZzFluentUI::ZzSidePane::paneWidthChanged,
            target.shell.get(), [&](int width) {
                if (callbackEntered || width != 410) {
                    return;
                }
                callbackEntered = true;
                injectionAccepted = targetPane->addWidget(
                    unknownRaw, QStringLiteral("Unknown"));
                if (injectionAccepted) {
                    zzReleaseAfterAdoption(unknown);
                    hidden = targetPane->setWidgetVisible(unknownRaw, false);
                }
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(injectionAccepted);
        QVERIFY(hidden);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        std::unique_ptr<QWidget> reclaimed(targetPane->takeWidget(unknownRaw));
        QCOMPARE(reclaimed.get(), unknownRaw);
        QVERIFY(target.shell->saveLayout());
    }

    void saveRejectsUnknownPhysicalSidePanel()
    {
        ZzShellFixture fixture;
        auto registered = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, registered.get()));
        zzReleaseAfterAdoption(registered);
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto unknown = std::make_unique<QWidget>();
        QWidget *const unknownRaw = unknown.get();
        QVERIFY(pane->addWidget(unknownRaw, QStringLiteral("Unknown")));
        zzReleaseAfterAdoption(unknown);
        QVERIFY(pane->setWidgetVisible(unknownRaw, false));

        const auto saved = fixture.shell->saveLayout();

        QVERIFY(!saved);
        QCOMPARE(saved.error().code(), ZzCore::ZzErrorCode::InvalidState);
        std::unique_ptr<QWidget> reclaimed(pane->takeWidget(unknownRaw));
        QCOMPARE(reclaimed.get(), unknownRaw);
        QVERIFY(fixture.shell->saveLayout());
    }

    void restoreRejectsBottomContentOutsideStack()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setCollapsed(false);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setCollapsed(true);
        bool callbackEntered = false;
        bool directTakeAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::collapsedChanged,
            target.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                directTakeAccepted =
                    bottomPane->takeWidget(targetContentRaw) == targetContentRaw;
                if (directTakeAccepted) {
                    targetContentRaw->setParent(bottomPane);
                }
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(directTakeAccepted);
        QCOMPARE(bottomPane->widgetCount(), 0);
        QCOMPARE(targetContentRaw->parentWidget(), bottomPane);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void restoreRejectsBottomCurrentOverwrittenWithinCommit()
    {
        ZzShellFixture source;
        auto sourceOne = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom-one"), QStringLiteral("Bottom one"), zzIcon(),
            sourceOne.get()));
        zzReleaseAfterAdoption(sourceOne);
        auto sourceTwo = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom-two"), QStringLiteral("Bottom two"), zzIcon(),
            sourceTwo.get()));
        zzReleaseAfterAdoption(sourceTwo);
        QVERIFY(source.shell->showPanel(zzPanelId("bottom-one"), true));
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetOne = std::make_unique<QWidget>();
        QWidget *const targetOneRaw = targetOne.get();
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom-one"), QStringLiteral("Bottom one"), zzIcon(),
            targetOne.get()));
        zzReleaseAfterAdoption(targetOne);
        auto targetTwo = std::make_unique<QWidget>();
        QWidget *const targetTwoRaw = targetTwo.get();
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom-two"), QStringLiteral("Bottom two"), zzIcon(),
            targetTwo.get()));
        zzReleaseAfterAdoption(targetTwo);
        auto *const bottomPane = target.shell->bottomPane();
        QVERIFY(bottomPane->setCurrentWidget(targetOneRaw));
        bottomPane->setCollapsed(true);
        bool callbackEntered = false;
        bool overwriteAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::collapsedChanged,
            target.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                overwriteAccepted = bottomPane->setCurrentWidget(targetTwoRaw);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(overwriteAccepted);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(bottomPane->currentWidget(), targetOneRaw);
    }

    void restoreRejectsSideCurrentOverwrittenWithinCommit()
    {
        ZzShellFixture source;
        auto sourceOne = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("left-one"), QStringLiteral("Left one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceOne.get()));
        zzReleaseAfterAdoption(sourceOne);
        auto sourceTwo = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, sourceTwo.get()));
        zzReleaseAfterAdoption(sourceTwo);
        auto sourceRight = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceRight.get()));
        zzReleaseAfterAdoption(sourceRight);
        auto *const sourceLeft = source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QVERIFY(sourceLeft->setCurrentWidget(
            sourceLeft->panelStack()->panels().constFirst()));
        sourceLeft->setPaneWidth(410);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetOne = std::make_unique<QWidget>();
        QWidget *const targetOneRaw = targetOne.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("left-one"), QStringLiteral("Left one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetOne.get()));
        zzReleaseAfterAdoption(targetOne);
        auto targetTwo = std::make_unique<QWidget>();
        QWidget *const targetTwoRaw = targetTwo.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, targetTwo.get()));
        zzReleaseAfterAdoption(targetTwo);
        auto targetRight = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, targetRight.get()));
        zzReleaseAfterAdoption(targetRight);
        auto *const targetLeft = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const targetRightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(targetLeft->setCurrentWidget(targetOneRaw));
        targetLeft->setPaneWidth(230);
        targetRightPane->setCollapsed(true);
        bool callbackEntered = false;
        bool overwriteAccepted = false;
        QObject::connect(
            targetRightPane, &ZzFluentUI::ZzSidePane::collapsedChanged,
            target.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                overwriteAccepted = targetLeft->setCurrentWidget(targetTwoRaw);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(overwriteAccepted);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(targetLeft->currentWidget(), targetOneRaw);
        QCOMPARE(targetLeft->paneWidth(), 230);
        QVERIFY(targetRightPane->isCollapsed());
    }

    void restoreRejectsTitleModeOverwrittenWithinCommit()
    {
        ZzShellFixture source;
        source.shell->setApplicationTitle(QStringLiteral("Application"));
        source.shell->setCustomTitle(QStringLiteral("Source custom"));
        source.shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Custom);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        target.shell->setApplicationTitle(QStringLiteral("Application"));
        target.shell->setCustomTitle(QStringLiteral("Target custom"));
        target.shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Application);
        bool callbackEntered = false;
        QObject::connect(
            &target.host, &QWidget::windowTitleChanged,
            target.shell.get(), [&](const QString &title) {
                if (callbackEntered || title != QStringLiteral("Target custom")) {
                    return;
                }
                callbackEntered = true;
                target.shell->setTitleMode(
                    ZzPureTools::ZzWorkspaceTitleMode::Application);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(
            target.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        QCOMPARE(target.host.windowTitle(), QStringLiteral("Application"));
    }

    void restoreRejectsTitleSinksOverwrittenWithinCommit()
    {
        ZzShellFixture source;
        source.shell->setApplicationTitle(QStringLiteral("Application"));
        source.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Custom);
        source.shell->setCustomTitle(QStringLiteral("Requested"));
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        target.shell->setApplicationTitle(QStringLiteral("Before"));
        target.shell->setCustomTitle(QStringLiteral("Before custom"));
        target.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        bool callbackEntered = false;
        QObject::connect(
            &target.host, &QWidget::windowTitleChanged,
            target.shell.get(), [&](const QString &) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                target.host.setWindowTitle(QStringLiteral("Polluted host"));
                target.titleBar.setTitle(QStringLiteral("Polluted bar"));
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(target.host.windowTitle(), QStringLiteral("Before"));
        QCOMPARE(target.titleBar.title(), QStringLiteral("Before"));
        QCOMPARE(
            target.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::Application);
    }

    void restoresLayoutWithoutOptionalTitleBar()
    {
        QMainWindow host;
        auto created = ZzPureTools::ZzWorkspaceShell::create(&host, nullptr);
        QVERIFY(created);
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell =
            std::move(created).value();
        const auto saved = shell->saveLayout();
        QVERIFY(saved);
        QVERIFY(shell->restoreLayout(saved.value()));
    }

    void rollbackReportsDeletedBottomContent()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<QWidget>();
        QPointer<QWidget> targetContentGuard(targetContent.get());
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool callbackEntered = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                delete targetContentGuard.data();
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(targetContentGuard.isNull());
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
    }

    void restoreRejectsLaterOverwriteOfCommittedSideState()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        auto *const sourceSide = source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        sourceSide->setMaximumPaneWidth(800);
        sourceSide->setPaneWidth(410);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const targetSide = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        targetSide->setMaximumPaneWidth(800);
        targetSide->setPaneWidth(230);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool callbackEntered = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                targetSide->setPaneWidth(333);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(targetSide->paneWidth(), 230);
    }

    void commitsWorkspaceSubsystemsInDocumentedOrder()
    {
        ZzShellFixture source;
        auto sourceSide = std::make_unique<QWidget>();
        auto sourceBottom = std::make_unique<QWidget>();
        auto sourceDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSide.get()));
        zzReleaseAfterAdoption(sourceSide);
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            sourceBottom.get()));
        zzReleaseAfterAdoption(sourceBottom);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        zzReleaseAfterAdoption(sourceDock);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(410);
        QVERIFY(source.shell->showPanel(zzPanelId("side"), false));
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(390);
        source.shell->bottomPane()->setCollapsed(false);
        const auto sourceRoot = source.shell->splitWorkspace()->groupIds().constFirst();
        QVERIFY(source.shell->splitWorkspace()->splitGroup(
            sourceRoot, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("second"))).has_value());
        source.host.setWindowTitle(QStringLiteral("Application"));
        source.shell->setApplicationTitle(QStringLiteral("Application"));
        source.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetSide = std::make_unique<QWidget>();
        auto targetBottom = std::make_unique<QWidget>();
        auto targetDock = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSide.get()));
        zzReleaseAfterAdoption(targetSide);
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            targetBottom.get()));
        zzReleaseAfterAdoption(targetBottom);
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        zzReleaseAfterAdoption(targetDock);
        target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(230);
        target.shell->bottomPane()->setMaximumPaneHeight(800);
        target.shell->bottomPane()->setPaneHeight(240);
        target.host.setWindowTitle(QStringLiteral("Application"));
        target.shell->setApplicationTitle(QStringLiteral("Application"));
        target.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        auto *const dockPanel = target.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);

        QStringList order;
        QObject::connect(
            dockPanel, &QDockWidget::dockLocationChanged,
            target.shell.get(), [&](Qt::DockWidgetArea) {
                if (!order.contains(QStringLiteral("qt"))) {
                    order.append(QStringLiteral("qt"));
                }
            });
        QObject::connect(
            target.shell->splitWorkspace(),
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged,
            target.shell.get(), [&] {
                if (!order.contains(QStringLiteral("split"))) {
                    order.append(QStringLiteral("split"));
                }
            });
        QObject::connect(
            target.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left),
            &ZzFluentUI::ZzSidePane::paneWidthChanged,
            target.shell.get(), [&](int) {
                if (!order.contains(QStringLiteral("side"))) {
                    order.append(QStringLiteral("side"));
                }
            });
        QObject::connect(
            target.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int) {
                if (!order.contains(QStringLiteral("bottom"))) {
                    order.append(QStringLiteral("bottom"));
                }
            });
        QObject::connect(
            target.shell->activityBar(ZzFluentUI::ZzSidePaneEdge::Left),
            &ZzFluentUI::ZzActivityBar::activeSourceIndexesChanged,
            target.shell.get(), [&](const QList<QModelIndex> &) {
                if (!order.contains(QStringLiteral("activity"))) {
                    order.append(QStringLiteral("activity"));
                }
            });
        QObject::connect(
            &target.host, &QWidget::windowTitleChanged,
            target.shell.get(), [&](const QString &) {
                if (!order.contains(QStringLiteral("title"))) {
                    order.append(QStringLiteral("title"));
                }
            });

        QVERIFY(target.shell->restoreLayout(requested.value()));
        const auto position = [&order](const char *name) {
            return order.indexOf(QString::fromLatin1(name));
        };
        for (const char *name : {"qt", "split", "side", "bottom", "activity"}) {
            QVERIFY2(position(name) >= 0, name);
        }
        QVERIFY(position("qt") < position("split"));
        QVERIFY(position("split") < position("side"));
        QVERIFY(position("side") < position("bottom"));
        QVERIFY(position("bottom") < position("activity"));
        if (position("title") >= 0) {
            QVERIFY(position("activity") <= position("title"));
        }
    }

    void rollsBackEveryCommittedSubsystemWhenBottomCommitFails()
    {
        ZzShellFixture source;
        auto sourceSide = std::make_unique<QWidget>();
        auto sourceBottom = std::make_unique<QWidget>();
        auto sourceDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSide.get()));
        zzReleaseAfterAdoption(sourceSide);
        QVERIFY(source.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            sourceBottom.get()));
        zzReleaseAfterAdoption(sourceBottom);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        zzReleaseAfterAdoption(sourceDock);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(420);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        source.shell->bottomPane()->setCollapsed(false);
        const auto sourceRoot = source.shell->splitWorkspace()->groupIds().constFirst();
        QVERIFY(source.shell->splitWorkspace()->splitGroup(
            sourceRoot, Qt::Vertical, ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("requested-second")))
                    .has_value());
        source.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Custom);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetSide = std::make_unique<QWidget>();
        auto targetBottom = std::make_unique<QWidget>();
        auto targetDock = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSide.get()));
        zzReleaseAfterAdoption(targetSide);
        QVERIFY(target.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            targetBottom.get()));
        zzReleaseAfterAdoption(targetBottom);
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        zzReleaseAfterAdoption(targetDock);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        leftPane->setPaneWidth(222);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setPaneHeight(240);
        bottomPane->setMaximumPaneHeight(300);
        bottomPane->setCollapsed(true);
        target.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        const auto splitIdsBefore = target.shell->splitWorkspace()->groupIds();
        const auto splitActiveBefore = target.shell->splitWorkspace()->activeGroupId();
        auto *const dockPanel = target.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        const QByteArray qtBefore = target.host.saveState(1);
        const auto leftActiveBefore = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->activeSourceIndexes();

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(!restored.error().technicalMessage().contains(
            QStringLiteral("rollback failed")));
        QCOMPARE(target.host.saveState(1), qtBefore);
        QCOMPARE(target.host.dockWidgetArea(dockPanel), Qt::LeftDockWidgetArea);
        QCOMPARE(target.shell->splitWorkspace()->groupIds(), splitIdsBefore);
        QCOMPARE(target.shell->splitWorkspace()->activeGroupId(), splitActiveBefore);
        QCOMPARE(leftPane->paneWidth(), 222);
        QCOMPARE(bottomPane->paneHeight(), 240);
        QVERIFY(bottomPane->isCollapsed());
        QCOMPARE(
            target.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->activeSourceIndexes(),
            leftActiveBefore);
        QCOMPARE(
            target.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::Application);
    }

    void rejectsMagicVersionLengthDigestAndOversizeCorruption()
    {
        ZzShellFixture fixture;
        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        const QByteArray &valid = saved.value();

        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, 0, 'X')));
        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, 5, '\x04')));
        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, 11, '\x7f')));
        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, valid.size() - 1,
                static_cast<char>(valid.back() ^ 0x5a))));
        QVERIFY(!fixture.shell->restoreLayout(
            QByteArray(zzWorkspaceMaximumLayoutSize + 1, 'x')));
    }

    void boundsAndDeduplicatesNearLimitSideLayoutEntries()
    {
        ZzShellFixture fixture;
        ZzTestVersionTwoLayout baseline;
        baseline.qtState = fixture.host.saveState(1);
        baseline.splitState = fixture.shell->splitWorkspace()->saveLayout();
        const QByteArray saved = zzVersionTwoLayout(baseline);
        QVERIFY(!saved.isEmpty());

        QElapsedTimer timer;
        timer.start();
        const auto maximumUnique = zzLayoutWithSideEntries(
            saved, 4096);
        QVERIFY(!maximumUnique.isEmpty());
        QVERIFY(fixture.shell->restoreLayout(maximumUnique));
        QVERIFY2(timer.elapsed() < 1000,
            "The maximum valid side layout must be decoded on the GUI thread promptly");

        const auto excessive = zzLayoutWithSideEntries(saved, 4097);
        QVERIFY(!excessive.isEmpty());
        QVERIFY(!fixture.shell->restoreLayout(excessive));

        const auto duplicateTail = zzLayoutWithSideEntries(
            saved, 4096, QStringLiteral("side-0000"));
        QVERIFY(!duplicateTail.isEmpty());
        QVERIFY(!fixture.shell->restoreLayout(duplicateTail));
    }

    void ignoresUnknownPanelIdsDuringRestore()
    {
        ZzShellFixture source;
        auto ghostSide = std::make_unique<QWidget>();
        auto ghostDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("ghost-side"), QStringLiteral("Ghost side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, ghostSide.get()));
        zzReleaseAfterAdoption(ghostSide);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("ghost-dock"), QStringLiteral("Ghost dock"), zzIcon(),
            Qt::RightDockWidgetArea, ghostDock.get()));
        zzReleaseAfterAdoption(ghostDock);
        auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        auto known = std::make_unique<QWidget>();
        QWidget *const knownRaw = known.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("known"), QStringLiteral("Known"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, known.get()));
        zzReleaseAfterAdoption(known);

        QVERIFY(!target.shell->restoreLayout(saved.value()));
        QCOMPARE(
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->currentWidget(),
            knownRaw);
        QCOMPARE(
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->pageCount(),
            1);
    }

    void restoresSavedSidePanelOrderAcrossRegistrationOrders()
    {
        ZzShellFixture source;
        for (const char *id : {"alpha", "beta"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(source.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }
        auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        for (const char *id : {"beta", "alpha"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(target.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }
        QVERIFY(target.shell->restoreLayout(saved.value()));

        QAbstractItemModel *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("alpha"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("beta"));
    }

    void rollsBackQtAndShellSnapshotsWhenQtRestoreFails()
    {
        ZzShellFixture fixture;
        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);
        auto *dockPanel = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(500);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Custom);
        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(321);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        const QByteArray qtBefore = fixture.host.saveState(1);
        const QByteArray validEnvelopeWithInvalidQtState = zzReplaceQtState(
            saved.value(), QByteArrayLiteral("not-a-qmainwindow-state"));
        const auto restored = fixture.shell->restoreLayout(
            validEnvelopeWithInvalidQtState);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and was rolled back"));

        QCOMPARE(fixture.host.saveState(1), qtBefore);
        QCOMPARE(fixture.host.dockWidgetArea(dockPanel), Qt::LeftDockWidgetArea);
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->paneWidth(),
            321);
        QCOMPARE(
            fixture.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::Application);
    }

    void rollsBackQtSnapshotWhenShellApplyFails()
    {
        ZzShellFixture source;
        auto sourceSide = std::make_unique<QWidget>();
        auto sourceDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSide.get()));
        zzReleaseAfterAdoption(sourceSide);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        zzReleaseAfterAdoption(sourceDock);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(800);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(700);
        auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetSide = std::make_unique<QWidget>();
        auto targetDock = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSide.get()));
        zzReleaseAfterAdoption(targetSide);
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        zzReleaseAfterAdoption(targetDock);
        target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(321);
        auto *dockPanel = target.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        const QByteArray qtBefore = target.host.saveState(1);

        QVERIFY(!target.shell->restoreLayout(requested.value()));

        QCOMPARE(target.host.saveState(1), qtBefore);
        QCOMPARE(target.host.dockWidgetArea(dockPanel), Qt::LeftDockWidgetArea);
        QCOMPARE(
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->paneWidth(),
            321);
    }

    void reportsWhenLayoutRollbackFails()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(800);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(700);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<ZzParentChangeWidget>();
        bool armed = false;
        bool callbackEntered = false;
        targetContent->parentChanged = [&] {
            if (!armed || callbackEntered) {
                return;
            }
            callbackEntered = true;
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(100);
        };
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(321);
        armed = true;

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
    }

    void rollbackFailurePreservesSecondaryActivityTier()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightSecondary,
            sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(800);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(700);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<ZzParentChangeWidget>();
        ZzParentChangeWidget *const targetContentRaw = targetContent.get();
        bool armed = false;
        bool callbackEntered = false;
        targetContent->parentChanged = [&] {
            if (!armed || callbackEntered) {
                return;
            }
            callbackEntered = true;
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(100);
        };
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        leftPane->setPaneWidth(321);
        armed = true;

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        const bool survivesOnLeft =
            leftPane->panelStack()->panels().contains(targetContentRaw);
        const bool survivesOnRight =
            rightPane->panelStack()->panels().contains(targetContentRaw);
        QVERIFY(survivesOnLeft != survivesOnRight);
        const auto expectedArea = survivesOnLeft
            ? ZzFluentUI::ZzActivityArea::LeftSecondary
            : ZzFluentUI::ZzActivityArea::RightSecondary;
        QAbstractItemModel *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(
            model->index(0, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            expectedArea);
        QVERIFY(target.shell->saveLayout());
    }

    void rollbackSynchronizesActivityAfterOwnershipAudit()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const rightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        target.shell->bottomPane()->setPaneHeight(240);
        target.shell->bottomPane()->setMaximumPaneHeight(300);
        int parentRemovalCount = 0;
        bool wrongPaneAccepted = false;
        targetContentRaw->parentRemoved = [&] {
            ++parentRemovalCount;
            if (parentRemovalCount == 2) {
                wrongPaneAccepted = rightPane->addWidget(
                    targetContentRaw, QStringLiteral("Side"));
            }
        };

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        QCOMPARE(parentRemovalCount, 2);
        QVERIFY(wrongPaneAccepted);
        QVERIFY(rightPane->isAncestorOf(targetContentRaw));
        QVERIFY(rightPane->visibleWidgets().contains(targetContentRaw));
        const QModelIndex sideIndex = rightBar->model()->index(0, 0);
        QCOMPARE(
            sideIndex.data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightPrimary);
        QVERIFY(rightBar->activeSourceIndexes().contains(sideIndex));
    }

    void restoreRejectsSourcePaneDestroyedByCurrentWidgetSignal()
    {
        ZzShellFixture source;
        auto sourceFirst = std::make_unique<QWidget>();
        auto sourceSecond = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceFirst.get()));
        zzReleaseAfterAdoption(sourceFirst);
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSecond.get()));
        zzReleaseAfterAdoption(sourceSecond);
        QVERIFY(source.shell->showPanel(zzPanelId("first"), true));
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetFirst = std::make_unique<QWidget>();
        auto targetSecond = std::make_unique<QWidget>();
        QPointer<QWidget> targetFirstGuard(targetFirst.get());
        QPointer<QWidget> targetSecondGuard(targetSecond.get());
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("first"), QStringLiteral("First"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetFirst.get()));
        zzReleaseAfterAdoption(targetFirst);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("second"), QStringLiteral("Second"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSecond.get()));
        zzReleaseAfterAdoption(targetSecond);
        QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(
            target.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left));
        bool callbackEntered = false;
        QObject::connect(
            leftPaneGuard, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            target.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != targetFirstGuard) {
                    return;
                }
                callbackEntered = true;
                delete leftPaneGuard.data();
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(leftPaneGuard.isNull());
        QVERIFY(targetFirstGuard.isNull());
        QVERIFY(targetSecondGuard.isNull());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void restoreRollsBackWhenTargetPaneIsDestroyedDuringTake()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        leftPane->setPaneWidth(337);
        QVERIFY(leftPane->panelStack()->setPanelSizes({456}));
        QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(
            target.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right));
        bool armed = true;
        targetContentRaw->parentRemoved = [&] {
            if (!armed) {
                return;
            }
            armed = false;
            delete rightPaneGuard.data();
        };

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!armed);
        QVERIFY(rightPaneGuard.isNull());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(leftPane->isAncestorOf(targetContentRaw));
        QCOMPARE(leftPane->visibleWidgets(), QList<QWidget *>({targetContentRaw}));
        QCOMPARE(leftPane->panelStack()->panelSizes(), QList<int>({456}));
        QCOMPARE(leftPane->paneWidth(), 337);
    }

    void restoreCleansRegistrationWhenContentGetsThirdPartyOwner()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdParty;
        auto targetContent = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        bool armed = true;
        targetContentRaw->parentRemoved = [&] {
            if (!armed) {
                return;
            }
            armed = false;
            targetContentRaw->setParent(&thirdParty);
        };

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!armed);
        QVERIFY(!restored);
        QCOMPARE(targetContentRaw->parentWidget(), &thirdParty);
        QCOMPARE(
            target.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(),
            0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void rollbackCleansEveryInvalidPanelRegistration()
    {
        ZzShellFixture source;
        for (const char *id : {"one", "two", "three"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(source.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::RightPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdParty;
        QList<ZzParentRemovedWidget *> invalid;
        for (const char *id : {"one", "two", "three"}) {
            auto content = std::make_unique<ZzParentRemovedWidget>();
            auto *const raw = content.get();
            QVERIFY(target.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            invalid.append(raw);
            zzReleaseAfterAdoption(content);
        }
        bool armed = true;
        invalid.constFirst()->parentRemoved = [&] {
            if (!armed) {
                return;
            }
            armed = false;
            for (ZzParentRemovedWidget *const content : invalid) {
                content->setParent(&thirdParty);
            }
        };

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!restored);
        QVERIFY(!armed);
        for (ZzParentRemovedWidget *const content : invalid) {
            QCOMPARE(content->parentWidget(), &thirdParty);
        }
        QCOMPARE(
            target.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 0);
        for (const char *id : {"one", "two", "three"}) {
            auto replacement = std::make_unique<QWidget>();
            QVERIFY(target.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary,
                replacement.get()));
            zzReleaseAfterAdoption(replacement);
        }
    }

    void rollbackCleanupUsesCurrentRowsAfterEarlierDeletion()
    {
        ZzShellFixture source;
        for (const char *id : {"a", "b"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(source.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::RightPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdPartyOwner;
        auto first = std::make_unique<QWidget>();
        auto second = std::make_unique<QWidget>();
        QPointer<QWidget> firstGuard(first.get());
        QWidget *const secondRaw = second.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("a"), QStringLiteral("a"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, first.get()));
        zzReleaseAfterAdoption(first);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("b"), QStringLiteral("b"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, second.get()));
        zzReleaseAfterAdoption(second);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool callbackEntered = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (callbackEntered || height != 500) {
                    return;
                }
                callbackEntered = true;
                delete firstGuard.data();
                secondRaw->setParent(&thirdPartyOwner);
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(callbackEntered);
        QVERIFY(firstGuard.isNull());
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        QCOMPARE(secondRaw->parentWidget(), &thirdPartyOwner);
        for (const auto edge : {
                 ZzFluentUI::ZzSidePaneEdge::Left,
                 ZzFluentUI::ZzSidePaneEdge::Right}) {
            QVERIFY(!target.shell->sidePane(edge)->panelStack()->panels()
                .contains(secondRaw));
        }
        QCOMPARE(
            target.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(),
            0);
        QVERIFY(target.shell->saveLayout());
        for (const char *id : {"a", "b"}) {
            auto replacement = std::make_unique<QWidget>();
            QVERIFY(target.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary,
                replacement.get()));
            zzReleaseAfterAdoption(replacement);
        }
    }

    void rollbackCleanupPreservesRegistrationWhenTakeCallbackReaddsContent()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdPartyOwner;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool cleanupArmed = false;
        int takeCallbackCount = 0;
        bool readdAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (cleanupArmed || height != 500) {
                    return;
                }
                targetContentRaw->setParent(&thirdPartyOwner);
                cleanupArmed = true;
            });
        QObject::connect(
            rightPane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            target.shell.get(), [&](QWidget *current) {
                if (!cleanupArmed || takeCallbackCount != 0
                    || current != nullptr) {
                    return;
                }
                ++takeCallbackCount;
                targetContentRaw->setParent(nullptr);
                readdAccepted = rightPane->addWidget(
                    targetContentRaw, QStringLiteral("Side"));
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(cleanupArmed);
        QCOMPARE(takeCallbackCount, 1);
        QVERIFY(readdAccepted);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        const bool survivesOnLeft =
            leftPane->panelStack()->panels().contains(targetContentRaw);
        const bool survivesOnRight =
            rightPane->panelStack()->panels().contains(targetContentRaw);
        const int physicalOccurrences = int(survivesOnLeft)
            + int(survivesOnRight);
        auto *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        const int modelRows = model->rowCount();
        const auto saved = target.shell->saveLayout();
        const QString diagnostic = QStringLiteral(
            "physical occurrences=%1, activity rows=%2, save accepted=%3")
                                       .arg(physicalOccurrences)
                                       .arg(modelRows)
                                       .arg(bool(saved));
        QVERIFY2(
            physicalOccurrences == 1 && modelRows == 1 && saved,
            qPrintable(diagnostic));
        QVERIFY(survivesOnLeft);
        QVERIFY(!survivesOnRight);
        const QModelIndex sideIndex = model->index(0, 0);
        QCOMPARE(
            sideIndex.data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftPrimary);
        QCOMPARE(leftPane->currentWidget(), targetContentRaw);
        QCOMPARE(
            leftPane->visibleWidgets(), QList<QWidget *>({targetContentRaw}));
        auto *const leftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QCOMPARE(leftBar->currentSourceIndex(), sideIndex);
        QCOMPARE(leftBar->activeSourceIndexes(), QList<QModelIndex>({sideIndex}));
        QVERIFY(!rightBar->currentSourceIndex().isValid());
        QVERIFY(rightBar->activeSourceIndexes().isEmpty());

        auto reclaimed = target.shell->takePanel(zzPanelId("side"));
        QVERIFY(reclaimed);
        QCOMPARE(reclaimed.value(), targetContentRaw);
        std::unique_ptr<QWidget> reclaimedContent(reclaimed.value());
    }

    void rollbackCleanupPreservesRegistrationWhenRowsRemovedReaddsContent()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdPartyOwner;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QPointer<QWidget> targetContentGuard(targetContentRaw);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(rightPane);
        auto *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool cleanupArmed = false;
        int rowsRemovedCallbackCount = 0;
        bool readdAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (cleanupArmed || height != 500
                    || targetContentGuard == nullptr) {
                    return;
                }
                targetContentGuard->setParent(&thirdPartyOwner);
                cleanupArmed = true;
            });
        QObject::connect(
            model, &QAbstractItemModel::rowsRemoved,
            target.shell.get(),
            [&](const QModelIndex &, int, int) {
                if (!cleanupArmed || rowsRemovedCallbackCount != 0) {
                    return;
                }
                ++rowsRemovedCallbackCount;
                if (targetContentGuard == nullptr
                    || rightPaneGuard == nullptr) {
                    return;
                }
                targetContentGuard->setParent(nullptr);
                readdAccepted = rightPaneGuard->addWidget(
                    targetContentGuard, QStringLiteral("Side"));
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(cleanupArmed);
        QCOMPARE(rowsRemovedCallbackCount, 1);
        QVERIFY(!targetContentGuard.isNull());
        QVERIFY(!rightPaneGuard.isNull());
        QVERIFY(readdAccepted);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        const bool survivesOnLeft =
            leftPane->panelStack()->panels().contains(targetContentRaw);
        const bool survivesOnRight =
            rightPane->panelStack()->panels().contains(targetContentRaw);
        const int physicalOccurrences = int(survivesOnLeft)
            + int(survivesOnRight);
        const int modelRows = model->rowCount();
        const auto saved = target.shell->saveLayout();
        const QString diagnostic = QStringLiteral(
            "physical occurrences=%1, activity rows=%2, save accepted=%3")
                                       .arg(physicalOccurrences)
                                       .arg(modelRows)
                                       .arg(bool(saved));
        QVERIFY2(
            physicalOccurrences == 1 && modelRows == 1 && saved,
            qPrintable(diagnostic));
        QVERIFY(!survivesOnLeft);
        QVERIFY(survivesOnRight);
        const QModelIndex sideIndex = model->index(0, 0);
        QCOMPARE(
            sideIndex.data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightPrimary);
        QCOMPARE(rightPane->currentWidget(), targetContentRaw);
        QCOMPARE(
            rightPane->visibleWidgets(), QList<QWidget *>({targetContentRaw}));
        auto *const leftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(!leftBar->currentSourceIndex().isValid());
        QVERIFY(leftBar->activeSourceIndexes().isEmpty());
        QCOMPARE(rightBar->currentSourceIndex(), sideIndex);
        QCOMPARE(rightBar->activeSourceIndexes(), QList<QModelIndex>({sideIndex}));

        auto reclaimed = target.shell->takePanel(zzPanelId("side"));
        QVERIFY(reclaimed);
        QCOMPARE(reclaimed.value(), targetContentRaw);
        std::unique_ptr<QWidget> reclaimedContent(reclaimed.value());
    }

    void rollbackCleanupPreservesRegistrationWhenCollapsedReaddsContent()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdPartyOwner;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QPointer<QWidget> targetContentGuard(targetContentRaw);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(leftPane);
        QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(rightPane);
        auto *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool cleanupArmed = false;
        int collapsedCallbackCount = 0;
        bool readdAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (cleanupArmed || height != 500
                    || targetContentGuard == nullptr) {
                    return;
                }
                targetContentGuard->setParent(&thirdPartyOwner);
                cleanupArmed = true;
            });
        QObject::connect(
            leftPane, &ZzFluentUI::ZzSidePane::collapsedChanged,
            target.shell.get(), [&](bool collapsed) {
                if (!cleanupArmed || !collapsed
                    || collapsedCallbackCount != 0) {
                    return;
                }
                ++collapsedCallbackCount;
                if (targetContentGuard == nullptr
                    || rightPaneGuard == nullptr) {
                    return;
                }
                targetContentGuard->setParent(nullptr);
                readdAccepted = rightPaneGuard->addWidget(
                    targetContentGuard, QStringLiteral("Side"));
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(cleanupArmed);
        QCOMPARE(collapsedCallbackCount, 1);
        QVERIFY(!targetContentGuard.isNull());
        QVERIFY(!leftPaneGuard.isNull());
        QVERIFY(!rightPaneGuard.isNull());
        QVERIFY(readdAccepted);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        const bool survivesOnLeft =
            leftPane->panelStack()->panels().contains(targetContentRaw);
        const bool survivesOnRight =
            rightPane->panelStack()->panels().contains(targetContentRaw);
        const int physicalOccurrences = int(survivesOnLeft)
            + int(survivesOnRight);
        const int modelRows = model->rowCount();
        const auto saved = target.shell->saveLayout();
        const QString diagnostic = QStringLiteral(
            "physical occurrences=%1, activity rows=%2, save accepted=%3")
                                       .arg(physicalOccurrences)
                                       .arg(modelRows)
                                       .arg(bool(saved));
        QVERIFY2(
            physicalOccurrences == 1 && modelRows == 1 && saved,
            qPrintable(diagnostic));
        QVERIFY(!survivesOnLeft);
        QVERIFY(survivesOnRight);
        const QModelIndex sideIndex = model->index(0, 0);
        QCOMPARE(
            sideIndex.data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightPrimary);
        QCOMPARE(rightPane->currentWidget(), targetContentRaw);
        QCOMPARE(
            rightPane->visibleWidgets(), QList<QWidget *>({targetContentRaw}));
        auto *const leftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QVERIFY(!leftBar->currentSourceIndex().isValid());
        QVERIFY(leftBar->activeSourceIndexes().isEmpty());
        QCOMPARE(rightBar->currentSourceIndex(), sideIndex);
        QCOMPARE(rightBar->activeSourceIndexes(), QList<QModelIndex>({sideIndex}));

        auto reclaimed = target.shell->takePanel(zzPanelId("side"));
        QVERIFY(reclaimed);
        QCOMPARE(reclaimed.value(), targetContentRaw);
        std::unique_ptr<QWidget> reclaimedContent(reclaimed.value());
    }

    void rollbackCleanupPreservesRegistrationWhenCurrentSyncReaddsContent()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        auto sourceAnchor = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("anchor"), QStringLiteral("Anchor"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceAnchor.get()));
        zzReleaseAfterAdoption(sourceAnchor);
        source.shell->bottomPane()->setMaximumPaneHeight(800);
        source.shell->bottomPane()->setPaneHeight(500);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        QWidget thirdPartyOwner;
        auto targetContent = std::make_unique<QWidget>();
        QWidget *const targetContentRaw = targetContent.get();
        QPointer<QWidget> targetContentGuard(targetContentRaw);
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        auto targetAnchor = std::make_unique<QWidget>();
        QWidget *const targetAnchorRaw = targetAnchor.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("anchor"), QStringLiteral("Anchor"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetAnchor.get()));
        zzReleaseAfterAdoption(targetAnchor);
        auto *const leftPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(rightPane);
        auto *const leftBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const model = leftBar->model();
        auto *const bottomPane = target.shell->bottomPane();
        bottomPane->setMaximumPaneHeight(800);
        bottomPane->setPaneHeight(240);
        bool cleanupArmed = false;
        bool currentReaddArmed = false;
        int currentCallbackCount = 0;
        bool readdAccepted = false;
        QObject::connect(
            bottomPane, &ZzFluentUI::ZzBottomPane::paneHeightChanged,
            target.shell.get(), [&](int height) {
                if (cleanupArmed || height != 500
                    || targetContentGuard == nullptr) {
                    return;
                }
                targetContentGuard->setParent(&thirdPartyOwner);
                cleanupArmed = true;
            });
        QObject::connect(
            leftBar, &ZzFluentUI::ZzActivityBar::currentSourceIndexChanged,
            target.shell.get(), [&](const QModelIndex &current) {
                if (!currentReaddArmed || !current.isValid()
                    || currentCallbackCount != 0) {
                    return;
                }
                ++currentCallbackCount;
                if (targetContentGuard == nullptr
                    || rightPaneGuard == nullptr) {
                    return;
                }
                targetContentGuard->setParent(nullptr);
                readdAccepted = rightPaneGuard->addWidget(
                    targetContentGuard, QStringLiteral("Side"));
            });
        QObject::connect(
            model, &QAbstractItemModel::rowsRemoved,
            target.shell.get(),
            [&](const QModelIndex &, int, int) {
                if (!cleanupArmed || currentReaddArmed) {
                    return;
                }
                leftBar->setCurrentSourceIndex({});
                currentReaddArmed = true;
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        const QString restoreDiagnostic = restored
            ? QStringLiteral("restore accepted before cleanup callback")
            : restored.error().technicalMessage();
        QVERIFY2(cleanupArmed, qPrintable(restoreDiagnostic));
        QVERIFY(currentReaddArmed);
        QCOMPARE(currentCallbackCount, 1);
        QVERIFY(!targetContentGuard.isNull());
        QVERIFY(!rightPaneGuard.isNull());
        QVERIFY(readdAccepted);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
        const bool survivesOnLeft =
            leftPane->panelStack()->panels().contains(targetContentRaw);
        const bool survivesOnRight =
            rightPane->panelStack()->panels().contains(targetContentRaw);
        const int physicalOccurrences = int(survivesOnLeft)
            + int(survivesOnRight);
        const int modelRows = model->rowCount();
        const auto saved = target.shell->saveLayout();
        const QString diagnostic = QStringLiteral(
            "physical occurrences=%1, activity rows=%2, save accepted=%3")
                                       .arg(physicalOccurrences)
                                       .arg(modelRows)
                                       .arg(bool(saved));
        QVERIFY2(
            physicalOccurrences == 1 && modelRows == 2 && saved,
            qPrintable(diagnostic));
        QVERIFY(!survivesOnLeft);
        QVERIFY(survivesOnRight);
        QModelIndex sideIndex;
        QModelIndex anchorIndex;
        for (int row = 0; row < modelRows; ++row) {
            const QModelIndex candidate = model->index(row, 0);
            if (candidate.data().toString() == QStringLiteral("Side")) {
                sideIndex = candidate;
            } else if (candidate.data().toString()
                       == QStringLiteral("Anchor")) {
                anchorIndex = candidate;
            }
        }
        QVERIFY(sideIndex.isValid());
        QVERIFY(anchorIndex.isValid());
        QCOMPARE(
            sideIndex.data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::RightPrimary);
        QCOMPARE(leftPane->currentWidget(), targetAnchorRaw);
        QCOMPARE(rightPane->currentWidget(), targetContentRaw);
        QCOMPARE(leftBar->currentSourceIndex(), anchorIndex);
        QCOMPARE(leftBar->activeSourceIndexes(), QList<QModelIndex>({anchorIndex}));
        QCOMPARE(rightBar->currentSourceIndex(), sideIndex);
        QCOMPARE(rightBar->activeSourceIndexes(), QList<QModelIndex>({sideIndex}));

        auto reclaimed = target.shell->takePanel(zzPanelId("side"));
        QVERIFY(reclaimed);
        QCOMPARE(reclaimed.value(), targetContentRaw);
        std::unique_ptr<QWidget> reclaimedContent(reclaimed.value());
    }

    void restoreUsesPaneCurrentForUnknownCurrentFallback()
    {
        ZzShellFixture fixture;
        auto two = std::make_unique<QWidget>();
        auto one = std::make_unique<QWidget>();
        QWidget *const twoRaw = two.get();
        QWidget *const oneRaw = one.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("two"), QStringLiteral("Two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, two.get()));
        zzReleaseAfterAdoption(two);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("one"), QStringLiteral("One"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, one.get()));
        zzReleaseAfterAdoption(one);

        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const bar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        for (const QString &requestedCurrent : {
                 QStringLiteral("unknown"), QString{}}) {
            QModelIndex oneIndex;
            for (int row = 0; row < bar->model()->rowCount(); ++row) {
                const QModelIndex candidate = bar->model()->index(row, 0);
                if (candidate.data().toString() == QStringLiteral("One")) {
                    oneIndex = candidate;
                    break;
                }
            }
            QVERIFY(oneIndex.isValid());
            bar->setCurrentSourceIndex(oneIndex);
            QVERIFY(pane->setCurrentWidget(twoRaw));
            QCOMPARE(
                bar->currentSourceIndex().data().toString(),
                QStringLiteral("One"));

            ZzTestVersionTwoLayout layout;
            layout.qtState = fixture.host.saveState(1);
            layout.leftCurrent = requestedCurrent;
            layout.leftVisible = {
                QStringLiteral("one"), QStringLiteral("two")};
            layout.leftSizes = {120, 240};
            layout.sideEntries = {
                {QStringLiteral("one"),
                 ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
                {QStringLiteral("two"),
                 ZzFluentUI::ZzActivityArea::LeftPrimary, 1},
            };
            if (!requestedCurrent.isEmpty()) {
                layout.leftVisible.append(requestedCurrent);
                layout.leftSizes.append(360);
                layout.sideEntries.append({
                    requestedCurrent,
                    ZzFluentUI::ZzActivityArea::LeftPrimary, 2});
            }
            layout.splitState =
                fixture.shell->splitWorkspace()->saveLayout();
            const QByteArray encoded = zzVersionTwoLayout(layout);
            QVERIFY(!encoded.isEmpty());

            const auto restored = fixture.shell->restoreLayout(encoded);
            const QString restoreDiagnostic = restored
                ? QStringLiteral("restore accepted")
                : restored.error().technicalMessage();
            QVERIFY2(restored, qPrintable(restoreDiagnostic));
            QCOMPARE(pane->currentWidget(), oneRaw);
            QCOMPARE(
                bar->currentSourceIndex().data().toString(),
                QStringLiteral("One"));
        }
    }

    void restoreRejectsSplitMutationFromLayoutChanged()
    {
        ZzShellFixture source;
        auto *const sourceSplit = source.shell->splitWorkspace();
        const auto sourceRoot = sourceSplit->activeGroupId();
        const auto sourceSecond = sourceSplit->splitGroup(
            sourceRoot, Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("requested")));
        QVERIFY(sourceSecond.has_value());
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto *const targetSplit = target.shell->splitWorkspace();
        const QByteArray splitBefore = targetSplit->saveLayout();
        bool armed = true;
        std::optional<ZzFluentUI::ZzTabGroupId> injected;
        QObject::connect(
            targetSplit, &ZzFluentUI::ZzSplitWorkspace::layoutChanged,
            target.shell.get(), [&] {
                if (!armed) {
                    return;
                }
                armed = false;
                injected = targetSplit->splitGroup(
                    targetSplit->groupIds().constFirst(), Qt::Vertical,
                    ZzFluentUI::ZzSplitPlacement::After,
                    ZzFluentUI::ZzTabGroupId(QStringLiteral("pollution")));
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!armed);
        QVERIFY(injected.has_value());
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(targetSplit->saveLayout(), splitBefore);
    }

    void restorePreservesIndependentTitleAndAlwaysOnTopChanges()
    {
        ZzShellFixture source;
        auto *const sourceSplit = source.shell->splitWorkspace();
        const auto sourceSecond = sourceSplit->splitGroup(
            sourceSplit->activeGroupId(), Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            ZzFluentUI::ZzTabGroupId(QStringLiteral("requested")));
        QVERIFY(sourceSecond.has_value());
        source.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        target.shell->setApplicationTitle(QStringLiteral("Before app"));
        target.shell->setCustomTitle(QStringLiteral("Before custom"));
        target.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        auto *const targetSplit = target.shell->splitWorkspace();
        bool armed = true;
        bool topChanged = false;
        QObject::connect(
            targetSplit, &ZzFluentUI::ZzSplitWorkspace::layoutChanged,
            target.shell.get(), [&] {
                if (!armed) {
                    return;
                }
                armed = false;
                target.shell->setApplicationTitle(
                    QStringLiteral("Changed app"));
                target.shell->setCustomTitle(
                    QStringLiteral("Changed custom"));
                topChanged = bool(target.shell->setAlwaysOnTop(true));
                static_cast<void>(targetSplit->splitGroup(
                    targetSplit->groupIds().constFirst(), Qt::Vertical,
                    ZzFluentUI::ZzSplitPlacement::After,
                    ZzFluentUI::ZzTabGroupId(QStringLiteral("pollution"))));
            });

        const auto restored = target.shell->restoreLayout(requested.value());

        QVERIFY(!armed);
        QVERIFY(topChanged);
        QVERIFY(!restored);
        QCOMPARE(restored.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(target.shell->applicationTitle(), QStringLiteral("Changed app"));
        QCOMPARE(target.shell->customTitle(), QStringLiteral("Changed custom"));
        QVERIFY(target.shell->isAlwaysOnTop());
        QCOMPARE(
            target.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::Application);
    }

    void restoreScalesLinearlyForDenseBottomPanels()
    {
        const auto measureRestore = [](int panelCount)
            -> std::optional<qint64> {
            ZzShellFixture source;
            for (int index = 0; index < panelCount; ++index) {
                auto content = std::make_unique<QWidget>();
                const QString id = QStringLiteral("bottom-%1").arg(index);
                if (!source.shell->registerBottomPanel(
                        ZzPureTools::ZzWorkspacePanelId(id), id, zzIcon(),
                        content.get())) {
                    return std::nullopt;
                }
                zzReleaseAfterAdoption(content);
            }
            const auto requested = source.shell->saveLayout();
            if (!requested) {
                return std::nullopt;
            }

            ZzShellFixture target;
            for (int index = 0; index < panelCount; ++index) {
                auto content = std::make_unique<QWidget>();
                const QString id = QStringLiteral("bottom-%1").arg(index);
                if (!target.shell->registerBottomPanel(
                        ZzPureTools::ZzWorkspacePanelId(id), id, zzIcon(),
                        content.get())) {
                    return std::nullopt;
                }
                zzReleaseAfterAdoption(content);
            }
            QElapsedTimer timer;
            timer.start();
            for (int iteration = 0; iteration < 2; ++iteration) {
                if (!target.shell->restoreLayout(requested.value())) {
                    return std::nullopt;
                }
            }
            return timer.elapsed();
        };

        const auto smallResult = measureRestore(128);
        const auto largeResult = measureRestore(1024);
        QVERIFY(smallResult.has_value());
        QVERIFY(largeResult.has_value());
        const qint64 small = smallResult.value_or(qint64{-1});
        const qint64 large = largeResult.value_or(qint64{-1});
        QVERIFY2(
            large < small * 8 + 250,
            qPrintable(QStringLiteral(
                "dense bottom restore scaling exceeded bound: %1 ms vs %2 ms")
                .arg(large).arg(small)));
    }

    void survivesHostDestructionBeforeShell()
    {
        auto host = std::make_unique<ZzLayoutTornDownMainWindow>();
        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(host.get());
        auto result = ZzPureTools::ZzWorkspaceShell::create(
            host.get(), titleBar);
        QVERIFY(result);
        auto shell = std::move(result).value();
        auto content = std::make_unique<QWidget>();
        QPointer<QWidget> contentGuard(content.get());
        QVERIFY(shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto secondContent = std::make_unique<QWidget>();
        QVERIFY(shell->registerDockPanel(
            zzPanelId("second-dock"), QStringLiteral("Second dock"), zzIcon(),
            Qt::RightDockWidgetArea, secondContent.get()));
        zzReleaseAfterAdoption(secondContent);

        host.reset();

        QVERIFY(contentGuard.isNull());
        QCOMPARE(shell->workspaceWidget(), nullptr);
        QCOMPARE(shell->tabWidget(), nullptr);
        QCOMPARE(shell->commandPalette(), nullptr);
        QVERIFY(!shell->saveLayout());
        shell.reset();
    }

    void destroysHostOwnedShellWithMultipleDocksAfterHostLayoutIsTornDown()
    {
        auto host = std::make_unique<QMainWindow>();
        auto result = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(result);
        auto shell = std::move(result).value();
        for (const auto &[id, area] : std::array{
                 std::pair{"first", Qt::BottomDockWidgetArea},
                 std::pair{"second", Qt::RightDockWidgetArea}}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(shell->registerDockPanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(), area,
                content.get()));
            zzReleaseAfterAdoption(content);
        }
        QPointer<QObject> shellGuard(shell.get());
        shell->setParent(host.get());
        zzReleaseAfterAdoption(shell);

        host.reset();

        QVERIFY(shellGuard.isNull());
    }
};

QTEST_MAIN(ZzWorkspaceShellTest)
#include "ZzWorkspaceShellTest.moc"
