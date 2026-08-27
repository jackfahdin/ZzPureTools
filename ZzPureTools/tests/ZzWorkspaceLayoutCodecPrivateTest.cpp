#include "../widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h"

#include <functional>
#include <limits>
#include <optional>
#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtTest/QTest>

#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityArea.h>

namespace {

using ZzCodec = ZzPureTools::ZzWorkspaceLayoutCodecPrivate;
using ZzState = ZzPureTools::ZzWorkspaceLayoutStatePrivate;

[[nodiscard]] const ZzState::ZzWorkspaceProjection &
zzProjectionOrDefault(const ZzState::ZzLayoutRequest &request)
{
    static const ZzState::ZzWorkspaceProjection emptyProjection;
    if (!request.projection.has_value()) {
        return emptyProjection;
    }
    return *request.projection;
}

[[nodiscard]] ZzState::ZzWorkspaceProjection &
zzMutableProjection(ZzState::ZzLayoutRequest &request)
{
    if (!request.projection.has_value()) {
        request.projection = ZzState::ZzWorkspaceProjection{};
    }
    return *request.projection;
}

constexpr qsizetype zzMaximumLayoutSize = qsizetype{1024} * 1024;

struct ZzTestSideEntry final
{
    QString id;
    ZzFluentUI::ZzActivityArea area =
        ZzFluentUI::ZzActivityArea::LeftPrimary;
    qint32 order = 0;
};

struct ZzTestSplitNode final
{
    bool leaf = true;
    QString groupId;
    Qt::Orientation orientation = Qt::Horizontal;
    std::optional<quint8> encodedOrientation;
    QList<ZzTestSplitNode> children;
    QList<qint32> sizes;
};

struct ZzTestSavedPage final
{
    QString key;
    QString groupId;
    qint32 order = 0;
    quint8 current = 0;
};

struct ZzTestVersionTwoLayout final
{
    QByteArray qtState;
    quint8 leftCollapsed = 1;
    qint32 leftWidth = 280;
    QString leftCurrent;
    QStringList leftVisible;
    QList<qint32> leftSizes;
    quint8 rightCollapsed = 1;
    qint32 rightWidth = 280;
    QString rightCurrent;
    QStringList rightVisible;
    QList<qint32> rightSizes;
    QList<ZzTestSideEntry> sideEntries;
    QByteArray splitState;
    quint8 bottomCollapsed = 1;
    qint32 bottomHeight = 240;
    QString bottomCurrent;
    quint8 titleMode = 0;
};

void zzWriteSplitString(QDataStream &stream, const QString &value)
{
    stream << static_cast<quint16>(value.size());
    for (const QChar character : value) {
        stream << character.unicode();
    }
}

void zzWriteSplitNode(QDataStream &stream, const ZzTestSplitNode &node)
{
    if (node.leaf) {
        stream << quint8(0);
        zzWriteSplitString(stream, node.groupId);
        return;
    }
    stream << quint8(1)
           << node.encodedOrientation.value_or(
                  static_cast<quint8>(node.orientation))
           << static_cast<quint16>(node.children.size());
    for (const ZzTestSplitNode &child : node.children) {
        zzWriteSplitNode(stream, child);
    }
    stream << static_cast<quint16>(node.sizes.size());
    for (const qint32 size : node.sizes) {
        stream << size;
    }
}

[[nodiscard]] QByteArray zzEnvelope(
    const char *magic,
    quint16 schema,
    const QByteArray &payload)
{
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    if (stream.writeRawData(magic, 4) != 4) {
        return {};
    }
    stream << schema << static_cast<quint16>(QDataStream::Qt_6_8)
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

[[nodiscard]] QByteArray zzSplitLayout(
    const ZzTestSplitNode &root,
    const QString &activeGroup,
    const QList<ZzTestSavedPage> &pages = {},
    std::optional<quint16> pageCountOverride = std::nullopt)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    zzWriteSplitNode(stream, root);
    zzWriteSplitString(stream, activeGroup);
    stream << pageCountOverride.value_or(
        static_cast<quint16>(pages.size()));
    for (const ZzTestSavedPage &page : pages) {
        zzWriteSplitString(stream, page.key);
        zzWriteSplitString(stream, page.groupId);
        stream << page.order << page.current;
    }
    return zzEnvelope("ZZSW", 1, payload);
}

[[nodiscard]] ZzTestSplitNode zzLeaf(QString id)
{
    ZzTestSplitNode node;
    node.groupId = std::move(id);
    return node;
}

[[nodiscard]] ZzTestSplitNode zzFlatSplit(int groups)
{
    if (groups == 1) {
        return zzLeaf(QStringLiteral("group-0"));
    }
    ZzTestSplitNode root;
    root.leaf = false;
    root.orientation = Qt::Horizontal;
    for (int index = 0; index < groups; ++index) {
        root.children.append(
            zzLeaf(QStringLiteral("group-%1").arg(index)));
        root.sizes.append(1);
    }
    return root;
}

[[nodiscard]] ZzTestSplitNode zzDepthSplit(int depth, int *nextGroup)
{
    if (depth == 1) {
        return zzLeaf(QStringLiteral("group-%1").arg((*nextGroup)++));
    }
    ZzTestSplitNode root;
    root.leaf = false;
    root.orientation = depth % 2 == 0 ? Qt::Horizontal : Qt::Vertical;
    root.children = {
        zzDepthSplit(depth - 1, nextGroup),
        zzLeaf(QStringLiteral("group-%1").arg((*nextGroup)++))};
    root.sizes = {1, 1};
    return root;
}

[[nodiscard]] ZzTestSplitNode zzBalancedSplit(
    int leaves,
    int depth,
    int *nextGroup)
{
    if (leaves == 1) {
        return zzLeaf(QStringLiteral("group-%1").arg((*nextGroup)++));
    }
    const int leftLeaves = leaves / 2;
    ZzTestSplitNode root;
    root.leaf = false;
    root.orientation = depth % 2 == 1 ? Qt::Horizontal : Qt::Vertical;
    root.children = {
        zzBalancedSplit(leftLeaves, depth + 1, nextGroup),
        zzBalancedSplit(leaves - leftLeaves, depth + 1, nextGroup)};
    root.sizes = {1, 1};
    return root;
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
    for (const ZzTestSideEntry &entry : layout.sideEntries) {
        stream << entry.id << static_cast<quint8>(entry.area) << entry.order;
    }
    stream << layout.splitState << layout.bottomCollapsed
           << layout.bottomHeight << layout.bottomCurrent << layout.titleMode;
    return zzEnvelope("ZZWS", 2, payload);
}

[[nodiscard]] QByteArray zzVersionOneLayout(
    const QByteArray &qtState,
    bool leftCollapsed,
    qint32 leftWidth,
    bool rightCollapsed,
    qint32 rightWidth,
    const QString &leftCurrent,
    const QString &rightCurrent,
    const QList<ZzTestSideEntry> &entries,
    qint32 currentTabIndex,
    quint8 titleMode)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    stream << qtState << leftCollapsed << leftWidth
           << rightCollapsed << rightWidth
           << leftCurrent << rightCurrent
           << static_cast<quint32>(entries.size());
    for (const ZzTestSideEntry &entry : entries) {
        stream << entry.id << static_cast<quint8>(entry.area) << entry.order;
    }
    stream << currentTabIndex << titleMode;
    return zzEnvelope("ZZWS", 1, payload);
}

