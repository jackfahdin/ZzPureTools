#include "ZzExampleRadialGaugeHelpers.h"
#include "ZzExampleShowcasePagePrivate.h"

namespace ZzExample {
namespace {

/** @brief 装配径向报表示例页内容，不持有控件所有权。 */
class ZzExampleRadialGaugePage final
{
public:
    static void build(QVBoxLayout *mainLayout, QWidget *content);
};

} // namespace

void ZzExampleRadialGaugePage::build(QVBoxLayout *mainLayout, QWidget *content)
{
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);
    auto *description
        = new QLabel(zzGaugeText("基于 QDial 的径向仪表盘，保留范围、数值和交互能力；同一控件可组合 "
                                 "Track、Progress、Ranges 刻度环、数字标签和不同指针样式。"),
            content);
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    auto *structuresCard = makeCard(content);
    auto *structuresLayout = new QVBoxLayout(structuresCard);
    structuresLayout->setContentsMargins(16, 16, 16, 16);
    structuresLayout->setSpacing(12);
    structuresLayout->addWidget(makeSectionTitle(zzGaugeText("同一控件的三种配置"), structuresCard));

    auto *samplesLayout = new QHBoxLayout;
    samplesLayout->setSpacing(20);
    QList<ZzRadialGauge *> sampleGauges;
    const auto addSample
        = [samplesLayout, structuresCard, &sampleGauges](const QString &name, const auto &configure) {
              auto *sample = new QWidget(structuresCard);
              auto *sampleLayout = new QVBoxLayout(sample);
              sampleLayout->setContentsMargins(0, 0, 0, 0);
              sampleLayout->setSpacing(6);

              auto *gauge = new ZzRadialGauge(sample);
              gauge->setObjectName(QStringLiteral("zzRadialSample%1").arg(sampleGauges.size()));
              gauge->setFixedSize(220, 220);
              configure(gauge);

              auto *label = new QLabel(name, sample);
              label->setAlignment(Qt::AlignCenter);
              sampleLayout->addWidget(gauge, 0, Qt::AlignHCenter);
              sampleLayout->addWidget(label);
              samplesLayout->addWidget(sample, 1);
              sampleGauges.append(gauge);
          };

    addSample(zzGaugeText("经典指针"), configureClassicGauge);
    addSample(zzGaugeText("进度指针"), configureProgressGauge);
    addSample(zzGaugeText("彩色区间"), configureSpeedometerGauge);
    structuresLayout->addLayout(samplesLayout);

    auto *sharedValueLayout = new QHBoxLayout;
    auto *sharedValueLabel = new QLabel(zzGaugeText("公共数值"), structuresCard);
    auto *sharedValueSlider = makeValueSlider(structuresCard, 0, 100, 70);
    sharedValueSlider->setObjectName(QStringLiteral("zzRadialSharedValue"));
    sharedValueLayout->addWidget(sharedValueLabel);
    sharedValueLayout->addWidget(sharedValueSlider, 1);
    structuresLayout->addLayout(sharedValueLayout);

    QList<int> sampleAnimationDurations;
    sampleAnimationDurations.reserve(sampleGauges.size());
    for (ZzRadialGauge *sampleGauge : std::as_const(sampleGauges)) {
        sampleAnimationDurations.append(sampleGauge->valueAnimationDuration());
    }

