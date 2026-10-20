#include <ZzFluentUI/ZzToolTip.h>

#include <QPointer>
#include <QEventLoop>
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
    void showsTextAndFollowsAppPalette();
    void hideTextClosesInstance();
    void reuseKeepsSingleInstance();
    void emptyTextHides();
    void textRendersOpaqueOnScreen(); // 洗白回归：浅色底上文字像素必须接近 ToolTipText
};

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
    const QImage image = tip->grab().toImage().convertToFormat(QImage::Format_RGB32);

    // 文字像素必须接近 ToolTipText（子像素洗白会整体抬到 #d4d4d4 量级）
    int darkest = 255;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            darkest = qMin(darkest, qMin(c.red(), qMin(c.green(), c.blue())));
        }
    }
    QVERIFY2(darkest < 0x40,
             qPrintable(QStringLiteral("文字最深像素通道值 %1，疑似渲染洗白").arg(darkest)));

    ZzToolTip::hideText();
}

QTEST_MAIN(ZzToolTipTest)
#include "ZzToolTipTest.moc"
