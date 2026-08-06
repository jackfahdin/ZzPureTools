#include "ZzExampleCardsPagePrivate.h"

#include <array>
#include <cstddef>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzCarouselView.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzScrollArea.h>

#include "ZzExampleCardsPage.h"

namespace ZzExample {

namespace {

/** @brief 创建具有页面级字号的可换行标题。 */
[[nodiscard]] QLabel *zzCardsPageTitle(
    const QString &text,
    QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() + 10.0);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setWordWrap(true);
    return label;
}

/** @brief 增加页面分区标题与非装饰性分隔线。 */
void zzAddCardsSection(
    QVBoxLayout *layout,
    const QString &text,
    QWidget *parent)
{
    layout->addSpacing(12);
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() + 3.0);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    layout->addWidget(label);
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    layout->addWidget(separator);
}

/** @brief 使用当前 palette 创建无文件依赖的确定性预览图。 */
[[nodiscard]] QPixmap zzCardsPreviewPixmap(
    const QPalette &palette,
    int variant)
{
    QPixmap pixmap(640, 360);
    pixmap.fill(palette.color(QPalette::AlternateBase));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    const QColor accent = variant == 1
        ? palette.color(QPalette::Link)
        : palette.color(QPalette::Highlight);
    painter.setBrush(accent);
    if (variant == 0) {
        painter.drawRect(QRect(0, 0, 245, 360));
        painter.drawEllipse(QPoint(492, 92), 58, 58);
    } else if (variant == 1) {
        painter.drawRoundedRect(QRect(42, 42, 556, 138), 8, 8);
        painter.drawRect(QRect(438, 218, 160, 100));
    } else {
        painter.drawEllipse(QPoint(172, 180), 126, 126);
        painter.drawRect(QRect(340, 0, 300, 360));
    }

    painter.setBrush(palette.color(QPalette::Base));
    painter.drawRoundedRect(QRect(276, 82, 292, 48), 6, 6);
    painter.drawRoundedRect(QRect(276, 158, 212, 34), 6, 6);
    painter.drawRoundedRect(QRect(276, 218, 252, 24), 6, 6);
    return pixmap;
}

} // namespace