    for (ZzRadialGauge *sampleGauge : std::as_const(sampleGauges)) {
        sampleGauge->setValue(sharedValueSlider->value());
        QObject::connect(sampleGauge, &QDial::sliderPressed, structuresCard, [sampleGauges] {
            for (ZzRadialGauge *gauge : sampleGauges) {
                gauge->setValueAnimationDuration(0);
            }
        });
        QObject::connect(
            sampleGauge, &QDial::sliderReleased, structuresCard, [sampleGauges, sampleAnimationDurations] {
                for (qsizetype index = 0; index < sampleGauges.size(); ++index) {
                    sampleGauges.at(index)->setValueAnimationDuration(sampleAnimationDurations.at(index));
                }
            });
        QObject::connect(sharedValueSlider, &QSlider::valueChanged, sampleGauge, &ZzRadialGauge::setValue);
        QObject::connect(sampleGauge, &QDial::valueChanged, sharedValueSlider,
            [sampleGauge, sampleGauges, sharedValueSlider](int value) {
                if (sharedValueSlider->isSliderDown() || sampleGauge->isValueAnimating()) {
                    return;
                }

                const QSignalBlocker blocker(sharedValueSlider);
                sharedValueSlider->setValue(value);
                for (ZzRadialGauge *otherGauge : sampleGauges) {
                    if (otherGauge != sampleGauge) {
                        if (sampleGauge->isSliderDown()) {
                            const QSignalBlocker otherGaugeBlocker(otherGauge);
                            otherGauge->setValue(value);
                        } else {
                            otherGauge->setValue(value);
                        }
                    }
                }
            });
    }
    mainLayout->addWidget(structuresCard);

    auto *echartsCard = makeCard(content);
    auto *echartsLayout = new QVBoxLayout(echartsCard);
    echartsLayout->setContentsMargins(16, 16, 16, 16);
    echartsLayout->setSpacing(12);
    echartsLayout->addWidget(makeSectionTitle(zzGaugeText("ECharts 仪表盘配置"), echartsCard));

    auto *echartsDescription = new QLabel(zzGaugeText("单值示例由 ZzRadialGauge 的属性组合；多标题示例使用 "
                                                      "ZzMultiRadialGauge，共享刻度并绘制多条进度和指针。"),
        echartsCard);
    echartsDescription->setWordWrap(true);
    echartsLayout->addWidget(echartsDescription);

    auto *echartsGrid = new ZzFlowLayout(20, 18);
    QList<ZzRadialGauge *> echartsGauges;
    const auto addEChartsSample
        = [&](const QString &title, const QString &subtitle, const auto &createVisual) {
              auto *sample = new QWidget(echartsCard);
              auto *sampleLayout = new QVBoxLayout(sample);
              sampleLayout->setContentsMargins(4, 4, 4, 4);
              sampleLayout->setSpacing(4);

              QWidget *visual = createVisual(sample);
              sampleLayout->addWidget(visual, 0, Qt::AlignHCenter);

              auto *sampleTitle = new QLabel(title, sample);
              QFont sampleTitleFont = sampleTitle->font();
              sampleTitleFont.setBold(true);
              sampleTitle->setFont(sampleTitleFont);
              sampleTitle->setAlignment(Qt::AlignHCenter);
              sampleLayout->addWidget(sampleTitle);

              auto *sampleSubtitle = new QLabel(subtitle, sample);
              sampleSubtitle->setProperty("isSecondaryText", true);
              sampleSubtitle->setAlignment(Qt::AlignHCenter);
              sampleLayout->addWidget(sampleSubtitle);

              sample->setFixedWidth(228);
              echartsGrid->addWidget(sample);
          };

    const auto addEChartsGauge
        = [&](const QString &title, const QString &subtitle, void (*configure)(ZzRadialGauge *)) {
              addEChartsSample(title, subtitle, [configure, &echartsGauges](QWidget *parent) -> QWidget * {
                  auto *gauge = new ZzRadialGauge(parent);
                  gauge->setFixedSize(220, 220);
                  configure(gauge);
                  echartsGauges.append(gauge);
                  return gauge;
              });
          };

