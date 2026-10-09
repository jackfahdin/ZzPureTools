#include "ZzColorPickerPrivate.h"
#include "ZzColorPickerFluent.h"
#include "ZzColorSpectrum.h"
#include "ZzColorGradientSlider.h"
#include "ZzColorShadeStrip.h"
#include "ZzColorPickerMath.h"
#include "ZzColorRepresentationCombo.h"
#include "ZzColorPickerMetrics.h"

#include <QtCore/QSignalBlocker>
#include <QtCore/QPointer>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

void ZzColorPickerFluent::configureTabs(ZzTabBar *tabs)
{
    tabs->setAppearance(ZzTabBarAppearance::SegmentedWinUI3);
    tabs->setTearOffEnabled(false);
    tabs->setTabTransferEnabled(false);
    tabs->setMovable(false);
    tabs->setExpanding(true);
    tabs->setDrawBase(false);
    tabs->setFixedHeight(ZzColorPickerTabsHeight);
    tabs->setIconSize(QSize(14, 14));
    tabs->addTab(ZzSegoeIconFont::icon(ZzSegoeIcon::InkingTool), QString());
    tabs->addTab(ZzSegoeIconFont::icon(ZzSegoeIcon::Color), QString());
    tabs->addTab(ZzSegoeIconFont::icon(ZzSegoeIcon::Equalizer), QString());
}

