#include "ZzWorkspaceLayoutCodecPrivate.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QHash>
#include <QtCore/QIODevice>
#include <QtCore/QSet>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzPureTools {
namespace {

using ZzLayoutState = ZzWorkspaceLayoutStatePrivate;

constexpr qsizetype zzMaximumLayoutSize = qsizetype{1024} * 1024;
constexpr qsizetype zzDigestSize = 32;
constexpr qsizetype zzHeaderSize = 12;
constexpr quint16 zzWorkspaceVersion = 2;
constexpr quint16 zzLegacyWorkspaceVersion = 1;
constexpr quint16 zzSplitVersion = 1;
constexpr auto zzStreamVersion = QDataStream::Qt_6_8;
constexpr quint32 zzMaximumVisibleSidePanels = 32;
constexpr quint32 zzMaximumSideEntries = 4096;
constexpr int zzMaximumIdLength = 256;
constexpr int zzMaximumSplitGroups = 64;
constexpr int zzMaximumSplitDepth = 16;
constexpr int zzMaximumSplitNodes = 127;
constexpr int zzMaximumSavedPages = 4096;
constexpr int zzMaximumPageOrder = 65535;
constexpr int zzLegacyBottomHeight = 240;

struct ZzSideEntry final
{
    QString id;
    ZzFluentUI::ZzActivityArea area =
        ZzFluentUI::ZzActivityArea::LeftPrimary;
    int order = 0;
};

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<ZzValue>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

[[nodiscard]] bool zzIsAreaValue(quint8 value) noexcept
{
    return value <= static_cast<quint8>(
        ZzFluentUI::ZzActivityArea::RightSecondary);
}

[[nodiscard]] bool zzIsLeftArea(
    ZzFluentUI::ZzActivityArea area) noexcept
{
    return area == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area == ZzFluentUI::ZzActivityArea::LeftSecondary;
}

[[nodiscard]] bool zzIsTitleMode(quint8 value) noexcept
{
    return value <= static_cast<quint8>(ZzLayoutState::ZzTitleMode::Custom);
}

[[nodiscard]] bool zzReadByteArray(
    QDataStream &stream,
    QByteArray *value,
    quint32 maximumSize)
{
    Q_ASSERT(value != nullptr);
    quint32 length = 0;
    stream >> length;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    if (length == std::numeric_limits<quint32>::max()) {
        value->clear();
        return true;
    }
    if (length > maximumSize) {
        return false;
    }
    value->resize(static_cast<qsizetype>(length));
    return length == 0
        || stream.readRawData(value->data(), static_cast<qint64>(length))
            == static_cast<qint64>(length);
}

/** @brief 在分配 QString 前读取并限制 Qt 标准 UTF-16 字段长度。 */
[[nodiscard]] bool zzReadQtString(
    QDataStream &stream,
    QString *value,
    bool allowEmpty)
{
    Q_ASSERT(value != nullptr);
    quint32 byteLength = 0;
    stream >> byteLength;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    if (byteLength == std::numeric_limits<quint32>::max()) {
        value->clear();
        return allowEmpty;
    }
    if (byteLength % 2 != 0
        || byteLength / 2 > static_cast<quint32>(zzMaximumIdLength)) {
        return false;
    }
    const quint32 length = byteLength / 2;
    value->clear();
    value->reserve(static_cast<qsizetype>(length));
    for (quint32 index = 0; index < length; ++index) {
        quint16 codeUnit = 0;
        stream >> codeUnit;
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
        value->append(QChar(codeUnit));
    }
    *value = value->trimmed();
    return allowEmpty || !value->isEmpty();
}

[[nodiscard]] bool zzReadSplitString(
    QDataStream &stream,
    QString *value,
    bool allowEmpty = false)
{
    Q_ASSERT(value != nullptr);
    quint16 length = 0;
    stream >> length;
    if (stream.status() != QDataStream::Ok
        || length > zzMaximumIdLength) {
        return false;
    }
    value->clear();
    value->reserve(length);
    for (quint16 index = 0; index < length; ++index) {
        quint16 codeUnit = 0;
        stream >> codeUnit;
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
        value->append(QChar(codeUnit));
    }
    *value = value->trimmed();
    return allowEmpty || !value->isEmpty();
}

void zzWriteSplitString(QDataStream &stream, const QString &value)
{
    stream << static_cast<quint16>(value.size());
    for (const QChar character : value) {
        stream << character.unicode();
    }
}

[[nodiscard]] bool zzDecodeEnvelope(
    const QByteArray &encoded,
    QByteArrayView magic,
    const QSet<quint16> &versions,
    quint16 *schema,
    QByteArray *payload)
{
    Q_ASSERT(schema != nullptr);
    Q_ASSERT(payload != nullptr);
    if (encoded.size() < zzHeaderSize + zzDigestSize
        || encoded.size() > zzMaximumLayoutSize) {
        return false;
    }
    QDataStream stream(encoded);
    stream.setVersion(zzStreamVersion);
    char rawMagic[4]{};
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (stream.readRawData(rawMagic, 4) != 4) {
        return false;
    }
    stream >> *schema >> streamVersion >> payloadLength;
    const qint64 expectedSize = zzHeaderSize
        + static_cast<qint64>(payloadLength) + zzDigestSize;
    if (stream.status() != QDataStream::Ok
        || QByteArrayView(rawMagic, 4) != magic
        || !versions.contains(*schema)
        || streamVersion != static_cast<quint16>(zzStreamVersion)
        || expectedSize != encoded.size()) {
        return false;
    }
    payload->resize(static_cast<qsizetype>(payloadLength));
    if (payloadLength > 0
        && stream.readRawData(payload->data(), payloadLength)
            != static_cast<qint64>(payloadLength)) {
        return false;
    }
    QByteArray digest(zzDigestSize, Qt::Uninitialized);
    return stream.readRawData(digest.data(), zzDigestSize) == zzDigestSize
        && stream.status() == QDataStream::Ok && stream.atEnd()
        && digest == QCryptographicHash::hash(
            *payload, QCryptographicHash::Sha256);
}

[[nodiscard]] QByteArray zzEncodeEnvelope(
    QByteArrayView magic,
    quint16 schema,
    const QByteArray &payload)
{
    const qsizetype totalSize = zzHeaderSize + payload.size() + zzDigestSize;
    if (totalSize > zzMaximumLayoutSize) {
        return {};
    }
    QByteArray encoded;
    encoded.reserve(totalSize);
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setVersion(zzStreamVersion);
    if (stream.writeRawData(magic.data(), 4) != 4) {
        return {};
    }
    stream << schema << static_cast<quint16>(zzStreamVersion)
           << static_cast<quint32>(payload.size());
    if (stream.writeRawData(payload.constData(), payload.size())
            != payload.size()
        || stream.status() != QDataStream::Ok) {
        return {};
    }
    encoded.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return encoded;
}

[[nodiscard]] bool zzReadSplitNode(
    QDataStream &stream,
    int depth,
    std::optional<Qt::Orientation> parentOrientation,
    int *nodeCount,
    QSet<QString> *groups,
    ZzLayoutState::ZzSplitNode *node)
{
    Q_ASSERT(nodeCount != nullptr);
    Q_ASSERT(groups != nullptr);
    Q_ASSERT(node != nullptr);
    if (depth > zzMaximumSplitDepth || ++(*nodeCount) > zzMaximumSplitNodes) {
        return false;
    }
    quint8 kind = 0;
    stream >> kind;
    if (stream.status() != QDataStream::Ok || kind > 1) {
        return false;
    }
    node->currentIndex = -1;
    if (kind == 0) {
        QString id;
        if (!zzReadSplitString(stream, &id)
            || groups->contains(id)
            || groups->size() >= zzMaximumSplitGroups) {
            return false;
        }
        groups->insert(id);
        node->leaf = true;
        node->groupId = std::move(id);
        return true;
    }

    quint8 orientation = 0;
    quint16 childCount = 0;
    stream >> orientation >> childCount;
    if (stream.status() != QDataStream::Ok
        || (orientation != static_cast<quint8>(Qt::Horizontal)
            && orientation != static_cast<quint8>(Qt::Vertical))
        || childCount < 2 || childCount > zzMaximumSplitGroups) {
        return false;
    }
    const auto parsedOrientation = static_cast<Qt::Orientation>(orientation);
    if (parentOrientation.has_value()
        && parentOrientation.value() == parsedOrientation) {
        return false;
    }
    node->leaf = false;
    node->orientation = parsedOrientation;
    node->children.clear();
    node->children.reserve(childCount);
    for (quint16 index = 0; index < childCount; ++index) {
        ZzLayoutState::ZzSplitNode child;
        if (!zzReadSplitNode(stream, depth + 1, parsedOrientation,
                nodeCount, groups, &child)) {
            return false;
        }
        node->children.append(std::move(child));
    }
    quint16 sizeCount = 0;
    stream >> sizeCount;
    if (stream.status() != QDataStream::Ok || sizeCount != childCount) {
        return false;
    }
    node->sizes.clear();
    node->sizes.reserve(sizeCount);
    for (quint16 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        stream >> size;
        if (stream.status() != QDataStream::Ok
            || size <= 0 || size > zzMaximumLayoutSize) {
            return false;
        }
        node->sizes.append(size);
    }
    return true;
}

[[nodiscard]] bool zzValidateAndWriteSplitNode(
    QDataStream &stream,
    const ZzLayoutState::ZzSplitNode &node,
    int depth,
    std::optional<Qt::Orientation> parentOrientation,
    int *nodeCount,
    QSet<QString> *groups,
    QStringList *groupOrder)
{
    if (depth > zzMaximumSplitDepth || ++(*nodeCount) > zzMaximumSplitNodes
        || node.currentIndex < -1) {
        return false;
    }
    if (node.leaf) {
        const QString id = node.groupId.trimmed();
        if (id.isEmpty() || id.size() > zzMaximumIdLength
            || !node.children.isEmpty() || !node.sizes.isEmpty()
            || groups->contains(id) || groups->size() >= zzMaximumSplitGroups) {
            return false;
        }
        groups->insert(id);
        groupOrder->append(id);
        stream << quint8(0);
        zzWriteSplitString(stream, id);
        return stream.status() == QDataStream::Ok;
    }
    if (!node.groupId.isEmpty() || node.currentIndex != -1
        || (node.orientation != Qt::Horizontal
            && node.orientation != Qt::Vertical)
        || (parentOrientation.has_value()
            && parentOrientation.value() == node.orientation)
        || node.children.size() < 2
        || node.children.size() > zzMaximumSplitGroups
        || node.sizes.size() != node.children.size()) {
        return false;
    }
    stream << quint8(1) << static_cast<quint8>(node.orientation)
           << static_cast<quint16>(node.children.size());
    for (const ZzLayoutState::ZzSplitNode &child : node.children) {
        if (!zzValidateAndWriteSplitNode(stream, child, depth + 1,
                node.orientation, nodeCount, groups, groupOrder)) {
            return false;
        }
    }
    stream << static_cast<quint16>(node.sizes.size());
    for (const int size : node.sizes) {
        if (size <= 0 || size > zzMaximumLayoutSize) {
            return false;
        }
        stream << static_cast<qint32>(size);
    }
    return stream.status() == QDataStream::Ok;
}

void zzCollectGroupOrder(
    const ZzLayoutState::ZzSplitNode &node,
    QStringList *groupOrder)
{
    if (node.leaf) {
        groupOrder->append(node.groupId);
        return;
    }
    for (const auto &child : node.children) {
        zzCollectGroupOrder(child, groupOrder);
    }
}

void zzClearSplitCurrentIndexes(ZzLayoutState::ZzSplitNode *node)
{
    node->currentIndex = -1;
    for (auto &child : node->children) {
        zzClearSplitCurrentIndexes(&child);
    }
}

[[nodiscard]] std::optional<ZzLayoutState::ZzSplitProjection>
zzReadSplit(const QByteArray &encoded)
{
    quint16 schema = 0;
    QByteArray payload;
    if (!zzDecodeEnvelope(encoded, QByteArrayView("ZZSW", 4),
            {zzSplitVersion}, &schema, &payload)) {
        return std::nullopt;
    }
    QDataStream stream(payload);
    stream.setVersion(zzStreamVersion);
    ZzLayoutState::ZzSplitProjection split;
    QSet<QString> groups;
    int nodeCount = 0;
    if (!zzReadSplitNode(stream, 1, std::nullopt,
            &nodeCount, &groups, &split.root)) {
        return std::nullopt;
    }
    QString activeGroup;
    if (!zzReadSplitString(stream, &activeGroup)
        || !groups.contains(activeGroup)) {
        return std::nullopt;
    }
    split.activeGroup = std::move(activeGroup);

    quint16 pageCount = 0;
    stream >> pageCount;
    if (stream.status() != QDataStream::Ok
        || pageCount > zzMaximumSavedPages) {
        return std::nullopt;
    }
    split.savedPages.reserve(pageCount);
    QSet<QString> keys;
    QSet<QString> groupsWithCurrent;
    QHash<QString, QSet<int>> ordersByGroup;
    for (quint16 index = 0; index < pageCount; ++index) {
        ZzLayoutState::ZzSplitSavedPage page;
        qint32 order = 0;
        quint8 current = 0;
        if (!zzReadSplitString(stream, &page.key)
            || !zzReadSplitString(stream, &page.groupId)) {
            return std::nullopt;
        }
        stream >> order >> current;
        if (stream.status() != QDataStream::Ok
            || keys.contains(page.key) || !groups.contains(page.groupId)
            || order < 0 || order > zzMaximumPageOrder
            || ordersByGroup[page.groupId].contains(order)
            || current > 1
            || (current == 1 && groupsWithCurrent.contains(page.groupId))) {
            return std::nullopt;
        }
        keys.insert(page.key);
        ordersByGroup[page.groupId].insert(order);
        if (current == 1) {
            groupsWithCurrent.insert(page.groupId);
        }
        page.order = order;
        page.current = current == 1;
        split.savedPages.append(std::move(page));
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
        return std::nullopt;
    }
    zzCollectGroupOrder(split.root, &split.groupOrder);
    return split;
}

[[nodiscard]] QByteArray zzWriteSplit(
    const ZzLayoutState::ZzSplitProjection &split)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(zzStreamVersion);
    QSet<QString> groups;
    QStringList groupOrder;
    int nodeCount = 0;
    if (!zzValidateAndWriteSplitNode(stream, split.root, 1, std::nullopt,
            &nodeCount, &groups, &groupOrder)
        || split.groupOrder != groupOrder) {
        return {};
    }
    const QString activeGroup = split.activeGroup.trimmed();
    if (activeGroup.size() > zzMaximumIdLength
        || !groups.contains(activeGroup)) {
        return {};
    }
    zzWriteSplitString(stream, activeGroup);
    if (split.savedPages.size() > zzMaximumSavedPages) {
        return {};
    }
    stream << static_cast<quint16>(split.savedPages.size());
    QSet<QString> keys;
    QSet<QString> groupsWithCurrent;
    QHash<QString, QSet<int>> ordersByGroup;
    for (const ZzLayoutState::ZzSplitSavedPage &rawPage : split.savedPages) {
        const QString key = rawPage.key.trimmed();
        const QString group = rawPage.groupId.trimmed();
        if (key.isEmpty() || key.size() > zzMaximumIdLength
            || group.isEmpty() || group.size() > zzMaximumIdLength
            || keys.contains(key) || !groups.contains(group)
            || rawPage.order < 0 || rawPage.order > zzMaximumPageOrder
            || ordersByGroup[group].contains(rawPage.order)
            || (rawPage.current && groupsWithCurrent.contains(group))) {
            return {};
        }
        keys.insert(key);
        ordersByGroup[group].insert(rawPage.order);
        if (rawPage.current) {
            groupsWithCurrent.insert(group);
        }
        zzWriteSplitString(stream, key);
        zzWriteSplitString(stream, group);
        stream << static_cast<qint32>(rawPage.order)
               << static_cast<quint8>(rawPage.current ? 1 : 0);
    }
    if (stream.status() != QDataStream::Ok) {
        return {};
    }
    return zzEncodeEnvelope(
        QByteArrayView("ZZSW", 4), zzSplitVersion, payload);
}