    addEChartsGauge(
        zzGaugeText("基础仪表盘"), QStringLiteral("Gauge Basic chart"), configureEChartsBasicGauge);
    addEChartsGauge(zzGaugeText("简单仪表盘"), QStringLiteral("Simple Gauge"), configureEChartsSimpleGauge);
    addEChartsGauge(zzGaugeText("速度仪表盘"), QStringLiteral("Speed Gauge"), configureEChartsSpeedGauge);
    addEChartsGauge(
        zzGaugeText("进度仪表盘"), QStringLiteral("Progress Gauge"), configureEChartsProgressGauge);
    addEChartsGauge(
        zzGaugeText("阶段速度仪表盘"), QStringLiteral("Stage Speed Gauge"), configureEChartsStageGauge);
    addEChartsGauge(zzGaugeText("等级仪表盘"), QStringLiteral("Grade Gauge"), configureEChartsGradeGauge);

    ZzMultiRadialGaugeItem *goodGaugeItem = nullptr;
    ZzMultiRadialGaugeItem *betterGaugeItem = nullptr;
    ZzMultiRadialGaugeItem *perfectGaugeItem = nullptr;
    addEChartsSample(zzGaugeText("多标题仪表盘"), QStringLiteral("Multi Title Gauge"),
        [&goodGaugeItem, &betterGaugeItem, &perfectGaugeItem](QWidget *parent) -> QWidget * {
            auto *gauge = new ZzMultiRadialGauge(parent);
            gauge->setFixedSize(220, 220);
            configureEChartsMultiTitleGauge(gauge);
            goodGaugeItem = gauge->addItem(QStringLiteral("Good"), 20.0, QColor(QStringLiteral("#5470C6")));
            betterGaugeItem
                = gauge->addItem(QStringLiteral("Better"), 40.0, QColor(QStringLiteral("#B8DE29")));
            perfectGaugeItem
                = gauge->addItem(QStringLiteral("Perfect"), 60.0, QColor(QStringLiteral("#555672")));
            goodGaugeItem->setTitleOffset(QPointF(-0.4, 0.8));
            goodGaugeItem->setDetailOffset(QPointF(-0.4, 0.95));
            betterGaugeItem->setTitleOffset(QPointF(0.0, 0.8));
            betterGaugeItem->setDetailOffset(QPointF(0.0, 0.95));
            perfectGaugeItem->setTitleOffset(QPointF(0.4, 0.8));
            perfectGaugeItem->setDetailOffset(QPointF(0.4, 0.95));
            return gauge;
        });

    addEChartsGauge(
        zzGaugeText("气温仪表盘"), QStringLiteral("Temperature Gauge"), configureEChartsTemperatureGauge);

    ZzMultiProgressRingItem *perfectRingItem = nullptr;
    ZzMultiProgressRingItem *goodRingItem = nullptr;
    ZzMultiProgressRingItem *commonRingItem = nullptr;
    addEChartsSample(zzGaugeText("得分环"), QStringLiteral("Ring Gauge"),
        [&perfectRingItem, &goodRingItem, &commonRingItem](QWidget *parent) -> QWidget * {
            auto *ring = new ZzMultiProgressRing(parent);
            ring->setFixedSize(220, 220);
            ring->setRingWidth(8.0);
            ring->setRingSpacing(6.0);
            ring->setRingPadding(13.0);
            ring->setCapStyle(Qt::RoundCap);
            ring->setTrackVisible(false);
            perfectRingItem
                = ring->addItem(QStringLiteral("Perfect"), 20.0, QColor(QStringLiteral("#5470C6")));
            goodRingItem = ring->addItem(QStringLiteral("Good"), 40.0, QColor(QStringLiteral("#B8DE29")));
            commonRingItem
                = ring->addItem(QStringLiteral("Commonly"), 60.0, QColor(QStringLiteral("#5C5F7A")));
            return ring;
        });

    addEChartsGauge(
        zzGaugeText("气压表"), QStringLiteral("Gauge Barometer chart"), configureEChartsBarometerGauge);

    echartsLayout->addLayout(echartsGrid);