void ZzColorPickerPrivate::buildFluentPresentation()
{
    fluentHost = new QWidget(q_ptr);
    fluentHost->setObjectName(QStringLiteral("zzColorPickerFluent"));
    fluentLayout = new QVBoxLayout(fluentHost);
    fluentLayout->setContentsMargins(ZzColorPickerPadding, ZzColorPickerPadding,
                                    ZzColorPickerPadding, ZzColorPickerPadding);
    fluentLayout->setSpacing(ZzColorPickerSpacing);
    shadeStrip = new ZzColorShadeStrip(fluentHost);
    fluentLayout->addWidget(shadeStrip);
    auto *segmentedTabs = new ZzTabBar(fluentHost);
    ZzColorPickerFluent::configureTabs(segmentedTabs);
    tabs = segmentedTabs;
    tabs->setObjectName(QStringLiteral("zzColorPickerTabs"));
    fluentLayout->addWidget(tabs);
    pages = new QStackedWidget(fluentHost);
    pages->setObjectName(QStringLiteral("zzColorPickerPages"));
    spectrumPage = new QWidget(pages);
    palettePage = new QWidget(pages);
    slidersPage = new QWidget(pages);
    auto *spectrumLayout = new QHBoxLayout(spectrumPage);
    spectrumLayout->setContentsMargins(0, 0, 0, 0);
    spectrumLayout->setSpacing(ZzColorPickerSpacing);
    spectrum = new ZzColorSpectrum(spectrumPage);
    valueSlider = new ZzColorGradientSlider(Qt::Vertical, spectrumPage);
    valueSlider->setObjectName(QStringLiteral("zzColorValueSlider"));
    alphaSlider = new ZzColorGradientSlider(Qt::Vertical, spectrumPage);
    alphaSlider->setObjectName(QStringLiteral("zzColorAlphaSlider"));
    spectrumLayout->addWidget(valueSlider);
    spectrumLayout->addWidget(spectrum, 1);
    spectrumLayout->addWidget(alphaSlider);
    auto *paletteLayout = new QVBoxLayout(palettePage);
    paletteLayout->setContentsMargins(0, 0, 0, 0);
    auto *slidersLayout = new QVBoxLayout(slidersPage);
    slidersLayout->setContentsMargins(0, 0, 0, 0);
    representationCombo = new ZzColorRepresentationCombo(editorHost);
    representationCombo->setObjectName(QStringLiteral("zzColorRepresentationCombo"));
    representationCombo->addItems({QStringLiteral("RGB"), QStringLiteral("HSV")});
    slidersLayout->addStretch();
    for (int index = 0; index < 4; ++index) {
        channelSliders[index] = new ZzColorGradientSlider(Qt::Horizontal, editorHost);
        channelSliders[index]->setObjectName(QStringLiteral("zzColorChannelSlider%1").arg(index));
        QObject::connect(channelSliders[index], &QAbstractSlider::valueChanged, q_ptr,
            [this, index](int channelValue) {
                if (syncing) {
                    return;
                }
                const QList<ZzSpinBox *> editors{redSpinBox, greenSpinBox, blueSpinBox, alphaSpinBox};
                editors.at(index)->setValue(channelValue);
            });
    }
    pages->addWidget(spectrumPage);
    pages->addWidget(palettePage);
    pages->addWidget(slidersPage);
    fluentLayout->addWidget(pages, 1);
    q_ptr->layout()->addWidget(fluentHost);
    QObject::connect(tabs, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
    QObject::connect(representationCombo, &QComboBox::currentIndexChanged, q_ptr,
        [this](int index) {
            q_ptr->setColorRepresentation(index == 0 ? ZzColorPicker::Rgba : ZzColorPicker::Hsva);
        });
    spectrum->edited = [this](qreal h, qreal s) { commitHsv(h, s, value); };
    shadeStrip->selected = [this](QColor color) { q_ptr->setCurrentColor(color); };
    QObject::connect(valueSlider, &QAbstractSlider::valueChanged, q_ptr, [this](int v) {
        if (!syncing) {
            commitHsv(hue, saturation, v / 255.0);
        }
    });
    QObject::connect(alphaSlider, &QAbstractSlider::valueChanged, q_ptr, [this](int a) {
        if (!syncing && alphaEnabled) {
            QColor color = currentColor;
            color.setAlpha(a);
            q_ptr->setCurrentColor(color);
        }
    });
}

void ZzColorPickerPrivate::syncAppearance()
{
    const bool fluent = appearance == ZzColorPicker::Fluent;
    // Reparent the same model/view and editors; no QObject is created on switching.
    auto *previewWidget = preview;
    if (fluent) {
        fluentLayout->insertWidget(0, previewWidget);
        palettePage->layout()->addWidget(paletteView);
        static_cast<QVBoxLayout *>(slidersPage->layout())->insertWidget(0, editorHost);
        previewWidget->setFixedHeight(ZzColorPickerPreviewHeight);
    } else {
        compactLayout->insertWidget(0, previewWidget);
        compactLayout->insertWidget(1, paletteView);
        compactLayout->addWidget(editorHost);
        previewWidget->setFixedHeight(ZzColorPickerCompactPreviewHeight);
    }
    const QList<QWidget *> elements{redLabel, greenLabel, blueLabel, alphaLabel,
        redSpinBox, greenSpinBox, blueSpinBox, alphaSpinBox, hexLabel, hexEditor};
    for (QWidget *element : elements) {
        editorLayout->removeWidget(element);
    }
    for (auto *slider : channelSliders) {
        editorLayout->removeWidget(slider);
        slider->setVisible(fluent);
    }
    const QList<QLabel *> labels{redLabel, greenLabel, blueLabel, alphaLabel};
    const QList<ZzSpinBox *> editors{redSpinBox, greenSpinBox, blueSpinBox, alphaSpinBox};
    for (int index = 0; index < 4; ++index) {
        auto *editor = editors.at(index);
        editor->setButtonSymbols(fluent ? QAbstractSpinBox::NoButtons : QAbstractSpinBox::UpDownArrows);
        editor->setFixedWidth(fluent ? ZzColorPickerChannelWidth : ZzColorPickerCompactChannelWidth);
        editorLayout->addWidget(labels.at(index), fluent ? index + 1 : 0, fluent ? 0 : index);
        editorLayout->addWidget(editor, fluent ? index + 1 : 1, fluent ? 1 : index);
        if (fluent) {
            editorLayout->addWidget(channelSliders[index], index + 1, 2, 1, 3);
        }
    }
    editorLayout->removeWidget(representationCombo);
    representationCombo->setVisible(fluent);
    if (fluent) {
        editorLayout->addWidget(representationCombo, 0, 0, 1, 2);
        editorLayout->addWidget(hexEditor, 0, 2, 1, 3);
        hexLabel->hide();
    } else {
        compactLayout->addWidget(hexLabel);
        compactLayout->addWidget(hexEditor);
    }
    editorLayout->setColumnStretch(4, 1);
    compactHost->setVisible(!fluent);
    fluentHost->setVisible(fluent);
    previewWidget->show();
    paletteView->show();
    editorHost->show();
    syncPaletteMetrics();
    syncDerivedState();
    syncVisibility();
}

void ZzColorPickerPrivate::syncVisibility()
{
    if (!tabs) {
        return;
    }
    const bool fluent = appearance == ZzColorPicker::Fluent;
    const bool visible[]{spectrumVisible, paletteVisible, sliderVisible};
    for (int index = 0; index < 3; ++index) {
        // Qt 6.11 clears QTabBar's pending layout flag on a no-op visibility
        // assignment. Preserve it while other tabs are being restored.
        if (tabs->isTabVisible(index) != visible[index]) {
            tabs->setTabVisible(index, visible[index]);
        }
    }
    int selected = tabs->currentIndex();
    if (selected < 0 || !visible[selected]) {
        selected = -1;
        for (int index = 0; index < 3; ++index) {
            if (visible[index]) {
                selected = index;
                break;
            }
        }
        tabs->setCurrentIndex(selected);
    }
    tabs->setVisible(selected >= 0);
    pages->setVisible(selected >= 0);
    if (selected >= 0) {
        pages->setCurrentIndex(selected);
    }
    preview->setVisible(previewVisible);
    shadeStrip->setVisible(previewVisible);
    alphaSlider->setVisible(alphaEnabled && alphaSliderVisible);
    paletteView->setVisible(fluent || paletteVisible);
    editorHost->setVisible(fluent || sliderVisible);
    const QList<QWidget *> inputs{redSpinBox, greenSpinBox, blueSpinBox, hexEditor};
    for (auto *input : inputs) {
        input->setVisible(channelTextInputVisible);
    }
    alphaLabel->setVisible(alphaEnabled);
    hexLabel->setVisible(!fluent && channelTextInputVisible);
    alphaSpinBox->setVisible(alphaEnabled && channelTextInputVisible);
    channelSliders[3]->setVisible(fluent && alphaEnabled && alphaSliderVisible);
}

void ZzColorPickerPrivate::commitHsv(qreal h, qreal s, qreal v)
{
    hue = h;
    saturation = s;
    value = v;
    const QPointer<ZzColorPicker> guard(q_ptr);
    q_ptr->setCurrentColor(zzColorFromHsv(h, s, v, currentColor.alphaF()));
    if (guard) {
        syncDerivedState();
    }
}

void ZzColorPickerPrivate::syncFluentState()
{
    if (!spectrum) {
        return;
    }
    const bool hsv = appearance == ZzColorPicker::Fluent && representation == ZzColorPicker::Hsva;
    const QSignalBlocker comboBlocker(representationCombo);
    representationCombo->setCurrentIndex(representation == ZzColorPicker::Hsva ? 1 : 0);
    const QList<QLabel *> labels{redLabel, greenLabel, blueLabel, alphaLabel};
    const QList<ZzSpinBox *> editors{redSpinBox, greenSpinBox, blueSpinBox, alphaSpinBox};
    const QStringList names = hsv
        ? QStringList{ZzColorPicker::tr("色相"), ZzColorPicker::tr("饱和度"), ZzColorPicker::tr("明度"), ZzColorPicker::tr("透明度")}
        : QStringList{ZzColorPicker::tr("红色"), ZzColorPicker::tr("绿色"), ZzColorPicker::tr("蓝色"), ZzColorPicker::tr("透明度")};
    if (hsv) {
        editors[0]->setValue(qRound(hue * 360));
        editors[1]->setValue(qRound(saturation * 100));
        editors[2]->setValue(qRound(value * 100));
    }
    for (int index = 0; index < 4; ++index) {
        const bool fluent = appearance == ZzColorPicker::Fluent;
        labels[index]->setText(fluent ? QString(hsv ? "HSVA" : "RGBA").mid(index, 1) : names[index]);
        labels[index]->setBuddy(editors[index]);
        editors[index]->setAccessibleName(names[index]);
        auto *slider = channelSliders[index];
        const QSignalBlocker blocker(slider);
        slider->setRange(0, editors[index]->maximum());
        slider->setValue(editors[index]->value());
        slider->setAccessibleName(names[index]);
        QList<QColor> stops;
        const int count = hsv && index == 0 ? 7 : 2;
        for (int stop = 0; stop < count; ++stop) {
            const qreal t = stop / qreal(count - 1);
            QColor color = currentColor;
            color.setAlpha(255);
            if (index == 3) {
                color.setAlphaF(static_cast<float>(t));
            } else if (hsv) {
                color = zzColorFromHsv(index == 0 ? t : hue, index == 1 ? t : saturation,
                                        index == 2 ? t : value);
            } else if (index == 0) {
                color.setRed(qRound(t * 255));
            } else if (index == 1) {
                color.setGreen(qRound(t * 255));
            } else {
                color.setBlue(qRound(t * 255));
            }
            stops.append(color);
        }
        slider->setColors(stops);
    }
    spectrum->setState(hue, saturation, shape == ZzColorPicker::Ring);
    spectrum->setAccessibleName(ZzColorPicker::tr("色相和饱和度色谱"));
    spectrum->setAccessibleDescription(ZzColorPicker::tr("方向键调整色相和饱和度，Shift 加快调整"));
    valueSlider->setAccessibleName(ZzColorPicker::tr("明度"));
    alphaSlider->setAccessibleName(ZzColorPicker::tr("透明度"));
    shadeStrip->setAccessibleName(ZzColorPicker::tr("颜色明暗色阶"));
    representationCombo->setAccessibleName(ZzColorPicker::tr("颜色表示"));
    const QStringList pageNames{ZzColorPicker::tr("色谱"), ZzColorPicker::tr("色板"), ZzColorPicker::tr("滑条")};
    for (int index = 0; index < 3; ++index) {
        tabs->setTabToolTip(index, pageNames[index]);
        tabs->setTabWhatsThis(index, pageNames[index]);
        tabs->setAccessibleTabName(index, pageNames[index]);
    }
    tabs->setAccessibleName(ZzColorPicker::tr("颜色编辑方式"));
    const QSignalBlocker valueBlocker(valueSlider);
    const QSignalBlocker alphaBlocker(alphaSlider);
    valueSlider->setValue(qRound(value * 255));
    valueSlider->setColors({QColor::fromRgb(0, 0, 0), zzColorFromHsv(hue, saturation, 1)});
    alphaSlider->setValue(currentColor.alpha());
    QColor transparent = currentColor;
    transparent.setAlpha(0);
    QColor opaque = currentColor;
    opaque.setAlpha(255);
    alphaSlider->setColors({transparent, opaque});
    shadeStrip->setColor(currentColor, hue, saturation);
}

} // namespace ZzFluentUI