[[nodiscard]] bool zzSameEncodableSplit(
    const ZzLayoutState::ZzSplitProjection &split,
    const QByteArray &canonicalState)
{
    const auto decodedCanonical = zzReadSplit(canonicalState);
    if (!decodedCanonical.has_value()) {
        return false;
    }
    ZzLayoutState::ZzSplitProjection expected = split;
    zzClearSplitCurrentIndexes(&expected.root);
    expected.canonicalState.clear();
    ZzLayoutState::ZzSplitProjection actual = *decodedCanonical;
    actual.canonicalState.clear();
    return expected == actual;
}

[[nodiscard]] bool zzHasOnlyUnsetCurrentIndexes(
    const ZzLayoutState::ZzSplitNode &node)
{
    if (node.currentIndex != -1) {
        return false;
    }
    return std::all_of(node.children.cbegin(), node.children.cend(),
        [](const ZzLayoutState::ZzSplitNode &child) {
            return zzHasOnlyUnsetCurrentIndexes(child);
        });
}

[[nodiscard]] bool zzValidateSourceSplit(
    const ZzLayoutState::ZzLayoutRequest &request,
    const ZzLayoutState::ZzSplitProjection &split)
{
    using ZzSourceSchema = ZzLayoutState::ZzLayoutRequest::ZzSourceSchema;
    if (request.sourceSchema == ZzSourceSchema::VersionTwo) {
        return zzHasOnlyUnsetCurrentIndexes(split.root);
    }
    if (request.sourceSchema != ZzSourceSchema::VersionOne) {
        return false;
    }
    return split.root.leaf
        && split.root.groupId == QStringLiteral("legacy-root")
        && split.root.orientation == Qt::Horizontal
        && split.root.children.isEmpty()
        && split.root.sizes.isEmpty()
        && split.root.currentIndex >= -1
        && split.activeGroup == QStringLiteral("legacy-root")
        && split.groupOrder
            == QStringList({QStringLiteral("legacy-root")})
        && split.savedPages.isEmpty();
}