    auto *echartsValueLayout = new QHBoxLayout;
    auto *echartsValueLabel = new QLabel(zzGaugeText("公共百分比（拖动后同步）"), echartsCard);
    auto *echartsValueSlider = makeValueSlider(echartsCard, 0, 100, 60);
    echartsValueSlider->setObjectName(QStringLiteral("zzRadialPresetValue"));
    echartsValueLayout->addWidget(echartsValueLabel);
    echartsValueLayout->addWidget(echartsValueSlider, 1);
    echartsLayout->addLayout(echartsValueLayout);

    for (ZzRadialGauge *gauge : std::as_const(echartsGauges)) {
        QObject::connect(echartsValueSlider, &QSlider::valueChanged, gauge, [gauge](int percent) {
            const qreal range = static_cast<qreal>(gauge->maximum()) - gauge->minimum();
            gauge->setValue(gauge->minimum() + qRound(range * percent / 100.0));
        });
    }
    QObject::connect(echartsValueSlider, &QSlider::valueChanged, echartsCard, [=](int percent) {
        perfectRingItem->setValue(qMax(0, percent - 40));
        goodRingItem->setValue(qMax(0, percent - 20));
        commonRingItem->setValue(percent);
        goodGaugeItem->setValue(qMax(0, percent - 40));
        betterGaugeItem->setValue(qMax(0, percent - 20));
        perfectGaugeItem->setValue(percent);
    });
    mainLayout->addWidget(echartsCard);

    auto *propertiesCard = makeCard(content);
    auto *propertiesLayout = new QVBoxLayout(propertiesCard);
    propertiesLayout->setContentsMargins(16, 16, 16, 16);
    propertiesLayout->setSpacing(12);
    propertiesLayout->addWidget(makeSectionTitle(zzGaugeText("实时属性"), propertiesCard));

    auto *livePropertyTabs = makePropertyTabs(propertiesCard);
    livePropertyTabs->setObjectName(QStringLiteral("zzRadialPropertyTabs"));

    livePropertyTabs->setMinimumHeight(640);

    auto *gaugePropertyPage = new QWidget(livePropertyTabs);
    auto *gaugePropertyLayout = new QVBoxLayout(gaugePropertyPage);
    gaugePropertyLayout->setContentsMargins(12, 12, 12, 12);
    gaugePropertyLayout->setSpacing(12);

    auto *ringPropertyPage = new QWidget(livePropertyTabs);
    auto *ringPropertyLayout = new QVBoxLayout(ringPropertyPage);
    ringPropertyLayout->setContentsMargins(12, 12, 12, 12);
    ringPropertyLayout->setSpacing(12);

    auto *multiGaugePropertyPage = new QWidget(livePropertyTabs);
    auto *multiGaugePropertyLayout = new QVBoxLayout(multiGaugePropertyPage);
    multiGaugePropertyLayout->setContentsMargins(12, 12, 12, 12);
    multiGaugePropertyLayout->setSpacing(12);

    livePropertyTabs->addTab(gaugePropertyPage, QStringLiteral("ZzRadialGauge"));
    livePropertyTabs->addTab(ringPropertyPage, zzGaugeText("得分环"));
    livePropertyTabs->addTab(multiGaugePropertyPage, QStringLiteral("ZzMultiRadialGauge"));
    propertiesLayout->addWidget(livePropertyTabs);

    buildRadialGaugeEditor(propertiesCard, gaugePropertyPage, gaugePropertyLayout);
    buildRadialRingEditor(ringPropertyPage, ringPropertyLayout);
    buildMultiRadialGaugeEditor(multiGaugePropertyPage, multiGaugePropertyLayout);
    mainLayout->addWidget(propertiesCard);
    mainLayout->addStretch();
}

void ZzExampleShowcasePagePrivate::buildRadialGauge(QVBoxLayout *mainLayout, QWidget *content)
{
    ZzExampleRadialGaugePage::build(mainLayout, content);
}

} // namespace ZzExample
