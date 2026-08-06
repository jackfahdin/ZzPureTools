#include "ZzExampleCardsViewModel.h"

#include <array>

#include <QtCore/QCoreApplication>
#include <QtGui/QStandardItem>

#include <ZzFluentUI/ZzCarouselView.h>

namespace ZzExample {

ZzExampleCardsViewModel::ZzExampleCardsViewModel()
{
    const std::array<std::array<QString, 2>, 3> values{{
        {QCoreApplication::translate("ZzPureToolsExample", "Fluent 主题"), QCoreApplication::translate("ZzPureToolsExample", "应用级不可变主题快照")},
        {QCoreApplication::translate("ZzPureToolsExample", "窗口适配"), QCoreApplication::translate("ZzPureToolsExample", "逐窗口无边框代理与系统行为")},
        {QCoreApplication::translate("ZzPureToolsExample", "数据效率"), QCoreApplication::translate("ZzPureToolsExample", "固定绘制复杂度与模型所有权")},
    }};
    for (const auto &value : values) {
        auto *item = new QStandardItem(value.at(0));
        item->setData(
            value.at(1), ZzFluentUI::ZzCarouselView::DescriptionRole);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        appendRow(item);
    }
}

ZzExampleCardsViewModel::~ZzExampleCardsViewModel() = default;

} // namespace ZzExample