void zzAppendAreaEntries(
    const QStringList &ids,
    ZzFluentUI::ZzActivityArea area,
    QList<ZzSideEntry> *entries)
{
    for (qsizetype index = 0; index < ids.size(); ++index) {
        entries->append({ids.at(index), area, static_cast<int>(index)});
    }
}

[[nodiscard]] bool zzBuildEntries(
    const ZzLayoutState::ZzWorkspaceProjection &projection,
    QList<ZzSideEntry> *entries)
{
    entries->clear();
    const std::array<qsizetype, 4> counts = {
        projection.activity.leftPrimary.size(),
        projection.activity.leftSecondary.size(),
        projection.activity.rightPrimary.size(),
        projection.activity.rightSecondary.size()};
    qsizetype totalCount = 0;
    for (const qsizetype count : counts) {
        if (count > zzMaximumSideEntries
            || totalCount > zzMaximumSideEntries - count) {
            return false;
        }
        totalCount += count;
    }
    entries->reserve(totalCount);
    zzAppendAreaEntries(projection.activity.leftPrimary,
        ZzFluentUI::ZzActivityArea::LeftPrimary, entries);
    zzAppendAreaEntries(projection.activity.leftSecondary,
        ZzFluentUI::ZzActivityArea::LeftSecondary, entries);
    zzAppendAreaEntries(projection.activity.rightPrimary,
        ZzFluentUI::ZzActivityArea::RightPrimary, entries);
    zzAppendAreaEntries(projection.activity.rightSecondary,
        ZzFluentUI::ZzActivityArea::RightSecondary, entries);
    QSet<QString> ids;
    QStringList leftOrder;
    QStringList rightOrder;
    for (ZzSideEntry &entry : *entries) {
        entry.id = entry.id.trimmed();
        if (entry.id.isEmpty() || entry.id.size() > zzMaximumIdLength
            || ids.contains(entry.id)) {
            return false;
        }
        ids.insert(entry.id);
        (zzIsLeftArea(entry.area) ? leftOrder : rightOrder).append(entry.id);
    }
    return leftOrder == projection.leftSide.order
        && rightOrder == projection.rightSide.order;
}

