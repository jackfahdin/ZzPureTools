#include "../ZzExampleAudioSpectrumPage.h"
#include <ZzFluentUI/ZzAudioSpectrum.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <QComboBox>
#include <QCheckBox>
#include <QHideEvent>
#include <QShowEvent>
#include <QTimer>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QScopeGuard>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <algorithm>
#include <memory>

class ZzExampleAudioSpectrumTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void synthControlsAndDestruction()
    {
        ZzFluentUI::ZzThemeController theme;
        auto *style = new ZzFluentUI::ZzFluentStyle(&theme);
        QApplication::setStyle(style);
        const auto restoreStyle = qScopeGuard([] { QApplication::setStyle(QStringLiteral("Fusion")); });
        QWidget host;
        auto *page = ZzExample::ZzExampleAudioSpectrumPage::create(&host);
        host.resize(800, 700);
        page->resize(800, 700);
        host.show();
        auto *spectrum = page->findChild<ZzFluentUI::ZzAudioSpectrum *>(QStringLiteral("zzSpectrumPreview"));
        QVERIFY(spectrum != nullptr);
        auto *bars = page->findChild<QSpinBox *>(QStringLiteral("spectrumBars"));
        auto *fft = page->findChild<QComboBox *>(QStringLiteral("spectrumFft"));
        auto *source = page->findChild<QComboBox *>(QStringLiteral("spectrumSource"));
        auto *play = page->findChild<QPushButton *>(QStringLiteral("spectrumPlay"));
        QVERIFY(bars && fft && source && play);
        auto *sourceTimer = page->findChild<QTimer *>(QStringLiteral("spectrumDemoTimer"));
        QVERIFY(sourceTimer && sourceTimer->isActive());
        // Minimized windows can receive Hide while isVisible() remains true.
        QHideEvent hidden;
        QCoreApplication::sendEvent(page, &hidden);
        QVERIFY(!sourceTimer->isActive());
        play->setChecked(false);
        play->setChecked(true);
        QVERIFY(!sourceTimer->isActive());
        QShowEvent shown;
        QCoreApplication::sendEvent(page, &shown);
        QVERIFY(sourceTimer->isActive());
        for (const char *name : {"spectrumRainbow", "spectrumReflection", "spectrumPeak", "spectrumMotion"}) {
            auto *box = page->findChild<QCheckBox *>(QString::fromLatin1(name));
            QVERIFY(box);
            box->setChecked(false);
        }
        QVERIFY(!spectrum->rainbowEnabled() && !spectrum->reflectionEnabled()
            && !spectrum->peakEnabled() && !spectrum->animationEnabled());
        for (auto *box : page->findChildren<QCheckBox *>()) box->setChecked(true);
        bars->setValue(32);
        QCOMPARE(spectrum->barCount(), 32);
        fft->setCurrentIndex(2);
        QCOMPARE(spectrum->fftSize(), 4096);
        const auto hasSignal = [spectrum] {
            const auto levels = spectrum->levels();
            return std::any_of(levels.cbegin(), levels.cend(), [](qreal value) { return value > 0.1; });
        };
        ZZ_VERIFY_EVENTUALLY(hasSignal());
        const QString previewPath = qEnvironmentVariable("ZZ_SPECTRUM_PREVIEW");
        if (!previewPath.isEmpty()) QVERIFY(page->grab().save(previewPath));
        source->setCurrentIndex(3);
        spectrum->setAnimationEnabled(false);
        ZZ_VERIFY_EVENTUALLY(spectrum->displayedLevels() == QVector<qreal>(32, 0));
        play->click();
        QVERIFY(!play->isChecked());
        auto *low = page->findChild<QSpinBox *>(QStringLiteral("spectrumLow"));
        auto *high = page->findChild<QSpinBox *>(QStringLiteral("spectrumHigh"));
        QVERIFY(low && high);
        high->setValue(100);
        low->setValue(1000);
        QCOMPARE(low->value(), static_cast<int>(spectrum->minimumFrequency()));
        QVERIFY(low->value() < high->value());
        QPointer<ZzFluentUI::ZzAudioSpectrum> guard(spectrum);
        delete page;
        QVERIFY(guard.isNull());
        QCoreApplication::processEvents();
    }
};
QTEST_MAIN(ZzExampleAudioSpectrumTest)
#include "ZzExampleAudioSpectrumTest.moc"
