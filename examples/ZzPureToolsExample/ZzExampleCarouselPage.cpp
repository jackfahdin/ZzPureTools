#include "ZzExampleCarouselPlayback.h"
#include "ZzExampleRadialGaugeHelpers.h"
#include "ZzExampleShowcasePagePrivate.h"

#include <QDoubleSpinBox>
#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QSpinBox>
#include <QStandardItemModel>
#include <array>

namespace ZzExample {
namespace {
    using ZzFluentUI::ZzCarouselView;
    constexpr std::array<const char*, 5> imageNames { "cliff", "grapes", "rainier", "sunset",
        "valley" };
    constexpr std::array<const char*, 5> imageTitles { "海岸峭壁", "新鲜葡萄", "雷尼尔雪山",
        "金色晚霞", "群山峡谷" };
    constexpr std::array<const char*, 5> imageDescriptions { "沿着海岸，寻找下一段旅程。",
        "阳光下的果实与自然色彩。", "远山、白雪与辽阔的天空。", "在一天结束时，留住温暖的光。",
        "走进山谷，感受宁静与开阔。" };

    /** @brief 页面模型负责读取可选素材；缺图时生成随主题变化的本地预览。 */
    class ZzCarouselPreviewModel final : public QStandardItemModel {
    public:
        explicit ZzCarouselPreviewModel(ZzCarouselView* view, bool photos, QObject* owner)
            : QStandardItemModel(photos ? 5 : 3, 1, owner)
            , view_(view)
        {
            for (int row = 0; row < rowCount(); ++row) {
                setData(index(row, 0),
                    photos ? zzGaugeText(imageTitles.at(static_cast<size_t>(row)))
                           : zzGaugeText("灵感卡片 %1").arg(row + 1));
                setData(index(row, 0),
                    photos ? zzGaugeText(imageDescriptions.at(static_cast<size_t>(row)))
                           : zzGaugeText("图片、标题和说明可以自由组合。"),
                    ZzCarouselView::DescriptionRole);
                if (photos)
                    images_.at(static_cast<size_t>(row)).load(QStringLiteral(":/ZzPureToolsExample/carousel/%1.jpg")
                            .arg(QString::fromLatin1(imageNames.at(static_cast<size_t>(row)))));
            }
            refresh();
            view->installEventFilter(this);
        }

    protected:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if (event->type() == QEvent::PaletteChange)
                refresh();
            return QStandardItemModel::eventFilter(object, event);
        }

    private:
        void refresh()
        {
            for (int row = 0; row < rowCount(); ++row) {
                QPixmap picture = images_.at(static_cast<size_t>(row));
                if (picture.isNull()) {
                    picture = QPixmap(960, 540);
                    QPainter painter(&picture);
                    const auto palette = view_->palette();
                    const bool dark = palette.color(QPalette::Window).lightness() < 128;
                    QColor accent = palette.color(QPalette::Highlight);
                    accent = QColor::fromHsl(
                        (accent.hslHue() + row * 43 + 360) % 360, dark ? 95 : 135, dark ? 55 : 155);
                    QLinearGradient gradient(0, 0, 960, 540);
                    gradient.setColorAt(0, accent);
                    gradient.setColorAt(1, palette.color(QPalette::Window));
                    painter.fillRect(picture.rect(), gradient);
                    painter.setRenderHint(QPainter::Antialiasing);
                    QColor glow = palette.color(QPalette::WindowText);
                    glow.setAlpha(18);
                    painter.setBrush(glow);
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(QPointF(710, 100), 210, 210);
                    painter.drawEllipse(QPointF(340, 560), 340, 260);
                }
                setData(index(row, 0), picture, Qt::DecorationRole);
            }
        }
        ZzCarouselView* view_;
        std::array<QPixmap, 5> images_;
    };

    QFormLayout* carouselForm(QWidget* parent)
    {
        auto* form = new QFormLayout(parent);
        form->setContentsMargins(12, 12, 12, 12);
        form->setVerticalSpacing(10);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        return form;
    }
    /** @brief 装配轮播示例页内容，不持有控件所有权。 */
    class ZzExampleCarouselPage final
    {
    public:
        static void build(ZzExampleShowcasePagePrivate *host, QVBoxLayout* mainLayout, QWidget* content);
    };

} // namespace