[[nodiscard]] bool zzValidateSide(
    const ZzLayoutState::ZzSideProjection &side,
    bool left,
    const QList<ZzSideEntry> &entries,
    QSet<QString> *visibleIds)
{
    if (side.width <= 0 || side.width > zzMaximumLayoutSize
        || side.visible.size() > zzMaximumVisibleSidePanels
        || side.visible.size() != side.sizes.size()) {
        return false;
    }
    const QString current = side.current.trimmed();
    if (current.size() > zzMaximumIdLength
        || (!current.isEmpty() && !side.visible.contains(current))) {
        return false;
    }
    for (qsizetype index = 0; index < side.visible.size(); ++index) {
        const QString id = side.visible.at(index).trimmed();
        const auto found = std::find_if(entries.cbegin(), entries.cend(),
            [&id](const ZzSideEntry &entry) { return entry.id == id; });
        if (id.isEmpty() || id.size() > zzMaximumIdLength
            || visibleIds->contains(id) || side.sizes.at(index) <= 0
            || side.sizes.at(index) > zzMaximumLayoutSize
            || found == entries.cend() || zzIsLeftArea(found->area) != left) {
            return false;
        }
        visibleIds->insert(id);
    }
    return true;
}

void zzPopulateProjectionEntries(
    const QList<ZzSideEntry> &entries,
    ZzLayoutState::ZzWorkspaceProjection *projection)
{
    struct ZzOrderedId final
    {
        int order = 0;
        QString id;
    };
    std::array<QList<ZzOrderedId>, 4> areas;
    for (const ZzSideEntry &entry : entries) {
        areas.at(static_cast<std::size_t>(entry.area)).append(
            {entry.order, entry.id});
    }
    for (auto &area : areas) {
        std::sort(area.begin(), area.end(),
            [](const ZzOrderedId &left, const ZzOrderedId &right) {
                return left.order < right.order;
            });
    }
    const auto ids = [](const QList<ZzOrderedId> &ordered) {
        QStringList result;
        result.reserve(ordered.size());
        for (const auto &entry : ordered) {
            result.append(entry.id);
        }
        return result;
    };
    projection->activity.leftPrimary = ids(areas.at(0));
    projection->activity.leftSecondary = ids(areas.at(1));
    projection->activity.rightPrimary = ids(areas.at(2));
    projection->activity.rightSecondary = ids(areas.at(3));
    projection->leftSide.order = projection->activity.leftPrimary
        + projection->activity.leftSecondary;
    projection->rightSide.order = projection->activity.rightPrimary
        + projection->activity.rightSecondary;
}

