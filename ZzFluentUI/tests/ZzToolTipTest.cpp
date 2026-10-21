#include <ZzFluentUI/ZzToolTip.h>

#include <QPointer>
#include <QEventLoop>
#include <QScreen>
#include <QtTest>

using namespace ZzFluentUI;

namespace {

QWidget *findTipLabel()
{
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (tlw->objectName() == QStringLiteral("ZzToolTipLabel"))
            return tlw;
    }
    return nullptr;
}

} // namespace

class ZzToolTipTest final : public QObject {
    Q_OBJECT
private slots:
    void init() { originalPalette_ = QApplication::palette(); }
    void cleanup()
    {
        ZzToolTip::hideText();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::setPalette(originalPalette_);
    }
    void showsTextAndFollowsAppPalette();
    void hideTextClosesInstance();
    void reuseKeepsSingleInstance();
    void emptyTextHides();
    void textRendersOpaqueOnScreen(); // 洗白回归：浅色底上文字像素必须接近 ToolTipText
    void survivesPreviousOwnerDestruction();
    void retiredTipCannotCloseReplacement();
    void followsPaletteWhileVisible();
    void supportsMultilineAndScreenBounds();
    void clampsPositionsOutsideAvailableScreen();
    void closesWhenOwnerHides();
    void dismissesOnInput_data();
    void dismissesOnInput();
private:
    QPalette originalPalette_;
};

void ZzToolTipTest::survivesPreviousOwnerDestruction()
{
    auto *firstHost = new QWidget;
    QWidget secondHost;
    firstHost->show();
    secondHost.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("first"), firstHost);
    QPointer<QWidget> first = findTipLabel();
    ZzToolTip::showText(QPoint(60, 60), QStringLiteral("second"), &secondHost);
    delete firstHost;
    QVERIFY(ZzToolTip::isVisible());
    QCOMPARE(findTipLabel(), first.data());
    QCOMPARE(findTipLabel()->accessibleName(), QStringLiteral("second"));
}

void ZzToolTipTest::retiredTipCannotCloseReplacement()
{
    QWidget host;
    host.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("first"), &host);
    QPointer<QWidget> retired = findTipLabel();
    ZzToolTip::hideText();
    QWidget nextHost;
    nextHost.show();
    ZzToolTip::showText(QPoint(60, 60), QStringLiteral("second"), &nextHost);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&host, &leave);
    QVERIFY(ZzToolTip::isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(retired.isNull());
    QVERIFY(ZzToolTip::isVisible());
}

void ZzToolTipTest::followsPaletteWhileVisible()
{
    QWidget host;
    QPalette local = host.palette();
    local.setColor(QPalette::ToolTipText, Qt::red);
    host.setPalette(local);
    host.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("theme"), &host);
    QPointer<QWidget> tip = findTipLabel();
    QVERIFY(tip);
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::ToolTipBase, QColor("#123456"));
    palette.setColor(QPalette::ToolTipText, QColor("#abcdef"));
    QApplication::setPalette(palette);
    QTRY_COMPARE(tip->palette().color(QPalette::ToolTipText), QColor("#abcdef"));
    QCOMPARE(tip->palette().color(QPalette::ToolTipBase), QColor("#123456"));
    QCOMPARE(findTipLabel(), tip.data());
}

void ZzToolTipTest::supportsMultilineAndScreenBounds()
{
    QWidget host;
    host.show();
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    ZzToolTip::showText(screen.bottomRight(), QStringLiteral("first\nsecond"), &host);
    auto *tip = findTipLabel();
    QVERIFY(tip);
    QVERIFY(tip->height() >= 2 * tip->fontMetrics().height());
    ZzToolTip::showText(screen.bottomRight(), QString(4000, QLatin1Char('W')), &host);
    QVERIFY(screen.contains(tip->geometry()));
    QVERIFY(tip->height() > tip->fontMetrics().height() + 14);
}

void ZzToolTipTest::closesWhenOwnerHides()
{
    QWidget host;
    auto *child = new QWidget(&host);
    host.show();
    child->show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("hide"), child);
    host.hide();
    QVERIFY(!ZzToolTip::isVisible());
}

void ZzToolTipTest::clampsPositionsOutsideAvailableScreen()
{
    QWidget host;
    host.show();
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    ZzToolTip::showText(screen.bottomRight() + QPoint(200, 200),
                       QStringLiteral("outside"), &host);
    const auto *tip = findTipLabel();
    QVERIFY(tip);
    QVERIFY(screen.contains(tip->geometry()));
}

