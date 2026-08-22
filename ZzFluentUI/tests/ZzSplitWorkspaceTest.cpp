#include <QtCore/QAbstractAnimation>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QDrag>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QMouseEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSplitter>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace {

bool zzFocusBelongsTo(
    QWidget *focused,
    const ZzFluentUI::ZzTabWidget *tabs)
{
    return focused != nullptr && tabs != nullptr
        && (focused == tabs || tabs->isAncestorOf(focused));
}

QList<ZzFluentUI::ZzTabGroupId> zzCreateFourHorizontalGroups(
    ZzFluentUI::ZzSplitWorkspace &workspace)
{
    const auto root = workspace.groupIds().constFirst();
    for (int addedCount = 0; addedCount < 3; ++addedCount) {
        if (!workspace.splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value()) {
            return {};
        }
    }
    return workspace.groupIds();
}

void zzSetGroupCenter(
    ZzFluentUI::ZzSplitWorkspace &workspace,
    const ZzFluentUI::ZzTabGroupId &id,
    const QPoint &center)
{
    workspace.tabWidget(id)->setGeometry(
        center.x() - 20, center.y() - 20, 40, 40);
}

QList<QList<int>> zzSplitterSizes(const ZzFluentUI::ZzSplitWorkspace &workspace)
{
    QList<QList<int>> result;
    const auto splitters = workspace.findChildren<QSplitter *>();
    result.reserve(splitters.size());
    for (const QSplitter *splitter : splitters) {
        result.push_back(splitter->sizes());
    }
    return result;
}

struct ZzTestLayoutNode final
{
    explicit ZzTestLayoutNode(const QString &leafId)
        : id(leafId)
    {
    }

    ZzTestLayoutNode(
        quint8 branchOrientation,
        const QList<int> &branchSizes,
        const QList<ZzTestLayoutNode> &branchChildren)
        : leaf(false)
        , orientation(branchOrientation)
        , sizes(branchSizes)
        , children(branchChildren)
    {
    }

    bool leaf = true;
    QString id;
    quint8 orientation = static_cast<quint8>(Qt::Horizontal);
    QList<int> sizes;
    QList<ZzTestLayoutNode> children;
};

struct ZzTestLayoutPage final
{
    QString key;
    QString groupId;
    qint32 order = 0;
    bool current = false;
};

void zzWriteLayoutString(QDataStream &stream, const QString &value)
{
    stream << static_cast<quint16>(value.size());
    for (const QChar character : value) {
        stream << character.unicode();
    }
}

void zzWriteLayoutNode(QDataStream &stream, const ZzTestLayoutNode &node)
{
    stream << static_cast<quint8>(node.leaf ? 0 : 1);
    if (node.leaf) {
        zzWriteLayoutString(stream, node.id);
        return;
    }

    stream << node.orientation
           << static_cast<quint16>(node.children.size());
    for (const auto &child : node.children) {
        zzWriteLayoutNode(stream, child);
    }
    stream << static_cast<quint16>(node.sizes.size());
    for (const int size : node.sizes) {
        stream << static_cast<qint32>(size);
    }
}

QByteArray zzWrapLayoutPayload(
    const QByteArray &payload,
    quint16 schemaVersion = 1,
    quint16 streamVersion = QDataStream::Qt_6_8)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    stream.writeRawData("ZZSW", 4);
    stream << schemaVersion << streamVersion
           << static_cast<quint32>(payload.size());
    stream.writeRawData(payload.constData(), payload.size());
    result.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return result;
}

QByteArray zzBuildTestLayout(
    const ZzTestLayoutNode &root,
    const QString &activeId,
    const QList<ZzTestLayoutPage> &pages = {})
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    zzWriteLayoutNode(stream, root);
    zzWriteLayoutString(stream, activeId);
    stream << static_cast<quint16>(pages.size());
    for (const auto &page : pages) {
        zzWriteLayoutString(stream, page.key);
        zzWriteLayoutString(stream, page.groupId);
        stream << page.order << static_cast<quint8>(page.current ? 1 : 0);
    }
    return zzWrapLayoutPayload(payload);
}

} // namespace

class ZzSplitWorkspaceTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizesAndBoundsUniquePageLayoutKeys()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        auto *firstPage = new QWidget;
        auto *secondPage = new QWidget;
        workspace.tabWidget(root)->addTab(
            firstPage, QStringLiteral("First"));
        workspace.tabWidget(root)->addTab(
            secondPage, QStringLiteral("Second"));

        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("  terminal:a  ")));
        QCOMPARE(
            workspace.pageLayoutKey(firstPage),
            QStringLiteral("terminal:a"));
        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral("editor:b")));
        QVERIFY(!workspace.setPageLayoutKey(
            secondPage, QStringLiteral(" terminal:a ")));
        QCOMPARE(
            workspace.pageLayoutKey(secondPage),
            QStringLiteral("editor:b"));

        const QString maximumKey(256, u'k');
        QVERIFY(workspace.setPageLayoutKey(secondPage, maximumKey));
        QCOMPARE(workspace.pageLayoutKey(secondPage), maximumKey);
        QVERIFY(!workspace.setPageLayoutKey(
            secondPage, QString(257, u'x')));
        QCOMPARE(workspace.pageLayoutKey(secondPage), maximumKey);

        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral(" \t\n ")));
        QVERIFY(workspace.pageLayoutKey(secondPage).isEmpty());
        QWidget outsider;
        QVERIFY(!workspace.setPageLayoutKey(&outsider, maximumKey));
    }

    void savesAndRestoresKeyedPagesTransactionally()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId targetId(
            QStringLiteral("target-group"));
        QVERIFY(workspace.splitGroup(
            sourceId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            targetId).has_value());
        workspace.resize(900, 500);
        workspace.show();
        QCoreApplication::processEvents();
        const auto splitters = workspace.findChildren<QSplitter *>();
        QCOMPARE(splitters.size(), 1);
        splitters.constFirst()->setSizes({271, 629});

        auto *firstPage = new QWidget;
        auto *missingPage = new QWidget;
        auto *unkeyedPage = new QWidget;
        auto *targetPage = new QWidget;
        workspace.tabWidget(sourceId)->addTab(
            firstPage, QStringLiteral("Terminal"));
        workspace.tabWidget(sourceId)->addTab(
            missingPage, QStringLiteral("Missing"));
        workspace.tabWidget(sourceId)->addTab(
            unkeyedPage, QStringLiteral("Unkeyed"));
        workspace.tabWidget(targetId)->addTab(
            targetPage, QStringLiteral("Editor"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("terminal:a")));
        QVERIFY(workspace.setPageLayoutKey(
            missingPage, QStringLiteral("missing:c")));
        QVERIFY(workspace.setPageLayoutKey(
            targetPage, QStringLiteral("editor:b")));
        workspace.tabWidget(sourceId)->setCurrentWidget(firstPage);
        workspace.tabWidget(targetId)->setCurrentWidget(targetPage);
        QVERIFY(workspace.setActiveGroup(sourceId));

        const auto savedIds = workspace.groupIds();
        const auto savedSizes = zzSplitterSizes(workspace);
        const QByteArray saved = workspace.saveLayout();
        QVERIFY(!saved.isEmpty());
        QDataStream envelope(saved);
        envelope.setVersion(QDataStream::Qt_6_8);
        char magic[4] {};
        QCOMPARE(envelope.readRawData(magic, 4), 4);
        QCOMPARE(QByteArray(magic, 4), QByteArrayLiteral("ZZSW"));
        quint16 schemaVersion = 0;
        quint16 streamVersion = 0;
        quint32 payloadLength = 0;
        envelope >> schemaVersion >> streamVersion >> payloadLength;
        QCOMPARE(schemaVersion, quint16(1));
        QCOMPARE(streamVersion, quint16(QDataStream::Qt_6_8));
        QCOMPARE(
            saved.size(),
            qsizetype(12 + payloadLength + 32));
        const QByteArray payload = saved.sliced(12, payloadLength);
        QCOMPARE(
            saved.last(32),
            QCryptographicHash::hash(
                payload, QCryptographicHash::Sha256));

        delete missingPage;
        QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        QVERIFY(workspace.removeEmptyGroup(sourceId));
        QCOMPARE(workspace.groupIds(), QList {targetId});

        QVERIFY(workspace.restoreLayout(saved));
        QCOMPARE(workspace.groupIds(), savedIds);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            savedIds.size());
        QCOMPARE(workspace.activeGroupId(), sourceId);
        QCOMPARE(zzSplitterSizes(workspace), savedSizes);
        QCOMPARE(workspace.tabWidget(sourceId)->indexOf(firstPage), 0);
        QCOMPARE(workspace.tabWidget(targetId)->indexOf(targetPage), 0);
        QCOMPARE(workspace.tabWidget(targetId)->indexOf(unkeyedPage), 1);
        QCOMPARE(workspace.tabWidget(sourceId)->currentWidget(), firstPage);
        QCOMPARE(workspace.tabWidget(targetId)->currentWidget(), targetPage);
        QCOMPARE(
            workspace.pageLayoutKey(firstPage),
            QStringLiteral("terminal:a"));
        QCOMPARE(
            workspace.savedGroupForPageKey(QStringLiteral("terminal:a")),
            sourceId);
        QCOMPARE(
            workspace.savedGroupForPageKey(QStringLiteral(" missing:c ")),
            sourceId);
        QCOMPARE(
            workspace.savedGroupForPageKey(QStringLiteral("editor:b")),
            targetId);
        QVERIFY(workspace.savedGroupForPageKey(
            QStringLiteral("unknown")).isValid() == false);
        QCOMPARE(workspace.saveLayout(), saved);
    }

    void rejectsMalformedAndOverLimitLayoutsWithoutMutation()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto rootId = workspace.groupIds().constFirst();
        auto *page = new QWidget;
        workspace.tabWidget(rootId)->addTab(page, QStringLiteral("Stable"));
        QVERIFY(workspace.setPageLayoutKey(
            page, QStringLiteral("stable:key")));
        const QByteArray stableLayout = workspace.saveLayout();
        QVERIFY(workspace.restoreLayout(stableLayout));
        const auto stableIds = workspace.groupIds();
        const auto stableActive = workspace.activeGroupId();
        const auto stableMapping = workspace.savedGroupForPageKey(
            QStringLiteral("stable:key"));

        QList<QByteArray> invalidLayouts;
        const QByteArray stablePayload = stableLayout.sliced(
            12, stableLayout.size() - 44);
        QByteArray damagedMagic = stableLayout;
        damagedMagic[0] = 'X';
        invalidLayouts.push_back(damagedMagic);
        invalidLayouts.push_back(zzWrapLayoutPayload(
            stablePayload, 2, QDataStream::Qt_6_8));
        invalidLayouts.push_back(zzWrapLayoutPayload(
            stablePayload, 1, 0));
        QByteArray damagedDigest = stableLayout;
        damagedDigest[damagedDigest.size() - 1] ^= 0x01;
        invalidLayouts.push_back(damagedDigest);
        invalidLayouts.push_back(stableLayout.first(stableLayout.size() - 1));
        invalidLayouts.push_back(stableLayout + QByteArrayLiteral("tail"));
        invalidLayouts.push_back(
            zzWrapLayoutPayload(QByteArray(1024 * 1024 + 1, 'x')));

        const ZzTestLayoutNode duplicateIds(
            static_cast<quint8>(Qt::Horizontal),
            {100, 100},
            {ZzTestLayoutNode(QStringLiteral("duplicate")),
             ZzTestLayoutNode(QStringLiteral("duplicate"))});
        invalidLayouts.push_back(zzBuildTestLayout(
            duplicateIds, QStringLiteral("duplicate")));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QStringLiteral("only")),
            QStringLiteral("only"),
            {{QStringLiteral("same"), QStringLiteral("only"), 0, true},
             {QStringLiteral(" same "), QStringLiteral("only"), 1, false}}));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QStringLiteral("only")),
            QStringLiteral("missing-active")));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QStringLiteral("only")),
            QStringLiteral("only"),
            {{QStringLiteral("missing-group-key"),
              QStringLiteral("missing-group"),
              0,
              false}}));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QStringLiteral("only")),
            QStringLiteral("only"),
            {{QStringLiteral("   "), QStringLiteral("only"), 0, false}}));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QStringLiteral("only")),
            QStringLiteral("only"),
            {{QString(257, u'k'), QStringLiteral("only"), 0, false}}));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QStringLiteral("only")),
            QStringLiteral("only"),
            {{QStringLiteral("negative-order"),
              QStringLiteral("only"),
              -1,
              false}}));

        QByteArray invalidCurrentPayload;
        QDataStream invalidCurrentStream(
            &invalidCurrentPayload, QIODevice::WriteOnly);
        invalidCurrentStream.setVersion(QDataStream::Qt_6_8);
        zzWriteLayoutNode(
            invalidCurrentStream,
            ZzTestLayoutNode(QStringLiteral("only")));
        zzWriteLayoutString(
            invalidCurrentStream, QStringLiteral("only"));
        invalidCurrentStream << quint16(1);
        zzWriteLayoutString(
            invalidCurrentStream, QStringLiteral("bad-current"));
        zzWriteLayoutString(
            invalidCurrentStream, QStringLiteral("only"));
        invalidCurrentStream << qint32(0) << quint8(2);
        invalidLayouts.push_back(zzWrapLayoutPayload(
            invalidCurrentPayload));

        QByteArray tooManyPagesPayload;
        QDataStream tooManyPagesStream(
            &tooManyPagesPayload, QIODevice::WriteOnly);
        tooManyPagesStream.setVersion(QDataStream::Qt_6_8);
        zzWriteLayoutNode(
            tooManyPagesStream,
            ZzTestLayoutNode(QStringLiteral("only")));
        zzWriteLayoutString(
            tooManyPagesStream, QStringLiteral("only"));
        tooManyPagesStream << quint16(4097);
        invalidLayouts.push_back(zzWrapLayoutPayload(
            tooManyPagesPayload));

        ZzTestLayoutNode tooDeep(QStringLiteral("deep-17"));
        for (int depth = 16; depth >= 1; --depth) {
            const quint8 orientation = static_cast<quint8>(
                depth % 2 == 0 ? Qt::Horizontal : Qt::Vertical);
            tooDeep = ZzTestLayoutNode(
                orientation,
                {100, 100},
                {ZzTestLayoutNode(
                     QStringLiteral("side-%1").arg(depth)),
                 tooDeep});
        }
        invalidLayouts.push_back(zzBuildTestLayout(
            tooDeep, QStringLiteral("side-1")));

        QList<ZzTestLayoutNode> sixtyFiveLeaves;
        QList<int> sixtyFiveSizes;
        for (int index = 0; index < 65; ++index) {
            sixtyFiveLeaves.push_back(
                ZzTestLayoutNode(
                    QStringLiteral("group-%1").arg(index)));
            sixtyFiveSizes.push_back(1);
        }
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(
                static_cast<quint8>(Qt::Horizontal),
                sixtyFiveSizes,
                sixtyFiveLeaves),
            QStringLiteral("group-0")));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(
             9,
             {100, 100},
             {ZzTestLayoutNode(QStringLiteral("a")),
              ZzTestLayoutNode(QStringLiteral("b"))}),
            QStringLiteral("a")));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(
             static_cast<quint8>(Qt::Horizontal),
             {100},
             {ZzTestLayoutNode(QStringLiteral("a")),
              ZzTestLayoutNode(QStringLiteral("b"))}),
            QStringLiteral("a")));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(
             static_cast<quint8>(Qt::Horizontal),
             {100, 0},
             {ZzTestLayoutNode(QStringLiteral("a")),
              ZzTestLayoutNode(QStringLiteral("b"))}),
            QStringLiteral("a")));
        invalidLayouts.push_back(zzBuildTestLayout(
            ZzTestLayoutNode(QString(257, u'g')), QString(257, u'g')));

        QByteArray invalidTagPayload;
        QDataStream invalidTagStream(
            &invalidTagPayload, QIODevice::WriteOnly);
        invalidTagStream.setVersion(QDataStream::Qt_6_8);
        invalidTagStream << static_cast<quint8>(9);
        invalidLayouts.push_back(zzWrapLayoutPayload(invalidTagPayload));

        for (qsizetype invalidIndex = 0;
             invalidIndex < invalidLayouts.size();
             ++invalidIndex) {
            const QByteArray &invalid = invalidLayouts.at(invalidIndex);
            QVERIFY2(
                !workspace.restoreLayout(invalid),
                qPrintable(QStringLiteral("invalid layout index %1")
                               .arg(invalidIndex)));
            QCOMPARE(workspace.groupIds(), stableIds);
            QCOMPARE(workspace.activeGroupId(), stableActive);
            QCOMPARE(workspace.tabWidget(rootId)->indexOf(page), 0);
            QCOMPARE(workspace.pageLayoutKey(page), QStringLiteral("stable:key"));
            QCOMPARE(
                workspace.savedGroupForPageKey(QStringLiteral("stable:key")),
                stableMapping);
            QCOMPARE(workspace.saveLayout(), stableLayout);
        }
    }

    void destroyedStagingTargetRollsBackLayoutRestore()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId targetId(
            QStringLiteral("restore-target"));
        QVERIFY(workspace.splitGroup(
            sourceId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            targetId).has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(
            page, QStringLiteral("Rollback"));
        QVERIFY(workspace.setPageLayoutKey(
            page, QStringLiteral("rollback:key")));
        const QByteArray sourceLayout = workspace.saveLayout();

        QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        QVERIFY(workspace.removeEmptyGroup(sourceId));
        const QByteArray targetLayout = workspace.saveLayout();
        QVERIFY(workspace.restoreLayout(targetLayout));
        QCOMPARE(
            workspace.savedGroupForPageKey(
                QStringLiteral("rollback:key")),
            targetId);

        const auto beforeIds = workspace.groupIds();
        const auto beforeActive = workspace.activeGroupId();
        auto *const originalTabs = workspace.tabWidget(targetId);
        QWidget *const beforeCurrent = originalTabs->currentWidget();
        bool destroyedStagingTabs = false;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (destroyedStagingTabs) {
                    return;
                }
                destroyedStagingTabs = true;
                const auto allTabs = workspace.findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs != originalTabs) {
                        delete tabs;
                    }
                }
            });

        QVERIFY(!workspace.restoreLayout(sourceLayout));
        QVERIFY(destroyedStagingTabs);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            beforeIds.size());
        QCOMPARE(workspace.activeGroupId(), beforeActive);
        QCOMPARE(workspace.tabWidget(targetId), originalTabs);
        QCOMPARE(originalTabs->indexOf(page), 0);
        QCOMPARE(originalTabs->currentWidget(), beforeCurrent);
        QCOMPARE(
            workspace.savedGroupForPageKey(
                QStringLiteral("rollback:key")),
            targetId);
        QCOMPARE(workspace.saveLayout(), targetLayout);
    }

    void removedStagingTabRollsBackWithoutLosingPage()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId targetId(
            QStringLiteral("removed-staging-target"));
        QVERIFY(workspace.splitGroup(
            sourceId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            targetId).has_value());
        QPointer<QWidget> detachedPage = new QWidget;
        auto *currentPage = new QWidget;
        workspace.tabWidget(sourceId)->addTab(
            detachedPage, QStringLiteral("Detached"));
        workspace.tabWidget(sourceId)->addTab(
            currentPage, QStringLiteral("Current"));
        auto *const sourceTabs = workspace.tabWidget(sourceId);
        sourceTabs->setTabToolTip(0, QStringLiteral("Detached tooltip"));
        sourceTabs->setTabWhatsThis(0, QStringLiteral("Detached help"));
        sourceTabs->setTabEnabled(0, false);
        sourceTabs->fluentTabBar()->setTabData(
            0, QStringLiteral("detached-data"));
        sourceTabs->fluentTabBar()->setTabTextColor(0, QColor(Qt::red));
        sourceTabs->setTabPinned(0, true);
        sourceTabs->setTabModified(0, true);
        sourceTabs->setTabAttention(0, true);
        sourceTabs->setTabCloseEnabled(0, false);
        QVERIFY(workspace.setPageLayoutKey(
            detachedPage, QStringLiteral("detached:key")));
        const QByteArray sourceLayout = workspace.saveLayout();

        QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        QVERIFY(workspace.removeEmptyGroup(sourceId));
        auto *const originalTabs = workspace.tabWidget(targetId);
        originalTabs->setCurrentWidget(detachedPage);
        const auto beforeIds = workspace.groupIds();
        const auto beforeActive = workspace.activeGroupId();
        bool removedFromStaging = false;
        bool removedDuringMetadataRestore = false;
        bool connectedStaging = false;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (connectedStaging) {
                    return;
                }
                connectedStaging = true;
                const auto allTabs = workspace.findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs == originalTabs) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
                        &workspace,
                        [&, tabs](int index, bool pinned) {
                            if (!removedFromStaging
                                || removedDuringMetadataRestore
                                || !pinned
                                || tabs->widget(index) != detachedPage) {
                                return;
                            }
                            removedDuringMetadataRestore = true;
                            tabs->removeTab(index);
                        });
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, tabs](ZzFluentUI::ZzTabWidget *,
                                  int,
                                  int,
                                  QWidget *page) {
                            if (removedFromStaging
                                || page != detachedPage) {
                                return;
                            }
                            removedFromStaging = true;
                            const int pageIndex = tabs->indexOf(page);
                            tabs->setTabPinned(pageIndex, false);
                            tabs->removeTab(tabs->indexOf(page));
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(sourceLayout));
        QVERIFY(removedFromStaging);
        QVERIFY(!detachedPage.isNull());
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(workspace.activeGroupId(), beforeActive);
        QCOMPARE(workspace.tabWidget(targetId), originalTabs);
        QCOMPARE(originalTabs->indexOf(detachedPage), 0);
        QCOMPARE(originalTabs->indexOf(currentPage), 1);
        QCOMPARE(originalTabs->currentWidget(), detachedPage);
        QCOMPARE(originalTabs->tabText(0), QStringLiteral("Detached"));
        QCOMPARE(
            originalTabs->tabToolTip(0),
            QStringLiteral("Detached tooltip"));
        QCOMPARE(
            originalTabs->tabWhatsThis(0),
            QStringLiteral("Detached help"));
        QVERIFY(!originalTabs->isTabEnabled(0));
        QCOMPARE(
            originalTabs->fluentTabBar()->tabData(0).toString(),
            QStringLiteral("detached-data"));
        QCOMPARE(
            originalTabs->fluentTabBar()->tabTextColor(0),
            QColor(Qt::red));
        QVERIFY(originalTabs->isTabPinned(0));
        QVERIFY(originalTabs->isTabModified(0));
        QVERIFY(originalTabs->hasTabAttention(0));
        QVERIFY(!originalTabs->isTabCloseEnabled(0));
    }

    void restoreRollbackLeavesThirdPartyPageAlone()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto sourceId = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId targetId(
            QStringLiteral("third-party-target"));
        QVERIFY(workspace.splitGroup(
            sourceId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            targetId).has_value());
        auto *firstPage = new QWidget;
        auto *claimedPage = new QWidget;
        auto *currentPage = new QWidget;
        workspace.tabWidget(sourceId)->addTab(
            firstPage, QStringLiteral("First"));
        workspace.tabWidget(sourceId)->addTab(
            claimedPage, QStringLiteral("Claimed"));
        workspace.tabWidget(sourceId)->addTab(
            currentPage, QStringLiteral("Current"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("first:key")));
        QVERIFY(workspace.setPageLayoutKey(
            claimedPage, QStringLiteral("claimed:key")));
        workspace.tabWidget(sourceId)->setCurrentWidget(currentPage);
        const QByteArray sourceLayout = workspace.saveLayout();

        while (workspace.tabWidget(sourceId)->count() > 0) {
            QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        }
        QVERIFY(workspace.removeEmptyGroup(sourceId));
        workspace.tabWidget(targetId)->setCurrentWidget(currentPage);
        const QByteArray targetLayout = workspace.saveLayout();
        QVERIFY(workspace.restoreLayout(targetLayout));
        auto *const originalTabs = workspace.tabWidget(targetId);
        const auto beforeIds = workspace.groupIds();
        const auto beforeActive = workspace.activeGroupId();
        bool claimed = false;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (!claimed
                    && originalTabs->indexOf(claimedPage) < 0) {
                    claimed = true;
                    thirdParty.addTab(
                        claimedPage, QStringLiteral("Claimed"));
                }
            });

        QVERIFY(!workspace.restoreLayout(sourceLayout));
        QVERIFY(claimed);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            beforeIds.size());
        QCOMPARE(workspace.activeGroupId(), beforeActive);
        QCOMPARE(workspace.tabWidget(targetId), originalTabs);
        QCOMPARE(originalTabs->indexOf(firstPage), 0);
        QCOMPARE(originalTabs->indexOf(currentPage), 1);
        QCOMPARE(originalTabs->currentWidget(), currentPage);
        QCOMPARE(thirdParty.indexOf(claimedPage), 0);
        QCOMPARE(
            workspace.savedGroupForPageKey(
                QStringLiteral("claimed:key")),
            targetId);
        QCOMPARE(workspace.saveLayout(), targetLayout);
    }

    void replacementStagingTargetWithSameIdIsNotRemoved()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto savedSourceId = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId currentTargetId(
            QStringLiteral("replacement-current"));
        QVERIFY(workspace.splitGroup(
            savedSourceId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            currentTargetId).has_value());
        auto *page = new QWidget;
        workspace.tabWidget(savedSourceId)->addTab(
            page, QStringLiteral("Replacement"));
        QVERIFY(workspace.setPageLayoutKey(
            page, QStringLiteral("replacement:key")));
        const QByteArray sourceLayout = workspace.saveLayout();
        QVERIFY(workspace.transferTab(
            savedSourceId, 0, currentTargetId));
        QVERIFY(workspace.removeEmptyGroup(savedSourceId));

        auto *const originalTabs = workspace.tabWidget(currentTargetId);
        QPointer<ZzFluentUI::ZzTabWidget> replacementTabs;
        bool replaced = false;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (replaced) {
                    return;
                }
                replaced = true;
                const auto allTabs = workspace.findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs != originalTabs) {
                        delete tabs;
                    }
                }
                const auto replacement = workspace.splitGroup(
                    currentTargetId,
                    Qt::Vertical,
                    ZzFluentUI::ZzSplitPlacement::After,
                    savedSourceId);
                if (replacement.has_value()) {
                    replacementTabs = workspace.tabWidget(savedSourceId);
                }
            });

        QVERIFY(!workspace.restoreLayout(sourceLayout));
        QVERIFY(replaced);
        QVERIFY(!replacementTabs.isNull());
        QCOMPARE(
            workspace.tabWidget(savedSourceId),
            replacementTabs.data());
        QCOMPARE(workspace.tabWidget(currentTargetId), originalTabs);
        QCOMPARE(originalTabs->indexOf(page), 0);
    }

    void restoreSignalsMayInvalidateParticipants()
    {
        {
            ZzFluentUI::ZzSplitWorkspace workspace;
            const auto sourceId = workspace.groupIds().constFirst();
            const auto targetId = workspace.splitGroup(
                sourceId,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(targetId.has_value());
            QPointer<QWidget> page = new QWidget;
            workspace.tabWidget(sourceId)->addTab(
                page, QStringLiteral("Deleted page"));
            QVERIFY(workspace.setPageLayoutKey(
                page, QStringLiteral("deleted-page:key")));
            const QByteArray saved = workspace.saveLayout();
            QVERIFY(workspace.transferTab(
                sourceId, 0, targetId.value()));
            auto *const originalTabs = workspace.tabWidget(
                targetId.value());
            bool connectedStaging = false;
            connect(
                originalTabs,
                &QTabWidget::currentChanged,
                &workspace,
                [&](int) {
                    if (connectedStaging) {
                        return;
                    }
                    connectedStaging = true;
                    const auto allTabs = workspace.findChildren<
                        ZzFluentUI::ZzTabWidget *>();
                    for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                        if (tabs == originalTabs) {
                            continue;
                        }
                        connect(
                            tabs,
                            &ZzFluentUI::ZzTabWidget::tabTransferred,
                            &workspace,
                            [&](ZzFluentUI::ZzTabWidget *,
                                int,
                                int,
                                QWidget *) {
                                delete page.data();
                            });
                    }
                });

            QVERIFY(!workspace.restoreLayout(saved));
            QVERIFY(page.isNull());
            QCOMPARE(workspace.groupIds().size(), 2);
        }

        {
            ZzFluentUI::ZzSplitWorkspace workspace;
            const auto sourceId = workspace.groupIds().constFirst();
            const auto targetId = workspace.splitGroup(
                sourceId,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(targetId.has_value());
            auto *page = new QWidget;
            workspace.tabWidget(sourceId)->addTab(
                page, QStringLiteral("Deleted source"));
            QVERIFY(workspace.setPageLayoutKey(
                page, QStringLiteral("deleted-source:key")));
            const QByteArray saved = workspace.saveLayout();
            QVERIFY(workspace.transferTab(
                sourceId, 0, targetId.value()));
            QPointer<ZzFluentUI::ZzTabWidget> deletedSource =
                workspace.tabWidget(targetId.value());
            bool connectedStaging = false;
            connect(
                deletedSource,
                &QTabWidget::currentChanged,
                &workspace,
                [&](int) {
                    if (connectedStaging) {
                        return;
                    }
                    connectedStaging = true;
                    const auto allTabs = workspace.findChildren<
                        ZzFluentUI::ZzTabWidget *>();
                    for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                        if (tabs == deletedSource) {
                            continue;
                        }
                        connect(
                            tabs,
                            &ZzFluentUI::ZzTabWidget::tabTransferred,
                            &workspace,
                            [&](ZzFluentUI::ZzTabWidget *,
                                int,
                                int,
                                QWidget *) {
                                delete deletedSource.data();
                            });
                    }
                });

            QVERIFY(!workspace.restoreLayout(saved));
            QVERIFY(deletedSource.isNull());
            QVERIFY(workspace.tabWidget(targetId.value()) == nullptr);
        }

        {
            auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
            QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
            const auto sourceId = rawWorkspace->groupIds().constFirst();
            const auto targetId = rawWorkspace->splitGroup(
                sourceId,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(targetId.has_value());
            auto *page = new QWidget;
            rawWorkspace->tabWidget(sourceId)->addTab(
                page, QStringLiteral("Deleted workspace"));
            QVERIFY(rawWorkspace->setPageLayoutKey(
                page, QStringLiteral("deleted-workspace:key")));
            const QByteArray saved = rawWorkspace->saveLayout();
            QVERIFY(rawWorkspace->transferTab(
                sourceId, 0, targetId.value()));
            auto *const originalTabs = rawWorkspace->tabWidget(
                targetId.value());
            bool connectedStaging = false;
            connect(
                originalTabs,
                &QTabWidget::currentChanged,
                rawWorkspace,
                [rawWorkspace,
                 originalTabs,
                 &connectedStaging](int) {
                    if (connectedStaging) {
                        return;
                    }
                    connectedStaging = true;
                    const auto allTabs = rawWorkspace->findChildren<
                        ZzFluentUI::ZzTabWidget *>();
                    for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                        if (tabs == originalTabs) {
                            continue;
                        }
                        connect(
                            tabs,
                            &ZzFluentUI::ZzTabWidget::tabTransferred,
                            tabs,
                            [rawWorkspace](
                                ZzFluentUI::ZzTabWidget *,
                                int,
                                int,
                                QWidget *) {
                                delete rawWorkspace;
                            });
                    }
                });

            QVERIFY(!rawWorkspace->restoreLayout(saved));
            QVERIFY(workspace.isNull());
        }
    }

    void transfersTabsAndRollsBackEdgeDrops()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Page"));

        QVERIFY(workspace.transferTab(sourceId, 0, targetId.value(), 0));
        QCOMPARE(workspace.tabWidget(targetId.value())->widget(0), page);

        const QList<ZzFluentUI::ZzTabGroupId> before = workspace.groupIds();
        const QList<QList<int>> beforeSizes = zzSplitterSizes(workspace);
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex = workspace.tabWidget(targetId.value())->indexOf(page);
        QVERIFY(!workspace.moveTabToDropZone(
            targetId.value(),
            99,
            targetId.value(),
            ZzFluentUI::ZzWorkspaceDropZone::Left));
        QCOMPARE(workspace.groupIds(), before);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(workspace.tabWidget(targetId.value())->indexOf(page), beforeIndex);
    }

    void movesTabsToAllFiveDropZones()
    {
        const QList<ZzFluentUI::ZzWorkspaceDropZone> edgeZones {
            ZzFluentUI::ZzWorkspaceDropZone::Left,
            ZzFluentUI::ZzWorkspaceDropZone::Top,
            ZzFluentUI::ZzWorkspaceDropZone::Right,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom,
        };
        for (const auto zone : edgeZones) {
            ZzFluentUI::ZzSplitWorkspace workspace;
            const auto targetId = workspace.groupIds().constFirst();
            const auto sourceId = workspace.splitGroup(
                targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(sourceId.has_value());
            auto *page = new QWidget;
            workspace.tabWidget(sourceId.value())->addTab(
                page, QStringLiteral("Edge"));
            QSignalSpy committedSpy(
                &workspace,
                &ZzFluentUI::ZzSplitWorkspace::tabDropCommitted);
            QSignalSpy layoutSpy(
                &workspace,
                &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
            layoutSpy.clear();

            QVERIFY(workspace.moveTabToDropZone(
                sourceId.value(), 0, targetId, zone));
            QCOMPARE(workspace.groupIds().size(), 2);
            QCOMPARE(committedSpy.size(), 1);
            QCOMPARE(layoutSpy.size(), 1);
            const auto destinationId = workspace.activeGroupId();
            QVERIFY(destinationId != targetId);
            QCOMPARE(workspace.tabWidget(destinationId)->widget(0), page);
            QVERIFY(workspace.tabWidget(sourceId.value()) == nullptr);

            const auto *splitter = workspace.findChild<QSplitter *>();
            QVERIFY(splitter != nullptr);
            const Qt::Orientation expectedOrientation =
                zone == ZzFluentUI::ZzWorkspaceDropZone::Left
                    || zone == ZzFluentUI::ZzWorkspaceDropZone::Right
                ? Qt::Horizontal
                : Qt::Vertical;
            QCOMPARE(splitter->orientation(), expectedOrientation);
            const auto ids = workspace.groupIds();
            const qsizetype destinationPosition = ids.indexOf(destinationId);
            const qsizetype targetPosition = ids.indexOf(targetId);
            if (zone == ZzFluentUI::ZzWorkspaceDropZone::Left
                || zone == ZzFluentUI::ZzWorkspaceDropZone::Top) {
                QCOMPARE(destinationPosition + 1, targetPosition);
            } else {
                QCOMPARE(destinationPosition, targetPosition + 1);
            }
        }

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Center"));
        QSignalSpy committedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::tabDropCommitted);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
        layoutSpy.clear();
        QVERIFY(workspace.moveTabToDropZone(
            sourceId,
            0,
            targetId.value(),
            ZzFluentUI::ZzWorkspaceDropZone::Center));
        QCOMPARE(workspace.tabWidget(targetId.value())->widget(0), page);
        QCOMPARE(committedSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 0);
    }

    void mirrorsPhysicalLeftAndRightDropZonesInRtl()
    {
        for (const auto zone : {
                 ZzFluentUI::ZzWorkspaceDropZone::Left,
                 ZzFluentUI::ZzWorkspaceDropZone::Right}) {
            ZzFluentUI::ZzSplitWorkspace workspace;
            workspace.setLayoutDirection(Qt::RightToLeft);
            const auto targetId = workspace.groupIds().constFirst();
            auto *page = new QWidget;
            workspace.tabWidget(targetId)->addTab(page, QStringLiteral("RTL"));
            workspace.tabWidget(targetId)->addTab(
                new QWidget, QStringLiteral("Retained"));

            QVERIFY(workspace.moveTabToDropZone(
                targetId, 0, targetId, zone));
            const auto ids = workspace.groupIds();
            QCOMPARE(ids.size(), 2);
            const auto destinationId = workspace.activeGroupId();
            const qsizetype destinationIndex = ids.indexOf(destinationId);
            QCOMPARE(
                destinationIndex,
                zone == ZzFluentUI::ZzWorkspaceDropZone::Left ? 1 : 0);
            QCOMPARE(workspace.tabWidget(destinationId)->widget(0), page);
        }
    }

    void transferSignalsMayInvalidateParticipants()
    {
        enum class Action { DeleteSource, DeleteTarget, DeletePage, DeleteWorkspace };
        for (const auto action : {
                 Action::DeleteSource,
                 Action::DeleteTarget,
                 Action::DeletePage,
                 Action::DeleteWorkspace}) {
            auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
            QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
            const auto sourceId = rawWorkspace->groupIds().constFirst();
            const auto targetId = rawWorkspace->splitGroup(
                sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(targetId.has_value());
            QPointer<ZzFluentUI::ZzTabWidget> source =
                rawWorkspace->tabWidget(sourceId);
            QPointer<ZzFluentUI::ZzTabWidget> target =
                rawWorkspace->tabWidget(targetId.value());
            QPointer<QWidget> page = new QWidget;
            source->addTab(page, QStringLiteral("Guarded"));
            connect(
                target,
                &ZzFluentUI::ZzTabWidget::tabTransferred,
                target,
                [&, action](ZzFluentUI::ZzTabWidget *, int, int, QWidget *) {
                    if (action == Action::DeleteSource) {
                        delete source.data();
                    } else if (action == Action::DeleteTarget) {
                        delete target.data();
                    } else if (action == Action::DeletePage) {
                        delete page.data();
                    } else {
                        delete workspace.data();
                    }
                });

            const bool transferred = rawWorkspace->transferTab(
                sourceId, 0, targetId.value(), 0);
            if (action == Action::DeleteWorkspace) {
                QVERIFY(workspace.isNull());
                QVERIFY(transferred);
            } else {
                QVERIFY(!workspace.isNull());
                QVERIFY(!transferred);
                delete workspace.data();
            }
        }
    }

    void edgeTransferSignalsMayInvalidateParticipants()
    {
        enum class Action {
            DeleteSource,
            DeleteOriginalTarget,
            DeleteTemporary,
            DeletePage,
            DeleteWorkspace
        };
        for (const auto action : {
                 Action::DeleteSource,
                 Action::DeleteOriginalTarget,
                 Action::DeleteTemporary,
                 Action::DeletePage,
                 Action::DeleteWorkspace}) {
            auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
            QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
            const auto targetId = rawWorkspace->groupIds().constFirst();
            const auto sourceId = rawWorkspace->splitGroup(
                targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(sourceId.has_value());
            QPointer<ZzFluentUI::ZzTabWidget> source =
                rawWorkspace->tabWidget(sourceId.value());
            QPointer<ZzFluentUI::ZzTabWidget> originalTarget =
                rawWorkspace->tabWidget(targetId);
            QPointer<ZzFluentUI::ZzTabWidget> temporary;
            QPointer<QWidget> page = new QWidget;
            source->addTab(page, QStringLiteral("Edge guarded"));
            const auto beforeIds = rawWorkspace->groupIds();
            connect(
                source,
                &QTabWidget::currentChanged,
                source,
                [&, action](int) {
                    if (!temporary.isNull() || workspace.isNull()) {
                        return;
                    }
                    for (const auto &id : workspace->groupIds()) {
                        if (!beforeIds.contains(id)) {
                            temporary = workspace->tabWidget(id);
                            break;
                        }
                    }
                    if (temporary.isNull()) {
                        return;
                    }
                    connect(
                        temporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        temporary,
                        [&, action](
                            ZzFluentUI::ZzTabWidget *, int, int, QWidget *) {
                            if (action == Action::DeleteSource) {
                                delete source.data();
                            } else if (
                                action == Action::DeleteOriginalTarget) {
                                delete originalTarget.data();
                            } else if (action == Action::DeleteTemporary) {
                                delete temporary.data();
                            } else if (action == Action::DeletePage) {
                                delete page.data();
                            } else {
                                delete workspace.data();
                            }
                        });
                });

            const bool moved = rawWorkspace->moveTabToDropZone(
                sourceId.value(),
                0,
                targetId,
                ZzFluentUI::ZzWorkspaceDropZone::Top);
            if (action == Action::DeleteWorkspace) {
                QVERIFY(workspace.isNull());
                QVERIFY(moved);
                continue;
            }
            QVERIFY(!workspace.isNull());
            if (action == Action::DeleteSource) {
                QVERIFY(moved);
                QCOMPARE(workspace->groupIds().size(), 2);
                QVERIFY(!workspace->groupIds().contains(sourceId.value()));
            } else if (action == Action::DeleteOriginalTarget) {
                QVERIFY(!moved);
                QVERIFY(workspace->tabWidget(targetId) == nullptr);
                QVERIFY(!page.isNull());
                QCOMPARE(
                    workspace->tabWidget(sourceId.value())->indexOf(page),
                    0);
            } else {
                QVERIFY(!moved);
            }
            delete workspace.data();
        }
    }

    void edgeFailurePreservesNestedTreeStateAndSignals()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        const auto nestedId = workspace.splitGroup(
            targetId, Qt::Vertical, ZzFluentUI::ZzSplitPlacement::Before);
        QVERIFY(nestedId.has_value());
        workspace.resize(900, 700);
        workspace.show();
        QCoreApplication::processEvents();
        for (QSplitter *splitter : workspace.findChildren<QSplitter *>()) {
            splitter->setSizes(
                splitter->orientation() == Qt::Horizontal
                    ? QList<int> {241, 659}
                    : QList<int> {173, 527});
        }
        QVERIFY(workspace.setActiveGroup(nestedId.value()));
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Nested rollback"));

        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        const auto beforeActive = workspace.activeGroupId();
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex = workspace.tabWidget(sourceId.value())->indexOf(page);
        QSignalSpy addedSpy(
            &workspace, &ZzFluentUI::ZzSplitWorkspace::groupAdded);
        QSignalSpy removedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAboutToBeRemoved);
        QSignalSpy activeSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::activeGroupChanged);
        QSignalSpy committedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::tabDropCommitted);
        QSignalSpy layoutSpy(
            &workspace, &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
        bool destroyedTemporary = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (destroyedTemporary) {
                    return;
                }
                for (const auto &id : workspace.groupIds()) {
                    if (!beforeIds.contains(id)) {
                        destroyedTemporary = true;
                        delete workspace.tabWidget(id);
                        return;
                    }
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Right));
        QVERIFY(destroyedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(workspace.activeGroupId(), beforeActive);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(workspace.tabWidget(sourceId.value())->indexOf(page), beforeIndex);
        QCOMPARE(addedSpy.size(), 0);
        QCOMPARE(removedSpy.size(), 0);
        QCOMPARE(activeSpy.size(), 0);
        QCOMPARE(committedSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);
    }

    void deletedSourceTargetIdentityIsNotRecreated()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceTargetId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> sourceTarget =
            workspace.tabWidget(sourceTargetId);
        QPointer<QWidget> page = new QWidget;
        sourceTarget->addTab(page, QStringLiteral("Moved"));
        sourceTarget->addTab(new QWidget, QStringLiteral("Deleted"));
        const auto beforeIds = workspace.groupIds();
        QPointer<ZzFluentUI::ZzTabWidget> temporary;
        connect(
            sourceTarget,
            &QTabWidget::currentChanged,
            sourceTarget,
            [&](int) {
                if (!temporary.isNull()) {
                    return;
                }
                for (const auto &id : workspace.groupIds()) {
                    if (!beforeIds.contains(id)) {
                        temporary = workspace.tabWidget(id);
                        break;
                    }
                }
                if (!temporary.isNull()) {
                    connect(
                        temporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        temporary,
                        [&](ZzFluentUI::ZzTabWidget *, int, int, QWidget *) {
                            delete sourceTarget.data();
                        });
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceTargetId,
            0,
            sourceTargetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));
        QVERIFY(sourceTarget.isNull());
        QVERIFY(workspace.tabWidget(sourceTargetId) == nullptr);
        QCOMPARE(workspace.groupIds().size(), 1);
        QVERIFY(!page.isNull());
        QCOMPARE(workspace.tabWidget(workspace.groupIds().constFirst())->indexOf(page), 0);
    }

    void dragTokensDoNotAllocateTimers()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        QCOMPARE(workspace.findChildren<QTimer *>().size(), 0);
    }

    void edgeMoveFromActiveSourceEmitsActiveChange()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        workspace.tabWidget(sourceId.value())->addTab(
            new QWidget, QStringLiteral("Active source"));
        QVERIFY(workspace.setActiveGroup(sourceId.value()));
        QSignalSpy activeSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::activeGroupChanged);

        QVERIFY(workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Top));

        QCOMPARE(activeSpy.size(), 1);
        QCOMPARE(
            activeSpy.constFirst().constFirst().value<ZzFluentUI::ZzTabGroupId>(),
            workspace.activeGroupId());
        QVERIFY(workspace.activeGroupId() != sourceId.value());
    }

    void replacementTargetWithSameIdIsNotRemoved()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        QPointer<ZzFluentUI::ZzTabWidget> originalTarget =
            workspace.tabWidget(targetId);
        QPointer<ZzFluentUI::ZzTabWidget> replacementTarget;
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Replacement target"));
        const auto beforeIds = workspace.groupIds();
        bool replacedTarget = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                for (const auto &id : workspace.groupIds()) {
                    if (beforeIds.contains(id)) {
                        continue;
                    }
                    auto *temporary = workspace.tabWidget(id);
                    connect(
                        temporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&](ZzFluentUI::ZzTabWidget *, int, int, QWidget *) {
                            if (replacedTarget) {
                                return;
                            }
                            delete originalTarget.data();
                            if (!workspace.removeEmptyGroup(targetId)) {
                                return;
                            }
                            const auto replacementId = workspace.splitGroup(
                                sourceId.value(),
                                Qt::Horizontal,
                                ZzFluentUI::ZzSplitPlacement::After,
                                targetId);
                            if (!replacementId.has_value()) {
                                return;
                            }
                            replacementTarget = workspace.tabWidget(targetId);
                            replacedTarget = !replacementTarget.isNull();
                        });
                    return;
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));

        QVERIFY(replacedTarget);
        QVERIFY(originalTarget.isNull());
        QVERIFY(!replacementTarget.isNull());
        QCOMPARE(workspace.tabWidget(targetId), replacementTarget.data());
        QCOMPARE(workspace.tabWidget(sourceId.value())->indexOf(page), 0);
    }

    void replacementSourceWithSameIdIsNotRemoved()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        QPointer<ZzFluentUI::ZzTabWidget> originalSource =
            workspace.tabWidget(sourceId.value());
        QPointer<ZzFluentUI::ZzTabWidget> replacementSource;
        auto *page = new QWidget;
        originalSource->addTab(page, QStringLiteral("Replacement source"));
        const auto beforeIds = workspace.groupIds();
        bool replacedSource = false;
        connect(
            originalSource,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                for (const auto &id : workspace.groupIds()) {
                    if (beforeIds.contains(id)) {
                        continue;
                    }
                    auto *temporary = workspace.tabWidget(id);
                    connect(
                        temporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&](ZzFluentUI::ZzTabWidget *, int, int, QWidget *) {
                            if (replacedSource) {
                                return;
                            }
                            if (!workspace.removeEmptyGroup(sourceId.value())) {
                                return;
                            }
                            const auto replacementId = workspace.splitGroup(
                                targetId,
                                Qt::Horizontal,
                                ZzFluentUI::ZzSplitPlacement::After,
                                sourceId.value());
                            if (!replacementId.has_value()) {
                                return;
                            }
                            replacementSource =
                                workspace.tabWidget(sourceId.value());
                            replacedSource = !replacementSource.isNull();
                        });
                    return;
                }
            });

        QVERIFY(workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));

        QVERIFY(replacedSource);
        QVERIFY(originalSource.isNull());
        QVERIFY(!replacementSource.isNull());
        QCOMPARE(
            workspace.tabWidget(sourceId.value()),
            replacementSource.data());
        QCOMPARE(
            workspace.tabWidget(workspace.activeGroupId())->indexOf(page),
            0);
    }

    void replacementTemporaryWithSameIdIsNotRemoved()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Replacement temporary"));
        const auto beforeIds = workspace.groupIds();
        ZzFluentUI::ZzTabGroupId temporaryId;
        QPointer<ZzFluentUI::ZzTabWidget> originalTemporary;
        QPointer<ZzFluentUI::ZzTabWidget> replacementTemporary;
        bool replacedTemporary = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                for (const auto &id : workspace.groupIds()) {
                    if (beforeIds.contains(id)) {
                        continue;
                    }
                    temporaryId = id;
                    originalTemporary = workspace.tabWidget(id);
                    connect(
                        originalTemporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, temporary = originalTemporary](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int targetIndex,
                            QWidget *) {
                            if (replacedTemporary) {
                                return;
                            }
                            if (!temporary->transferTabTo(
                                    &thirdParty, targetIndex)
                                || !workspace.removeEmptyGroup(temporaryId)) {
                                return;
                            }
                            const auto replacementId = workspace.splitGroup(
                                targetId,
                                Qt::Vertical,
                                ZzFluentUI::ZzSplitPlacement::After,
                                temporaryId);
                            if (!replacementId.has_value()) {
                                return;
                            }
                            replacementTemporary =
                                workspace.tabWidget(temporaryId);
                            replacedTemporary =
                                !replacementTemporary.isNull();
                        });
                    return;
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));

        QVERIFY(replacedTemporary);
        QVERIFY(originalTemporary.isNull());
        QVERIFY(!replacementTemporary.isNull());
        QCOMPARE(
            workspace.tabWidget(temporaryId),
            replacementTemporary.data());
        QCOMPARE(thirdParty.indexOf(page), 0);
    }

    void doesNotTakeBackPageClaimedByThirdParty()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Claimed"));
        connect(
            workspace.tabWidget(targetId.value()),
            &ZzFluentUI::ZzTabWidget::tabTransferred,
            &workspace,
            [&](ZzFluentUI::ZzTabWidget *, int, int targetIndex, QWidget *) {
                workspace.tabWidget(targetId.value())->transferTabTo(
                    &thirdParty, targetIndex);
            });

        QVERIFY(!workspace.transferTab(sourceId, 0, targetId.value(), 0));
        QCOMPARE(thirdParty.indexOf(page), 0);
        QCOMPARE(workspace.tabWidget(sourceId)->indexOf(page), -1);
        QCOMPARE(workspace.tabWidget(targetId.value())->indexOf(page), -1);
    }

    void edgeFailureRestoresTreeButLeavesThirdPartyPageAlone()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        ZzFluentUI::ZzTabWidget thirdParty;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Claimed edge"));
        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        bool connectedTemporary = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (connectedTemporary) {
                    return;
                }
                for (const auto &id : workspace.groupIds()) {
                    if (beforeIds.contains(id)) {
                        continue;
                    }
                    connectedTemporary = true;
                    auto *temporary = workspace.tabWidget(id);
                    connect(
                        temporary,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, temporary](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int targetIndex,
                            QWidget *) {
                            temporary->transferTabTo(
                                &thirdParty, targetIndex);
                        });
                    return;
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));
        QVERIFY(connectedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(thirdParty.indexOf(page), 0);
        QCOMPARE(workspace.tabWidget(sourceId.value())->indexOf(page), -1);
    }

    void edgeFailureRestoresPageWhenTemporaryTargetIsDestroyed()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId.value())->addTab(
            page, QStringLiteral("Rollback"));
        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex =
            workspace.tabWidget(sourceId.value())->indexOf(page);
        bool destroyedTemporary = false;
        connect(
            workspace.tabWidget(sourceId.value()),
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (destroyedTemporary) {
                    return;
                }
                for (const auto &id : workspace.groupIds()) {
                    if (!beforeIds.contains(id)) {
                        destroyedTemporary = true;
                        delete workspace.tabWidget(id);
                        return;
                    }
                }
            });

        QVERIFY(!workspace.moveTabToDropZone(
            sourceId.value(),
            0,
            targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Top));
        QVERIFY(destroyedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(
            workspace.tabWidget(sourceId.value())->indexOf(page),
            beforeIndex);
    }

    void rejectsForgedDragTokensAndKeepsOverlayBudgetBounded()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const auto targetId = workspace.splitGroup(
            sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(targetId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(sourceId)->addTab(page, QStringLiteral("Drag"));
        workspace.resize(800, 400);
        workspace.show();
        ZzFluentUI::ZzSplitWorkspace otherWorkspace;
        otherWorkspace.resize(400, 300);
        workspace.raise();
        workspace.activateWindow();
        QCoreApplication::processEvents();

        QMimeData forged;
        forged.setData(
            QByteArrayLiteral("application/x-zz-split-workspace-tab-v1"),
            QByteArrayLiteral("forged"));
        QDragEnterEvent forgedEnter(
            QPoint(10, 10), Qt::MoveAction, &forged,
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&workspace, &forgedEnter);
        QVERIFY(!forgedEnter.isAccepted());

        if (QApplication::platformName() == QStringLiteral("offscreen")) {
            for (qsizetype count = workspace.groupIds().size();
                 count < 12;
                 ++count) {
                QVERIFY(workspace.splitGroup(
                    targetId.value(),
                    Qt::Horizontal,
                    ZzFluentUI::ZzSplitPlacement::After).has_value());
            }
            QCOMPARE(
                workspace.findChildren<QWidget *>(
                    QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
                0);
            return;
        }

        bool sawRealDrag = false;
        bool acceptedEnter = false;
        bool acceptedMove = false;
        bool acceptedMoveAfterOverlayDeletion = false;
        bool acceptedLongDragMoves = true;
        bool acceptedDrop = false;
        bool rejectedByOtherWorkspace = false;
        qsizetype overlayCount = -1;
        QTimer::singleShot(0, &workspace, [&] {
            auto *drag = workspace.findChild<QDrag *>();
            if (drag == nullptr) {
                QDrag::cancel();
                return;
            }
            sawRealDrag = true;
            const QPoint targetCenter = workspace.tabWidget(targetId.value())
                                            ->mapTo(
                                                &workspace,
                                                workspace.tabWidget(
                                                    targetId.value())
                                                    ->rect()
                                                    .center());
            QDragEnterEvent enter(
                targetCenter,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &enter);
            acceptedEnter = enter.isAccepted();

            QDragEnterEvent foreignEnter(
                otherWorkspace.rect().center(),
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&otherWorkspace, &foreignEnter);
            rejectedByOtherWorkspace = !foreignEnter.isAccepted();

            QDragMoveEvent move(
                targetCenter,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &move);
            acceptedMove = move.isAccepted();
            overlayCount = workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size();
            const auto overlays = workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay"));
            if (!overlays.isEmpty()) {
                delete overlays.constFirst();
            }
            QDragMoveEvent moveAfterDeletion(
                targetCenter,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &moveAfterDeletion);
            acceptedMoveAfterOverlayDeletion =
                moveAfterDeletion.isAccepted();
            for (int moveCount = 0; moveCount < 6; ++moveCount) {
                QTest::qWait(900);
                QDragMoveEvent continuedMove(
                    targetCenter,
                    Qt::MoveAction,
                    drag->mimeData(),
                    Qt::LeftButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(&workspace, &continuedMove);
                acceptedLongDragMoves = acceptedLongDragMoves
                    && continuedMove.isAccepted();
            }

            QDropEvent drop(
                QPointF(targetCenter),
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &drop);
            acceptedDrop = drop.isAccepted();
            QDrag::cancel();
        });
        auto *sourceBar = workspace.tabWidget(sourceId)->fluentTabBar();
        const QPoint pressPosition = sourceBar->tabRect(0).center();
        const QPoint movePosition = pressPosition
            + QPoint(QApplication::startDragDistance() + 2, 0);
        QTest::mousePress(
            sourceBar,
            Qt::LeftButton,
            Qt::NoModifier,
            pressPosition);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(movePosition),
            QPointF(sourceBar->mapToGlobal(movePosition)),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QCoreApplication::sendEvent(sourceBar, &move);
        QTest::mouseRelease(
            sourceBar,
            Qt::LeftButton,
            Qt::NoModifier,
            movePosition);

        QVERIFY(sawRealDrag);
        QVERIFY(acceptedEnter);
        QVERIFY(rejectedByOtherWorkspace);
        QVERIFY(acceptedMove);
        QVERIFY(acceptedMoveAfterOverlayDeletion);
        QVERIFY(acceptedLongDragMoves);
        QVERIFY(acceptedDrop);
        QCOMPARE(overlayCount, 1);
        QCOMPARE(workspace.tabWidget(targetId.value())->indexOf(page), 0);
        QCOMPARE(
            workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
            1);

        for (qsizetype count = workspace.groupIds().size();
             count < 12;
             ++count) {
            QVERIFY(workspace.splitGroup(
                targetId.value(),
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value());
        }
        QCOMPARE(
            workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
            1);
    }

    void expiredRealDragTokenCannotBeReissued()
    {
        if (QApplication::platformName() == QStringLiteral("offscreen")) {
            return;
        }

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        workspace.tabWidget(sourceId)->addTab(
            new QWidget, QStringLiteral("Expiring drag"));
        workspace.resize(500, 300);
        workspace.show();
        workspace.raise();
        workspace.activateWindow();
        QCoreApplication::processEvents();

        bool sawRealDrag = false;
        bool acceptedInitialEnter = false;
        bool rejectedExpiredEnter = false;
        QTimer::singleShot(0, &workspace, [&] {
            auto *drag = workspace.findChild<QDrag *>();
            if (drag == nullptr) {
                QDrag::cancel();
                return;
            }
            sawRealDrag = true;
            const QPoint center = workspace.rect().center();
            QDragEnterEvent initialEnter(
                center,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &initialEnter);
            acceptedInitialEnter = initialEnter.isAccepted();

            QTest::qWait(5100);
            QDragEnterEvent expiredEnter(
                center,
                Qt::MoveAction,
                drag->mimeData(),
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(&workspace, &expiredEnter);
            rejectedExpiredEnter = !expiredEnter.isAccepted();
            QDrag::cancel();
        });

        auto *sourceBar = workspace.tabWidget(sourceId)->fluentTabBar();
        const QPoint pressPosition = sourceBar->tabRect(0).center();
        const QPoint movePosition = pressPosition
            + QPoint(QApplication::startDragDistance() + 2, 0);
        QTest::mousePress(
            sourceBar,
            Qt::LeftButton,
            Qt::NoModifier,
            pressPosition);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(movePosition),
            QPointF(sourceBar->mapToGlobal(movePosition)),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QCoreApplication::sendEvent(sourceBar, &move);
        QTest::mouseRelease(
            sourceBar,
            Qt::LeftButton,
            Qt::NoModifier,
            movePosition);

        QVERIFY(sawRealDrag);
        QVERIFY(acceptedInitialEnter);
        QVERIFY(rejectedExpiredEnter);
    }

    void splitsFlattensAndKeepsOneLeaf()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        QCOMPARE(workspace.groupIds().size(), 1);
        const auto root = workspace.groupIds().constFirst();
        workspace.tabWidget(root)->addTab(
            new QWidget, QStringLiteral("Pinned"));
        const auto right = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(right.has_value());
        const auto farRight = workspace.splitGroup(
            right.value(), Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(farRight.has_value());
        QCOMPARE(workspace.groupIds().size(), 3);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 1);
        QVERIFY(workspace.removeEmptyGroup(right.value()));
        QCOMPARE(workspace.groupIds().size(), 2);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 1);
        QVERIFY(!workspace.removeEmptyGroup(root));
    }

    void rejectsDuplicateRequestedIdWithoutSignals()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId requested(
            QStringLiteral("requested-group"));
        QSignalSpy addedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAdded);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);

        const auto added = workspace.splitGroup(
            root,
            Qt::Vertical,
            ZzFluentUI::ZzSplitPlacement::Before,
            requested);
        QCOMPARE(added, std::optional(requested));
        QCOMPARE(addedSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);

        const auto duplicate = workspace.splitGroup(
            root,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            requested);
        QVERIFY(!duplicate.has_value());
        QCOMPARE(workspace.groupIds().size(), 2);
        QCOMPARE(addedSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
    }

    void rejectsInvalidSplitPlacementWithoutMutation()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        QSignalSpy addedSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAdded);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);

        const auto added = workspace.splitGroup(
            root,
            Qt::Horizontal,
            static_cast<ZzFluentUI::ZzSplitPlacement>(255));

        QVERIFY(!added.has_value());
        QCOMPARE(workspace.groupIds().size(), 1);
        QCOMPARE(addedSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);
    }

    void generatesCanonicalGroupIdAndKeepsOneTabWidgetPerLeaf()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        QVERIFY(root.isValid());
        QVERIFY(!root.value().startsWith(u'{'));
        QVERIFY(!root.value().endsWith(u'}'));
        QCOMPARE(root.value().size(), 36);

        const auto second = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(second.has_value());
        QCOMPARE(workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(), 2);

        for (const auto &id : workspace.groupIds()) {
            auto *tabs = workspace.tabWidget(id);
            QVERIFY(tabs != nullptr);
            QCOMPARE(workspace.groupId(tabs), id);
            QAccessibleInterface *interface =
                QAccessible::queryAccessibleInterface(tabs->fluentTabBar());
            QVERIFY(interface != nullptr);
            QCOMPARE(interface->role(), QAccessible::PageTabList);
        }
    }

    void enforcesSixtyFourGroupLimit()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        for (int groupCount = 2; groupCount <= 64; ++groupCount) {
            QVERIFY(workspace.splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value());
        }
        QCOMPARE(workspace.groupIds().size(), 64);
        QVERIFY(!workspace.splitGroup(
            root,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After).has_value());
        QCOMPARE(workspace.groupIds().size(), 64);
    }

    void enforcesSixteenLevelDepthLimit()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        auto source = workspace.groupIds().constFirst();
        for (int split = 1; split < 16; ++split) {
            const auto added = workspace.splitGroup(
                source,
                split % 2 == 0 ? Qt::Horizontal : Qt::Vertical,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY2(added.has_value(), "the first fifteen nested splits must fit");
            source = added.value();
        }

        QVERIFY(!workspace.splitGroup(
            source,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After).has_value());
        QCOMPARE(workspace.groupIds().size(), 16);
    }

    void removesOnlyEmptyNonFinalGroupsAndEmitsAfterCommit()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const auto empty = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(empty.has_value());
        workspace.tabWidget(empty.value())->addTab(
            new QWidget, QStringLiteral("Occupied"));

        QSignalSpy removingSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAboutToBeRemoved);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
        QVERIFY(!workspace.removeEmptyGroup(empty.value()));
        QCOMPARE(removingSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);

        delete workspace.tabWidget(empty.value())->widget(0);
        QVERIFY(workspace.removeEmptyGroup(empty.value()));
        QCOMPARE(removingSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
        QVERIFY(workspace.tabWidget(empty.value()) == nullptr);
        QVERIFY(!workspace.removeEmptyGroup(root));
        QCOMPARE(removingSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
    }

    void keepsActiveGroupIdempotentAndTracksFocus()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const auto right = workspace.splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(right.has_value());
        QSignalSpy activeSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::activeGroupChanged);

        QCOMPARE(workspace.activeGroupId(), root);
        QVERIFY(workspace.setActiveGroup(root));
        QCOMPARE(activeSpy.size(), 0);
        QVERIFY(workspace.setActiveGroup(right.value()));
        QCOMPARE(workspace.activeGroupId(), right.value());
        QCOMPARE(activeSpy.size(), 1);
        QVERIFY(workspace.setActiveGroup(right.value()));
        QCOMPARE(activeSpy.size(), 1);
        QVERIFY(!workspace.setActiveGroup(
            ZzFluentUI::ZzTabGroupId(QStringLiteral("missing"))));
        QCOMPARE(activeSpy.size(), 1);

        workspace.resize(800, 400);
        workspace.show();
        workspace.tabWidget(root)->setFocus(Qt::OtherFocusReason);
        QTRY_COMPARE(workspace.activeGroupId(), root);
        QCOMPARE(activeSpy.size(), 2);
    }

    void focusesPhysicalAdjacentGroupInLtrAndRtl()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto left = workspace.groupIds().constFirst();
        const auto right = workspace.splitGroup(
            left, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(right.has_value());
        workspace.resize(800, 400);
        workspace.show();
        QCoreApplication::processEvents();

        QVERIFY(workspace.setActiveGroup(left));
        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), right.value());
        QVERIFY(zzFocusBelongsTo(
            QApplication::focusWidget(), workspace.tabWidget(right.value())));
        QWidget *const boundaryFocus = QApplication::focusWidget();
        QVERIFY(!workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), right.value());
        QCOMPARE(QApplication::focusWidget(), boundaryFocus);

        workspace.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        const auto ids = workspace.groupIds();
        QVERIFY(ids.size() == 2);
        ZzFluentUI::ZzTabGroupId physicalLeft;
        ZzFluentUI::ZzTabGroupId physicalRight;
        for (const auto &id : ids) {
            const int centerX = workspace.tabWidget(id)
                                    ->mapToGlobal(
                                        workspace.tabWidget(id)->rect().center())
                                    .x();
            if (!physicalLeft.isValid()
                || centerX < workspace.tabWidget(physicalLeft)
                                   ->mapToGlobal(
                                       workspace.tabWidget(physicalLeft)
                                           ->rect().center())
                                   .x()) {
                physicalRight = physicalLeft;
                physicalLeft = id;
            } else {
                physicalRight = id;
            }
        }
        QVERIFY(physicalLeft.isValid());
        QVERIFY(physicalRight.isValid());
        QVERIFY(workspace.setActiveGroup(physicalLeft));
        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), physicalRight);
        QVERIFY(zzFocusBelongsTo(
            QApplication::focusWidget(), workspace.tabWidget(physicalRight)));
    }

    void adjacentFocusPrioritizesPrimaryAxisDistance()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto ids = zzCreateFourHorizontalGroups(workspace);
        QCOMPARE(ids.size(), 4);
        const auto active = ids.at(0);
        const auto fartherPrimary = ids.at(1);
        const auto nearestPrimary = ids.at(2);
        const auto bestSecondaryOnly = ids.at(3);
        QVERIFY(workspace.setActiveGroup(active));

        zzSetGroupCenter(workspace, active, QPoint(100, 400));
        zzSetGroupCenter(workspace, fartherPrimary, QPoint(400, 400));
        zzSetGroupCenter(workspace, nearestPrimary, QPoint(300, 750));
        zzSetGroupCenter(workspace, bestSecondaryOnly, QPoint(350, 400));

        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), nearestPrimary);
    }

    void adjacentFocusUsesSecondaryDistanceWhenPrimaryTies()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto ids = zzCreateFourHorizontalGroups(workspace);
        QCOMPARE(ids.size(), 4);
        const auto active = ids.at(0);
        const auto fartherSecondary = ids.at(1);
        const auto nearestSecondary = ids.at(2);
        const auto fartherPrimary = ids.at(3);
        QVERIFY(workspace.setActiveGroup(active));

        zzSetGroupCenter(workspace, active, QPoint(100, 400));
        zzSetGroupCenter(workspace, fartherSecondary, QPoint(300, 100));
        zzSetGroupCenter(workspace, nearestSecondary, QPoint(300, 350));
        zzSetGroupCenter(workspace, fartherPrimary, QPoint(350, 400));

        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), nearestSecondary);
    }

    void adjacentFocusUsesStableOrderWhenDistancesTie()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto ids = zzCreateFourHorizontalGroups(workspace);
        QCOMPARE(ids.size(), 4);
        const auto active = ids.at(0);
        const auto firstStableCandidate = ids.at(1);
        const auto secondStableCandidate = ids.at(2);
        const auto fartherPrimary = ids.at(3);
        QVERIFY(workspace.setActiveGroup(active));

        zzSetGroupCenter(workspace, active, QPoint(100, 400));
        zzSetGroupCenter(workspace, firstStableCandidate, QPoint(300, 350));
        zzSetGroupCenter(workspace, secondStableCandidate, QPoint(300, 350));
        zzSetGroupCenter(workspace, fartherPrimary, QPoint(350, 400));

        QVERIFY(workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), firstStableCandidate);
    }

    void survivesOneThousandSplitRemoveCyclesWithoutObjectGrowth()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto root = workspace.groupIds().constFirst();
        const qsizetype timerCount = workspace.findChildren<QTimer *>().size();
        const qsizetype animationCount =
            workspace.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            const auto added = workspace.splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After);
            QVERIFY(added.has_value());
            QVERIFY(workspace.removeEmptyGroup(added.value()));
        }

        QCOMPARE(workspace.groupIds().size(), 1);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 0);
        QCOMPARE(workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(), 1);
        QCOMPARE(workspace.findChildren<QTimer *>().size(), timerCount);
        QCOMPARE(
            workspace.findChildren<QAbstractAnimation *>().size(),
            animationCount);
    }

    void signalHandlersMayDeleteWorkspace()
    {
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto root = rawWorkspace->groupIds().constFirst();
        connect(
            rawWorkspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAdded,
            rawWorkspace,
            [rawWorkspace] { delete rawWorkspace; });

        const auto added = rawWorkspace->splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(added.has_value());
        QVERIFY(workspace.isNull());
    }

    void splitterDestructionMayDeleteWorkspace()
    {
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto root = rawWorkspace->groupIds().constFirst();
        QVERIFY(rawWorkspace->splitGroup(
            root,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After).has_value());
        auto *splitter = rawWorkspace->findChild<QSplitter *>();
        QVERIFY(splitter != nullptr);
        connect(
            splitter,
            &QObject::destroyed,
            rawWorkspace,
            [rawWorkspace] { delete rawWorkspace; });

        const auto added = rawWorkspace->splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(added.has_value());
        QVERIFY(workspace.isNull());
    }

    void removalSignalHandlerMayDeleteWorkspace()
    {
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto root = rawWorkspace->groupIds().constFirst();
        const auto added = rawWorkspace->splitGroup(
            root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(added.has_value());
        connect(
            rawWorkspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAboutToBeRemoved,
            rawWorkspace,
            [rawWorkspace] { delete rawWorkspace; });

        QVERIFY(rawWorkspace->removeEmptyGroup(added.value()));
        QVERIFY(workspace.isNull());
    }

};

QTEST_MAIN(ZzSplitWorkspaceTest)

#include "ZzSplitWorkspaceTest.moc"