[[nodiscard]] bool zzReadSideEntries(
    QDataStream &stream,
    quint32 count,
    QList<ZzSideEntry> *entries)
{
    if (count > zzMaximumSideEntries) {
        return false;
    }
    entries->clear();
    entries->reserve(static_cast<qsizetype>(count));
    QSet<QString> ids;
    std::array<QSet<int>, 4> orders;
    for (quint32 index = 0; index < count; ++index) {
        ZzSideEntry entry;
        quint8 area = 0;
        qint32 order = 0;
        if (!zzReadQtString(stream, &entry.id, false)) {
            return false;
        }
        stream >> area >> order;
        if (stream.status() != QDataStream::Ok || !zzIsAreaValue(area)
            || order < 0
            || ids.contains(entry.id)
            || orders.at(area).contains(order)) {
            return false;
        }
        ids.insert(entry.id);
        orders.at(area).insert(order);
        entry.area = static_cast<ZzFluentUI::ZzActivityArea>(area);
        entry.order = order;
        entries->append(std::move(entry));
    }
    return true;
}

[[nodiscard]] bool zzReadVisibleSide(
    QDataStream &stream,
    ZzLayoutState::ZzSideProjection *side,
    QSet<QString> *allVisible)
{
    quint32 count = 0;
    stream >> count;
    if (stream.status() != QDataStream::Ok
        || count > zzMaximumVisibleSidePanels) {
        return false;
    }
    side->visible.clear();
    side->visible.reserve(static_cast<qsizetype>(count));
    for (quint32 index = 0; index < count; ++index) {
        QString id;
        if (!zzReadQtString(stream, &id, false)
            || allVisible->contains(id)) {
            return false;
        }
        allVisible->insert(id);
        side->visible.append(std::move(id));
    }
    quint32 sizeCount = 0;
    stream >> sizeCount;
    if (stream.status() != QDataStream::Ok || sizeCount != count) {
        return false;
    }
    side->sizes.clear();
    side->sizes.reserve(static_cast<qsizetype>(sizeCount));
    for (quint32 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        stream >> size;
        if (stream.status() != QDataStream::Ok
            || size <= 0 || size > zzMaximumLayoutSize) {
            return false;
        }
        side->sizes.append(size);
    }
    return true;
}

