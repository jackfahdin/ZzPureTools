#include "../widgets/src/private/ZzWorkspaceTopologyCodecPrivate.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtGui/QImage>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzWorkspacePageId.h>

namespace {

using ZzCodec = ZzPureTools::ZzWorkspaceTopologyCodecPrivate;
using ZzState = ZzPureTools::ZzWorkspaceTopologyStatePrivate;
using ZzWindowClosePolicy = ZzPureTools::ZzWindowClosePolicy;
using ZzWorkspaceTitleMode = ZzPureTools::ZzWorkspaceTitleMode;

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

/** @brief 从已编码拓扑的固定头部读取大端载荷长度。 */
[[nodiscard]] quint32 payloadLength(const QByteArray &encoded)
{
    Q_ASSERT(encoded.size() >= 12);
    QDataStream stream(encoded);
    const qint64 skipped = stream.skipRawData(8);
    Q_ASSERT(skipped == 8);
    Q_UNUSED(skipped);
    quint32 length = 0;
    stream >> length;
    Q_ASSERT(stream.status() == QDataStream::Ok);
    return length;
}

[[nodiscard]] QByteArray withPayloadByteChanged(
    const QByteArray &encoded,
    qsizetype payloadOffset,
    char value)
{
    QByteArray result = encoded;
    result[payloadOffset] = value;
    const quint32 encodedPayloadLength = payloadLength(result);
    const QByteArray payload = result.mid(12, encodedPayloadLength);
    const QByteArray digest = QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256);
    std::copy(digest.cbegin(), digest.cend(),
        result.begin() + 12 + encodedPayloadLength);
    return result;
}

struct IconEntryOffsets final
{
    qsizetype uuid = -1;
    qsizetype size = -1;
    qsizetype data = -1;
    quint32 byteCount = 0;
};

[[nodiscard]] QList<IconEntryOffsets> iconEntryOffsets(
    const QByteArray &encoded)
{
    const quint32 length = payloadLength(encoded);
    const QByteArray payload = encoded.mid(12, static_cast<qsizetype>(length));
    const qsizetype marker = payload.lastIndexOf(QByteArrayLiteral("ZZIC"));
    if (marker < 0) return {};
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    if (!stream.device()->seek(marker + 4)) return {};
    quint16 count = 0;
    stream >> count;
    if (stream.status() != QDataStream::Ok) return {};
    QList<IconEntryOffsets> result;
    result.reserve(count);
    for (quint16 index = 0; index < count; ++index) {
        IconEntryOffsets offsets;
        offsets.uuid = static_cast<qsizetype>(stream.device()->pos());
        if (stream.skipRawData(16) != 16) return {};
        offsets.size = static_cast<qsizetype>(stream.device()->pos());
        stream >> offsets.byteCount;
        offsets.data = static_cast<qsizetype>(stream.device()->pos());
        if (stream.status() != QDataStream::Ok
            || stream.skipRawData(static_cast<int>(offsets.byteCount))
                != static_cast<int>(offsets.byteCount)) {
            return {};
        }
        result.append(offsets);
    }
    return stream.status() == QDataStream::Ok && stream.atEnd()
        ? result
        : QList<IconEntryOffsets> {};
}

void refreshPayload(QByteArray *encoded, const QByteArray &payload)
{
    Q_ASSERT(encoded != nullptr && encoded->size() >= 44);
    Q_ASSERT(payload.size() <= std::numeric_limits<quint32>::max());
    QByteArray result = encoded->left(12);
    const quint32 length = static_cast<quint32>(payload.size());
    result[8] = static_cast<char>((length >> 24) & 0xff);
    result[9] = static_cast<char>((length >> 16) & 0xff);
    result[10] = static_cast<char>((length >> 8) & 0xff);
    result[11] = static_cast<char>(length & 0xff);
    result.append(payload);
    result.append(QCryptographicHash::hash(payload, QCryptographicHash::Sha256));
    *encoded = std::move(result);
}

void patchPayloadUInt16(QByteArray *encoded, qsizetype offset, quint16 value)
{
    Q_ASSERT(encoded != nullptr);
    QByteArray payload = encoded->mid(12, static_cast<qsizetype>(payloadLength(*encoded)));
    payload[offset] = static_cast<char>((value >> 8) & 0xff);
    payload[offset + 1] = static_cast<char>(value & 0xff);
    refreshPayload(encoded, payload);
}

void patchPayloadUInt32(QByteArray *encoded, qsizetype offset, quint32 value)
{
    Q_ASSERT(encoded != nullptr);
    QByteArray payload = encoded->mid(12, static_cast<qsizetype>(payloadLength(*encoded)));
    payload[offset] = static_cast<char>((value >> 24) & 0xff);
    payload[offset + 1] = static_cast<char>((value >> 16) & 0xff);
    payload[offset + 2] = static_cast<char>((value >> 8) & 0xff);
    payload[offset + 3] = static_cast<char>(value & 0xff);
    refreshPayload(encoded, payload);
}