[[nodiscard]] ZzTestVersionTwoLayout zzValidVersionTwoLayout()
{
    ZzTestVersionTwoLayout layout;
    layout.splitState = zzSplitLayout(
        zzLeaf(QStringLiteral("editor")), QStringLiteral("editor"));
    return layout;
}

[[nodiscard]] QByteArray zzMutateDigest(QByteArray encoded)
{
    encoded[encoded.size() - 1] = static_cast<char>(
        encoded.at(encoded.size() - 1) ^ 0x01);
    return encoded;
}

[[nodiscard]] QByteArray zzLayoutWithVisibleCount(int count)
{
    auto layout = zzValidVersionTwoLayout();
    layout.leftCollapsed = 0;
    for (int index = 0; index < count; ++index) {
        const QString id = QStringLiteral("side-%1").arg(index);
        layout.leftVisible.append(id);
        layout.leftSizes.append(index + 1);
        layout.sideEntries.append({id,
            ZzFluentUI::ZzActivityArea::LeftPrimary, index});
    }
    layout.leftCurrent = layout.leftVisible.value(0);
    return zzVersionTwoLayout(layout);
}

[[nodiscard]] QByteArray zzLayoutWithSideCount(int count)
{
    auto layout = zzValidVersionTwoLayout();
    for (int index = 0; index < count; ++index) {
        layout.sideEntries.append({
            QStringLiteral("side-%1").arg(index, 4, 10, QLatin1Char('0')),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            index});
    }
    return zzVersionTwoLayout(layout);
}

[[nodiscard]] QByteArray zzLayoutWithSplit(
    const ZzTestSplitNode &root,
    const QString &active,
    const QList<ZzTestSavedPage> &pages = {})
{
    auto layout = zzValidVersionTwoLayout();
    layout.splitState = zzSplitLayout(root, active, pages);
    return zzVersionTwoLayout(layout);
}

