#include "ZzNavigationPanePrivate.h"

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationView.h>

#include "ZzNavigationProjectionModel.h"

namespace ZzFluentUI {

namespace {

constexpr int zzRegularNavigationWidth = 240;
constexpr int zzCompactNavigationWidth = 48;
constexpr int zzRegularItemHeight = 40;
constexpr int zzCompactItemHeight = 32;
constexpr int zzMaximumVisibleFooterRows = 6;

/** @brief 以一次选择模型事务同步当前项和单选状态。 */
void zzSyncViewSelection(
    ZzNavigationView *view,
    const QModelIndex &index)
{
    Q_ASSERT(view != nullptr);
    QItemSelectionModel *const selectionModel = view != nullptr
        ? view->selectionModel() : nullptr;
    if (selectionModel == nullptr) {
        return;
    }
    const bool alreadySynchronized = selectionModel->currentIndex() == index
        && (index.isValid()
            ? selectionModel->isSelected(index)
            : !selectionModel->hasSelection());
    if (alreadySynchronized) {
        return;
    }
    {
        const QSignalBlocker blocker(selectionModel);
        selectionModel->setCurrentIndex(
            index,
            index.isValid()
                ? QItemSelectionModel::ClearAndSelect
                    | QItemSelectionModel::Rows
                : QItemSelectionModel::Clear);
    }
    view->viewport()->update();
}

} // namespace

ZzNavigationPanePrivate::ZzNavigationPanePrivate(
    ZzNavigationPane *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    primaryProjection = new ZzNavigationProjectionModel(
        ZzNavigationProjection::Primary, q_ptr);
    footerProjection = new ZzNavigationProjectionModel(
        ZzNavigationProjection::Footer, q_ptr);
    primaryView = new ZzNavigationView(q_ptr);
    footerView = new ZzNavigationView(q_ptr);
    primaryView->setModel(primaryProjection);
    footerView->setModel(footerProjection);
    footerView->hide();
    footerView->setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(primaryView, 1);
    layout->addWidget(footerView);

    QObject::connect(
        primaryView,
        &ZzNavigationView::navigationRequested,
        q_ptr,
        [this](const QModelIndex &index) {
            activateProjectedIndex(primaryProjection, index);
        });
    QObject::connect(
        footerView,
        &ZzNavigationView::navigationRequested,
        q_ptr,
        [this](const QModelIndex &index) {
            activateProjectedIndex(footerProjection, index);
        });
    const auto projectionReset = [this] {
        updateFooterGeometry();
        restoreCurrentSelection();
    };
    QObject::connect(
        primaryProjection,
        &QAbstractItemModel::modelReset,
        q_ptr,
        projectionReset);
    QObject::connect(
        footerProjection,
        &QAbstractItemModel::modelReset,
        q_ptr,
        projectionReset);

    applyCompact(false);
}

ZzNavigationPanePrivate::~ZzNavigationPanePrivate()
{
    QObject::disconnect(modelDestroyedConnection);
    if (adaptiveWindow != nullptr) {
        adaptiveWindow->removeEventFilter(q_ptr);
    }
}

void ZzNavigationPanePrivate::setModel(QAbstractItemModel *model)
{
    if (sourceModel.data() == model) {
        return;
    }
    QObject::disconnect(modelDestroyedConnection);
    sourceModel = model;
    currentSourceIndex = QPersistentModelIndex();
    primaryProjection->setSourceModel(model);
    footerProjection->setSourceModel(model);
    if (model != nullptr) {
        modelDestroyedConnection = QObject::connect(
            model,
            &QObject::destroyed,
            q_ptr,
            [this] {
                handleModelDestroyed();
            });
    } else {
        modelDestroyedConnection = {};
    }
    setCurrentSourceIndex({});
    updateFooterGeometry();
    Q_EMIT q_ptr->modelChanged(model);
}

void ZzNavigationPanePrivate::handleModelDestroyed()
{
    sourceModel.clear();
    currentSourceIndex = QPersistentModelIndex();
    primaryProjection->setSourceModel(nullptr);
    footerProjection->setSourceModel(nullptr);
    modelDestroyedConnection = {};
    setCurrentSourceIndex({});
    updateFooterGeometry();
    Q_EMIT q_ptr->modelChanged(nullptr);
}

void ZzNavigationPanePrivate::activateProjectedIndex(
    ZzNavigationProjectionModel *projection,
    const QModelIndex &proxyIndex)
{
    Q_ASSERT(projection != nullptr);
    if (projection == nullptr) {
        return;
    }
    const QModelIndex sourceIndex = projection->mapToSource(proxyIndex);
    if (!sourceIndex.isValid()) {
        return;
    }
    setCurrentSourceIndex(sourceIndex);
    Q_EMIT q_ptr->navigationRequested(sourceIndex);
}

void ZzNavigationPanePrivate::setCurrentSourceIndex(
    const QModelIndex &index)
{
    const bool accepted = index.isValid()
        && index.model() == sourceModel.data()
        && !index.parent().isValid() && index.column() == 0;
    const QModelIndex sourceIndex = accepted ? index : QModelIndex();
    const QModelIndex primaryIndex = accepted
        ? primaryProjection->mapFromSource(sourceIndex) : QModelIndex();
    const QModelIndex footerIndex = accepted
        ? footerProjection->mapFromSource(sourceIndex) : QModelIndex();

    zzSyncViewSelection(primaryView, primaryIndex);
    zzSyncViewSelection(footerView, footerIndex);
    currentSourceIndex = primaryIndex.isValid() || footerIndex.isValid()
        ? QPersistentModelIndex(sourceIndex) : QPersistentModelIndex();
}

void ZzNavigationPanePrivate::restoreCurrentSelection()
{
    const QModelIndex sourceIndex = currentSourceIndex;
    setCurrentSourceIndex(sourceIndex);
}

void ZzNavigationPanePrivate::updateFooterGeometry()
{
    const int rows = footerProjection->rowCount();
    if (rows <= 0) {
        footerView->hide();
        footerView->setFocusPolicy(Qt::NoFocus);
        footerView->setFixedHeight(0);
        return;
    }
    const int visibleRows = std::min(rows, zzMaximumVisibleFooterRows);
    const int itemHeight = compact
        ? zzCompactItemHeight : zzRegularItemHeight;
    footerView->setFixedHeight(
        visibleRows * itemHeight + footerView->frameWidth() * 2);
    footerView->setFocusPolicy(Qt::StrongFocus);
    footerView->show();
}

void ZzNavigationPanePrivate::rebindAdaptiveWindow()
{
    QWidget *candidate = q_ptr->parentWidget() != nullptr
        ? q_ptr->window() : nullptr;
    if (candidate == q_ptr) {
        candidate = nullptr;
    }
    if (adaptiveWindow.data() == candidate) {
        syncDisplayMode();
        return;
    }
    if (adaptiveWindow != nullptr) {
        adaptiveWindow->removeEventFilter(q_ptr);
    }
    adaptiveWindow = candidate;
    if (adaptiveWindow != nullptr) {
        adaptiveWindow->installEventFilter(q_ptr);
    }
    syncDisplayMode();
}

void ZzNavigationPanePrivate::syncDisplayMode()
{
    bool useCompact = false;
    switch (displayMode) {
    case ZzNavigationDisplayMode::Regular:
        break;
    case ZzNavigationDisplayMode::Compact:
        useCompact = true;
        break;
    case ZzNavigationDisplayMode::Adaptive:
        useCompact = adaptiveWindow != nullptr
            && adaptiveWindow->width() < adaptiveThreshold;
        break;
    }
    applyCompact(useCompact);
}

void ZzNavigationPanePrivate::applyCompact(bool useCompact)
{
    if (compact == useCompact
        && q_ptr->width() == (useCompact
            ? zzCompactNavigationWidth : zzRegularNavigationWidth)) {
        updateFooterGeometry();
        return;
    }
    const bool changed = compact != useCompact;
    compact = useCompact;
    primaryView->setCompact(compact);
    footerView->setCompact(compact);
    q_ptr->setFixedWidth(
        compact ? zzCompactNavigationWidth : zzRegularNavigationWidth);
    updateFooterGeometry();
    if (changed) {
        Q_EMIT q_ptr->effectiveCompactChanged(compact);
    }
}

} // namespace ZzFluentUI
