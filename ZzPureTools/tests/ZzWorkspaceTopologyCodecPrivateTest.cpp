#include "../widgets/src/private/ZzWorkspaceTopologyCodecPrivate.h"

#include <algorithm>
#include <array>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtWidgets/QApplication>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzWorkspacePageId.h>

namespace {

using ZzCodec = ZzPureTools::ZzWorkspaceTopologyCodecPrivate;
using ZzState = ZzPureTools::ZzWorkspaceTopologyStatePrivate;

[[nodiscard]] ZzState::ZzPageState page(
    const QUuid &windowId,
    const QString &group,
    int index,
    QString key)
{
    ZzState::ZzPageState value;
    value.pageId = ZzFluentUI::ZzWorkspacePageId::create();
    value.layoutKey = std::move(key);
    value.windowId = windowId;
    value.groupId = group;
    value.index = index;
    return value;
}

[[nodiscard]] ZzState::ZzWindowState window(
    const QUuid &id,
    const QString &group,
    int pageCount,
    int windowIndex)
{
    ZzState::ZzWindowState value;
    value.windowId = id;
    value.configuration.title =
        QStringLiteral("Window %1").arg(windowIndex);
    value.configuration.alwaysOnTop = windowIndex % 2 == 0;
    value.geometry = QRect(80 + windowIndex * 25, 100, 900, 640);
    value.screenName = QStringLiteral("screen-%1").arg(windowIndex);
    value.visible = true;
    value.maximized = windowIndex == 1;
    value.alwaysOnTop = value.configuration.alwaysOnTop;
    value.workspaceState = QByteArrayLiteral("ZZSW\0workspace-layout");
    for (int index = 0; index < pageCount; ++index) {
        value.pages.append(page(
            id, group, index,
            QStringLiteral("document/%1/%2").arg(windowIndex).arg(index)));
    }
    return value;
}

[[nodiscard]] ZzState topology()
{
    ZzState value;
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();
    value.windows.append(window(first, QStringLiteral("left-a"), 3, 0));
    value.windows.append(window(second, QStringLiteral("right-a"), 2, 1));
    value.windows[0].pages[1].groupId = QStringLiteral("left-b");
    value.windows[0].pages[2].groupId = QStringLiteral("left-c");
    value.windows[1].pages[1].groupId = QStringLiteral("right-b");
    value.windows[1].pages[0].origins.append(
        {first, QStringLiteral("left"), 2});
    return value;
}

[[nodiscard]] QByteArray withPayloadByteChanged(
    const QByteArray &encoded,
    qsizetype payloadOffset,
    char value)
{
    QByteArray result = encoded;
    result[payloadOffset] = value;
    const quint32 payloadLength =
        (static_cast<quint8>(result.at(8)) << 24)
        | (static_cast<quint8>(result.at(9)) << 16)
        | (static_cast<quint8>(result.at(10)) << 8)
        | static_cast<quint8>(result.at(11));
    const QByteArray payload = result.mid(12, payloadLength);
    const QByteArray digest = QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256);
    std::copy(digest.cbegin(), digest.cend(), result.begin() + 12 + payloadLength);
    return result;
}

void refreshDigest(QByteArray *encoded)
{
    const quint32 payloadLength =
        (static_cast<quint8>(encoded->at(8)) << 24)
        | (static_cast<quint8>(encoded->at(9)) << 16)
        | (static_cast<quint8>(encoded->at(10)) << 8)
        | static_cast<quint8>(encoded->at(11));
    const QByteArray payload = encoded->mid(12, payloadLength);
    const QByteArray digest = QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256);
    std::copy(digest.cbegin(), digest.cend(),
        encoded->begin() + 12 + payloadLength);
}

class ZzWorkspaceTopologyCodecPrivateTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripIsStableAndDigestMatches()
    {
        const ZzState source = topology();
        const auto encoded = ZzCodec::encode(source);
        QVERIFY(encoded);
        QCOMPARE(ZzCodec::encode(source).value(), encoded.value());

        QDataStream stream(encoded.value());
        stream.setVersion(QDataStream::Qt_6_8);
        char magic[4]{};
        quint16 schema = 0;
        quint16 streamVersion = 0;
        quint32 payloadLength = 0;
        QCOMPARE(stream.readRawData(magic, 4), 4);
        stream >> schema >> streamVersion >> payloadLength;
        QCOMPARE(QByteArray(magic, 4), QByteArrayLiteral("ZZWT"));
        QCOMPARE(schema, quint16(2));
        QCOMPARE(streamVersion, quint16(QDataStream::Qt_6_8));
        QCOMPARE(payloadLength,
            quint32(encoded.value().size() - 12 - 32));

        const auto decoded = ZzCodec::decode(encoded.value());
        QVERIFY(decoded);
        QCOMPARE(decoded.value(), source);
        QCOMPARE(QCryptographicHash::hash(
                     encoded.value().mid(12, payloadLength),
                     QCryptographicHash::Sha256),
            encoded.value().right(32));
    }

    void rejectsEnvelopeTampering()
    {
        const ZzState source = topology();
        const auto encoded = ZzCodec::encode(source);
        QVERIFY(encoded);
        const QByteArray value = encoded.value();

        QByteArray badMagic = value;
        badMagic[0] = 'X';
        QVERIFY(!ZzCodec::decode(badMagic));

        QByteArray badSchema = value;
        badSchema[4] = 0;
        badSchema[5] = 3;
        QVERIFY(!ZzCodec::decode(badSchema));

        QByteArray badStream = value;
        badStream[6] = 0;
        badStream[7] = 1;
        QVERIFY(!ZzCodec::decode(badStream));

        QByteArray badLength = value;
        badLength[11] = static_cast<char>(badLength.at(11) + 1);
        QVERIFY(!ZzCodec::decode(badLength));

        QVERIFY(!ZzCodec::decode(withPayloadByteChanged(value, 12, 33)));

        QByteArray badUuid = value;
        std::fill(badUuid.begin() + 14, badUuid.begin() + 30, char(0));
        refreshDigest(&badUuid);
        QVERIFY(!ZzCodec::decode(badUuid));

        const qsizetype firstPageCount =
            12 + 2 + 16
            + 2 + source.windows[0].configuration.title.size() * 2
            + 2 + source.windows[0].screenName.size() * 2
            + 6 + 13 * 4 + 4 + source.windows[0].workspaceState.size();
        QByteArray badPageCount = value;
        badPageCount[firstPageCount] = 0x10;
        badPageCount[firstPageCount + 1] = 0x01;
        refreshDigest(&badPageCount);
        QVERIFY(!ZzCodec::decode(badPageCount));

        QByteArray badDigest = value;
        badDigest.back() ^= 0x01;
        QVERIFY(!ZzCodec::decode(badDigest));

        QVERIFY(!ZzCodec::decode(value.left(value.size() - 1)));
    }

    void rejectsInvalidTopologyBeforeWidgets()
    {
        const auto encoded = ZzCodec::encode(topology());
        QVERIFY(encoded);
        const qsizetype widgetsBefore = QApplication::allWidgets().size();

        ZzState tooManyWindows;
        for (int index = 0; index < 33; ++index) {
            tooManyWindows.windows.append(window(
                QUuid::createUuid(), QStringLiteral("group"), 0, index));
        }
        QVERIFY(!ZzCodec::encode(tooManyWindows));

        ZzState duplicateWindows = topology();
        duplicateWindows.windows[1].windowId =
            duplicateWindows.windows[0].windowId;
        QVERIFY(!ZzCodec::encode(duplicateWindows));

        ZzState duplicatePages = topology();
        duplicatePages.windows[1].pages[0].pageId =
            duplicatePages.windows[0].pages[0].pageId;
        QVERIFY(!ZzCodec::encode(duplicatePages));

        ZzState duplicateKeys = topology();
        duplicateKeys.windows[1].pages[0].layoutKey =
            duplicateKeys.windows[0].pages[0].layoutKey;
        QVERIFY(!ZzCodec::encode(duplicateKeys));

        ZzState negativeGeometry = topology();
        negativeGeometry.windows[0].geometry.setWidth(-1);
        QVERIFY(!ZzCodec::encode(negativeGeometry));

        ZzState oversizedString = topology();
        oversizedString.windows[0].screenName = QString(257, QLatin1Char('x'));
        QVERIFY(!ZzCodec::encode(oversizedString));

        ZzState tooManyGroups = topology();
        for (int index = 0; index < 65; ++index) {
            tooManyGroups.windows[0].pages.append(page(
                tooManyGroups.windows[0].windowId,
                QStringLiteral("group-%1").arg(index), index,
                QStringLiteral("extra/%1").arg(index)));
        }
        QVERIFY(!ZzCodec::encode(tooManyGroups));

        ZzState tooDeep = topology();
        tooDeep.windows[0].treeDepth = ZzState::MaximumTreeDepth + 1;
        QVERIFY(!ZzCodec::encode(tooDeep));

        ZzState oversizedWorkspace = topology();
        oversizedWorkspace.windows[0].workspaceState = QByteArray(
            ZzState::MaximumWorkspaceStateSize + 1, 'x');
        QVERIFY(!ZzCodec::encode(oversizedWorkspace));

        ZzState truncatedOrigins = topology();
        truncatedOrigins.windows[1].pages[0].origins =
            QList<ZzState::ZzPageOrigin>(33, {
                truncatedOrigins.windows[0].windowId,
                QStringLiteral("left"), 0});
        QVERIFY(!ZzCodec::encode(truncatedOrigins));
        QCOMPARE(QApplication::allWidgets().size(), widgetsBefore);
    }

    void preservesLegacySplitBlob()
    {
        const QByteArray legacy = QByteArrayLiteral("ZZSW\0legacy-bytes");
        const auto decoded = ZzCodec::decode(legacy);
        QVERIFY(decoded);
        QCOMPARE(decoded.value().windows.size(), 1);
        QCOMPARE(decoded.value().windows.front().workspaceState, legacy);
        QVERIFY(decoded.value().windows.front().windowId.isNull());
    }
};

} // namespace

QTEST_MAIN(ZzWorkspaceTopologyCodecPrivateTest)
#include "ZzWorkspaceTopologyCodecPrivateTest.moc"
