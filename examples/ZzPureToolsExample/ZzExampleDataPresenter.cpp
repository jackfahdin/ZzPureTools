#include "ZzExampleDataPresenter.h"

#include <QtCore/QAbstractItemModel>

#include "ZzExampleDataPage.h"
#include "ZzExampleDataViewModel.h"

namespace ZzExample {

namespace {

/** @brief 将页面类型和可见顶层行数格式化为展示状态。 */
[[nodiscard]] QString zzDataStatusText(
    ZzExampleDataPageKind kind,
    int visibleRows)
{
    switch (kind) {
    case ZzExampleDataPageKind::List:
        return QStringLiteral("当前显示 %1 条列表记录").arg(visibleRows);
    case ZzExampleDataPageKind::Table:
        return QStringLiteral("当前显示 %1 行表格记录").arg(visibleRows);
    case ZzExampleDataPageKind::Tree:
        return QStringLiteral("当前显示 %1 个顶层节点").arg(visibleRows);
    }
    return {};
}

} // namespace

ZzExampleDataPresenter::ZzExampleDataPresenter(
    ZzExampleDataPageKind kind,
    ZzExampleDataPage *view,
    ZzExampleDataViewModel *viewModel)
{
    Q_ASSERT(view != nullptr);
    Q_ASSERT(viewModel != nullptr);
    const auto refresh = [kind, view, viewModel] {
        view->setStatusText(
            zzDataStatusText(kind, viewModel->visibleRowCount()));
    };
    QObject::connect(
        view,
        &ZzExampleDataPage::filterRequested,
        this,
        [viewModel, refresh](const QString &text) {
            viewModel->applyFilter(text);
            refresh();
        });
    QObject::connect(
        view,
        &ZzExampleDataPage::appendRequested,
        this,
        [viewModel, refresh] {
            viewModel->appendSample();
            refresh();
        });
    QObject::connect(
        view,
        &ZzExampleDataPage::resetRequested,
        this,
        [viewModel, refresh] {
            viewModel->resetSamples();
            refresh();
        });
    QObject::connect(
        viewModel,
        &QAbstractItemModel::modelReset,
        this,
        refresh);
    refresh();
}

ZzExampleDataPresenter::~ZzExampleDataPresenter() = default;

} // namespace ZzExample