ZzExampleCardsPagePrivate::ZzExampleCardsPagePrivate(
    ZzExampleCardsPage *page)
    : q_ptr(page)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzExampleCardsPagePrivate::initialize(
    const QString &title,
    QAbstractItemModel *carouselModel)
{
    Q_ASSERT(carouselModel != nullptr);
    q_ptr->setObjectName(QStringLiteral("zzExampleCardsPage"));
    q_ptr->setAccessibleName(title);
    auto *rootLayout = new QVBoxLayout(q_ptr);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *scrollArea = new ZzFluentUI::ZzScrollArea(q_ptr);
    scrollArea->setObjectName(QStringLiteral("zzExampleCardsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 28, 32, 32);
    layout->setSpacing(12);
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    layout->addWidget(zzCardsPageTitle(title, content));
    auto *description = new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "卡片与轮播只消费本地展示值，图片预览随当前主题即时重建"),
        content);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *message = new ZzFluentUI::ZzMessageBar(content);
    message->setObjectName(QStringLiteral("zzExampleCardsMessage"));
    message->setText(QCoreApplication::translate("ZzPureToolsExample", "选择任一卡片以查看交互状态"));
    message->setSeverity(ZzFluentUI::ZzMessageSeverity::Information);
    message->setClosable(false);

    zzAddCardsSection(layout, QCoreApplication::translate("ZzPureToolsExample", "操作卡片"), content);
    auto *actionHost = new QWidget(content);
    auto *actionLayout = new ZzFluentUI::ZzFlowLayout(12, 12, actionHost);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    const std::array<std::array<QString, 2>, 3> actionValues{{
        {QCoreApplication::translate("ZzPureToolsExample", "性能概览"), QCoreApplication::translate("ZzPureToolsExample", "启动、空闲与绘制门禁")},
        {QCoreApplication::translate("ZzPureToolsExample", "发布矩阵"), QCoreApplication::translate("ZzPureToolsExample", "共享库与静态库组合")},
        {QCoreApplication::translate("ZzPureToolsExample", "平台能力"), QCoreApplication::translate("ZzPureToolsExample", "Linux、Windows 与 macOS")},
    }};
    for (const auto &value : actionValues) {
        auto *card = new ZzFluentUI::ZzActionCard(
            value.at(0), value.at(1), actionHost);
        card->setIcon(card->style()->standardIcon(QStyle::SP_FileDialogInfoView));
        card->setCheckable(true);
        card->setMinimumSize(250, 82);
        QObject::connect(
            card,
            &QAbstractButton::clicked,
            message,
            [message, card](bool checked) {
                message->setSeverity(
                    checked
                        ? ZzFluentUI::ZzMessageSeverity::Success
                        : ZzFluentUI::ZzMessageSeverity::Information);
                message->setText(
                    checked
                        ? QCoreApplication::translate("ZzPureToolsExample", "已选择：%1").arg(card->text())
                        : QCoreApplication::translate("ZzPureToolsExample", "已取消：%1").arg(card->text()));
            });
        actionLayout->addWidget(card);
    }
    layout->addWidget(actionHost);
    layout->addWidget(message);

    zzAddCardsSection(layout, QCoreApplication::translate("ZzPureToolsExample", "图片卡片"), content);
    auto *imageHost = new QWidget(content);
    auto *imageLayout = new ZzFluentUI::ZzFlowLayout(12, 12, imageHost);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    const std::array<std::array<QString, 2>, 3> imageValues{{
        {QCoreApplication::translate("ZzPureToolsExample", "低延迟界面"), QCoreApplication::translate("ZzPureToolsExample", "确定性本地预览")},
        {QCoreApplication::translate("ZzPureToolsExample", "跨平台窗口"), QCoreApplication::translate("ZzPureToolsExample", "统一 Fluent 展示")},
        {QCoreApplication::translate("ZzPureToolsExample", "模型视图"), QCoreApplication::translate("ZzPureToolsExample", "有界数据与绘制路径")},
    }};
    for (std::size_t index = 0; index < imageCards.size(); ++index) {
        const auto &value = imageValues.at(index);
        auto *card = new ZzFluentUI::ZzImageCard(
            value.at(0), value.at(1), imageHost);
        card->setCheckable(true);
        card->setMinimumSize(260, 210);
        card->setMaximumSize(320, 236);
        imageCards.at(index) = card;
        imageLayout->addWidget(card);
    }
    layout->addWidget(imageHost);

    zzAddCardsSection(layout, QCoreApplication::translate("ZzPureToolsExample", "轮播"), content);
    auto *carousel = new ZzFluentUI::ZzCarouselView(content);
    carousel->setObjectName(QStringLiteral("zzExampleCardsCarousel"));
    carousel->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "组件能力轮播"));
    carousel->setAnimationDuration(220);
    carousel->setWrapAroundEnabled(true);
    carousel->setMinimumHeight(240);
    carousel->setModel(carouselModel);
    auto *carouselStatus = new QLabel(QCoreApplication::translate("ZzPureToolsExample", "第 1 项，共 3 项"), content);
    QObject::connect(
        carousel,
        &ZzFluentUI::ZzCarouselView::currentRowChanged,
        carouselStatus,
        [carouselStatus](int row) {
            carouselStatus->setText(
                QCoreApplication::translate("ZzPureToolsExample", "第 %1 项，共 3 项").arg(row + 1));
        });
    layout->addWidget(carousel);
    layout->addWidget(carouselStatus);

    zzAddCardsSection(layout, QCoreApplication::translate("ZzPureToolsExample", "数字显示"), content);
    auto *displayRow = new QHBoxLayout;
    displayRow->setSpacing(12);
    const std::array<std::pair<QString, double>, 3> displayValues{{
        {QCoreApplication::translate("ZzPureToolsExample", "C++ 标准"), 20.0},
        {QCoreApplication::translate("ZzPureToolsExample", "Qt 主版本"), 6.0},
        {QCoreApplication::translate("ZzPureToolsExample", "页面数量"), 12.0},
    }};
    for (const auto &[labelText, value] : displayValues) {
        auto *displayHost = new QWidget(content);
        auto *displayLayout = new QVBoxLayout(displayHost);
        displayLayout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel(labelText, displayHost);
        auto *display = new QLCDNumber(4, displayHost);
        display->setAccessibleName(labelText);
        display->setSegmentStyle(QLCDNumber::Flat);
        display->display(value);
        display->setMinimumHeight(88);
        displayLayout->addWidget(label);
        displayLayout->addWidget(display);
        displayRow->addWidget(displayHost, 1);
    }
    layout->addLayout(displayRow);
    layout->addStretch(1);

    refreshPalettePreviews();
}

void ZzExampleCardsPagePrivate::refreshPalettePreviews()
{
    for (std::size_t index = 0; index < imageCards.size(); ++index) {
        auto *card = imageCards.at(index);
        if (card != nullptr) {
            card->setPixmap(zzCardsPreviewPixmap(
                card->palette(), static_cast<int>(index)));
        }
    }
}

} // namespace ZzExample
