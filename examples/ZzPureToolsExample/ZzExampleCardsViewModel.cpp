#include "ZzExampleCardsViewModel.h"

#include <array>

#include <QtGui/QStandardItem>

#include <ZzFluentUI/ZzCarouselView.h>

namespace ZzExample {

ZzExampleCardsViewModel::ZzExampleCardsViewModel()
{
    const std::array<std::array<QString, 2>, 3> values{{
        {QStringLiteral("Fluent 主题"), QStringLiteral("应用级不可变主题快照")},
        {QStringLiteral("窗口适配"), QStringLiteral("逐窗口无边框代理与系统行为")},
        {QStringLiteral("数据效率"), QStringLiteral("固定绘制复杂度与模型所有权")},
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