void ZzExampleCarouselPage::build(ZzExampleShowcasePagePrivate *host, QVBoxLayout* mainLayout, QWidget* content)
{
    auto* hint = new QLabel(
        zzGaugeText("图文轮播、悬停导航与可点击分页。支持键盘方向键和滚轮切换。"), content);
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);
    auto* card = makeCard(content);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeSectionTitle(zzGaugeText("图文轮播"), card));
    auto* view = new ZzCarouselView(card);
    view->setObjectName(QStringLiteral("carouselPreview"));
    view->setAccessibleName(zzGaugeText("风景轮播"));
    view->setFixedHeight(280);
    view->setModel(new ZzCarouselPreviewModel(view, true, host->q_ptr));
    layout->addWidget(view);
    auto* playback = new ZzExampleCarouselPlayback(view, card);
    playback->setObjectName(QStringLiteral("carouselPlayback"));

    auto* status = new QLabel(card);
    status->setObjectName(QStringLiteral("carouselStatus"));
    const auto refreshStatus = [view, status] {
        status->setText(zzGaugeText("当前第 %1 / %2 张")
                .arg(view->currentRow() + 1)
                .arg(view->model()->rowCount()));
    };
    QObject::connect(view, &ZzCarouselView::currentRowChanged, status, refreshStatus);
    refreshStatus();
    layout->addWidget(status);
    auto* commands = new QHBoxLayout;
    auto* previous = new QPushButton(zzGaugeText("上一张"), card);
    previous->setObjectName(QStringLiteral("carouselPrevious"));
    auto* next = new QPushButton(zzGaugeText("下一张"), card);
    next->setObjectName(QStringLiteral("carouselNext"));
    QObject::connect(previous, &QPushButton::clicked, view, &ZzCarouselView::showPrevious);
    QObject::connect(next, &QPushButton::clicked, view, &ZzCarouselView::showNext);
    commands->addWidget(previous);
    commands->addWidget(next);
    commands->addStretch();
    layout->addLayout(commands);

    auto* tabs = makePropertyTabs(card);
    tabs->setObjectName(QStringLiteral("carouselEditorTabs"));
    auto* behavior = new QWidget(tabs);
    auto* appearance = new QWidget(tabs);
    auto* behaviorForm = carouselForm(behavior);
    auto* appearanceForm = carouselForm(appearance);
    tabs->addTab(behavior, zzGaugeText("播放与交互"));
    tabs->addTab(appearance, zzGaugeText("显示与布局"));
    const auto makeCheck = [](QFormLayout* form, const char* name, const char* text) {
        auto* check = new QCheckBox(zzGaugeText(text), form->parentWidget());
        check->setObjectName(QString::fromLatin1(name));
        form->addRow(check);
        return check;
    };
    auto* play = makeCheck(behaviorForm, "carouselAutoPlay", "自动播放");
    auto* wrap = makeCheck(behaviorForm, "carouselWrap", "首尾循环");
    auto* hover = makeCheck(behaviorForm, "carouselPauseOnHover", "悬停时暂停播放");
    auto* interval = new QSpinBox(behavior);
    interval->setObjectName(QStringLiteral("carouselInterval"));
    interval->setRange(500, 20000);
    interval->setSingleStep(500);
    interval->setSuffix(QStringLiteral(" ms"));
    behaviorForm->addRow(zzGaugeText("轮播间隔"), interval);
    auto* duration = new QSpinBox(behavior);
    duration->setObjectName(QStringLiteral("carouselDuration"));
    duration->setRange(0, 1000);
    duration->setSingleStep(50);
    duration->setSuffix(QStringLiteral(" ms"));
    behaviorForm->addRow(zzGaugeText("切换时长"), duration);
    auto* disabled = makeCheck(behaviorForm, "carouselDisabled", "禁用轮播");
    auto* immersive = makeCheck(appearanceForm, "carouselImmersive", "沉浸式图片");
    auto* indicators = makeCheck(appearanceForm, "carouselIndicators", "显示分页点");
    auto* navigation = makeCheck(appearanceForm, "carouselNavigation", "显示切换按钮");
    auto* trigger = new QComboBox(appearance);
    trigger->setObjectName(QStringLiteral("carouselTrigger"));
    trigger->addItem(zzGaugeText("悬停或聚焦时显示"), ZzCarouselView::OnHover);
    trigger->addItem(zzGaugeText("始终显示"), ZzCarouselView::AlwaysVisible);
    appearanceForm->addRow(zzGaugeText("按钮显隐"), trigger);
    auto* radius = new QDoubleSpinBox(appearance);
    radius->setObjectName(QStringLiteral("carouselRadius"));
    radius->setRange(0, 48);
    radius->setDecimals(1);
    appearanceForm->addRow(zzGaugeText("圆角半径"), radius);
    auto* aspect = new QComboBox(appearance);
    aspect->setObjectName(QStringLiteral("carouselAspect"));
    aspect->addItem(zzGaugeText("等比填满（裁剪）"), Qt::KeepAspectRatioByExpanding);
    aspect->addItem(zzGaugeText("完整显示（留边）"), Qt::KeepAspectRatio);
    aspect->addItem(zzGaugeText("拉伸填满"), Qt::IgnoreAspectRatio);
    appearanceForm->addRow(zzGaugeText("图片适配"), aspect);
    auto* rtl = makeCheck(appearanceForm, "carouselRtl", "从右向左布局");
    QObject::connect(play, &QCheckBox::toggled, playback, &ZzExampleCarouselPlayback::setEnabled);
    QObject::connect(wrap, &QCheckBox::toggled, view, &ZzCarouselView::setWrapAroundEnabled);
    QObject::connect(
        hover, &QCheckBox::toggled, playback, &ZzExampleCarouselPlayback::setPauseOnHover);
    QObject::connect(
        interval, &QSpinBox::valueChanged, playback, &ZzExampleCarouselPlayback::setInterval);
    QObject::connect(
        duration, &QSpinBox::valueChanged, view, &ZzCarouselView::setAnimationDuration);
    QObject::connect(
        disabled, &QCheckBox::toggled, view, [view](bool value) { view->setEnabled(!value); });
    QObject::connect(immersive, &QCheckBox::toggled, view, &ZzCarouselView::setImmersive);
    QObject::connect(indicators, &QCheckBox::toggled, view, &ZzCarouselView::setShowIndicators);
    QObject::connect(
        navigation, &QCheckBox::toggled, view, &ZzCarouselView::setShowNavigationButtons);
    QObject::connect(trigger, &QComboBox::currentIndexChanged, view, [view, trigger] {
        view->setNavigationButtonTrigger(
            static_cast<ZzCarouselView::ZzNavigationButtonTrigger>(trigger->currentData().toInt()));
    });
    QObject::connect(radius, &QDoubleSpinBox::valueChanged, view, &ZzCarouselView::setBorderRadius);
    QObject::connect(aspect, &QComboBox::currentIndexChanged, view, [view, aspect] {
        view->setImageAspectRatioMode(
            static_cast<Qt::AspectRatioMode>(aspect->currentData().toInt()));
    });
    QObject::connect(rtl, &QCheckBox::toggled, view, [view](bool value) {
        view->setLayoutDirection(value ? Qt::RightToLeft : Qt::LeftToRight);
    });
    layout->addWidget(tabs);
    auto* reset = new QPushButton(zzGaugeText("重置属性"), card);
    reset->setObjectName(QStringLiteral("carouselReset"));
    const auto resetState = [=] {
        play->setChecked(true);
        wrap->setChecked(true);
        hover->setChecked(true);
        interval->setValue(3500);
        duration->setValue(550);
        disabled->setChecked(false);
        immersive->setChecked(true);
        indicators->setChecked(true);
        navigation->setChecked(true);
        trigger->setCurrentIndex(0);
        view->setNavigationButtonTrigger(ZzCarouselView::OnHover);
        radius->setValue(6.0);
        aspect->setCurrentIndex(0);
        view->setImageAspectRatioMode(Qt::KeepAspectRatioByExpanding);
        rtl->setChecked(false);
        view->setCurrentRow(0);
    };
    QObject::connect(reset, &QPushButton::clicked, card, resetState);
    resetState();
    layout->addWidget(reset, 0, Qt::AlignLeft);
    mainLayout->addWidget(card);

    auto* textCard = makeCard(content);
    auto* textLayout = new QVBoxLayout(textCard);
    textLayout->setContentsMargins(16, 16, 16, 16);
    textLayout->addWidget(makeSectionTitle(zzGaugeText("文字卡片轮播"), textCard));
    auto* textView = new ZzCarouselView(textCard);
    textView->setObjectName(QStringLiteral("carouselCards"));
    textView->setFixedHeight(190);
    textView->setModel(new ZzCarouselPreviewModel(textView, false, host->q_ptr));
    textView->setWrapAroundEnabled(true);
    textLayout->addWidget(textView);
    mainLayout->addWidget(textCard);
    auto* code = new QPlainTextEdit(content);
    code->setReadOnly(true);
    code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    code->setPlainText(
        QStringLiteral("auto *view = new ZzFluentUI::ZzCarouselView(parent);\n"
                       "view->setModel(model);\n"
                       "view->setImmersive(true);\n"
                       "view->setWrapAroundEnabled(true);\n"
                       "view->setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::OnHover);\n"
                       "view->setImageAspectRatioMode(Qt::KeepAspectRatioByExpanding);\n"
                       "view->setBorderRadius(6.0);"));
    code->setFixedHeight(180);
    mainLayout->addWidget(code);
}

void ZzExampleShowcasePagePrivate::buildCarousel(QVBoxLayout* mainLayout, QWidget* content)
{
    ZzExampleCarouselPage::build(this, mainLayout, content);
}

} // namespace ZzExample
