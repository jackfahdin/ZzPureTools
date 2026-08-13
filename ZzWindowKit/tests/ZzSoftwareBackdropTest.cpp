#include <QtTest/QTest>

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QWidget>

#include "ZzSoftwareBackdrop.h"

namespace {

[[nodiscard]] qsizetype childCount(const QWidget &widget)
{
    return widget.findChildren<QWidget *>().size();
}

} // namespace

/** @brief 验证私有软件材质层的缓存、生命周期和输入穿透边界。 */
class ZzSoftwareBackdropTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsInvalidHost()
    {
        ZzWindowKit::ZzSoftwareBackdrop backdrop;
        QWidget host;
        QWidget child(&host);

        QVERIFY(!backdrop.attach(nullptr));
        QVERIFY(!backdrop.attach(&child));
        QVERIFY(backdrop.attach(&host));
        QVERIFY(!backdrop.attach(&host));
    }

    void enablesStableLayer()
    {
        QWidget host;
        host.resize(320, 180);
        ZzWindowKit::ZzSoftwareBackdrop backdrop;
        QVERIFY(backdrop.attach(&host));

        auto *layer = host.findChild<QWidget *>(
            QStringLiteral("zzSoftwareBackdropLayer"));
        QVERIFY(layer != nullptr);
        if (layer == nullptr) {
            return;
        }
        QVERIFY(layer->testAttribute(Qt::WA_TransparentForMouseEvents));
        const auto objectCount = childCount(host);
        QVERIFY(backdrop.setEnabled(true));
        QVERIFY(backdrop.isEnabled());
        QCOMPARE(backdrop.rebuildCount(), std::size_t{1U});
        QVERIFY(!layer->isHidden());
        QVERIFY(backdrop.setEnabled(true));
        QCOMPARE(backdrop.rebuildCount(), std::size_t{1U});
        QCOMPARE(childCount(host), objectCount);

        QVERIFY(backdrop.setEnabled(false));
        QVERIFY(!backdrop.isEnabled());
        QVERIFY(layer == host.findChild<QWidget *>(
            QStringLiteral("zzSoftwareBackdropLayer")));
    }

    void rebuildsOnlyForEnvironmentChanges()
    {
        QWidget host;
        host.resize(320, 180);
        ZzWindowKit::ZzSoftwareBackdrop backdrop;
        QVERIFY(backdrop.attach(&host));
        QVERIFY(backdrop.setEnabled(true));
        const auto initial = backdrop.rebuildCount();

        host.resize(480, 240);
        QResizeEvent resizeEvent(QSize(480, 240), QSize(320, 180));
        QCoreApplication::sendEvent(&host, &resizeEvent);
        QCOMPARE(backdrop.rebuildCount(), initial);
        const auto resized = backdrop.rebuildCount();
        QEvent paletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(&host, &paletteChange);
        QVERIFY(backdrop.rebuildCount() > resized);
        const auto paletteChanged = backdrop.rebuildCount();
        QEvent repaint(QEvent::UpdateRequest);
        QCoreApplication::sendEvent(&host, &repaint);
        QCOMPARE(backdrop.rebuildCount(), paletteChanged);
    }

    void layerPaintsMaterialImage()
    {
        QWidget host;
        host.resize(160, 100);
        ZzWindowKit::ZzSoftwareBackdrop backdrop;
        QVERIFY(backdrop.attach(&host));
        QVERIFY(backdrop.setEnabled(true));
        auto *layer = host.findChild<QWidget *>(
            QStringLiteral("zzSoftwareBackdropLayer"));
        QVERIFY(layer != nullptr);
        if (layer == nullptr) {
            return;
        }

        QImage image(layer->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        layer->render(&image);
        QVERIFY(!image.isNull());
        QVERIFY(image.pixelColor(image.rect().center()).alpha() > 0);
    }

    void repeatedToggleDoesNotGrowObjects()
    {
        QWidget host;
        host.resize(240, 120);
        ZzWindowKit::ZzSoftwareBackdrop backdrop;
        QVERIFY(backdrop.attach(&host));
        const auto objectCount = childCount(host);
        auto *layer = host.findChild<QWidget *>(
            QStringLiteral("zzSoftwareBackdropLayer"));
        QVERIFY(layer != nullptr);
        if (layer == nullptr) {
            return;
        }

        for (int index = 0; index < 1000; ++index) {
            QVERIFY(backdrop.setEnabled((index % 2) == 0));
        }
        QCOMPARE(childCount(host), objectCount);
        QCOMPARE(
            host.findChild<QWidget *>(
                QStringLiteral("zzSoftwareBackdropLayer")),
            layer);
    }

    void hostDestructionIsSafe()
    {
        ZzWindowKit::ZzSoftwareBackdrop backdrop;
        auto host = std::make_unique<QWidget>();
        QVERIFY(backdrop.attach(host.get()));
        QVERIFY(backdrop.setEnabled(true));
        host.reset();
        QVERIFY(!backdrop.isEnabled());
        QVERIFY(!backdrop.setEnabled(true));
    }
};

QTEST_MAIN(ZzSoftwareBackdropTest)

#include "ZzSoftwareBackdropTest.moc"
