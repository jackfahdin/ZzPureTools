#include <functional>
#include <optional>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QEvent>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QDrag>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLayout>
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

ZzFluentUI::ZzTabGroupId zzTabGroupIdOrInvalid(
    const std::optional<ZzFluentUI::ZzTabGroupId> &groupId)
{
    return groupId.value_or(ZzFluentUI::ZzTabGroupId{});
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

class ZzTestEventFilter final : public QObject {
public:
  std::function<bool(QObject *, QEvent *)> callback;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    return callback ? callback(watched, event) : false;
  }
};

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

    void clearsLayoutKeysWhenLivePagesLeaveWorkspace() {
      ZzFluentUI::ZzSplitWorkspace workspace;
      ZzFluentUI::ZzTabWidget thirdParty;
      const auto groupId = workspace.groupIds().constFirst();
      auto *removedPage = new QWidget;
      auto *replacementPage = new QWidget;
      auto *claimedPage = new QWidget;
      auto *claimedReplacement = new QWidget;
      auto *const tabs = workspace.tabWidget(groupId);
      tabs->addTab(removedPage, QStringLiteral("Removed"));
      tabs->addTab(replacementPage, QStringLiteral("Replacement"));
      tabs->addTab(claimedPage, QStringLiteral("Claimed"));
      tabs->addTab(claimedReplacement, QStringLiteral("Claimed replacement"));

      QVERIFY(workspace.setPageLayoutKey(removedPage,
                                         QStringLiteral("reusable:removed")));
      tabs->removeTab(tabs->indexOf(removedPage));
      QVERIFY(workspace.pageLayoutKey(removedPage).isEmpty());
      QVERIFY(workspace.setPageLayoutKey(replacementPage,
                                         QStringLiteral("reusable:removed")));

      QVERIFY(workspace.setPageLayoutKey(claimedPage,
                                         QStringLiteral("reusable:claimed")));
      QVERIFY(tabs->transferTabTo(&thirdParty, tabs->indexOf(claimedPage)));
      QCOMPARE(thirdParty.indexOf(claimedPage), 0);
      QVERIFY(workspace.pageLayoutKey(claimedPage).isEmpty());
      QVERIFY(workspace.setPageLayoutKey(claimedReplacement,
                                         QStringLiteral("reusable:claimed")));
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

    // Qt 页面所有权由 TabWidget 接管，分析器无法沿 QObject 父子关系追踪释放。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void preservesMissingPageSlotsAcrossRepeatedSaves()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<QWidget> missingPage = new QWidget;
        auto *livePage = new QWidget;
        workspace.tabWidget(groupId)->addTab(
            missingPage, QStringLiteral("Missing first"));
        workspace.tabWidget(groupId)->addTab(
            livePage, QStringLiteral("Live second"));
        QVERIFY(workspace.setPageLayoutKey(
            missingPage, QStringLiteral("missing:first")));
        QVERIFY(workspace.setPageLayoutKey(
            livePage, QStringLiteral("live:second")));
        const QByteArray original = workspace.saveLayout();
        QVERIFY(!original.isEmpty());

        delete missingPage.data();
        QVERIFY(missingPage.isNull());
        QVERIFY(workspace.restoreLayout(original));
        QCOMPARE(workspace.tabWidget(groupId)->indexOf(livePage), 0);

        const QByteArray repeated = workspace.saveLayout();
        QVERIFY(!repeated.isEmpty());
        QVERIFY(workspace.restoreLayout(repeated));
        QCOMPARE(workspace.tabWidget(groupId)->indexOf(livePage), 0);
        QCOMPARE(
            workspace.savedGroupForPageKey(QStringLiteral("missing:first")),
            groupId);
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

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
            QStringLiteral("only"),
            {{QStringLiteral("first-order"),
              QStringLiteral("only"),
              7,
              false},
             {QStringLiteral("duplicate-order"),
              QStringLiteral("only"),
              7,
              false}}));
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
        bool metadataSignalLeaked = false;
        bool connectedStaging = false;
        connect(
            originalTabs,
            &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
            &workspace,
            [&](int, bool) {
                if (removedFromStaging) {
                    metadataSignalLeaked = true;
                }
            });
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
                            page->setParent(nullptr);
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(sourceLayout));
        QVERIFY(removedFromStaging);
        QVERIFY(!metadataSignalLeaked);
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

    void rollbackRechecksOrderAfterTabMovedCallbacks()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *firstPage = new QWidget;
        auto *secondPage = new QWidget;
        auto *thirdPage = new QWidget;
        workspace.tabWidget(groupId)->addTab(
            firstPage, QStringLiteral("First"));
        workspace.tabWidget(groupId)->addTab(
            secondPage, QStringLiteral("Second"));
        workspace.tabWidget(groupId)->addTab(
            thirdPage, QStringLiteral("Third"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("order-rollback:first")));
        const QByteArray saved = workspace.saveLayout();

        auto *const originalTabs = workspace.tabWidget(groupId);
        originalTabs->setCurrentWidget(firstPage);
        originalTabs->setTabCloseEnabled(
            originalTabs->indexOf(thirdPage), false);

        bool connectedStaging = false;
        bool removedFromStaging = false;
        bool rollbackArmed = false;
        bool tabMovedLeaked = false;
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
                        [&, tabs](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int,
                            QWidget *page) {
                            if (removedFromStaging || page != firstPage) {
                                return;
                            }
                            const int thirdIndex =
                                originalTabs->indexOf(thirdPage);
                            const QIcon thirdIcon =
                                originalTabs->tabIcon(thirdIndex);
                            const QString thirdText =
                                originalTabs->tabText(thirdIndex);
                            originalTabs->removeTab(thirdIndex);
                            QCOMPARE(
                                originalTabs->insertTab(
                                    0, thirdPage, thirdIcon, thirdText),
                                0);
                            originalTabs->setTabCloseEnabled(0, false);
                            QCOMPARE(originalTabs->indexOf(thirdPage), 0);
                            QCOMPARE(originalTabs->indexOf(secondPage), 1);
                            rollbackArmed = true;
                            removedFromStaging = true;
                            tabs->removeTab(tabs->indexOf(page));
                            page->setParent(nullptr);
                        });
                }
            });
        connect(
            originalTabs->fluentTabBar(),
            &QTabBar::tabMoved,
            &workspace,
            [&](int, int) {
                if (rollbackArmed) {
                    tabMovedLeaked = true;
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(removedFromStaging);
        QVERIFY(rollbackArmed);
        QVERIFY(!tabMovedLeaked);
        QCOMPARE(originalTabs->indexOf(firstPage), 0);
        QCOMPARE(originalTabs->indexOf(secondPage), 1);
        QCOMPARE(originalTabs->indexOf(thirdPage), 2);
        QCOMPARE(originalTabs->currentWidget(), firstPage);
        QVERIFY(!originalTabs->isTabCloseEnabled(2));
    }

    void rollbackRechecksCurrentPageAfterCurrentChangedCallbacks()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto sourceId = workspace.groupIds().constFirst();
        const ZzFluentUI::ZzTabGroupId targetId(
            QStringLiteral("current-rollback-target"));
        QVERIFY(workspace.splitGroup(
            sourceId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After,
            targetId).has_value());
        auto *firstPage = new QWidget;
        auto *secondPage = new QWidget;
        auto *thirdPage = new QWidget;
        workspace.tabWidget(sourceId)->addTab(
            firstPage, QStringLiteral("First"));
        workspace.tabWidget(sourceId)->addTab(
            secondPage, QStringLiteral("Second"));
        workspace.tabWidget(sourceId)->addTab(
            thirdPage, QStringLiteral("Third"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("current-rollback:first")));
        const QByteArray sourceLayout = workspace.saveLayout();

        while (workspace.tabWidget(sourceId)->count() > 0) {
            QVERIFY(workspace.transferTab(sourceId, 0, targetId));
        }
        QVERIFY(workspace.removeEmptyGroup(sourceId));
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(targetId);
        originalTabs->setCurrentWidget(firstPage);
        originalTabs->setTabCloseEnabled(
            originalTabs->indexOf(thirdPage), false);

        bool connectedStaging = false;
        bool removedFromStaging = false;
        bool rollbackArmed = false;
        bool currentSignalLeaked = false;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (!connectedStaging) {
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
                            [&, tabs](
                                ZzFluentUI::ZzTabWidget *,
                                int,
                                int,
                                QWidget *page) {
                                if (page == thirdPage) {
                                    tabs->setTabCloseEnabled(
                                        tabs->indexOf(page), true);
                                    rollbackArmed = true;
                                }
                                if (removedFromStaging
                                    || page != firstPage) {
                                    return;
                                }
                                removedFromStaging = true;
                                tabs->removeTab(tabs->indexOf(page));
                            });
                    }
                }
                if (rollbackArmed && !currentSignalLeaked) {
                    currentSignalLeaked = true;
                    delete originalTabs.data();
                }
            });

        QVERIFY(!workspace.restoreLayout(sourceLayout));
        QVERIFY(removedFromStaging);
        QVERIFY(rollbackArmed);
        QVERIFY(!currentSignalLeaked);
        QVERIFY(!originalTabs.isNull());
        QCOMPARE(originalTabs->indexOf(firstPage), 0);
        QCOMPARE(originalTabs->indexOf(secondPage), 1);
        QCOMPARE(originalTabs->indexOf(thirdPage), 2);
        QCOMPARE(originalTabs->currentWidget(), firstPage);
        QVERIFY(!originalTabs->isTabCloseEnabled(2));
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
            QVERIFY(workspace.transferTab(sourceId, 0,
                                          zzTabGroupIdOrInvalid(targetId)));
            auto *const originalTabs =
                workspace.tabWidget(zzTabGroupIdOrInvalid(targetId));
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
            QPointer<QWidget> page = new QWidget;
            workspace.tabWidget(sourceId)->addTab(
                page, QStringLiteral("Deleted source"));
            QVERIFY(workspace.setPageLayoutKey(
                page, QStringLiteral("deleted-source:key")));
            const QByteArray saved = workspace.saveLayout();
            QVERIFY(workspace.transferTab(sourceId, 0,
                                          zzTabGroupIdOrInvalid(targetId)));
            QPointer<ZzFluentUI::ZzTabWidget> deletedSource =
                workspace.tabWidget(zzTabGroupIdOrInvalid(targetId));
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
            QVERIFY(!page.isNull());
            ZzFluentUI::ZzTabWidget *recoveryOwner = nullptr;
            for (const auto &id : workspace.groupIds()) {
                auto *const tabs = workspace.tabWidget(id);
                QVERIFY(tabs != nullptr);
                if (tabs->indexOf(page) >= 0) {
                    recoveryOwner = tabs;
                }
            }
            QVERIFY(recoveryOwner != nullptr);
            QCOMPARE(
                workspace.pageLayoutKey(page),
                QStringLiteral("deleted-source:key"));
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
            QVERIFY(rawWorkspace->transferTab(sourceId, 0,
                                              zzTabGroupIdOrInvalid(targetId)));
            auto *const originalTabs =
                rawWorkspace->tabWidget(zzTabGroupIdOrInvalid(targetId));
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

    void restoreRollbackSweepsPastDeletedSnapshotPage()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(groupId);
        QPointer<QWidget> firstPage = new QWidget;
        QPointer<QWidget> secondPage = new QWidget;
        originalTabs->addTab(firstPage, QStringLiteral("Deleted first"));
        originalTabs->addTab(secondPage, QStringLiteral("Surviving second"));
        originalTabs->setTabToolTip(
            1, QStringLiteral("Surviving second tooltip"));
        originalTabs->setTabModified(1, true);
        originalTabs->setTabCloseEnabled(1, false);
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool sawFirstTransfer = false;
        bool deletedFirst = false;
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
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&](ZzFluentUI::ZzTabWidget *,
                            int,
                            int,
                            QWidget *page) {
                            if (page == firstPage) {
                                sawFirstTransfer = true;
                                return;
                            }
                            if (page == secondPage && sawFirstTransfer
                                && !deletedFirst) {
                                deletedFirst = true;
                                delete firstPage.data();
                            }
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(sawFirstTransfer);
        QVERIFY(deletedFirst);
        QVERIFY(firstPage.isNull());
        QVERIFY(!secondPage.isNull());
        QVERIFY(!originalTabs.isNull());
        QCOMPARE(workspace.tabWidget(groupId), originalTabs.data());
        QCOMPARE(originalTabs->count(), 1);
        QCOMPARE(originalTabs->indexOf(secondPage), 0);
        QCOMPARE(
            originalTabs->tabText(0), QStringLiteral("Surviving second"));
        QCOMPARE(
            originalTabs->tabToolTip(0),
            QStringLiteral("Surviving second tooltip"));
        QVERIFY(originalTabs->isTabModified(0));
        QVERIFY(!originalTabs->isTabCloseEnabled(0));
    }

    void restoreRollbackPreservesPageAddedToStagedTarget()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> addedPage;
        originalTabs->addTab(capturedPage, QStringLiteral("Captured page"));
        QVERIFY(workspace.setPageLayoutKey(
            capturedPage, QStringLiteral("added-during-restore:captured")));
        const QByteArray saved = workspace.saveLayout();
        const qsizetype tabWidgetBudget = workspace.findChildren<
            ZzFluentUI::ZzTabWidget *>().size();

        bool connectedStaging = false;
        bool addedToStaging = false;
        bool addedKeySet = false;
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
                        [&, tabs](ZzFluentUI::ZzTabWidget *,
                                  int,
                                  int,
                                  QWidget *page) {
                            if (addedToStaging || page != capturedPage) {
                                return;
                            }
                            addedToStaging = true;
                            addedPage = new QWidget;
                            const int index = tabs->addTab(
                                addedPage, QStringLiteral("Added page"));
                            tabs->setTabToolTip(
                                index, QStringLiteral("Added tooltip"));
                            tabs->setTabWhatsThis(
                                index, QStringLiteral("Added help"));
                            tabs->fluentTabBar()->setTabData(
                                index, QStringLiteral("added-data"));
                            tabs->fluentTabBar()->setTabTextColor(
                                index, QColor(Qt::magenta));
                            tabs->setTabEnabled(index, false);
                            tabs->setTabPinned(index, true);
                            const int pinnedIndex = tabs->indexOf(addedPage);
                            tabs->setTabModified(pinnedIndex, true);
                            tabs->setTabAttention(pinnedIndex, true);
                            tabs->setTabCloseEnabled(pinnedIndex, false);
                            addedKeySet = workspace.setPageLayoutKey(
                                addedPage,
                                QStringLiteral("added-during-restore:new"));
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(addedToStaging);
        QVERIFY(addedKeySet);
        QVERIFY(!capturedPage.isNull());
        QVERIFY(!addedPage.isNull());
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs, originalTabs);
        QCOMPARE(restoredTabs->currentWidget(), capturedPage.data());
        QCOMPARE(restoredTabs->indexOf(addedPage), 0);
        QCOMPARE(restoredTabs->indexOf(capturedPage), 1);
        QCOMPARE(restoredTabs->tabText(0), QStringLiteral("Added page"));
        QCOMPARE(
            restoredTabs->tabToolTip(0), QStringLiteral("Added tooltip"));
        QCOMPARE(
            restoredTabs->tabWhatsThis(0), QStringLiteral("Added help"));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabData(0),
            QVariant(QStringLiteral("added-data")));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabTextColor(0),
            QColor(Qt::magenta));
        QVERIFY(!restoredTabs->isTabEnabled(0));
        QVERIFY(restoredTabs->isTabPinned(0));
        QVERIFY(!restoredTabs->isTabPinned(1));
        QVERIFY(restoredTabs->isTabModified(0));
        QVERIFY(restoredTabs->hasTabAttention(0));
        QVERIFY(!restoredTabs->isTabCloseEnabled(0));
        QCOMPARE(
            workspace.pageLayoutKey(addedPage),
            QStringLiteral("added-during-restore:new"));
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            tabWidgetBudget);
    }

    void restoreCommitPreservesPageAddedToOriginalSource()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> addedPage;
        originalTabs->addTab(capturedPage, QStringLiteral("Captured page"));
        const QString capturedKey = QStringLiteral("source-add:captured");
        const QString addedKey = QStringLiteral("source-add:new");
        QVERIFY(workspace.setPageLayoutKey(capturedPage, capturedKey));
        const QByteArray saved = workspace.saveLayout();
        const qsizetype tabWidgetBudget = workspace.findChildren<
            ZzFluentUI::ZzTabWidget *>().size();
        const qsizetype widgetBudget =
            workspace.findChildren<QWidget *>().size();
        QPixmap addedPixmap(4, 4);
        addedPixmap.fill(Qt::green);
        const QIcon addedIcon(addedPixmap);

        bool connectedStaging = false;
        bool addedToSource = false;
        bool addedKeySet = false;
        bool cleanupObserved = false;
        bool metadataPerturbed = false;
        bool deletionAttempted = false;
        ZzTestEventFilter cleanupFilter;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (connectedStaging) {
                    return;
                }
                connectedStaging = true;
                QWidget *rootHost = originalTabs.data();
                while (rootHost != nullptr
                       && rootHost->parentWidget() != &workspace) {
                    rootHost = rootHost->parentWidget();
                }
                QVERIFY(rootHost != nullptr);
                cleanupFilter.callback = [&](QObject *, QEvent *event) {
                    if (cleanupObserved
                        || event->type() != QEvent::ChildRemoved
                        || addedPage.isNull()) {
                        return false;
                    }
                    cleanupObserved = true;
                    QWidget *ancestor = addedPage->parentWidget();
                    while (ancestor != nullptr
                           && qobject_cast<ZzFluentUI::ZzTabWidget *>(ancestor)
                               == nullptr) {
                        ancestor = ancestor->parentWidget();
                    }
                    auto *const escrow =
                        qobject_cast<ZzFluentUI::ZzTabWidget *>(ancestor);
                    if (escrow == nullptr) {
                        return false;
                    }
                    const int preservedIndex = escrow->indexOf(addedPage);
                    if (preservedIndex < 0) {
                        return false;
                    }
                    connect(
                        escrow,
                        &ZzFluentUI::ZzTabWidget::tabModifiedChanged,
                        &workspace,
                        [&, escrow](int index, bool modified) {
                            if (deletionAttempted || !modified
                                || escrow->widget(index) != addedPage) {
                                return;
                            }
                            deletionAttempted = true;
                            delete workspace.tabWidget(groupId);
                        });
                    metadataPerturbed = true;
                    escrow->setTabModified(preservedIndex, false);
                    return false;
                };
                rootHost->installEventFilter(&cleanupFilter);
                const auto allTabs = workspace.findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&](ZzFluentUI::ZzTabWidget *source,
                            int,
                            int,
                            QWidget *transferredPage) {
                            if (addedToSource
                                || source != originalTabs.data()
                                || transferredPage != capturedPage) {
                                return;
                            }
                            addedToSource = true;
                            addedPage = new QWidget;
                            const int index = source->addTab(
                                addedPage,
                                addedIcon,
                                QStringLiteral("Added source page"));
                            source->setTabToolTip(
                                index, QStringLiteral("Added tooltip"));
                            source->setTabWhatsThis(
                                index, QStringLiteral("Added help"));
                            source->fluentTabBar()->setTabData(
                                index, QStringLiteral("added-data"));
                            source->fluentTabBar()->setTabTextColor(
                                index, QColor(Qt::magenta));
                            source->setTabEnabled(index, false);
                            source->setTabPinned(index, true);
                            const int pinnedIndex = source->indexOf(addedPage);
                            source->setTabModified(pinnedIndex, true);
                            source->setTabAttention(pinnedIndex, true);
                            source->setTabCloseEnabled(pinnedIndex, false);
                            addedKeySet = workspace.setPageLayoutKey(
                                addedPage, addedKey);
                        });
                }
            });

        const bool restored = workspace.restoreLayout(saved);
        QVERIFY(restored);
        QVERIFY(connectedStaging);
        QVERIFY(addedToSource);
        QVERIFY(addedKeySet);
        QVERIFY(cleanupObserved);
        QVERIFY(metadataPerturbed);
        QVERIFY(originalTabs.isNull());
        QVERIFY(!capturedPage.isNull());
        QVERIFY(!addedPage.isNull());
        QVERIFY(!deletionAttempted);
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs->indexOf(addedPage), 0);
        QCOMPARE(restoredTabs->indexOf(capturedPage), 1);
        QCOMPARE(restoredTabs->currentWidget(), capturedPage.data());
        QCOMPARE(
            restoredTabs->tabText(0), QStringLiteral("Added source page"));
        QCOMPARE(
            restoredTabs->tabIcon(0).cacheKey(), addedIcon.cacheKey());
        QCOMPARE(
            restoredTabs->tabToolTip(0), QStringLiteral("Added tooltip"));
        QCOMPARE(
            restoredTabs->tabWhatsThis(0), QStringLiteral("Added help"));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabData(0),
            QVariant(QStringLiteral("added-data")));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabTextColor(0),
            QColor(Qt::magenta));
        QVERIFY(!restoredTabs->isTabEnabled(0));
        QVERIFY(restoredTabs->isTabPinned(0));
        QVERIFY(restoredTabs->isTabModified(0));
        QVERIFY(restoredTabs->hasTabAttention(0));
        QVERIFY(!restoredTabs->isTabCloseEnabled(0));
        QCOMPARE(workspace.pageLayoutKey(capturedPage), capturedKey);
        QCOMPARE(workspace.pageLayoutKey(addedPage), addedKey);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            tabWidgetBudget);
        QCOMPARE(
            workspace.findChildren<QWidget *>().size(), widgetBudget + 1);
    }

    void restoreCleanupEscrowOrphanIsRefilled_data()
    {
        QTest::addColumn<int>("orphanKind");
        QTest::newRow("captured") << 0;
        QTest::newRow("preserved") << 1;
        QTest::newRow("unknown") << 2;
    }

    void restoreCleanupEscrowOrphanIsRefilled()
    {
        QFETCH(int, orphanKind);

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> preservedPage;
        QPointer<QWidget> unknownPage;
        QPixmap capturedPixmap(4, 4);
        capturedPixmap.fill(Qt::blue);
        const QIcon capturedIcon(capturedPixmap);
        const int capturedIndex = originalTabs->addTab(
            capturedPage, capturedIcon, QStringLiteral("Captured orphan"));
        originalTabs->setTabToolTip(
            capturedIndex, QStringLiteral("Captured tooltip"));
        originalTabs->setTabWhatsThis(
            capturedIndex, QStringLiteral("Captured help"));
        originalTabs->fluentTabBar()->setTabData(
            capturedIndex, QStringLiteral("captured-data"));
        originalTabs->fluentTabBar()->setTabTextColor(
            capturedIndex, QColor(Qt::red));
        originalTabs->setTabModified(capturedIndex, true);
        originalTabs->setTabAttention(capturedIndex, true);
        originalTabs->setTabCloseEnabled(capturedIndex, false);
        const QString capturedKey = QStringLiteral("cleanup-orphan:captured");
        const QString preservedKey = QStringLiteral("cleanup-orphan:preserved");
        QVERIFY(workspace.setPageLayoutKey(capturedPage, capturedKey));
        const QByteArray saved = workspace.saveLayout();
        const qsizetype tabWidgetBudget = workspace.findChildren<
            ZzFluentUI::ZzTabWidget *>().size();
        const qsizetype widgetBudget =
            workspace.findChildren<QWidget *>().size();
        QPixmap preservedPixmap(4, 4);
        preservedPixmap.fill(Qt::green);
        const QIcon preservedIcon(preservedPixmap);

        bool connectedStaging = false;
        bool addedToSource = false;
        bool preservedKeySet = false;
        bool cleanupObserved = false;
        bool escrowObserved = false;
        bool orphanRemoved = false;
        bool orphanStillOwnedByEscrow = false;
        ZzTestEventFilter cleanupFilter;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (connectedStaging) {
                    return;
                }
                connectedStaging = true;
                QWidget *rootHost = originalTabs.data();
                while (rootHost != nullptr
                       && rootHost->parentWidget() != &workspace) {
                    rootHost = rootHost->parentWidget();
                }
                QVERIFY(rootHost != nullptr);
                cleanupFilter.callback = [&](QObject *, QEvent *event) {
                    if (cleanupObserved
                        || event->type() != QEvent::ChildRemoved
                        || preservedPage.isNull()) {
                        return false;
                    }
                    cleanupObserved = true;
                    const QPointer<QWidget> ownershipProbe =
                        orphanKind == 1 ? preservedPage : capturedPage;
                    QWidget *ancestor = ownershipProbe->parentWidget();
                    while (ancestor != nullptr
                           && qobject_cast<ZzFluentUI::ZzTabWidget *>(ancestor)
                               == nullptr) {
                        ancestor = ancestor->parentWidget();
                    }
                    auto *const escrow =
                        qobject_cast<ZzFluentUI::ZzTabWidget *>(ancestor);
                    escrowObserved = escrow != nullptr
                        && escrow != originalTabs.data()
                        && escrow->parentWidget() == nullptr
                        && escrow->indexOf(capturedPage) >= 0
                        && escrow->indexOf(preservedPage) >= 0;
                    if (!escrowObserved) {
                        return false;
                    }
                    if (orphanKind == 2) {
                        unknownPage = new QWidget;
                        unknownPage->setWindowTitle(
                            QStringLiteral("Unknown escrow orphan"));
                        escrow->addTab(
                            unknownPage,
                            QStringLiteral("Unknown escrow orphan"));
                    }
                    const QPointer<QWidget> orphan = orphanKind == 0
                        ? capturedPage
                        : orphanKind == 1 ? preservedPage : unknownPage;
                    escrow->removeTab(escrow->indexOf(orphan));
                    orphanRemoved = !orphan.isNull()
                        && escrow->indexOf(orphan) < 0;
                    ancestor = orphan.isNull() ? nullptr
                                               : orphan->parentWidget();
                    while (ancestor != nullptr
                           && qobject_cast<ZzFluentUI::ZzTabWidget *>(ancestor)
                               == nullptr) {
                        ancestor = ancestor->parentWidget();
                    }
                    orphanStillOwnedByEscrow = ancestor == escrow;
                    return false;
                };
                rootHost->installEventFilter(&cleanupFilter);
                const auto allTabs = workspace.findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&](ZzFluentUI::ZzTabWidget *source,
                            int,
                            int,
                            QWidget *page) {
                            if (addedToSource || source != originalTabs.data()
                                || page != capturedPage) {
                                return;
                            }
                            addedToSource = true;
                            preservedPage = new QWidget;
                            int index = source->addTab(
                                preservedPage,
                                preservedIcon,
                                QStringLiteral("Preserved orphan"));
                            source->setTabToolTip(
                                index, QStringLiteral("Preserved tooltip"));
                            source->setTabWhatsThis(
                                index, QStringLiteral("Preserved help"));
                            source->fluentTabBar()->setTabData(
                                index, QStringLiteral("preserved-data"));
                            source->fluentTabBar()->setTabTextColor(
                                index, QColor(Qt::magenta));
                            source->setTabEnabled(index, false);
                            source->setTabPinned(index, true);
                            index = source->indexOf(preservedPage);
                            source->setTabModified(index, true);
                            source->setTabAttention(index, true);
                            source->setTabCloseEnabled(index, false);
                            preservedKeySet = workspace.setPageLayoutKey(
                                preservedPage, preservedKey);
                        });
                }
            });

        const bool restored = workspace.restoreLayout(saved);
        QVERIFY(connectedStaging);
        QVERIFY(addedToSource);
        QVERIFY(preservedKeySet);
        QVERIFY(cleanupObserved);
        QVERIFY(escrowObserved);
        QVERIFY(orphanRemoved);
        QVERIFY(orphanStillOwnedByEscrow);
        const QString outcome = QStringLiteral(
            "restored=%1 capturedAlive=%2 preservedAlive=%3 unknownAlive=%4")
                                    .arg(restored)
                                    .arg(!capturedPage.isNull())
                                    .arg(!preservedPage.isNull())
                                    .arg(!unknownPage.isNull());
        QVERIFY2(restored && !capturedPage.isNull()
                     && !preservedPage.isNull()
                     && (orphanKind != 2 || !unknownPage.isNull()),
                 qPrintable(outcome));
        QVERIFY(originalTabs.isNull());
        QCOMPARE(workspace.groupIds(), QList {groupId});
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs->count(), orphanKind == 2 ? 3 : 2);
        QCOMPARE(restoredTabs->indexOf(preservedPage), 0);
        QCOMPARE(restoredTabs->indexOf(capturedPage), 1);
        QCOMPARE(restoredTabs->currentWidget(), capturedPage.data());
        QCOMPARE(restoredTabs->tabText(0), QStringLiteral("Preserved orphan"));
        QCOMPARE(restoredTabs->tabIcon(0).cacheKey(), preservedIcon.cacheKey());
        QCOMPARE(
            restoredTabs->tabToolTip(0), QStringLiteral("Preserved tooltip"));
        QCOMPARE(
            restoredTabs->tabWhatsThis(0), QStringLiteral("Preserved help"));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabData(0),
            QVariant(QStringLiteral("preserved-data")));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabTextColor(0),
            QColor(Qt::magenta));
        QVERIFY(!restoredTabs->isTabEnabled(0));
        QVERIFY(restoredTabs->isTabPinned(0));
        QVERIFY(restoredTabs->isTabModified(0));
        QVERIFY(restoredTabs->hasTabAttention(0));
        QVERIFY(!restoredTabs->isTabCloseEnabled(0));
        QCOMPARE(restoredTabs->tabText(1), QStringLiteral("Captured orphan"));
        QCOMPARE(restoredTabs->tabIcon(1).cacheKey(), capturedIcon.cacheKey());
        QCOMPARE(
            restoredTabs->tabToolTip(1), QStringLiteral("Captured tooltip"));
        QCOMPARE(
            restoredTabs->tabWhatsThis(1), QStringLiteral("Captured help"));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabData(1),
            QVariant(QStringLiteral("captured-data")));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabTextColor(1), QColor(Qt::red));
        QVERIFY(restoredTabs->isTabEnabled(1));
        QVERIFY(!restoredTabs->isTabPinned(1));
        QVERIFY(restoredTabs->isTabModified(1));
        QVERIFY(restoredTabs->hasTabAttention(1));
        QVERIFY(!restoredTabs->isTabCloseEnabled(1));
        QCOMPARE(workspace.pageLayoutKey(capturedPage), capturedKey);
        QCOMPARE(workspace.pageLayoutKey(preservedPage), preservedKey);
        if (orphanKind == 2) {
            QVERIFY(restoredTabs->indexOf(unknownPage) >= 0);
            QCOMPARE(
                restoredTabs->tabText(restoredTabs->indexOf(unknownPage)),
                QStringLiteral("Unknown escrow orphan"));
        }
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            tabWidgetBudget);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            workspace.groupIds().size());
        QCOMPARE(
            workspace.findChildren<QWidget *>().size(),
            widgetBudget + (orphanKind == 2 ? 2 : 1));
    }

    void restoreCommitPropagatesDestroyedPreservedPageRefillFailure()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> preservedPage;
        originalTabs->addTab(capturedPage, QStringLiteral("Captured survivor"));
        originalTabs->setTabToolTip(
            0, QStringLiteral("Captured survivor tooltip"));
        originalTabs->setTabPinned(0, true);
        originalTabs->setTabModified(0, true);
        const QString capturedKey = QStringLiteral("refill-fail:captured");
        QVERIFY(workspace.setPageLayoutKey(capturedPage, capturedKey));
        const QByteArray saved = workspace.saveLayout();
        const qsizetype tabWidgetBudget = workspace.findChildren<
            ZzFluentUI::ZzTabWidget *>().size();

        bool connectedStaging = false;
        bool addedToSource = false;
        bool cleanupObserved = false;
        ZzTestEventFilter cleanupFilter;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            [&](int) {
                if (connectedStaging) {
                    return;
                }
                connectedStaging = true;
                QWidget *rootHost = originalTabs.data();
                while (rootHost != nullptr
                       && rootHost->parentWidget() != &workspace) {
                    rootHost = rootHost->parentWidget();
                }
                QVERIFY(rootHost != nullptr);
                cleanupFilter.callback = [&](QObject *, QEvent *event) {
                    if (cleanupObserved
                        || event->type() != QEvent::ChildRemoved
                        || preservedPage.isNull()) {
                        return false;
                    }
                    cleanupObserved = true;
                    delete preservedPage.data();
                    return false;
                };
                rootHost->installEventFilter(&cleanupFilter);
                const auto allTabs = workspace.findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&](ZzFluentUI::ZzTabWidget *source,
                            int,
                            int,
                            QWidget *page) {
                            if (addedToSource || source != originalTabs.data()
                                || page != capturedPage) {
                                return;
                            }
                            addedToSource = true;
                            preservedPage = new QWidget;
                            source->addTab(
                                preservedPage,
                                QStringLiteral("Destroyed preserved page"));
                        });
                }
            });

        const bool restored = workspace.restoreLayout(saved);
        QVERIFY(connectedStaging);
        QVERIFY(addedToSource);
        QVERIFY(cleanupObserved);
        QVERIFY(preservedPage.isNull());
        QVERIFY(!restored);
        QVERIFY(!capturedPage.isNull());
        QCOMPARE(workspace.groupIds(), QList {groupId});
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs->count(), 1);
        QCOMPARE(restoredTabs->indexOf(capturedPage), 0);
        QCOMPARE(restoredTabs->currentWidget(), capturedPage.data());
        QCOMPARE(restoredTabs->tabText(0), QStringLiteral("Captured survivor"));
        QCOMPARE(
            restoredTabs->tabToolTip(0),
            QStringLiteral("Captured survivor tooltip"));
        QVERIFY(restoredTabs->isTabPinned(0));
        QVERIFY(restoredTabs->isTabModified(0));
        QCOMPARE(workspace.pageLayoutKey(capturedPage), capturedKey);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            tabWidgetBudget);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            workspace.groupIds().size());
    }

    void restoreCallbacksCanUpdateStagedPageLayoutKey()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> page = new QWidget;
        originalTabs->addTab(page, QStringLiteral("Rekeyed page"));
        const QString oldKey = QStringLiteral("restore-rekey:old");
        const QString newKey = QStringLiteral("restore-rekey:new");
        QVERIFY(workspace.setPageLayoutKey(page, oldKey));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool keyCallbackRan = false;
        bool keyUpdated = false;
        QString keyObservedInCallback;
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
                            QWidget *transferredPage) {
                            if (keyCallbackRan || transferredPage != page) {
                                return;
                            }
                            keyCallbackRan = true;
                            keyUpdated = workspace.setPageLayoutKey(
                                page, newKey);
                            keyObservedInCallback =
                                workspace.pageLayoutKey(page);
                        });
                }
            });

        QVERIFY(workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(keyCallbackRan);
        QVERIFY(keyUpdated);
        QCOMPARE(keyObservedInCallback, newKey);
        QVERIFY(!page.isNull());
        QCOMPARE(workspace.tabWidget(groupId)->indexOf(page), 0);
        QCOMPARE(workspace.pageLayoutKey(page), newKey);

        const QByteArray repeated = workspace.saveLayout();
        QVERIFY(!repeated.isEmpty());
        QVERIFY(workspace.restoreLayout(repeated));
        QCOMPARE(workspace.pageLayoutKey(page), newKey);
        QVERIFY(!workspace.savedGroupForPageKey(oldKey).isValid());
        QCOMPARE(workspace.savedGroupForPageKey(newKey), groupId);
    }

    void restoreRollbackPreservesCurrentAfterRefillingAddedPage()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> addedPage;
        originalTabs->addTab(capturedPage, QStringLiteral("Captured page"));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool addedToStaging = false;
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
                        [&, tabs](ZzFluentUI::ZzTabWidget *,
                                  int,
                                  int,
                                  QWidget *transferredPage) {
                            if (addedToStaging
                                || transferredPage != capturedPage) {
                                return;
                            }
                            addedToStaging = true;
                            addedPage = new QWidget;
                            tabs->addTab(
                                addedPage, QStringLiteral("Added page"));
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(addedToStaging);
        QVERIFY(!capturedPage.isNull());
        QVERIFY(!addedPage.isNull());
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs->indexOf(capturedPage), 0);
        QCOMPARE(restoredTabs->indexOf(addedPage), 1);
        QCOMPARE(restoredTabs->currentWidget(), capturedPage.data());
    }

    void restoreCallbacksTreatRemovedStagedPageAsOutsider()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> removedPage = new QWidget;
        QPointer<QWidget> replacementPage = new QWidget;
        originalTabs->addTab(removedPage, QStringLiteral("Removed page"));
        originalTabs->addTab(
            replacementPage, QStringLiteral("Replacement page"));
        const QString key = QStringLiteral("restore-remove:reusable");
        QVERIFY(workspace.setPageLayoutKey(removedPage, key));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool removedFromStaging = false;
        QString removedKeyAfterRemoval = QStringLiteral("callback-not-run");
        bool replacementKeySet = false;
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
                        [&, tabs](ZzFluentUI::ZzTabWidget *,
                                  int,
                                  int,
                                  QWidget *transferredPage) {
                            if (removedFromStaging
                                || transferredPage != removedPage) {
                                return;
                            }
                            removedFromStaging = true;
                            tabs->removeTab(tabs->indexOf(removedPage));
                            removedKeyAfterRemoval =
                                workspace.pageLayoutKey(removedPage);
                            replacementKeySet = workspace.setPageLayoutKey(
                                replacementPage, key);
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(removedFromStaging);
        QVERIFY(removedKeyAfterRemoval.isEmpty());
        QVERIFY(replacementKeySet);
    }

    void restoreCleanupMayDeleteWorkspaceBeforeCurrentRepair()
    {
        ZzFluentUI::ZzTabWidget thirdParty;
        auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
        QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
        const auto groupId = rawWorkspace->groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            rawWorkspace->tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> addedPage;
        originalTabs->addTab(capturedPage, QStringLiteral("Captured page"));
        const QByteArray saved = rawWorkspace->saveLayout();

        bool connectedStaging = false;
        bool rollbackArmed = false;
        bool cleanupDeletedWorkspace = false;
        ZzTestEventFilter cleanupFilter;
        connect(
            originalTabs,
            &QTabWidget::currentChanged,
            rawWorkspace,
            [&](int) {
                if (connectedStaging || workspace.isNull()) {
                    return;
                }
                connectedStaging = true;
                QWidget *rootHost = originalTabs;
                while (rootHost != nullptr
                       && rootHost->parentWidget() != rawWorkspace) {
                    rootHost = rootHost->parentWidget();
                }
                QVERIFY(rootHost != nullptr);
                cleanupFilter.callback = [&](QObject *, QEvent *event) {
                    if (cleanupDeletedWorkspace
                        || event->type() != QEvent::ChildRemoved
                        || workspace.isNull()) {
                        return false;
                    }
                    cleanupDeletedWorkspace = true;
                    delete rawWorkspace;
                    return true;
                };
                rootHost->installEventFilter(&cleanupFilter);
                const auto allTabs = rawWorkspace->findChildren<
                    ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        rawWorkspace,
                        [&, tabs](ZzFluentUI::ZzTabWidget *,
                                  int,
                                  int,
                                  QWidget *transferredPage) {
                            if (rollbackArmed
                                || transferredPage != capturedPage) {
                                return;
                            }
                            rollbackArmed = true;
                            addedPage = new QWidget;
                            tabs->addTab(
                                addedPage, QStringLiteral("Added page"));
                            QVERIFY(tabs->transferTabTo(
                                &thirdParty,
                                tabs->indexOf(capturedPage)));
                        });
                }
            });

        const bool restored = rawWorkspace->restoreLayout(saved);
        QVERIFY(connectedStaging);
        QVERIFY(rollbackArmed);
        QVERIFY(cleanupDeletedWorkspace);
        QVERIFY(workspace.isNull());
        QVERIFY(!restored);
        QVERIFY(!capturedPage.isNull());
        QCOMPARE(thirdParty.indexOf(capturedPage), 0);
    }

    void restoreRollbackIgnoresDisabledPublicTabTransferSetting()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> capturedPage = new QWidget;
        QPointer<QWidget> addedPage;
        originalTabs->addTab(capturedPage, QStringLiteral("Captured page"));
        originalTabs->setTabToolTip(
            0, QStringLiteral("Captured tooltip"));
        originalTabs->setTabWhatsThis(
            0, QStringLiteral("Captured help"));
        originalTabs->fluentTabBar()->setTabData(
            0, QStringLiteral("captured-data"));
        originalTabs->fluentTabBar()->setTabTextColor(0, QColor(Qt::cyan));
        originalTabs->setTabModified(0, true);
        originalTabs->setTabAttention(0, true);
        originalTabs->setTabCloseEnabled(0, false);
        const QString capturedKey = QStringLiteral("disabled:captured");
        const QString addedKey = QStringLiteral("disabled:added");
        QVERIFY(workspace.setPageLayoutKey(capturedPage, capturedKey));
        const QByteArray saved = workspace.saveLayout();
        originalTabs->fluentTabBar()->setTabTransferEnabled(false);
        const qsizetype tabWidgetBudget = workspace.findChildren<
            ZzFluentUI::ZzTabWidget *>().size();

        bool transferSettingSignalLeaked = false;
        connect(
            originalTabs->fluentTabBar(),
            &ZzFluentUI::ZzTabBar::tabTransferEnabledChanged,
            &workspace,
            [&](bool) { transferSettingSignalLeaked = true; });
        bool connectedStaging = false;
        bool addedToStaging = false;
        bool addedKeySet = false;
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
                        [&, tabs](ZzFluentUI::ZzTabWidget *,
                                  int,
                                  int,
                                  QWidget *transferredPage) {
                            if (addedToStaging
                                || transferredPage != capturedPage) {
                                return;
                            }
                            addedToStaging = true;
                            addedPage = new QWidget;
                            const int index = tabs->addTab(
                                addedPage, QStringLiteral("Added page"));
                            tabs->setTabToolTip(
                                index, QStringLiteral("Added tooltip"));
                            tabs->setTabWhatsThis(
                                index, QStringLiteral("Added help"));
                            tabs->fluentTabBar()->setTabData(
                                index, QStringLiteral("added-data"));
                            tabs->fluentTabBar()->setTabTextColor(
                                index, QColor(Qt::magenta));
                            tabs->setTabEnabled(index, false);
                            tabs->setTabPinned(index, true);
                            const int pinnedIndex = tabs->indexOf(addedPage);
                            tabs->setTabModified(pinnedIndex, true);
                            tabs->setTabAttention(pinnedIndex, true);
                            tabs->setTabCloseEnabled(pinnedIndex, false);
                            addedKeySet = workspace.setPageLayoutKey(
                                addedPage, addedKey);
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(addedToStaging);
        QVERIFY(addedKeySet);
        QVERIFY(!capturedPage.isNull());
        QVERIFY(!addedPage.isNull());
        auto *const restoredTabs = workspace.tabWidget(groupId);
        if (restoredTabs == nullptr) {
          QFAIL("Expected the restored tab widget to remain registered");
        }
        QCOMPARE(restoredTabs, originalTabs);
        QVERIFY(!restoredTabs->fluentTabBar()->isTabTransferEnabled());
        QVERIFY(!transferSettingSignalLeaked);
        QCOMPARE(restoredTabs->indexOf(addedPage), 0);
        QCOMPARE(restoredTabs->indexOf(capturedPage), 1);
        QCOMPARE(restoredTabs->currentWidget(), capturedPage.data());
        QCOMPARE(restoredTabs->tabText(0), QStringLiteral("Added page"));
        QCOMPARE(
            restoredTabs->tabToolTip(0), QStringLiteral("Added tooltip"));
        QCOMPARE(
            restoredTabs->tabWhatsThis(0), QStringLiteral("Added help"));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabData(0),
            QVariant(QStringLiteral("added-data")));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabTextColor(0),
            QColor(Qt::magenta));
        QVERIFY(!restoredTabs->isTabEnabled(0));
        QVERIFY(restoredTabs->isTabPinned(0));
        QVERIFY(restoredTabs->isTabModified(0));
        QVERIFY(restoredTabs->hasTabAttention(0));
        QVERIFY(!restoredTabs->isTabCloseEnabled(0));
        QCOMPARE(
            restoredTabs->tabText(1), QStringLiteral("Captured page"));
        QCOMPARE(
            restoredTabs->tabToolTip(1),
            QStringLiteral("Captured tooltip"));
        QCOMPARE(
            restoredTabs->tabWhatsThis(1), QStringLiteral("Captured help"));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabData(1),
            QVariant(QStringLiteral("captured-data")));
        QCOMPARE(
            restoredTabs->fluentTabBar()->tabTextColor(1),
            QColor(Qt::cyan));
        QVERIFY(restoredTabs->isTabEnabled(1));
        QVERIFY(!restoredTabs->isTabPinned(1));
        QVERIFY(restoredTabs->isTabModified(1));
        QVERIFY(restoredTabs->hasTabAttention(1));
        QVERIFY(!restoredTabs->isTabCloseEnabled(1));
        QCOMPARE(workspace.pageLayoutKey(capturedPage), capturedKey);
        QCOMPARE(workspace.pageLayoutKey(addedPage), addedKey);
        QCOMPARE(
            workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
            tabWidgetBudget);
    }

    void restoreSourceDeletionRecoversPageWithoutHiddenEscrowGrowth()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        const auto secondGroupId = workspace.splitGroup(
            groupId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroupId.has_value());
        QPointer<QWidget> page = new QWidget;
        auto *initialTabs = workspace.tabWidget(groupId);
        initialTabs->addTab(page, QStringLiteral("Recovered page"));
        initialTabs->setTabToolTip(
            0, QStringLiteral("Recovered tooltip"));
        initialTabs->setTabWhatsThis(
            0, QStringLiteral("Recovered help"));
        initialTabs->fluentTabBar()->setTabData(
            0, QStringLiteral("recovered-data"));
        initialTabs->fluentTabBar()->setTabTextColor(0, QColor(Qt::cyan));
        initialTabs->setTabModified(0, true);
        initialTabs->setTabAttention(0, true);
        initialTabs->setTabCloseEnabled(0, false);
        const QString key = QStringLiteral("source-deletion:key");
        QVERIFY(workspace.setPageLayoutKey(page, key));
        const QByteArray saved = workspace.saveLayout();
        constexpr int restoreAttemptCount = 8;

        for (int iteration = 0; iteration < restoreAttemptCount;
             ++iteration) {
            QPointer<ZzFluentUI::ZzTabWidget> sourceTabs;
            for (const auto &id : workspace.groupIds()) {
                auto *const tabs = workspace.tabWidget(id);
                if (tabs != nullptr && tabs->indexOf(page) >= 0) {
                    sourceTabs = tabs;
                    break;
                }
            }
            QVERIFY2(
                !sourceTabs.isNull(),
                qPrintable(QStringLiteral("missing source before iteration %1")
                               .arg(iteration)));
            bool connectedStaging = false;
            bool deletedSource = false;
            connect(
                sourceTabs,
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
                        if (tabs == sourceTabs.data()) {
                            continue;
                        }
                        connect(
                            tabs,
                            &ZzFluentUI::ZzTabWidget::tabTransferred,
                            &workspace,
                            [&](ZzFluentUI::ZzTabWidget *,
                                int,
                                int,
                                QWidget *transferredPage) {
                                if (deletedSource || transferredPage != page) {
                                    return;
                                }
                                deletedSource = true;
                                delete sourceTabs.data();
                            });
                    }
                });

            QVERIFY(!workspace.restoreLayout(saved));
            QVERIFY(connectedStaging);
            QVERIFY(deletedSource);
            QVERIFY(sourceTabs.isNull());
            QVERIFY(!page.isNull());
            ZzFluentUI::ZzTabWidget *visibleTabs = nullptr;
            QList<ZzFluentUI::ZzTabWidget *> publicTabs;
            for (const auto &id : workspace.groupIds()) {
                auto *const tabs = workspace.tabWidget(id);
                QVERIFY(tabs != nullptr);
                publicTabs.push_back(tabs);
                if (tabs->indexOf(page) >= 0) {
                    visibleTabs = tabs;
                }
            }
            QVERIFY2(
                visibleTabs != nullptr,
                qPrintable(QStringLiteral("missing visible owner after %1")
                               .arg(iteration)));
            QVERIFY(!visibleTabs->isHidden());
            QCOMPARE(visibleTabs->indexOf(page), 0);
            QCOMPARE(
                visibleTabs->tabText(0), QStringLiteral("Recovered page"));
            QCOMPARE(
                visibleTabs->tabToolTip(0),
                QStringLiteral("Recovered tooltip"));
            QCOMPARE(
                visibleTabs->tabWhatsThis(0),
                QStringLiteral("Recovered help"));
            QCOMPARE(
                visibleTabs->fluentTabBar()->tabData(0),
                QVariant(QStringLiteral("recovered-data")));
            QCOMPARE(
                visibleTabs->fluentTabBar()->tabTextColor(0),
                QColor(Qt::cyan));
            QVERIFY(visibleTabs->isTabModified(0));
            QVERIFY(visibleTabs->hasTabAttention(0));
            QVERIFY(!visibleTabs->isTabCloseEnabled(0));
            QCOMPARE(visibleTabs->currentWidget(), page.data());
            QCOMPARE(workspace.pageLayoutKey(page), key);
            const auto workspaceTabs = workspace.findChildren<
                ZzFluentUI::ZzTabWidget *>();
            QCOMPARE(workspaceTabs.size(), publicTabs.size());
            for (ZzFluentUI::ZzTabWidget *tabs : workspaceTabs) {
                QVERIFY(publicTabs.contains(tabs));
            }
        }
    }

    void restoreKeepsEscrowAlignedWhenPinnedMetadataIsRepaired()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto firstGroupId = workspace.groupIds().constFirst();
        const auto secondGroupId = workspace.splitGroup(
            firstGroupId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroupId.has_value());
        QPointer<ZzFluentUI::ZzTabWidget> firstOriginalTabs =
            workspace.tabWidget(firstGroupId);
        QPointer<ZzFluentUI::ZzTabWidget> secondOriginalTabs =
            workspace.tabWidget(zzTabGroupIdOrInvalid(secondGroupId));
        QPointer<QWidget> unpinnedPage = new QWidget;
        QPointer<QWidget> pinnedPage = new QWidget;
        QPointer<QWidget> currentPage = new QWidget;
        firstOriginalTabs->addTab(
            pinnedPage, QStringLiteral("Pinned first group page"));
        firstOriginalTabs->addTab(
            currentPage, QStringLiteral("Current first group page"));
        secondOriginalTabs->addTab(
            unpinnedPage, QStringLiteral("Second group page"));
        firstOriginalTabs->setTabPinned(0, true);
        firstOriginalTabs->setTabToolTip(
            0, QStringLiteral("Pinned page tooltip"));
        firstOriginalTabs->setCurrentWidget(currentPage);
        QVERIFY(workspace.setPageLayoutKey(
            pinnedPage, QStringLiteral("pinned-repair:pinned")));
        QVERIFY(workspace.setPageLayoutKey(
            currentPage, QStringLiteral("pinned-repair:current")));
        QVERIFY(workspace.setPageLayoutKey(
            unpinnedPage, QStringLiteral("pinned-repair:unpinned")));
        const QByteArray saved = workspace.saveLayout();

        QVERIFY(workspace.transferTab(firstGroupId, 0,
                                      zzTabGroupIdOrInvalid(secondGroupId)));
        QVERIFY(workspace.transferTab(firstGroupId, 0,
                                      zzTabGroupIdOrInvalid(secondGroupId)));
        const int unpinnedIndex =
            secondOriginalTabs->indexOf(unpinnedPage);
        QVERIFY(unpinnedIndex >= 0);
        QVERIFY(workspace.transferTab(zzTabGroupIdOrInvalid(secondGroupId),
                                      unpinnedIndex, firstGroupId));
        QCOMPARE(firstOriginalTabs->indexOf(unpinnedPage), 0);
        QCOMPARE(secondOriginalTabs->indexOf(pinnedPage), 0);
        QCOMPARE(secondOriginalTabs->indexOf(currentPage), 1);

        bool connectedStaging = false;
        bool perturbedPinned = false;
        bool pinnedSignalLeaked = false;
        const auto connectStaging = [&](int) {
            if (connectedStaging) {
                return;
            }
            connectedStaging = true;
            const auto allTabs = workspace.findChildren<
                ZzFluentUI::ZzTabWidget *>();
            for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                if (tabs == firstOriginalTabs.data()
                    || tabs == secondOriginalTabs.data()) {
                    continue;
                }
                connect(
                    tabs,
                    &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
                    &workspace,
                    [&, tabs](int index, bool pinned) {
                        if (perturbedPinned && pinned
                            && tabs->widget(index) == pinnedPage) {
                            pinnedSignalLeaked = true;
                        }
                    });
                connect(
                    tabs,
                    &ZzFluentUI::ZzTabWidget::tabTransferred,
                    &workspace,
                    [&, tabs](ZzFluentUI::ZzTabWidget *,
                              int,
                              int targetIndex,
                              QWidget *page) {
                        if (!perturbedPinned && page == pinnedPage) {
                            perturbedPinned = true;
                            tabs->setTabPinned(targetIndex, false);
                        }
                    });
            }
        };
        connect(
            firstOriginalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            connectStaging);
        connect(
            secondOriginalTabs,
            &QTabWidget::currentChanged,
            &workspace,
            connectStaging);

        QVERIFY(workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(perturbedPinned);
        QVERIFY(!pinnedSignalLeaked);
        QVERIFY(!unpinnedPage.isNull());
        QVERIFY(!pinnedPage.isNull());
        QVERIFY(!currentPage.isNull());
        auto *const firstRestoredTabs = workspace.tabWidget(firstGroupId);
        auto *const secondRestoredTabs =
            workspace.tabWidget(zzTabGroupIdOrInvalid(secondGroupId));
        QVERIFY(firstRestoredTabs != nullptr);
        QVERIFY(secondRestoredTabs != nullptr);
        QCOMPARE(firstRestoredTabs->indexOf(pinnedPage), 0);
        QCOMPARE(firstRestoredTabs->indexOf(currentPage), 1);
        QCOMPARE(secondRestoredTabs->indexOf(unpinnedPage), 0);
        QCOMPARE(
            firstRestoredTabs->tabText(0),
            QStringLiteral("Pinned first group page"));
        QCOMPARE(
            firstRestoredTabs->tabToolTip(0),
            QStringLiteral("Pinned page tooltip"));
        QVERIFY(firstRestoredTabs->isTabPinned(0));
        QCOMPARE(firstRestoredTabs->currentWidget(), currentPage.data());
    }

    void restoreEscrowsAllPagesBeforeStagedCurrentChangedCallbacks_data()
    {
        QTest::addColumn<bool>("tabBarEmitter");
        QTest::newRow("tab-widget") << false;
        QTest::newRow("tab-bar") << true;
    }

    void restoreEscrowsAllPagesBeforeStagedCurrentChangedCallbacks()
    {
        QFETCH(bool, tabBarEmitter);

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto firstGroupId = workspace.groupIds().constFirst();
        const auto secondGroupId = workspace.splitGroup(
            firstGroupId,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroupId.has_value());
        QPointer<QWidget> firstPage = new QWidget;
        QPointer<QWidget> secondPage = new QWidget;
        workspace.tabWidget(firstGroupId)->addTab(
            firstPage, QStringLiteral("First staged page"));
        workspace.tabWidget(zzTabGroupIdOrInvalid(secondGroupId))
            ->addTab(secondPage, QStringLiteral("Later staged page"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("staged-current:first")));
        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral("staged-current:second")));
        const QByteArray saved = workspace.saveLayout();

        QVERIFY(workspace.transferTab(zzTabGroupIdOrInvalid(secondGroupId), 0,
                                      firstGroupId));
        QVERIFY(
            workspace.removeEmptyGroup(zzTabGroupIdOrInvalid(secondGroupId)));
        auto *const originalTabs = workspace.tabWidget(firstGroupId);
        QCOMPARE(originalTabs->count(), 2);

        bool connectedStaging = false;
        bool armed = false;
        bool invalidated = false;
        QPointer<ZzFluentUI::ZzTabWidget> firstStagedTabs;
        QPointer<ZzFluentUI::ZzTabWidget> laterStagedTabs;
        connect(
            originalTabs, &QTabWidget::currentChanged, &workspace, [&](int) {
                if (connectedStaging) {
                    return;
                }
                connectedStaging = true;
                const auto allTabs =
                    workspace.findChildren<ZzFluentUI::ZzTabWidget *>();
                for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
                    if (tabs == originalTabs) {
                        continue;
                    }
                    connect(tabs,
                            &ZzFluentUI::ZzTabWidget::tabTransferred,
                            &workspace,
                            [&, tabs](ZzFluentUI::ZzTabWidget *,
                                      int,
                                      int,
                                      QWidget *transferredPage) {
                                if (transferredPage == firstPage) {
                                    firstStagedTabs = tabs;
                                } else if (transferredPage == secondPage) {
                                    laterStagedTabs = tabs;
                                }
                                armed = !firstStagedTabs.isNull()
                                    && !laterStagedTabs.isNull();
                            });
                    const auto invalidateLater = [&, tabs](int) {
                        if (!armed || invalidated
                            || tabs != firstStagedTabs
                            || tabs->count() != 0) {
                            return;
                        }
                        invalidated = true;
                        delete laterStagedTabs.data();
                    };
                    if (tabBarEmitter) {
                        connect(tabs->fluentTabBar(),
                                &QTabBar::currentChanged,
                                &workspace,
                                invalidateLater);
                    } else {
                        connect(tabs,
                                &QTabWidget::currentChanged,
                                &workspace,
                                invalidateLater);
                    }
                }
            });

        const bool restored = workspace.restoreLayout(saved);
        QVERIFY(connectedStaging);
        QVERIFY(!firstStagedTabs.isNull());
        QVERIFY(!laterStagedTabs.isNull());
        QVERIFY(armed);
        QVERIFY(!invalidated);
        QVERIFY(restored);
        QVERIFY(!firstPage.isNull());
        QVERIFY(!secondPage.isNull());
        QCOMPARE(workspace.tabWidget(firstGroupId)->indexOf(firstPage), 0);
        QCOMPARE(workspace.tabWidget(zzTabGroupIdOrInvalid(secondGroupId))
                     ->indexOf(secondPage),
                 0);
    }

    void restoreCommitRefillSignalsCannotDeleteEscrowOrTarget_data()
    {
        QTest::addColumn<bool>("deleteSource");
        QTest::newRow("source") << true;
        QTest::newRow("target") << false;
    }

    // Qt 页面所有权由 TabWidget 接管，分析器无法沿 QObject 父子关系追踪释放。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void restoreCommitRefillSignalsCannotDeleteEscrowOrTarget()
    {
        QFETCH(bool, deleteSource);

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> firstPage = new QWidget;
        QPointer<QWidget> secondPage = new QWidget;
        originalTabs->addTab(firstPage, QStringLiteral("First refill page"));
        originalTabs->addTab(secondPage, QStringLiteral("Second refill page"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("commit-refill:first")));
        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral("commit-refill:second")));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool deletionAttempted = false;
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
                        [&, tabs](
                            ZzFluentUI::ZzTabWidget *source,
                            int,
                            int,
                            QWidget *) {
                            if (source == nullptr
                                || source->parentWidget() != nullptr) {
                                return;
                            }
                            deletionAttempted = true;
                            delete (deleteSource ? source : tabs);
                        });
                }
            });

        QVERIFY(workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(!deletionAttempted);
        QVERIFY(!firstPage.isNull());
        QVERIFY(!secondPage.isNull());
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs->indexOf(firstPage), 0);
        QCOMPARE(restoredTabs->indexOf(secondPage), 1);
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void restoreRollbackRefillSignalsCannotDeleteEscrowOrTarget_data()
    {
        QTest::addColumn<bool>("deleteSource");
        QTest::newRow("source") << true;
        QTest::newRow("target") << false;
    }

    // Qt 页面所有权由 TabWidget 接管，分析器无法沿 QObject 父子关系追踪释放。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void restoreRollbackRefillSignalsCannotDeleteEscrowOrTarget()
    {
        QFETCH(bool, deleteSource);

        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(groupId);
        QPointer<QWidget> firstPage = new QWidget;
        QPointer<QWidget> secondPage = new QWidget;
        originalTabs->addTab(firstPage, QStringLiteral("First rollback page"));
        originalTabs->addTab(secondPage,
                             QStringLiteral("Second rollback page"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("rollback-refill:first")));
        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral("rollback-refill:second")));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool removedFromStaging = false;
        bool deletionAttempted = false;
        connect(
            originalTabs,
            &ZzFluentUI::ZzTabWidget::tabTransferred,
            &workspace,
            [&](ZzFluentUI::ZzTabWidget *source,
                int,
                int,
                QWidget *) {
                if (source == nullptr
                    || source->parentWidget() != nullptr) {
                    return;
                }
                deletionAttempted = true;
                delete (deleteSource ? source : originalTabs.data());
            });
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
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, tabs](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int targetIndex,
                            QWidget *page) {
                            if (removedFromStaging || page != firstPage) {
                                return;
                            }
                            removedFromStaging = true;
                            tabs->removeTab(targetIndex);
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(removedFromStaging);
        QVERIFY(!deletionAttempted);
        QVERIFY(!originalTabs.isNull());
        QVERIFY(!firstPage.isNull());
        QVERIFY(!secondPage.isNull());
        QCOMPARE(originalTabs->indexOf(firstPage), 0);
        QCOMPARE(originalTabs->indexOf(secondPage), 1);
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    // Qt 页面所有权由 TabWidget 接管，分析器无法沿 QObject 父子关系追踪释放。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void restoreCommitRefillMetadataSignalsCannotDeleteTarget()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        auto *const originalTabs = workspace.tabWidget(groupId);
        QPointer<QWidget> firstPage = new QWidget;
        QPointer<QWidget> secondPage = new QWidget;
        originalTabs->addTab(firstPage,
                             QStringLiteral("First metadata page"));
        originalTabs->addTab(secondPage,
                             QStringLiteral("Second metadata page"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("commit-metadata:first")));
        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral("commit-metadata:second")));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool perturbedMetadata = false;
        bool deletionAttempted = false;
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
                        &ZzFluentUI::ZzTabWidget::tabModifiedChanged,
                        &workspace,
                        [&, tabs](int index, bool modified) {
                            if (deletionAttempted || modified
                                || workspace.tabWidget(groupId) != tabs
                                || tabs->widget(index) != secondPage) {
                                return;
                            }
                            deletionAttempted = true;
                            delete tabs;
                        });
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, tabs](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int targetIndex,
                            QWidget *page) {
                            if (perturbedMetadata || page != secondPage) {
                                return;
                            }
                            perturbedMetadata = true;
                            tabs->setTabModified(targetIndex, true);
                        });
                }
            });

        QVERIFY(workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(perturbedMetadata);
        QVERIFY(!deletionAttempted);
        QVERIFY(!firstPage.isNull());
        QVERIFY(!secondPage.isNull());
        auto *const restoredTabs = workspace.tabWidget(groupId);
        QVERIFY(restoredTabs != nullptr);
        QCOMPARE(restoredTabs->indexOf(firstPage), 0);
        QCOMPARE(restoredTabs->indexOf(secondPage), 1);
        QVERIFY(!restoredTabs->isTabModified(1));
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void restoreRollbackRefillMetadataSignalsCannotDeleteTarget()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto groupId = workspace.groupIds().constFirst();
        QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
            workspace.tabWidget(groupId);
        QPointer<QWidget> firstPage = new QWidget;
        QPointer<QWidget> secondPage = new QWidget;
        originalTabs->addTab(firstPage,
                             QStringLiteral("First rollback metadata"));
        originalTabs->addTab(secondPage,
                             QStringLiteral("Second rollback metadata"));
        QVERIFY(workspace.setPageLayoutKey(
            firstPage, QStringLiteral("rollback-metadata:first")));
        QVERIFY(workspace.setPageLayoutKey(
            secondPage, QStringLiteral("rollback-metadata:second")));
        const QByteArray saved = workspace.saveLayout();

        bool connectedStaging = false;
        bool removedFromStaging = false;
        bool perturbedMetadata = false;
        bool deletionAttempted = false;
        connect(
            originalTabs,
            &ZzFluentUI::ZzTabWidget::tabModifiedChanged,
            &workspace,
            [&](int index, bool modified) {
                if (deletionAttempted || modified
                    || !removedFromStaging
                    || originalTabs->widget(index) != secondPage) {
                    return;
                }
                deletionAttempted = true;
                delete originalTabs.data();
            });
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
                    if (tabs == originalTabs.data()) {
                        continue;
                    }
                    connect(
                        tabs,
                        &ZzFluentUI::ZzTabWidget::tabTransferred,
                        &workspace,
                        [&, tabs](
                            ZzFluentUI::ZzTabWidget *,
                            int,
                            int targetIndex,
                            QWidget *page) {
                            if (page == secondPage && !perturbedMetadata) {
                                perturbedMetadata = true;
                                tabs->setTabModified(targetIndex, true);
                                return;
                            }
                            if (page == firstPage && !removedFromStaging) {
                                removedFromStaging = true;
                                tabs->removeTab(targetIndex);
                            }
                        });
                }
            });

        QVERIFY(!workspace.restoreLayout(saved));
        QVERIFY(connectedStaging);
        QVERIFY(removedFromStaging);
        QVERIFY(perturbedMetadata);
        QVERIFY(!deletionAttempted);
        QVERIFY(!originalTabs.isNull());
        QVERIFY(!firstPage.isNull());
        QVERIFY(!secondPage.isNull());
        QCOMPARE(originalTabs->indexOf(firstPage), 0);
        QCOMPARE(originalTabs->indexOf(secondPage), 1);
        QVERIFY(!originalTabs->isTabModified(1));
    }

    void restoreCommitEventsMayInvalidateParticipants_data() {
      QTest::addColumn<int>("invalidation");
      QTest::newRow("cleanup-workspace") << 0;
      QTest::newRow("staged-root") << 1;
      QTest::newRow("leaf-target") << 2;
      QTest::newRow("cleanup-staged-root") << 3;
    }

    void restoreCommitEventsMayInvalidateParticipants() {
      QFETCH(int, invalidation);

      auto *rawWorkspace = new ZzFluentUI::ZzSplitWorkspace;
      QPointer<ZzFluentUI::ZzSplitWorkspace> workspace = rawWorkspace;
      const auto sourceId = rawWorkspace->groupIds().constFirst();
      const auto targetId = rawWorkspace->splitGroup(
          sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
      QVERIFY(targetId.has_value());
      QPointer<QWidget> page = new QWidget;
      rawWorkspace->tabWidget(sourceId)->addTab(
          page, QStringLiteral("Commit boundary"));
      QVERIFY(rawWorkspace->setPageLayoutKey(
          page, QStringLiteral("commit-boundary:key")));
      const QByteArray saved = rawWorkspace->saveLayout();
      QVERIFY(rawWorkspace->transferTab(sourceId, 0,
                                        zzTabGroupIdOrInvalid(targetId)));
      QVERIFY(rawWorkspace->removeEmptyGroup(sourceId));
      const auto beforeIds = rawWorkspace->groupIds();
      QPointer<ZzFluentUI::ZzTabWidget> originalTabs =
          rawWorkspace->tabWidget(zzTabGroupIdOrInvalid(targetId));

      ZzTestEventFilter eventFilter;
      bool installed = false;
      bool invalidated = false;
      QPointer<QObject> deletionTarget;
      connect(
          originalTabs, &QTabWidget::currentChanged, rawWorkspace, [&](int) {
            if (installed || workspace.isNull()) {
              return;
            }
            installed = true;
            const auto allTabs =
                rawWorkspace->findChildren<ZzFluentUI::ZzTabWidget *>();
            ZzFluentUI::ZzTabWidget *stagedLeaf = nullptr;
            QWidget *stagingHost = nullptr;
            for (ZzFluentUI::ZzTabWidget *tabs : allTabs) {
              if (tabs == originalTabs.data()) {
                continue;
              }
              if (stagedLeaf == nullptr) {
                stagedLeaf = tabs;
                QWidget *candidate = tabs;
                while (candidate->parentWidget() != rawWorkspace) {
                  candidate = candidate->parentWidget();
                  QVERIFY(candidate != nullptr);
                }
                stagingHost = candidate;
              }
              if (invalidation == 2) {
                connect(
                    tabs,
                    &ZzFluentUI::ZzTabWidget::tabTransferred,
                    rawWorkspace,
                    [&, tabs](
                        ZzFluentUI::ZzTabWidget *,
                        int,
                        int,
                        QWidget *transferredPage) {
                      if (transferredPage == page) {
                        deletionTarget = tabs;
                      }
                    });
              }
            }
            QVERIFY(stagedLeaf != nullptr);
            QWidget *const rootHost = stagingHost;
            QVERIFY(rootHost != nullptr);
            QVERIFY(rootHost->layout() != nullptr);
            QVERIFY(rootHost->layout()->count() == 1);
            QWidget *const oldRoot = rootHost->layout()->itemAt(0)->widget();
            QVERIFY(oldRoot == originalTabs.data());
            QWidget *stagedRoot = stagedLeaf;
            while (stagedRoot->parentWidget() != rootHost) {
              stagedRoot = stagedRoot->parentWidget();
              QVERIFY(stagedRoot != nullptr);
            }
            QVERIFY(stagedRoot != nullptr);

            const bool cleanupInvalidation =
                invalidation == 0 || invalidation == 3;
            QObject *const watched = cleanupInvalidation
                                         ? static_cast<QObject *>(rootHost)
                                         : static_cast<QObject *>(oldRoot);
            const QEvent::Type trigger =
                cleanupInvalidation ? QEvent::ChildRemoved : QEvent::Hide;
            if (invalidation == 1 || invalidation == 3) {
              deletionTarget = stagedRoot;
            }
            eventFilter.callback = [&, trigger](QObject *, QEvent *event) {
              if (invalidated || event->type() != trigger) {
                return false;
              }
              invalidated = true;
              if (invalidation == 0) {
                delete rawWorkspace;
              } else if (!deletionTarget.isNull()) {
                delete deletionTarget.data();
              }
              return true;
            };
            watched->installEventFilter(&eventFilter);
          });

      const bool restored = rawWorkspace->restoreLayout(saved);
      QVERIFY(installed);
      QVERIFY(invalidated);
      if (invalidation == 0) {
        QVERIFY(workspace.isNull());
        QVERIFY(!restored);
        return;
      }

      QVERIFY(!workspace.isNull());
      QVERIFY(!restored);
      QVERIFY(!page.isNull());
      QCOMPARE(rawWorkspace->groupIds(), beforeIds);
      auto *const rolledBackTabs =
          rawWorkspace->tabWidget(zzTabGroupIdOrInvalid(targetId));
      QVERIFY(rolledBackTabs != nullptr);
      QCOMPARE(rolledBackTabs->indexOf(page), 0);
      QCOMPARE(
          rawWorkspace->pageLayoutKey(page),
          QStringLiteral("commit-boundary:key"));
      if (invalidation != 3) {
        QCOMPARE(rolledBackTabs, originalTabs.data());
      }
      delete rawWorkspace;
    }

    void restoresRepeatedlyWithoutDeferredObjectGrowth() {
      ZzFluentUI::ZzSplitWorkspace workspace;
      const auto sourceId = workspace.groupIds().constFirst();
      const auto targetId = workspace.splitGroup(
          sourceId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
      QVERIFY(targetId.has_value());
      auto *page = new QWidget;
      workspace.tabWidget(sourceId)->addTab(page,
                                            QStringLiteral("Repeated restore"));
      QVERIFY(workspace.setPageLayoutKey(
          page, QStringLiteral("repeated-restore:key")));
      const QByteArray saved = workspace.saveLayout();

      QVERIFY(workspace.restoreLayout(saved));
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      const qsizetype widgetCount = QApplication::allWidgets().size();
      const qsizetype splitterCount =
          workspace.findChildren<QSplitter *>().size();
      const qsizetype tabWidgetCount =
          workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size();

      for (int iteration = 0; iteration < 24; ++iteration) {
        QVERIFY(workspace.restoreLayout(saved));
      }

      QCOMPARE(QApplication::allWidgets().size(), widgetCount);
      QCOMPARE(workspace.findChildren<QSplitter *>().size(), splitterCount);
      QCOMPARE(workspace.findChildren<ZzFluentUI::ZzTabWidget *>().size(),
               tabWidgetCount);
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

        QVERIFY(workspace.transferTab(sourceId, 0,
                                      zzTabGroupIdOrInvalid(targetId), 0));
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))->widget(0),
            page);

        const QList<ZzFluentUI::ZzTabGroupId> before = workspace.groupIds();
        const QList<QList<int>> beforeSizes = zzSplitterSizes(workspace);
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex =
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))->indexOf(page);
        QVERIFY(!workspace.moveTabToDropZone(
            zzTabGroupIdOrInvalid(targetId), 99,
            zzTabGroupIdOrInvalid(targetId),
            ZzFluentUI::ZzWorkspaceDropZone::Left));
        QCOMPARE(workspace.groupIds(), before);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))->indexOf(page),
            beforeIndex);
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
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
                ->addTab(page, QStringLiteral("Edge"));
            QSignalSpy committedSpy(
                &workspace,
                &ZzFluentUI::ZzSplitWorkspace::tabDropCommitted);
            QSignalSpy layoutSpy(
                &workspace,
                &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
            layoutSpy.clear();

            QVERIFY(workspace.moveTabToDropZone(zzTabGroupIdOrInvalid(sourceId),
                                                0, targetId, zone));
            QCOMPARE(workspace.groupIds().size(), 2);
            QCOMPARE(committedSpy.size(), 1);
            QCOMPARE(layoutSpy.size(), 1);
            const auto destinationId = workspace.activeGroupId();
            QVERIFY(destinationId != targetId);
            QCOMPARE(workspace.tabWidget(destinationId)->widget(0), page);
            QVERIFY(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)) ==
                    nullptr);

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
            sourceId, 0, zzTabGroupIdOrInvalid(targetId),
            ZzFluentUI::ZzWorkspaceDropZone::Center));
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))->widget(0),
            page);
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
                rawWorkspace->tabWidget(zzTabGroupIdOrInvalid(targetId));
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
                sourceId, 0, zzTabGroupIdOrInvalid(targetId), 0);
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
                rawWorkspace->tabWidget(zzTabGroupIdOrInvalid(sourceId));
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
                zzTabGroupIdOrInvalid(sourceId), 0, targetId,
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
                QVERIFY(!workspace->groupIds().contains(
                    zzTabGroupIdOrInvalid(sourceId)));
            } else if (action == Action::DeleteOriginalTarget) {
                QVERIFY(!moved);
                QVERIFY(workspace->tabWidget(targetId) == nullptr);
                QVERIFY(!page.isNull());
                QCOMPARE(workspace->tabWidget(zzTabGroupIdOrInvalid(sourceId))
                             ->indexOf(page),
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
        QVERIFY(workspace.setActiveGroup(zzTabGroupIdOrInvalid(nestedId)));
        auto *page = new QWidget;
        workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
            ->addTab(page, QStringLiteral("Nested rollback"));

        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        const auto beforeActive = workspace.activeGroupId();
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex =
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))->indexOf(page);
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
        connect(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)),
                &QTabWidget::currentChanged, &workspace, [&](int) {
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
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Right));
        QVERIFY(destroyedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(workspace.activeGroupId(), beforeActive);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))->indexOf(page),
            beforeIndex);
        QCOMPARE(addedSpy.size(), 0);
        QCOMPARE(removedSpy.size(), 0);
        QCOMPARE(activeSpy.size(), 0);
        QCOMPARE(committedSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);
    }

    // Qt 页面所有权由恢复后的 TabWidget 接管，分析器无法追踪该所有权迁移。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
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
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

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
        workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
            ->addTab(new QWidget, QStringLiteral("Active source"));
        QVERIFY(workspace.setActiveGroup(zzTabGroupIdOrInvalid(sourceId)));
        QSignalSpy activeSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::activeGroupChanged);

        QVERIFY(workspace.moveTabToDropZone(
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Top));

        QCOMPARE(activeSpy.size(), 1);
        QCOMPARE(
            activeSpy.constFirst().constFirst().value<ZzFluentUI::ZzTabGroupId>(),
            workspace.activeGroupId());
        QVERIFY(workspace.activeGroupId() != zzTabGroupIdOrInvalid(sourceId));
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
        workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
            ->addTab(page, QStringLiteral("Replacement target"));
        const auto beforeIds = workspace.groupIds();
        bool replacedTarget = false;
        connect(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)),
                &QTabWidget::currentChanged, &workspace, [&](int) {
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
                                zzTabGroupIdOrInvalid(sourceId), Qt::Horizontal,
                                ZzFluentUI::ZzSplitPlacement::After, targetId);
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
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));

        QVERIFY(replacedTarget);
        QVERIFY(originalTarget.isNull());
        QVERIFY(!replacementTarget.isNull());
        QCOMPARE(workspace.tabWidget(targetId), replacementTarget.data());
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))->indexOf(page),
            0);
    }

    void replacementSourceWithSameIdIsNotRemoved()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        QPointer<ZzFluentUI::ZzTabWidget> originalSource =
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId));
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
                            if (!workspace.removeEmptyGroup(
                                    zzTabGroupIdOrInvalid(sourceId))) {
                              return;
                            }
                            const auto replacementId = workspace.splitGroup(
                                targetId, Qt::Horizontal,
                                ZzFluentUI::ZzSplitPlacement::After,
                                zzTabGroupIdOrInvalid(sourceId));
                            if (!replacementId.has_value()) {
                                return;
                            }
                            replacementSource = workspace.tabWidget(
                                zzTabGroupIdOrInvalid(sourceId));
                            replacedSource = !replacementSource.isNull();
                        });
                    return;
                }
            });

        QVERIFY(workspace.moveTabToDropZone(
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));

        QVERIFY(replacedSource);
        QVERIFY(originalSource.isNull());
        QVERIFY(!replacementSource.isNull());
        QCOMPARE(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)),
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
        workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
            ->addTab(page, QStringLiteral("Replacement temporary"));
        const auto beforeIds = workspace.groupIds();
        ZzFluentUI::ZzTabGroupId temporaryId;
        QPointer<ZzFluentUI::ZzTabWidget> originalTemporary;
        QPointer<ZzFluentUI::ZzTabWidget> replacementTemporary;
        bool replacedTemporary = false;
        connect(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)),
                &QTabWidget::currentChanged, &workspace, [&](int) {
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
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
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
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId)),
            &ZzFluentUI::ZzTabWidget::tabTransferred, &workspace,
            [&](ZzFluentUI::ZzTabWidget *, int, int targetIndex, QWidget *) {
              workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))
                  ->transferTabTo(&thirdParty, targetIndex);
            });

        QVERIFY(!workspace.transferTab(sourceId, 0,
                                       zzTabGroupIdOrInvalid(targetId), 0));
        QCOMPARE(thirdParty.indexOf(page), 0);
        QCOMPARE(workspace.tabWidget(sourceId)->indexOf(page), -1);
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))->indexOf(page),
            -1);
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
        workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
            ->addTab(page, QStringLiteral("Claimed edge"));
        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        bool connectedTemporary = false;
        connect(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)),
                &QTabWidget::currentChanged, &workspace, [&](int) {
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
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Bottom));
        QVERIFY(connectedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(thirdParty.indexOf(page), 0);
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))->indexOf(page),
            -1);
    }

    void edgeFailureRestoresPageWhenTemporaryTargetIsDestroyed()
    {
        ZzFluentUI::ZzSplitWorkspace workspace;
        const auto targetId = workspace.groupIds().constFirst();
        const auto sourceId = workspace.splitGroup(
            targetId, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(sourceId.has_value());
        auto *page = new QWidget;
        workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))
            ->addTab(page, QStringLiteral("Rollback"));
        const auto beforeIds = workspace.groupIds();
        const auto beforeSizes = zzSplitterSizes(workspace);
        QWidget *const beforeParent = page->parentWidget();
        const int beforeIndex =
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))->indexOf(page);
        bool destroyedTemporary = false;
        connect(workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId)),
                &QTabWidget::currentChanged, &workspace, [&](int) {
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
            zzTabGroupIdOrInvalid(sourceId), 0, targetId,
            ZzFluentUI::ZzWorkspaceDropZone::Top));
        QVERIFY(destroyedTemporary);
        QCOMPARE(workspace.groupIds(), beforeIds);
        QCOMPARE(zzSplitterSizes(workspace), beforeSizes);
        QCOMPARE(page->parentWidget(), beforeParent);
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(sourceId))->indexOf(page),
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
              QVERIFY(workspace
                          .splitGroup(zzTabGroupIdOrInvalid(targetId),
                                      Qt::Horizontal,
                                      ZzFluentUI::ZzSplitPlacement::After)
                          .has_value());
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
            const QPoint targetCenter =
                workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))
                    ->mapTo(
                        &workspace,
                        workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))
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
        QCOMPARE(
            workspace.tabWidget(zzTabGroupIdOrInvalid(targetId))->indexOf(page),
            0);
        QCOMPARE(
            workspace.findChildren<QWidget *>(
                QStringLiteral("zzSplitWorkspaceDropOverlay")).size(),
            1);

        for (qsizetype count = workspace.groupIds().size();
             count < 12;
             ++count) {
          QVERIFY(workspace
                      .splitGroup(zzTabGroupIdOrInvalid(targetId),
                                  Qt::Horizontal,
                                  ZzFluentUI::ZzSplitPlacement::After)
                      .has_value());
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
        const auto farRight =
            workspace.splitGroup(zzTabGroupIdOrInvalid(right), Qt::Horizontal,
                                 ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(farRight.has_value());
        QCOMPARE(workspace.groupIds().size(), 3);
        QCOMPARE(workspace.findChildren<QSplitter *>().size(), 1);
        QVERIFY(workspace.removeEmptyGroup(zzTabGroupIdOrInvalid(right)));
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
            root, Qt::Horizontal,
            // 故意模拟反序列化产生的越界枚举，验证公开接口的输入防线。
            // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
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
            source = zzTabGroupIdOrInvalid(added);
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
        workspace.tabWidget(zzTabGroupIdOrInvalid(empty))
            ->addTab(new QWidget, QStringLiteral("Occupied"));

        QSignalSpy removingSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::groupAboutToBeRemoved);
        QSignalSpy layoutSpy(
            &workspace,
            &ZzFluentUI::ZzSplitWorkspace::layoutChanged);
        QVERIFY(!workspace.removeEmptyGroup(zzTabGroupIdOrInvalid(empty)));
        QCOMPARE(removingSpy.size(), 0);
        QCOMPARE(layoutSpy.size(), 0);

        delete workspace.tabWidget(zzTabGroupIdOrInvalid(empty))->widget(0);
        QVERIFY(workspace.removeEmptyGroup(zzTabGroupIdOrInvalid(empty)));
        QCOMPARE(removingSpy.size(), 1);
        QCOMPARE(layoutSpy.size(), 1);
        QVERIFY(workspace.tabWidget(zzTabGroupIdOrInvalid(empty)) == nullptr);
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
        QVERIFY(workspace.setActiveGroup(zzTabGroupIdOrInvalid(right)));
        QCOMPARE(workspace.activeGroupId(), zzTabGroupIdOrInvalid(right));
        QCOMPARE(activeSpy.size(), 1);
        QVERIFY(workspace.setActiveGroup(zzTabGroupIdOrInvalid(right)));
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
        QCOMPARE(workspace.activeGroupId(), zzTabGroupIdOrInvalid(right));
        QVERIFY(zzFocusBelongsTo(
            QApplication::focusWidget(),
            workspace.tabWidget(zzTabGroupIdOrInvalid(right))));
        QWidget *const boundaryFocus = QApplication::focusWidget();
        QVERIFY(!workspace.focusAdjacentGroup(Qt::RightEdge));
        QCOMPARE(workspace.activeGroupId(), zzTabGroupIdOrInvalid(right));
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
        const auto &active = ids.at(0);
        const auto &fartherPrimary = ids.at(1);
        const auto &nearestPrimary = ids.at(2);
        const auto &bestSecondaryOnly = ids.at(3);
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
        const auto &active = ids.at(0);
        const auto &fartherSecondary = ids.at(1);
        const auto &nearestSecondary = ids.at(2);
        const auto &fartherPrimary = ids.at(3);
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
        const auto &active = ids.at(0);
        const auto &firstStableCandidate = ids.at(1);
        const auto &secondStableCandidate = ids.at(2);
        const auto &fartherPrimary = ids.at(3);
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
            QVERIFY(workspace.removeEmptyGroup(zzTabGroupIdOrInvalid(added)));
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

        QVERIFY(rawWorkspace->removeEmptyGroup(zzTabGroupIdOrInvalid(added)));
        QVERIFY(workspace.isNull());
    }

};

QTEST_MAIN(ZzSplitWorkspaceTest)

#include "ZzSplitWorkspaceTest.moc"