[[nodiscard]] qsizetype firstOriginCountOffset(
    const QByteArray &encoded)
{
    const QByteArray payload = encoded.mid(12, encoded.size() - 12 - 32);
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    quint16 windowCount = 0;
    stream >> windowCount;
    for (quint16 windowIndex = 0; windowIndex < windowCount; ++windowIndex) {
        char uuid[16]{};
        stream.readRawData(uuid, 16);
        auto skipString = [&stream] {
            quint16 length = 0;
            stream >> length;
            stream.skipRawData(static_cast<qint64>(length) * 2);
        };
        skipString();
        skipString();
        stream.skipRawData(6 + 13 * 4);
        quint32 stateLength = 0;
        stream >> stateLength;
        stream.skipRawData(static_cast<qint64>(stateLength));
        quint16 pageCount = 0;
        stream >> pageCount;
        for (quint16 pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
            stream.readRawData(uuid, 16);
            skipString();
            stream.readRawData(uuid, 16);
            skipString();
            stream.skipRawData(4);
            const qint64 originCountOffset = stream.device()->pos();
            quint16 originCount = 0;
            stream >> originCount;
            for (quint16 originIndex = 0; originIndex < originCount;
                 ++originIndex) {
                stream.readRawData(uuid, 16);
                skipString();
                stream.skipRawData(4);
            }
            if (originCount > 0) {
                return 12 + originCountOffset;
            }
        }
    }
    return -1;
}

