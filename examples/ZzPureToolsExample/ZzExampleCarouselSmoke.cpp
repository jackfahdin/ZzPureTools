#include "ZzExampleCarouselSmoke.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <ZzFluentUI/ZzCarouselView.h>

namespace ZzExample {
bool zzCarouselPageReady(const QWidget& window)
{
    auto* page = window.findChild<QWidget*>(QStringLiteral("zzExampleCarouselPage"));
    if (!page)
        return false;
    auto* view = page->findChild<ZzFluentUI::ZzCarouselView*>(QStringLiteral("carouselPreview"));
    auto* next = page->findChild<QPushButton*>(QStringLiteral("carouselNext"));
    auto* reset = page->findChild<QPushButton*>(QStringLiteral("carouselReset"));
    auto* play = page->findChild<QCheckBox*>(QStringLiteral("carouselAutoPlay"));
    auto* pips = page->findChild<QCheckBox*>(QStringLiteral("carouselIndicators"));
    auto* radius = page->findChild<QDoubleSpinBox*>(QStringLiteral("carouselRadius"));
    auto* aspect = page->findChild<QComboBox*>(QStringLiteral("carouselAspect"));
    auto* duration = page->findChild<QSpinBox*>(QStringLiteral("carouselDuration"));
    if (!view || !next || !reset || !play || !pips || !radius || !aspect || !duration
        || !view->model() || view->model()->rowCount() != 5 || view->model()->parent() != page)
        return false;
    play->setChecked(false);
    duration->setValue(0);
    view->setCurrentRow(0);
    next->click();
    if (view->currentRow() != 1)
        return false;
    pips->click();
    radius->setValue(18);
    aspect->setCurrentIndex(1);
    if (view->showIndicators() || view->borderRadius() != 18
        || view->imageAspectRatioMode() != Qt::KeepAspectRatio)
        return false;
    reset->click();
    const bool valid = view->currentRow() == 0 && view->immersive() && view->showIndicators()
        && view->borderRadius() == 6 && play->isChecked()
        && view->navigationButtonTrigger() == ZzFluentUI::ZzCarouselView::OnHover;
    play->setChecked(false);
    duration->setValue(0);
    view->setCurrentRow(0);
    return valid;
}
} // namespace ZzExample