[[nodiscard]] bool zzValidateSideReferences(
    const ZzLayoutState::ZzWorkspaceProjection &projection,
    const QList<ZzSideEntry> &entries)
{
    QSet<QString> visible;
    return zzValidateSide(projection.leftSide, true, entries, &visible)
        && zzValidateSide(projection.rightSide, false, entries, &visible);
}

[[nodiscard]] bool zzValidateDerivedSideState(
    const ZzLayoutState::ZzLayoutRequest &request,
    const ZzLayoutState::ZzWorkspaceProjection &projection)
{
    const QSet<QString> leftActive(
        projection.leftSide.visible.cbegin(),
        projection.leftSide.visible.cend());
    const QSet<QString> rightActive(
        projection.rightSide.visible.cbegin(),
        projection.rightSide.visible.cend());
    return request.leftCurrent == projection.leftSide.current
        && request.rightCurrent == projection.rightSide.current
        && projection.activity.leftCurrent == projection.leftSide.current
        && projection.activity.rightCurrent == projection.rightSide.current
        && projection.activity.leftActive == leftActive
        && projection.activity.rightActive == rightActive;
}

[[nodiscard]] bool zzReadVersionTwoPayload(
    const QByteArray &payload,
    ZzLayoutState::ZzLayoutRequest *request)
{
    QDataStream stream(payload);
    stream.setVersion(zzStreamVersion);
    ZzLayoutState::ZzWorkspaceProjection projection;
    quint8 leftCollapsed = 0;
    qint32 leftWidth = 0;
    if (!zzReadByteArray(stream, &projection.dock.state,
            static_cast<quint32>(zzMaximumLayoutSize))) {
        return false;
    }
    stream >> leftCollapsed >> leftWidth;
    if (stream.status() != QDataStream::Ok || leftCollapsed > 1
        || leftWidth <= 0 || leftWidth > zzMaximumLayoutSize
        || !zzReadQtString(stream, &projection.leftSide.current, true)) {
        return false;
    }
    projection.leftSide.collapsed = leftCollapsed == 1;
    projection.leftSide.width = leftWidth;
    QSet<QString> visibleIds;
    if (!zzReadVisibleSide(stream, &projection.leftSide, &visibleIds)) {
        return false;
    }
    quint8 rightCollapsed = 0;
    qint32 rightWidth = 0;
    stream >> rightCollapsed >> rightWidth;
    if (stream.status() != QDataStream::Ok || rightCollapsed > 1
        || rightWidth <= 0 || rightWidth > zzMaximumLayoutSize
        || !zzReadQtString(stream, &projection.rightSide.current, true)
        || !zzReadVisibleSide(stream, &projection.rightSide, &visibleIds)) {
        return false;
    }
    projection.rightSide.collapsed = rightCollapsed == 1;
    projection.rightSide.width = rightWidth;

    quint32 sideCount = 0;
    stream >> sideCount;
    QList<ZzSideEntry> entries;
    if (stream.status() != QDataStream::Ok
        || !zzReadSideEntries(stream, sideCount, &entries)) {
        return false;
    }
    zzPopulateProjectionEntries(entries, &projection);
    if (!zzValidateSideReferences(projection, entries)) {
        return false;
    }
    QByteArray splitState;
    if (!zzReadByteArray(stream, &splitState,
            static_cast<quint32>(zzMaximumLayoutSize))) {
        return false;
    }
    auto split = zzReadSplit(splitState);
    if (!split.has_value()) {
        return false;
    }
    const QByteArray canonicalSplit = zzWriteSplit(*split);
    if (canonicalSplit.isEmpty()) {
        return false;
    }
    split->canonicalState = canonicalSplit;
    projection.split = std::move(*split);

    quint8 bottomCollapsed = 0;
    qint32 bottomHeight = 0;
    quint8 titleMode = 0;
    stream >> bottomCollapsed >> bottomHeight;
    if (stream.status() != QDataStream::Ok || bottomCollapsed > 1
        || bottomHeight <= 0 || bottomHeight > zzMaximumLayoutSize
        || !zzReadQtString(stream, &projection.bottom.current, true)) {
        return false;
    }
    stream >> titleMode;
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || !zzIsTitleMode(titleMode)) {
        return false;
    }
    projection.bottom.collapsed = bottomCollapsed == 1;
    projection.bottom.height = bottomHeight;
    if (!projection.bottom.current.isEmpty()) {
        projection.bottom.order = {projection.bottom.current};
        projection.bottom.visible = {projection.bottom.current};
    }
    projection.title.mode = static_cast<ZzLayoutState::ZzTitleMode>(titleMode);
    projection.activity.leftCurrent = projection.leftSide.current;
    projection.activity.rightCurrent = projection.rightSide.current;
    projection.activity.leftActive = QSet<QString>(
        projection.leftSide.visible.cbegin(), projection.leftSide.visible.cend());
    projection.activity.rightActive = QSet<QString>(
        projection.rightSide.visible.cbegin(), projection.rightSide.visible.cend());
    request->leftCurrent = projection.leftSide.current;
    request->rightCurrent = projection.rightSide.current;
    request->projection = std::move(projection);
    request->sourceSchema =
        ZzLayoutState::ZzLayoutRequest::ZzSourceSchema::VersionTwo;
    return true;
}