void ZzToolTipTest::dismissesOnInput_data()
{
    QTest::addColumn<int>("type");
    for (const auto type : {QEvent::MouseButtonPress, QEvent::Wheel, QEvent::KeyPress}) {
        QTest::newRow(QByteArray::number(type).constData()) << static_cast<int>(type);
    }
}

void ZzToolTipTest::dismissesOnInput()
{
    QFETCH(int, type);
    QWidget host;
    host.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("input"), &host);
    if (type == QEvent::KeyPress) {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
        QApplication::sendEvent(&host, &event);
    } else if (type == QEvent::Wheel) {
        QWheelEvent event(QPointF(1, 1), QPointF(1, 1), QPoint(), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&host, &event);
    } else {
        QTest::mousePress(&host, Qt::LeftButton);
    }
    QVERIFY(!ZzToolTip::isVisible());
}

void ZzToolTipTest::showsTextAndFollowsAppPalette()
{
    QWidget host;
    host.resize(200, 100);
    host.show();

    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::ToolTipBase, QColor("#f5f5f5"));
    palette.setColor(QPalette::ToolTipText, QColor("#1a1a1a"));
    palette.setColor(QPalette::Mid, QColor("#d1d1d1"));
    QApplication::setPalette(palette);

    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("https://a.com"), &host);
    QVERIFY(ZzToolTip::isVisible());
    QWidget *tip = findTipLabel();
    QVERIFY(tip != nullptr);
    QCOMPARE(tip->palette().color(QPalette::ToolTipText), QColor("#1a1a1a"));

    // 主题切换（palette 变更）后，新实例必须取到新颜色
    //（tooltip 生命周期短暂，主题切换时旧实例已被销毁/重建是主路径；
    //  offscreen 平台不会给既有窗口投递 ApplicationPaletteChange）
    ZzToolTip::hideText();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    palette.setColor(QPalette::ToolTipBase, QColor("#323232"));
    palette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    QApplication::setPalette(palette);
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("https://a.com"), &host);
    QWidget *tipDark = findTipLabel();
    QVERIFY(tipDark != nullptr);
    QCOMPARE(tipDark->palette().color(QPalette::ToolTipText), QColor("#ffffff"));

    ZzToolTip::hideText();
}

void ZzToolTipTest::hideTextClosesInstance()
{
    QWidget host;
    host.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("tip"), &host);
    QVERIFY(ZzToolTip::isVisible());
    ZzToolTip::hideText();
    QVERIFY(!ZzToolTip::isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(findTipLabel(), nullptr);
}

void ZzToolTipTest::reuseKeepsSingleInstance()
{
    QWidget host;
    host.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("one"), &host);
    QWidget *first = findTipLabel();
    ZzToolTip::showText(QPoint(60, 60), QStringLiteral("two"), &host);
    QCOMPARE(findTipLabel(), first);
    ZzToolTip::hideText();
}

void ZzToolTipTest::emptyTextHides()
{
    QWidget host;
    host.show();
    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("tip"), &host);
    QVERIFY(ZzToolTip::isVisible());
    ZzToolTip::showText(QPoint(50, 50), QString(), &host);
    QVERIFY(!ZzToolTip::isVisible());
}

void ZzToolTipTest::textRendersOpaqueOnScreen()
{
    QWidget host;
    host.show();

    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipText, QColor("#1a1a1a"));
    palette.setColor(QPalette::Mid, QColor("#d1d1d1"));
    QApplication::setPalette(palette);

    ZzToolTip::showText(QPoint(50, 50), QStringLiteral("https://a.com"), &host);
    QWidget *tip = findTipLabel();
    QVERIFY(tip != nullptr);
    const QImage image = tip->grab().toImage().convertToFormat(QImage::Format_ARGB32);

    // 文字像素必须接近 ToolTipText（子像素洗白会整体抬到 #d4d4d4 量级）
    int darkest = 255;
    // Exclude the frame and transparent corners: these are not text pixels.
    const qreal scale = image.devicePixelRatio();
    const QRect content = QRect(qRound(10 * scale), qRound(6 * scale),
        image.width() - qRound(20 * scale), image.height() - qRound(12 * scale));
    for (int y = content.top(); y <= content.bottom(); ++y) {
        for (int x = content.left(); x <= content.right(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.alpha() == 255)
                darkest = qMin(darkest, qMax(c.red(), qMax(c.green(), c.blue())));
        }
    }
    QVERIFY2(darkest < 0x40,
             qPrintable(QStringLiteral("文字最深像素通道值 %1，疑似渲染洗白").arg(darkest)));

    ZzToolTip::hideText();
}

QTEST_MAIN(ZzToolTipTest)
#include "ZzToolTipTest.moc"