void refreshDigest(QByteArray *encoded)
{
    const quint32 encodedPayloadLength = payloadLength(*encoded);
    const QByteArray payload = encoded->mid(12, encodedPayloadLength);
    const QByteArray digest = QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256);
    std::copy(digest.cbegin(), digest.cend(),
        encoded->begin() + 12 + encodedPayloadLength);
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

    void preservesWindowIconInRoundTrip()
    {
        ZzState source = topology();
        QImage image(3, 3, QImage::Format_ARGB32);
        image.fill(QColor(220, 40, 80, 255));
        image.setPixelColor(1, 1, QColor(30, 160, 240, 255));
        source.windows[0].configuration.icon = QIcon(QPixmap::fromImage(image));
        QImage secondImage(5, 5, QImage::Format_ARGB32);
        secondImage.fill(QColor(50, 180, 90, 255));
        source.windows[1].configuration.icon = QIcon(QPixmap::fromImage(secondImage));
        source.windows[0].iconImage = source.windows[0].configuration.icon
            .pixmap(QSize(32, 32)).toImage()
            .convertToFormat(QImage::Format_ARGB32)
            .scaled(QSize(32, 32), Qt::IgnoreAspectRatio,
                Qt::SmoothTransformation);
        source.windows[1].iconImage = source.windows[1].configuration.icon
            .pixmap(QSize(32, 32)).toImage()
            .convertToFormat(QImage::Format_ARGB32)
            .scaled(QSize(32, 32), Qt::IgnoreAspectRatio,
                Qt::SmoothTransformation);

        const auto encoded = ZzCodec::encode(source);
        QVERIFY(encoded);
        const auto decoded = ZzCodec::decode(encoded.value());
        QVERIFY(decoded);
        QVERIFY(!decoded.value().windows[0].iconImage.isNull());
        const QImage actual = decoded.value().windows[0].iconImage;
        const QImage expected = source.windows[0].iconImage;
        QCOMPARE(actual, expected);
        QCOMPARE(decoded.value(), source);
    }

    void omitsIconExtensionWithoutIcons()
    {
        const auto encoded = ZzCodec::encode(topology());
        QVERIFY(encoded);
        const quint32 length = payloadLength(encoded.value());
        const QByteArray payload = encoded.value().mid(
            12, static_cast<qsizetype>(length));
        QVERIFY(!payload.contains(QByteArrayLiteral("ZZIC")));
        const auto decoded = ZzCodec::decode(encoded.value());
        QVERIFY(decoded);
        for (const auto &window : decoded.value().windows) {
            QVERIFY(window.configuration.icon.isNull());
        }
    }

    void rejectsMalformedWindowIconExtension()
    {
        ZzState source = topology();
        QImage image(3, 3, QImage::Format_ARGB32);
        image.fill(QColor(220, 40, 80, 255));
        source.windows[0].configuration.icon = QIcon(QPixmap::fromImage(image));
        source.windows[1].configuration.icon = QIcon(QPixmap::fromImage(image));
        const QImage normalized = image.scaled(
            QSize(32, 32), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                   .convertToFormat(QImage::Format_ARGB32);
        source.windows[0].iconImage = normalized;
        source.windows[1].iconImage = normalized;
        const auto encoded = ZzCodec::encode(source);
        QVERIFY(encoded);
        const auto offsets = iconEntryOffsets(encoded.value());
        QCOMPARE(offsets.size(), 2);
        const QByteArray payload = encoded.value().mid(
            12, static_cast<qsizetype>(payloadLength(encoded.value())));

        QByteArray unknown = encoded.value();
        QByteArray unknownPayload = payload;
        const QByteArray unknownId = QUuid::createUuid().toRfc4122();
        std::copy(unknownId.cbegin(), unknownId.cend(),
            unknownPayload.begin() + offsets.front().uuid);
        refreshPayload(&unknown, unknownPayload);
        QVERIFY(!ZzCodec::decode(unknown));

        QByteArray duplicate = encoded.value();
        QByteArray duplicatePayload = payload;
        std::copy(duplicatePayload.cbegin() + offsets.front().uuid,
            duplicatePayload.cbegin() + offsets.front().uuid + 16,
            duplicatePayload.begin() + offsets.back().uuid);
        refreshPayload(&duplicate, duplicatePayload);
        QVERIFY(!ZzCodec::decode(duplicate));

        QByteArray invalidPng = encoded.value();
        QByteArray invalidPayload = payload;
        invalidPayload[offsets.front().data] = 'X';
        refreshPayload(&invalidPng, invalidPayload);
        QVERIFY(!ZzCodec::decode(invalidPng));

        QByteArray oversized = encoded.value();
        patchPayloadUInt32(&oversized, offsets.front().size, 256U * 1024U + 1U);
        QVERIFY(!ZzCodec::decode(oversized));

        const qsizetype marker = payload.lastIndexOf(QByteArrayLiteral("ZZIC"));
        QVERIFY(marker >= 0);
        QByteArray emptyCount = encoded.value();
        patchPayloadUInt16(&emptyCount, marker + 4, 0);
        QVERIFY(!ZzCodec::decode(emptyCount));
    }

    void rejectsEnvelopeTampering()
    {
        const ZzState source = topology();
        const auto encoded = ZzCodec::encode(source);
        QVERIFY(encoded);
        const QByteArray &value = encoded.value();
        const qsizetype widgetsBefore = QApplication::allWidgets().size();

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
            qsizetype{12} + 2 + 16
            + 2 + source.windows[0].configuration.title.size() * qsizetype{2}
            + 2 + source.windows[0].screenName.size() * qsizetype{2}
            + 6 + qsizetype{13} * 4 + 4
            + source.windows[0].workspaceState.size();
        QByteArray badPageCount = value;
        badPageCount[firstPageCount] = 0x10;
        badPageCount[firstPageCount + 1] = 0x01;
        refreshDigest(&badPageCount);
        QVERIFY(!ZzCodec::decode(badPageCount));

        const qsizetype originCountOffset = firstOriginCountOffset(value);
        QVERIFY(originCountOffset >= 0);
        QByteArray badOriginCount = value;
        badOriginCount[originCountOffset] = 0;
        badOriginCount[originCountOffset + 1] = 33;
        refreshDigest(&badOriginCount);
        QVERIFY(!ZzCodec::decode(badOriginCount));

        QByteArray badDigest = value;
        badDigest.back() ^= 0x01;
        QVERIFY(!ZzCodec::decode(badDigest));

        QVERIFY(!ZzCodec::decode(value.left(value.size() - 1)));
        QCOMPARE(QApplication::allWidgets().size(), widgetsBefore);
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

        ZzState oversizedTitle = topology();
        oversizedTitle.windows[0].configuration.title =
            QString(257, QLatin1Char('t'));
        QVERIFY(!ZzCodec::encode(oversizedTitle));

        ZzState invalidClosePolicy = topology();
        invalidClosePolicy.windows[0].configuration.closePolicy =
            static_cast<ZzWindowClosePolicy>(255);
        QVERIFY(!ZzCodec::encode(invalidClosePolicy));

        ZzState invalidTitleMode = topology();
        invalidTitleMode.windows[0].configuration.titleMode =
            static_cast<ZzWorkspaceTitleMode>(255);
        QVERIFY(!ZzCodec::encode(invalidTitleMode));

        ZzState mixedZeroSize = topology();
        mixedZeroSize.windows[0].configuration.minimumSize = QSize(0, 240);
        mixedZeroSize.windows[0].configuration.maximumSize = QSize(1920, 480);
        mixedZeroSize.windows[0].configuration.initialGeometry =
            QRect(20, 30, 320, 240);
        QVERIFY(ZzCodec::encode(mixedZeroSize));

        ZzState negativeSentinel = topology();
        negativeSentinel.windows[0].configuration.minimumSize = QSize(-2, -2);
        QVERIFY(!ZzCodec::encode(negativeSentinel));

        ZzState negativeInitialGeometry = topology();
        negativeInitialGeometry.windows[0].configuration.initialGeometry =
            QRect(20, 30, -1, -1);
        QVERIFY(!ZzCodec::encode(negativeInitialGeometry));

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