[[nodiscard]] bool zzEnsureLegacyCurrentEntry(
    const QString &current,
    ZzFluentUI::ZzActivityArea area,
    QList<ZzSideEntry> *entries)
{
    if (current.isEmpty()
        || std::any_of(entries->cbegin(), entries->cend(),
            [&current](const ZzSideEntry &entry) { return entry.id == current; })) {
        return true;
    }
    if (entries->size() >= zzMaximumSideEntries) {
        return false;
    }
    int maximumOrder = -1;
    QSet<int> usedOrders;
    for (const ZzSideEntry &entry : std::as_const(*entries)) {
        if (entry.area == area) {
            maximumOrder = std::max(maximumOrder, entry.order);
            usedOrders.insert(entry.order);
        }
    }
    int nextOrder = 0;
    if (maximumOrder < std::numeric_limits<qint32>::max()) {
        nextOrder = maximumOrder + 1;
    } else {
        while (usedOrders.contains(nextOrder)) {
            ++nextOrder;
        }
    }
    entries->append({current, area, nextOrder});
    return true;
}

[[nodiscard]] bool zzReadVersionOnePayload(
    const QByteArray &payload,
    ZzLayoutState::ZzLayoutRequest *request)
{
    QDataStream stream(payload);
    stream.setVersion(zzStreamVersion);
    ZzLayoutState::ZzWorkspaceProjection projection;
    quint8 leftCollapsed = 0;
    quint8 rightCollapsed = 0;
    qint32 leftWidth = 0;
    qint32 rightWidth = 0;
    if (!zzReadByteArray(stream, &projection.dock.state,
            static_cast<quint32>(zzMaximumLayoutSize))) {
        return false;
    }
    stream >> leftCollapsed >> leftWidth >> rightCollapsed >> rightWidth;
    if (stream.status() != QDataStream::Ok
        || leftCollapsed > 1 || rightCollapsed > 1
        || leftWidth <= 0 || leftWidth > zzMaximumLayoutSize
        || rightWidth <= 0 || rightWidth > zzMaximumLayoutSize
        || !zzReadQtString(stream, &projection.leftSide.current, true)
        || !zzReadQtString(stream, &projection.rightSide.current, true)) {
        return false;
    }
    projection.leftSide.collapsed = leftCollapsed == 1;
    projection.leftSide.width = leftWidth;
    projection.rightSide.collapsed = rightCollapsed == 1;
    projection.rightSide.width = rightWidth;
    quint32 sideCount = 0;
    stream >> sideCount;
    QList<ZzSideEntry> entries;
    if (stream.status() != QDataStream::Ok
        || !zzReadSideEntries(stream, sideCount, &entries)) {
        return false;
    }
    if (!zzEnsureLegacyCurrentEntry(projection.leftSide.current,
            ZzFluentUI::ZzActivityArea::LeftPrimary, &entries)
        || !zzEnsureLegacyCurrentEntry(projection.rightSide.current,
            ZzFluentUI::ZzActivityArea::RightPrimary, &entries)) {
        return false;
    }
    zzPopulateProjectionEntries(entries, &projection);
    if (!projection.leftSide.current.isEmpty()) {
        projection.leftSide.visible = {projection.leftSide.current};
        projection.leftSide.sizes = {1};
    }
    if (!projection.rightSide.current.isEmpty()) {
        projection.rightSide.visible = {projection.rightSide.current};
        projection.rightSide.sizes = {1};
    }
    if (!zzValidateSideReferences(projection, entries)) {
        return false;
    }
    qint32 currentTabIndex = -1;
    quint8 titleMode = 0;
    stream >> currentTabIndex >> titleMode;
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || currentTabIndex < -1 || !zzIsTitleMode(titleMode)) {
        return false;
    }
    projection.split.root.groupId = QStringLiteral("legacy-root");
    projection.split.root.currentIndex = currentTabIndex;
    projection.split.activeGroup = QStringLiteral("legacy-root");
    projection.split.groupOrder = {QStringLiteral("legacy-root")};
    projection.split.canonicalState = zzWriteSplit(projection.split);
    if (projection.split.canonicalState.isEmpty()) {
        return false;
    }
    projection.bottom.collapsed = true;
    projection.bottom.height = zzLegacyBottomHeight;
    projection.title.mode = static_cast<ZzLayoutState::ZzTitleMode>(titleMode);
    projection.activity.leftCurrent = projection.leftSide.current;
    projection.activity.rightCurrent = projection.rightSide.current;
    projection.activity.leftActive = QSet<QString>(
        projection.leftSide.visible.cbegin(), projection.leftSide.visible.cend());
    projection.activity.rightActive = QSet<QString>(
        projection.rightSide.visible.cbegin(), projection.rightSide.visible.cend());
    request->leftCurrent = projection.leftSide.current;
    request->rightCurrent = projection.rightSide.current;
    request->projection = std::move(projection);
    request->sourceSchema =
        ZzLayoutState::ZzLayoutRequest::ZzSourceSchema::VersionOne;
    return true;
}

} // namespace