[[nodiscard]] QByteArray zzLayoutAtEncodedSize(qsizetype size)
{
    auto layout = zzValidVersionTwoLayout();
    const qsizetype baseSize = zzVersionTwoLayout(layout).size();
    layout.qtState = QByteArray(size - baseSize, 'q');
    return zzVersionTwoLayout(layout);
}

} // namespace

class ZzWorkspaceLayoutCodecPrivateTest final : public QObject
{
    Q_OBJECT

private slots:
    void versionThreeRoundTripStoresOneCurrentAndExpandedPerSide()
    {
        auto layout = zzValidVersionTwoLayout();
        layout.leftCollapsed = 0;
        layout.leftWidth = 371;
        layout.leftCurrent = QStringLiteral("search");
        layout.leftVisible = {QStringLiteral("explorer"),
            QStringLiteral("search")};
        layout.leftSizes = {120, 251};
        layout.sideEntries = {
            {QStringLiteral("explorer"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("search"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1}};

        const auto versionTwo = ZzCodec::decode(zzVersionTwoLayout(layout));
        QVERIFY(versionTwo);
        const auto encoded = ZzCodec::encodeVersionThree(versionTwo.value());
        QVERIFY(encoded);

        QDataStream envelope(encoded.value());
        envelope.setVersion(QDataStream::Qt_6_8);
        char magic[4]{};
        quint16 schema = 0;
        quint16 streamVersion = 0;
        quint32 payloadSize = 0;
        QCOMPARE(envelope.readRawData(magic, 4), 4);
        envelope >> schema >> streamVersion >> payloadSize;
        QCOMPARE(schema, quint16(3));
        QCOMPARE(streamVersion, static_cast<quint16>(QDataStream::Qt_6_8));
        QVERIFY(payloadSize > 0);

        const auto decoded = ZzCodec::decode(encoded.value());
        QVERIFY(decoded);
        QCOMPARE(decoded.value().sourceSchema,
            ZzState::ZzLayoutRequest::ZzSourceSchema::VersionThree);
        const auto &projection = zzProjectionOrDefault(decoded.value());
        QCOMPARE(projection.leftSide.current, QStringLiteral("search"));
        QCOMPARE(projection.leftSide.visible,
            QStringList({QStringLiteral("search")}));
        QCOMPARE(projection.leftSide.sizes, QList<int>({1}));
    }

    void migratesSchemaOneIntoConcreteSchemaTwoRequest()
    {
        const QList<ZzTestSideEntry> entries = {
            {QStringLiteral("explorer"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("search"),
                ZzFluentUI::ZzActivityArea::LeftSecondary, 0},
            {QStringLiteral("terminal"),
                ZzFluentUI::ZzActivityArea::RightPrimary, 0}};
        const QByteArray encoded = zzVersionOneLayout(
            QByteArrayLiteral("qt-dock-v1"), false, 320, true, 440,
            QStringLiteral("explorer"), QStringLiteral("terminal"),
            entries, 2, 3);

        const auto decoded = ZzCodec::decode(encoded);
        QVERIFY(decoded);
        QVERIFY(decoded.value().projection.has_value());
        const auto &request = decoded.value();
        const auto &projection = zzProjectionOrDefault(request);
        QCOMPARE(request.sourceSchema,
            ZzState::ZzLayoutRequest::ZzSourceSchema::VersionOne);
        QCOMPARE(request.leftCurrent, QStringLiteral("explorer"));
        QCOMPARE(request.rightCurrent, QStringLiteral("terminal"));
        QCOMPARE(projection.dock.state, QByteArrayLiteral("qt-dock-v1"));
        QCOMPARE(projection.leftSide.order,
            QStringList({QStringLiteral("explorer"), QStringLiteral("search")}));
        QCOMPARE(projection.leftSide.visible,
            QStringList({QStringLiteral("explorer")}));
        QCOMPARE(projection.leftSide.sizes, QList<int>({1}));
        QCOMPARE(projection.rightSide.visible,
            QStringList({QStringLiteral("terminal")}));
        QCOMPARE(projection.activity.leftSecondary,
            QStringList({QStringLiteral("search")}));
        QVERIFY(projection.split.root.leaf);
        QCOMPARE(projection.split.root.groupId, QStringLiteral("legacy-root"));
        QCOMPARE(projection.split.root.currentIndex, 2);
        QCOMPARE(projection.split.activeGroup, QStringLiteral("legacy-root"));
        QCOMPARE(projection.split.groupOrder,
            QStringList({QStringLiteral("legacy-root")}));
        QVERIFY(projection.split.savedPages.isEmpty());
        QCOMPARE(projection.split.canonicalState,
            zzSplitLayout(zzLeaf(QStringLiteral("legacy-root")),
                QStringLiteral("legacy-root")));
        QVERIFY(projection.bottom.collapsed);
        QCOMPARE(projection.bottom.height, 240);
        QVERIFY(projection.bottom.current.isEmpty());
        QCOMPARE(projection.title.mode, ZzState::ZzTitleMode::Custom);

        const auto migrated = ZzCodec::encodeVersionTwo(request);
        QVERIFY(migrated);
        const auto decodedAgain = ZzCodec::decode(migrated.value());
        QVERIFY(decodedAgain);
        QVERIFY(decodedAgain.value().projection.has_value());
        QCOMPARE(decodedAgain.value().sourceSchema,
            ZzState::ZzLayoutRequest::ZzSourceSchema::VersionTwo);
        QCOMPARE(
            zzProjectionOrDefault(decodedAgain.value()).split.root.currentIndex,
            -1);
        const auto encodedAgain = ZzCodec::encodeVersionTwo(
            decodedAgain.value());
        QVERIFY(encodedAgain);
        QCOMPARE(encodedAgain.value(), migrated.value());
    }

    void decodesFullRangeSparseSideOrders_data()
    {
        QTest::addColumn<QByteArray>("encoded");
        QTest::addColumn<QStringList>("expectedOrder");
        QTest::addColumn<QByteArray>("expectedCanonical");

        const QStringList expectedOrder = {
            QStringLiteral("first"), QStringLiteral("sparse")};
        const QList<qint32> sparseOrders = {
            4096, std::numeric_limits<qint32>::max()};
        for (const qint32 sparseOrder : sparseOrders) {
            const QList<ZzTestSideEntry> sparseEntries = {
                {QStringLiteral("sparse"),
                    ZzFluentUI::ZzActivityArea::LeftPrimary, sparseOrder},
                {QStringLiteral("first"),
                    ZzFluentUI::ZzActivityArea::LeftPrimary, 0}};
            const QList<ZzTestSideEntry> canonicalEntries = {
                {QStringLiteral("first"),
                    ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
                {QStringLiteral("sparse"),
                    ZzFluentUI::ZzActivityArea::LeftPrimary, 1}};

            auto versionOneCanonical = zzValidVersionTwoLayout();
            versionOneCanonical.sideEntries = canonicalEntries;
            versionOneCanonical.splitState = zzSplitLayout(
                zzLeaf(QStringLiteral("legacy-root")),
                QStringLiteral("legacy-root"));
            QTest::newRow(qPrintable(
                QStringLiteral("v1-order-%1").arg(sparseOrder)))
                << zzVersionOneLayout({}, true, 280, true, 280, {}, {},
                       sparseEntries, -1, 0)
                << expectedOrder
                << zzVersionTwoLayout(versionOneCanonical);

            auto versionTwo = zzValidVersionTwoLayout();
            versionTwo.sideEntries = sparseEntries;
            auto versionTwoCanonical = versionTwo;
            versionTwoCanonical.sideEntries = canonicalEntries;
            QTest::newRow(qPrintable(
                QStringLiteral("v2-order-%1").arg(sparseOrder)))
                << zzVersionTwoLayout(versionTwo)
                << expectedOrder
                << zzVersionTwoLayout(versionTwoCanonical);
        }
    }

    void decodesFullRangeSparseSideOrders()
    {
        QFETCH(QByteArray, encoded);
        QFETCH(QStringList, expectedOrder);
        QFETCH(QByteArray, expectedCanonical);

        const auto decoded = ZzCodec::decode(encoded);
        QVERIFY(decoded);
        QCOMPARE(zzProjectionOrDefault(decoded.value()).activity.leftPrimary,
                 expectedOrder);
        const auto canonical = ZzCodec::encodeVersionTwo(decoded.value());
        QVERIFY(canonical);
        QCOMPARE(canonical.value(), expectedCanonical);
        const auto decodedAgain = ZzCodec::decode(canonical.value());
        QVERIFY(decodedAgain);
        const auto encodedAgain = ZzCodec::encodeVersionTwo(
            decodedAgain.value());
        QVERIFY(encodedAgain);
        QCOMPARE(encodedAgain.value(), canonical.value());
    }

    void addsMissingLegacyCurrentWithoutOverflow()
    {
        const QByteArray encoded = zzVersionOneLayout(
            {}, true, 280, true, 280, QStringLiteral("current"), {},
            {{QStringLiteral("last"),
                 ZzFluentUI::ZzActivityArea::LeftPrimary,
                 std::numeric_limits<qint32>::max()}},
            -1, 0);

        const auto decoded = ZzCodec::decode(encoded);
        QVERIFY(decoded);
        QCOMPARE(
            zzProjectionOrDefault(decoded.value()).activity.leftPrimary,
            QStringList({QStringLiteral("current"), QStringLiteral("last")}));

        auto expected = zzValidVersionTwoLayout();
        expected.leftCurrent = QStringLiteral("current");
        expected.leftVisible = {QStringLiteral("current")};
        expected.leftSizes = {1};
        expected.sideEntries = {
            {QStringLiteral("current"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("last"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1}};
        expected.splitState = zzSplitLayout(
            zzLeaf(QStringLiteral("legacy-root")),
            QStringLiteral("legacy-root"));
        const auto canonical = ZzCodec::encodeVersionTwo(decoded.value());
        QVERIFY(canonical);
        QCOMPARE(canonical.value(), zzVersionTwoLayout(expected));
        const auto decodedAgain = ZzCodec::decode(canonical.value());
        QVERIFY(decodedAgain);
        const auto encodedAgain = ZzCodec::encodeVersionTwo(
            decodedAgain.value());
        QVERIFY(encodedAgain);
        QCOMPARE(encodedAgain.value(), canonical.value());
    }

    void rejectsSchemaTwoCurrentIndexInjection_data()
    {
        QTest::addColumn<QByteArray>("encoded");
        QTest::addColumn<bool>("injectChild");

        QTest::newRow("single-leaf")
            << zzVersionTwoLayout(zzValidVersionTwoLayout()) << false;

        ZzTestSplitNode root;
        root.leaf = false;
        root.orientation = Qt::Horizontal;
        root.children = {
            zzLeaf(QStringLiteral("first")),
            zzLeaf(QStringLiteral("second"))};
        root.sizes = {1, 1};
        auto multiGroup = zzValidVersionTwoLayout();
        multiGroup.splitState = zzSplitLayout(
            root, QStringLiteral("first"));
        QTest::newRow("multi-group")
            << zzVersionTwoLayout(multiGroup) << true;
    }

    void rejectsSchemaTwoCurrentIndexInjection()
    {
        QFETCH(QByteArray, encoded);
        QFETCH(bool, injectChild);

        const auto decoded = ZzCodec::decode(encoded);
        QVERIFY(decoded);
        auto injected = decoded.value();
        QVERIFY(injected.projection.has_value());
        if (injectChild) {
          zzMutableProjection(injected).split.root.children[0].currentIndex = 0;
        } else {
          zzMutableProjection(injected).split.root.currentIndex = 0;
        }
        QVERIFY(!ZzCodec::encodeVersionTwo(injected));
    }

    void roundTripsSchemaTwoWithoutChangingBytesContract()
    {
        ZzTestSplitNode split;
        split.leaf = false;
        split.orientation = Qt::Horizontal;
        split.children = {
            zzLeaf(QStringLiteral("editor")),
            zzLeaf(QStringLiteral("preview"))};
        split.sizes = {640, 360};
        const QList<ZzTestSavedPage> pages = {
            {QStringLiteral("main.cpp"), QStringLiteral("editor"), 0, 1},
            {QStringLiteral("README.md"), QStringLiteral("preview"), 0, 1}};

        auto layout = zzValidVersionTwoLayout();
        layout.qtState = QByteArrayLiteral("qt-state-v1");
        layout.leftCollapsed = 0;
        layout.leftWidth = 312;
        layout.leftCurrent = QStringLiteral("explorer");
        layout.leftVisible = {
            QStringLiteral("explorer"), QStringLiteral("search")};
        layout.leftSizes = {300, 180};
        layout.rightCollapsed = 0;
        layout.rightWidth = 360;
        layout.rightCurrent = QStringLiteral("terminal");
        layout.rightVisible = {QStringLiteral("terminal")};
        layout.rightSizes = {220};
        layout.sideEntries = {
            {QStringLiteral("explorer"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("search"),
                ZzFluentUI::ZzActivityArea::LeftSecondary, 0},
            {QStringLiteral("terminal"),
                ZzFluentUI::ZzActivityArea::RightPrimary, 0}};
        layout.splitState = zzSplitLayout(
            split, QStringLiteral("preview"), pages);
        layout.bottomCollapsed = 0;
        layout.bottomHeight = 260;
        layout.bottomCurrent = QStringLiteral("problems");
        layout.titleMode = 2;
        const QByteArray encoded = zzVersionTwoLayout(layout);

        const auto decoded = ZzCodec::decode(encoded);
        QVERIFY(decoded);
        QCOMPARE(decoded.value().sourceSchema,
            ZzState::ZzLayoutRequest::ZzSourceSchema::VersionTwo);
        const auto &projection = zzProjectionOrDefault(decoded.value());
        QCOMPARE(projection.split.groupOrder,
            QStringList({QStringLiteral("editor"), QStringLiteral("preview")}));
        QCOMPARE(projection.split.savedPages.size(), 2);
        QCOMPARE(projection.split.canonicalState, layout.splitState);
        QVERIFY(projection.identities.isEmpty());
        QVERIFY(projection.leftSide.paneIdentity.object.isNull());
        const auto reencoded = ZzCodec::encodeVersionTwo(decoded.value());
        QVERIFY(reencoded);
        QCOMPARE(reencoded.value(), encoded);

        auto sparse = zzValidVersionTwoLayout();
        sparse.sideEntries = {
            {QStringLiteral("first"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("second"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 7}};
        const auto sparseDecoded = ZzCodec::decode(zzVersionTwoLayout(sparse));
        QVERIFY(sparseDecoded);
        QCOMPARE(
            zzProjectionOrDefault(sparseDecoded.value()).activity.leftPrimary,
            QStringList({QStringLiteral("first"), QStringLiteral("second")}));
        sparse.sideEntries[1].order = 1;
        const auto sparseCanonical = ZzCodec::encodeVersionTwo(
            sparseDecoded.value());
        QVERIFY(sparseCanonical);
        QCOMPARE(sparseCanonical.value(), zzVersionTwoLayout(sparse));
        const auto sparseDecodedAgain = ZzCodec::decode(
            sparseCanonical.value());
        QVERIFY(sparseDecodedAgain);
        const auto sparseEncodedAgain = ZzCodec::encodeVersionTwo(
            sparseDecodedAgain.value());
        QVERIFY(sparseEncodedAgain);
        QCOMPARE(sparseEncodedAgain.value(), sparseCanonical.value());
    }

    void writerRejectsSplitStateThatReaderRejects()
    {
        const QByteArray invalidSplit = zzSplitLayout(
            zzLeaf(QStringLiteral("group")), QStringLiteral("missing"));
        auto invalidLayout = zzValidVersionTwoLayout();
        invalidLayout.splitState = invalidSplit;
        QVERIFY(!ZzCodec::decode(zzVersionTwoLayout(invalidLayout)));

        const auto valid = ZzCodec::decode(
            zzVersionTwoLayout(zzValidVersionTwoLayout()));
        QVERIFY(valid);
        QVERIFY(valid.value().projection.has_value());
        auto invalidRequest = valid.value();
        zzMutableProjection(invalidRequest).split.canonicalState = invalidSplit;
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        invalidRequest = valid.value();
        zzMutableProjection(invalidRequest).leftSide.sizes = {1};
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        invalidRequest = valid.value();
        // 故意模拟反序列化产生的越界枚举，验证 writer 的输入防线。
        zzMutableProjection(invalidRequest).title.mode =
            // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
            static_cast<ZzState::ZzTitleMode>(255);
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        invalidRequest = valid.value();
        invalidRequest.leftCurrent = QStringLiteral("injected");
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        invalidRequest = valid.value();
        zzMutableProjection(invalidRequest).activity.leftCurrent =
            QStringLiteral("injected");
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        invalidRequest = valid.value();
        zzMutableProjection(invalidRequest)
            .activity.leftActive.insert(QStringLiteral("injected"));
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        invalidRequest = valid.value();
        invalidRequest.sourceSchema =
            ZzState::ZzLayoutRequest::ZzSourceSchema::VersionOne;
        QVERIFY(!ZzCodec::encodeVersionTwo(invalidRequest));

        const QByteArray crossSideCurrent = zzVersionOneLayout(
            {}, true, 280, true, 280,
            QStringLiteral("right-panel"), {},
            {{QStringLiteral("right-panel"),
                ZzFluentUI::ZzActivityArea::RightPrimary, 0}},
            -1, 0);
        QVERIFY(!ZzCodec::decode(crossSideCurrent));
    }

    void rejectsVisibleOrderOutsidePhysicalSubsequence()
    {
        auto layout = zzValidVersionTwoLayout();
        layout.leftCollapsed = 0;
        layout.leftCurrent = QStringLiteral("second");
        layout.leftVisible = {
            QStringLiteral("second"), QStringLiteral("first")};
        layout.leftSizes = {222, 111};
        layout.sideEntries = {
            {QStringLiteral("first"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("second"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1}};

        const auto decoded = ZzCodec::decode(zzVersionTwoLayout(layout));
        QVERIFY(!decoded);
        QCOMPARE(decoded.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        layout.leftVisible = {
            QStringLiteral("first"), QStringLiteral("second")};
        layout.leftSizes = {111, 222};
        layout.leftCurrent = QStringLiteral("first");
        const auto valid = ZzCodec::decode(zzVersionTwoLayout(layout));
        QVERIFY(valid);
        QVERIFY(valid.value().projection.has_value());

        auto invalidRequest = valid.value();
        zzMutableProjection(invalidRequest).leftSide.visible = {
            QStringLiteral("second"), QStringLiteral("first")};
        zzMutableProjection(invalidRequest).leftSide.sizes = {222, 111};
        zzMutableProjection(invalidRequest).leftSide.current =
            QStringLiteral("second");
        zzMutableProjection(invalidRequest).activity.leftCurrent =
            QStringLiteral("second");
        invalidRequest.leftCurrent = QStringLiteral("second");

        const auto encoded = ZzCodec::encodeVersionTwo(invalidRequest);
        QVERIFY(!encoded);
        QCOMPARE(encoded.error().code(), ZzCore::ZzErrorCode::InvalidState);
    }

    void canonicalSplitTargetRejectsLayoutChangedInjection()
    {
        ZzTestSplitNode original;
        original.leaf = false;
        original.orientation = Qt::Horizontal;
        original.children = {
            zzLeaf(QStringLiteral("left")), zzLeaf(QStringLiteral("right"))};
        original.sizes = {2, 1};
        auto layout = zzValidVersionTwoLayout();
        layout.splitState = zzSplitLayout(
            original, QStringLiteral("left"));
        const auto decoded = ZzCodec::decode(zzVersionTwoLayout(layout));
        QVERIFY(decoded);
        QVERIFY(decoded.value().projection.has_value());

        auto injected = decoded.value();
        zzMutableProjection(injected).split.canonicalState = zzSplitLayout(
            zzLeaf(QStringLiteral("injected")), QStringLiteral("injected"));
        QVERIFY(ZzCodec::canonicalizeSplit(
            zzMutableProjection(injected).split.canonicalState));
        QVERIFY(!ZzCodec::encodeVersionTwo(injected));

        injected = decoded.value();
        zzMutableProjection(injected).split.root.sizes = {1, 2};
        QVERIFY(!ZzCodec::encodeVersionTwo(injected));

        const QByteArray nonCanonical = zzSplitLayout(
            zzLeaf(QStringLiteral("  normalized  ")),
            QStringLiteral(" normalized "));
        const auto canonical = ZzCodec::canonicalizeSplit(nonCanonical);
        QVERIFY(canonical);
        QCOMPARE(canonical.value(), zzSplitLayout(
            zzLeaf(QStringLiteral("normalized")),
            QStringLiteral("normalized")));
    }

    void boundsAllCountsBeforeAllocation_data()
    {
        QTest::addColumn<QByteArray>("encoded");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("visible-32") << zzLayoutWithVisibleCount(32) << true;
        QTest::newRow("visible-33") << zzLayoutWithVisibleCount(33) << false;
        QTest::newRow("side-entries-4096")
            << zzLayoutWithSideCount(4096) << true;
        QTest::newRow("side-entries-4097")
            << zzLayoutWithSideCount(4097) << false;
        QTest::newRow("split-groups-64")
            << zzLayoutWithSplit(zzFlatSplit(64), QStringLiteral("group-0"))
            << true;
        QTest::newRow("split-groups-65")
            << zzLayoutWithSplit(zzFlatSplit(65), QStringLiteral("group-0"))
            << false;

        int nextGroup = 0;
        const ZzTestSplitNode depth16 = zzDepthSplit(16, &nextGroup);
        QTest::newRow("split-depth-16")
            << zzLayoutWithSplit(depth16, QStringLiteral("group-0")) << true;
        nextGroup = 0;
        const ZzTestSplitNode depth17 = zzDepthSplit(17, &nextGroup);
        QTest::newRow("split-depth-17")
            << zzLayoutWithSplit(depth17, QStringLiteral("group-0")) << false;

        nextGroup = 0;
        const ZzTestSplitNode nodes127 = zzBalancedSplit(64, 1, &nextGroup);
        QTest::newRow("split-nodes-127")
            << zzLayoutWithSplit(nodes127, QStringLiteral("group-0")) << true;
        nextGroup = 0;
        ZzTestSplitNode nodes128;
        nodes128.leaf = false;
        nodes128.orientation = Qt::Horizontal;
        nodes128.children = {
            zzBalancedSplit(64, 2, &nextGroup),
            zzLeaf(QStringLiteral("overflow"))};
        nodes128.sizes = {1, 1};
        QTest::newRow("split-nodes-128")
            << zzLayoutWithSplit(nodes128, QStringLiteral("group-0")) << false;

        QList<ZzTestSavedPage> pages;
        pages.reserve(4097);
        for (int index = 0; index < 4097; ++index) {
            pages.append({QStringLiteral("key-%1").arg(index),
                QStringLiteral("group-0"), index, quint8(index == 0)});
        }
        QTest::newRow("saved-pages-4096")
            << zzLayoutWithSplit(zzLeaf(QStringLiteral("group-0")),
                   QStringLiteral("group-0"), pages.first(4096))
            << true;
        QTest::newRow("saved-pages-4097")
            << zzLayoutWithSplit(zzLeaf(QStringLiteral("group-0")),
                   QStringLiteral("group-0"), pages)
            << false;

        QTest::newRow("id-256")
            << zzLayoutWithSplit(zzLeaf(QString(256, QLatin1Char('g'))),
                   QString(256, QLatin1Char('g')))
            << true;
        QTest::newRow("id-257")
            << zzLayoutWithSplit(zzLeaf(QString(257, QLatin1Char('g'))),
                   QString(257, QLatin1Char('g')))
            << false;
        QTest::newRow("key-256")
            << zzLayoutWithSplit(zzLeaf(QStringLiteral("group-0")),
                   QStringLiteral("group-0"),
                   {{QString(256, QLatin1Char('k')),
                       QStringLiteral("group-0"), 0, 1}})
            << true;
        QTest::newRow("key-257")
            << zzLayoutWithSplit(zzLeaf(QStringLiteral("group-0")),
                   QStringLiteral("group-0"),
                   {{QString(257, QLatin1Char('k')),
                       QStringLiteral("group-0"), 0, 1}})
            << false;

        QTest::newRow("envelope-1-mib")
            << zzLayoutAtEncodedSize(zzMaximumLayoutSize) << true;
        QTest::newRow("envelope-over-1-mib")
            << zzLayoutAtEncodedSize(zzMaximumLayoutSize + 1) << false;
        QByteArray truncated = zzVersionTwoLayout(zzValidVersionTwoLayout());
        truncated.chop(1);
        QTest::newRow("truncated") << truncated << false;

        auto duplicateId = zzValidVersionTwoLayout();
        duplicateId.sideEntries = {
            {QStringLiteral("duplicate"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("duplicate"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 1}};
        QTest::newRow("duplicate-id")
            << zzVersionTwoLayout(duplicateId) << false;
        auto duplicateOrder = zzValidVersionTwoLayout();
        duplicateOrder.sideEntries = {
            {QStringLiteral("first"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0},
            {QStringLiteral("second"),
                ZzFluentUI::ZzActivityArea::LeftPrimary, 0}};
        QTest::newRow("duplicate-order")
            << zzVersionTwoLayout(duplicateOrder) << false;
        ZzTestSplitNode duplicateGroup;
        duplicateGroup.leaf = false;
        duplicateGroup.orientation = Qt::Horizontal;
        duplicateGroup.children = {
            zzLeaf(QStringLiteral("same")), zzLeaf(QStringLiteral("same"))};
        duplicateGroup.sizes = {1, 1};
        QTest::newRow("split-duplicate-group")
            << zzLayoutWithSplit(duplicateGroup, QStringLiteral("same"))
            << false;
        QTest::newRow("split-duplicate-key")
            << zzLayoutWithSplit(zzLeaf(QStringLiteral("group-0")),
                   QStringLiteral("group-0"),
                   {{QStringLiteral("same"), QStringLiteral("group-0"), 0, 1},
                       {QStringLiteral("same"), QStringLiteral("group-0"), 1, 0}})
            << false;
        QTest::newRow("split-duplicate-order")
            << zzLayoutWithSplit(zzLeaf(QStringLiteral("group-0")),
                   QStringLiteral("group-0"),
                   {{QStringLiteral("first"), QStringLiteral("group-0"), 0, 1},
                       {QStringLiteral("second"), QStringLiteral("group-0"), 0, 0}})
            << false;
        ZzTestSplitNode invalidOrientation;
        invalidOrientation.leaf = false;
        invalidOrientation.encodedOrientation = 7;
        invalidOrientation.children = {
            zzLeaf(QStringLiteral("first")), zzLeaf(QStringLiteral("second"))};
        invalidOrientation.sizes = {1, 1};
        QTest::newRow("split-invalid-orientation")
            << zzLayoutWithSplit(invalidOrientation, QStringLiteral("first"))
            << false;
        auto invalidEnum = zzValidVersionTwoLayout();
        invalidEnum.titleMode = 255;
        QTest::newRow("invalid-enum")
            << zzVersionTwoLayout(invalidEnum) << false;
        QTest::newRow("digest-mutation")
            << zzMutateDigest(zzVersionTwoLayout(zzValidVersionTwoLayout()))
            << false;
    }

    void boundsAllCountsBeforeAllocation()
    {
        QFETCH(QByteArray, encoded);
        QFETCH(bool, accepted);
        QCOMPARE(static_cast<bool>(ZzCodec::decode(encoded)), accepted);
    }
};

QTEST_MAIN(ZzWorkspaceLayoutCodecPrivateTest)

#include "ZzWorkspaceLayoutCodecPrivateTest.moc"
