#include "ZzWorkspaceTopologyCodecPrivate.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QSet>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzPureTools {
namespace {

using ZzState = ZzWorkspaceTopologyStatePrivate;

constexpr quint16 zzSchema = 2;
constexpr quint16 zzStreamVersion = static_cast<quint16>(QDataStream::Qt_6_8);
constexpr qsizetype zzHeaderSize = 12;
constexpr qsizetype zzDigestSize = 32;

template<typename T>
[[nodiscard]] ZzCore::ZzResult<T> failure(
    ZzCore::ZzErrorCode code, QString message)
{
    return ZzCore::ZzResult<T>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

[[nodiscard]] bool readString(
    QDataStream &stream, QString *value, bool allowEmpty = true)
{
    quint16 length = 0;
    stream >> length;
    if (stream.status() != QDataStream::Ok
        || length > ZzState::MaximumStringLength) {
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
    return allowEmpty || !value->isEmpty();
}

void writeString(QDataStream &stream, const QString &value)
{
    stream << static_cast<quint16>(value.size());
    for (const QChar character : value) {
        stream << character.unicode();
    }
}

[[nodiscard]] bool readUuid(QDataStream &stream, QUuid *value)
{
    std::array<char, 16> bytes{};
    if (stream.readRawData(bytes.data(), bytes.size()) != bytes.size()) {
        return false;
    }
    *value = QUuid::fromRfc4122(QByteArray(bytes.data(), bytes.size()));
    return !value->isNull();
}

void writeUuid(QDataStream &stream, const QUuid &value)
{
    const QByteArray bytes = value.toRfc4122();
    stream.writeRawData(bytes.constData(), bytes.size());
}

[[nodiscard]] bool readByteArray(
    QDataStream &stream, QByteArray *value, int maximum)
{
    quint32 size = 0;
    stream >> size;
    if (stream.status() != QDataStream::Ok || size > quint32(maximum)) {
        return false;
    }
    value->resize(static_cast<qsizetype>(size));
    return size == 0
        || stream.readRawData(value->data(), static_cast<int>(size))
            == static_cast<int>(size);
}

void writeByteArray(QDataStream &stream, const QByteArray &value)
{
    stream << static_cast<quint32>(value.size());
    if (!value.isEmpty()) {
        stream.writeRawData(value.constData(), value.size());
    }
}

[[nodiscard]] bool readPage(QDataStream &stream, ZzState::ZzPageState *page)
{
    QUuid rawPageId;
    if (!readUuid(stream, &rawPageId)
        || !readString(stream, &page->layoutKey)
        || !readUuid(stream, &page->windowId)
        || !readString(stream, &page->groupId, false)) {
        return false;
    }
    qint32 index = -1;
    quint16 originCount = 0;
    stream >> index >> originCount;
    if (stream.status() != QDataStream::Ok
        || originCount > ZzState::MaximumOriginDepth) {
        return false;
    }
    page->pageId = ZzFluentUI::ZzWorkspacePageId::fromString(
        rawPageId.toString(QUuid::WithoutBraces));
    page->index = index;
    page->origins.clear();
    page->origins.reserve(originCount);
    for (quint16 i = 0; i < originCount; ++i) {
        ZzState::ZzPageOrigin origin;
        if (!readUuid(stream, &origin.windowId)
            || !readString(stream, &origin.groupId, false)) {
            return false;
        }
        stream >> origin.index;
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
        page->origins.append(std::move(origin));
    }
    return true;
}

void writePage(QDataStream &stream, const ZzState::ZzPageState &page)
{
    writeUuid(stream, QUuid::fromString(page.pageId.toString()));
    writeString(stream, page.layoutKey);
    writeUuid(stream, page.windowId);
    writeString(stream, page.groupId);
    stream << page.index << static_cast<quint16>(page.origins.size());
    for (const ZzState::ZzPageOrigin &origin : page.origins) {
        writeUuid(stream, origin.windowId);
        writeString(stream, origin.groupId);
        stream << origin.index;
    }
}

[[nodiscard]] bool readWindow(QDataStream &stream, ZzState::ZzWindowState *window)
{
    if (!readUuid(stream, &window->windowId)
        || !readString(stream, &window->configuration.title)
        || !readString(stream, &window->screenName)) {
        return false;
    }
    quint8 closePolicy = 0;
    quint8 titleMode = 0;
    quint8 configurationAlwaysOnTop = 0;
    quint8 alwaysOnTop = 0;
    quint8 visible = 0;
    quint8 maximized = 0;
    qint32 minimumWidth = 0;
    qint32 minimumHeight = 0;
    qint32 maximumWidth = 0;
    qint32 maximumHeight = 0;
    qint32 initialX = 0;
    qint32 initialY = 0;
    qint32 initialWidth = 0;
    qint32 initialHeight = 0;
    qint32 geometryX = 0;
    qint32 geometryY = 0;
    qint32 geometryWidth = 0;
    qint32 geometryHeight = 0;
    qint32 treeDepth = 0;
    quint16 pageCount = 0;
    stream >> closePolicy >> titleMode >> configurationAlwaysOnTop
           >> alwaysOnTop >> visible >> maximized
           >> minimumWidth >> minimumHeight >> maximumWidth >> maximumHeight
           >> initialX >> initialY >> initialWidth >> initialHeight
           >> geometryX >> geometryY >> geometryWidth >> geometryHeight
           >> treeDepth;
    if (stream.status() != QDataStream::Ok
        || closePolicy > static_cast<quint8>(ZzWindowClosePolicy::Delegate)
        || titleMode > static_cast<quint8>(ZzWorkspaceTitleMode::Custom)
        || configurationAlwaysOnTop > 1 || alwaysOnTop > 1 || visible > 1
        || maximized > 1) {
        return false;
    }
    window->configuration.closePolicy =
        static_cast<ZzWindowClosePolicy>(closePolicy);
    window->configuration.titleMode =
        static_cast<ZzWorkspaceTitleMode>(titleMode);
    window->configuration.alwaysOnTop = configurationAlwaysOnTop != 0;
    window->configuration.minimumSize = QSize(minimumWidth, minimumHeight);
    window->configuration.maximumSize = QSize(maximumWidth, maximumHeight);
    window->configuration.initialGeometry = QRect(
        initialX, initialY, initialWidth, initialHeight);
    window->geometry = QRect(
        geometryX, geometryY, geometryWidth, geometryHeight);
    window->treeDepth = treeDepth;
    window->visible = visible != 0;
    window->maximized = maximized != 0;
    window->alwaysOnTop = alwaysOnTop != 0;
    if (!readByteArray(stream, &window->workspaceState,
            ZzState::MaximumWorkspaceStateSize)) {
        return false;
    }
    stream >> pageCount;
    if (stream.status() != QDataStream::Ok || pageCount > ZzState::MaximumPages) {
        return false;
    }
    window->pages.clear();
    window->pages.reserve(pageCount);
    for (quint16 i = 0; i < pageCount; ++i) {
        ZzState::ZzPageState page;
        if (!readPage(stream, &page)) {
            return false;
        }
        window->pages.append(std::move(page));
    }
    return true;
}

void writeWindow(QDataStream &stream, const ZzState::ZzWindowState &window)
{
    writeUuid(stream, window.windowId);
    writeString(stream, window.configuration.title);
    writeString(stream, window.screenName);
    stream << static_cast<quint8>(window.configuration.closePolicy)
           << static_cast<quint8>(window.configuration.titleMode)
           << static_cast<quint8>(window.configuration.alwaysOnTop ? 1 : 0)
           << static_cast<quint8>(window.alwaysOnTop ? 1 : 0)
           << static_cast<quint8>(window.visible ? 1 : 0)
           << static_cast<quint8>(window.maximized ? 1 : 0)
           << static_cast<qint32>(window.configuration.minimumSize.width())
           << static_cast<qint32>(window.configuration.minimumSize.height())
           << static_cast<qint32>(window.configuration.maximumSize.width())
           << static_cast<qint32>(window.configuration.maximumSize.height())
           << static_cast<qint32>(window.configuration.initialGeometry.x())
           << static_cast<qint32>(window.configuration.initialGeometry.y())
           << static_cast<qint32>(window.configuration.initialGeometry.width())
           << static_cast<qint32>(window.configuration.initialGeometry.height())
           << static_cast<qint32>(window.geometry.x())
           << static_cast<qint32>(window.geometry.y())
           << static_cast<qint32>(window.geometry.width())
           << static_cast<qint32>(window.geometry.height())
           << static_cast<qint32>(window.treeDepth);
    writeByteArray(stream, window.workspaceState);
    stream << static_cast<quint16>(window.pages.size());
    for (const ZzState::ZzPageState &page : window.pages) {
        writePage(stream, page);
    }
}

[[nodiscard]] bool decodeEnvelope(
    const QByteArray &encoded, QByteArray *payload)
{
    if (encoded.size() < zzHeaderSize + zzDigestSize
        || encoded.size() > zzHeaderSize + ZzState::MaximumPayloadSize
            + zzDigestSize) {
        return false;
    }
    QDataStream stream(encoded);
    stream.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schema = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (stream.readRawData(magic, 4) != 4) {
        return false;
    }
    stream >> schema >> streamVersion >> payloadLength;
    const qint64 expected = zzHeaderSize + payloadLength + zzDigestSize;
    if (stream.status() != QDataStream::Ok
        || QByteArrayView(magic, 4) != QByteArrayView("ZZWT", 4)
        || schema != zzSchema || streamVersion != zzStreamVersion
        || payloadLength > ZzState::MaximumPayloadSize
        || expected != encoded.size()) {
        return false;
    }
    payload->resize(static_cast<qsizetype>(payloadLength));
    if (payloadLength > 0
        && stream.readRawData(payload->data(), payloadLength)
            != static_cast<qint64>(payloadLength)) {
        return false;
    }
    QByteArray digest(zzDigestSize, Qt::Uninitialized);
    if (stream.readRawData(digest.data(), zzDigestSize) != zzDigestSize
        || stream.status() != QDataStream::Ok || !stream.atEnd()) {
        return false;
    }
    return digest == QCryptographicHash::hash(
        *payload, QCryptographicHash::Sha256);
}

} // namespace

ZzCore::ZzResult<QByteArray> ZzWorkspaceTopologyCodecPrivate::encode(
    const ZzWorkspaceTopologyStatePrivate &state)
{
    if (!state.isValid()) {
        return failure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("ZZWT topology state is invalid"));
    }
    QByteArray payload;
    QDataStream payloadStream(&payload, QIODevice::WriteOnly);
    payloadStream.setVersion(QDataStream::Qt_6_8);
    payloadStream << static_cast<quint16>(state.windows.size());
    for (const ZzState::ZzWindowState &window : state.windows) {
        writeWindow(payloadStream, window);
    }
    if (payloadStream.status() != QDataStream::Ok
        || payload.size() > ZzState::MaximumPayloadSize) {
        return failure<QByteArray>(ZzCore::ZzErrorCode::Io,
            QStringLiteral("ZZWT payload serialization failed"));
    }

    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    stream.writeRawData("ZZWT", 4);
    stream << zzSchema << zzStreamVersion
           << static_cast<quint32>(payload.size());
    stream.writeRawData(payload.constData(), payload.size());
    encoded.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    if (stream.status() != QDataStream::Ok) {
        return failure<QByteArray>(ZzCore::ZzErrorCode::Io,
            QStringLiteral("ZZWT envelope serialization failed"));
    }
    return ZzCore::ZzResult<QByteArray>::success(std::move(encoded));
}

ZzCore::ZzResult<ZzWorkspaceTopologyStatePrivate>
ZzWorkspaceTopologyCodecPrivate::decode(const QByteArray &encoded)
{
    if (encoded.size() >= 4
        && QByteArrayView(encoded.constData(), 4)
            == QByteArrayView("ZZSW", 4)) {
        if (encoded.size() > ZzState::MaximumWorkspaceStateSize) {
            return failure<ZzState>(ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("ZZSW workspace state exceeds 1 MiB"));
        }
        ZzState state;
        ZzState::ZzWindowState window;
        window.workspaceState = encoded;
        state.windows.append(std::move(window));
        return ZzCore::ZzResult<ZzState>::success(std::move(state));
    }

    QByteArray payload;
    if (!decodeEnvelope(encoded, &payload)) {
        return failure<ZzState>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("ZZWT envelope is invalid"));
    }
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    quint16 windowCount = 0;
    stream >> windowCount;
    if (stream.status() != QDataStream::Ok || windowCount == 0
        || windowCount > ZzState::MaximumWindows) {
        return failure<ZzState>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("ZZWT window count is invalid"));
    }
    ZzState state;
    state.windows.reserve(windowCount);
    for (quint16 i = 0; i < windowCount; ++i) {
        ZzState::ZzWindowState window;
        if (!readWindow(stream, &window)) {
            return failure<ZzState>(ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("ZZWT window payload is invalid"));
        }
        state.windows.append(std::move(window));
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || !state.isValid()) {
        return failure<ZzState>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("ZZWT topology references are invalid"));
    }
    return ZzCore::ZzResult<ZzState>::success(std::move(state));
}

} // namespace ZzPureTools