ZzCore::ZzResult<ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest>
ZzWorkspaceLayoutCodecPrivate::decode(const QByteArray &encoded)
{
    quint16 schema = 0;
    QByteArray payload;
    if (!zzDecodeEnvelope(encoded, QByteArrayView("ZZWS", 4),
            {zzLegacyWorkspaceVersion, zzWorkspaceVersion},
            &schema, &payload)) {
        return zzFailure<ZzLayoutState::ZzLayoutRequest>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace layout envelope is invalid"));
    }
    ZzLayoutState::ZzLayoutRequest request;
    const bool valid = schema == zzLegacyWorkspaceVersion
        ? zzReadVersionOnePayload(payload, &request)
        : zzReadVersionTwoPayload(payload, &request);
    if (!valid) {
        return zzFailure<ZzLayoutState::ZzLayoutRequest>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace layout payload is invalid"));
    }
    return ZzCore::ZzResult<ZzLayoutState::ZzLayoutRequest>::success(
        std::move(request));
}

ZzCore::ZzResult<QByteArray>
ZzWorkspaceLayoutCodecPrivate::encodeVersionTwo(
    const ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest &request)
{
    if (!request.projection.has_value()) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout request has no projection"));
    }
    const ZzLayoutState::ZzWorkspaceProjection &projection =
        *request.projection;
    QList<ZzSideEntry> entries;
    const QByteArray generatedSplit = zzWriteSplit(projection.split);
    if (projection.dock.state.size() > zzMaximumLayoutSize
        || !zzBuildEntries(projection, &entries)
        || !zzValidateSideReferences(projection, entries)
        || !zzValidateDerivedSideState(request, projection)
        || !zzValidateSourceSplit(request, projection.split)
        || projection.bottom.height <= 0
        || projection.bottom.height > zzMaximumLayoutSize
        || projection.bottom.current.trimmed().size() > zzMaximumIdLength
        || !zzIsTitleMode(static_cast<quint8>(projection.title.mode))
        || generatedSplit.isEmpty()
        || generatedSplit != projection.split.canonicalState
        || !zzSameEncodableSplit(projection.split, generatedSplit)) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout projection is invalid"));
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(zzStreamVersion);
    const auto writeSide = [&stream](const ZzLayoutState::ZzSideProjection &side) {
        stream << static_cast<quint8>(side.collapsed ? 1 : 0)
               << static_cast<qint32>(side.width)
               << side.current.trimmed()
               << static_cast<quint32>(side.visible.size());
        for (const QString &id : side.visible) {
            stream << id.trimmed();
        }
        stream << static_cast<quint32>(side.sizes.size());
        for (const int size : side.sizes) {
            stream << static_cast<qint32>(size);
        }
    };
    stream << projection.dock.state;
    writeSide(projection.leftSide);
    writeSide(projection.rightSide);
    stream << static_cast<quint32>(entries.size());
    for (const ZzSideEntry &entry : std::as_const(entries)) {
        stream << entry.id << static_cast<quint8>(entry.area)
               << static_cast<qint32>(entry.order);
    }
    stream << generatedSplit
           << static_cast<quint8>(projection.bottom.collapsed ? 1 : 0)
           << static_cast<qint32>(projection.bottom.height)
           << projection.bottom.current.trimmed()
           << static_cast<quint8>(projection.title.mode);
    if (stream.status() != QDataStream::Ok) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::Io,
            QStringLiteral("Failed to serialize workspace payload"));
    }
    QByteArray encoded = zzEncodeEnvelope(
        QByteArrayView("ZZWS", 4), zzWorkspaceVersion, payload);
    if (encoded.isEmpty()) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout exceeds 1 MiB"));
    }
    return ZzCore::ZzResult<QByteArray>::success(std::move(encoded));
}

ZzCore::ZzResult<QByteArray>
ZzWorkspaceLayoutCodecPrivate::canonicalizeSplit(const QByteArray &encoded)
{
    const auto split = zzReadSplit(encoded);
    if (!split.has_value()) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Split layout is invalid"));
    }
    QByteArray canonical = zzWriteSplit(*split);
    if (canonical.isEmpty()) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Split layout cannot be canonicalized"));
    }
    return ZzCore::ZzResult<QByteArray>::success(std::move(canonical));
}

} // namespace ZzPureTools
