#include <QtCore/QAbstractAnimation>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzImageCard.h>

/** @brief 验证卡片控件保留 Qt 按钮语义并维持无对象增长绘制。 */
class ZzCardControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableDefaultsWithoutChildWidgets()
    {
        ZzFluentUI::ZzActionCard action;
        ZzFluentUI::ZzImageCard image;

        QCOMPARE(action.focusPolicy(), Qt::StrongFocus);
        QCOMPARE(image.focusPolicy(), Qt::StrongFocus);
        QVERIFY(!action.isCheckable());
        QVERIFY(!image.isCheckable());
        QVERIFY(!action.autoRepeat());
        QVERIFY(!image.autoRepeat());
        QVERIFY(action.isTrailingIndicatorVisible());
        QCOMPARE(
            image.aspectRatioMode(),
            Qt::KeepAspectRatioByExpanding);
        QVERIFY(action.findChildren<QWidget *>().isEmpty());
        QVERIFY(image.findChildren<QWidget *>().isEmpty());
        QVERIFY(action.findChildren<QTimer *>().isEmpty());
        QVERIFY(image.findChildren<QTimer *>().isEmpty());
        QVERIFY(action.findChildren<QAbstractAnimation *>().isEmpty());
        QVERIFY(image.findChildren<QAbstractAnimation *>().isEmpty());
        QVERIFY(action.sizeHint().width() >= action.minimumSizeHint().width());
        QVERIFY(image.sizeHint().height() >= image.minimumSizeHint().height());
    }

    void emitsPropertySignalsOnlyForEffectiveChanges()
    {
        ZzFluentUI::ZzActionCard action;
        QSignalSpy actionDescriptionSpy(
            &action,
            &ZzFluentUI::ZzActionCard::descriptionChanged);
        QSignalSpy indicatorSpy(
            &action,
            &ZzFluentUI::ZzActionCard::trailingIndicatorVisibleChanged);
        action.setDescription({});
        QCOMPARE(actionDescriptionSpy.count(), 0);
        action.setDescription(QStringLiteral("Open settings"));
        QCOMPARE(actionDescriptionSpy.count(), 1);
        action.setDescription(QStringLiteral("Open settings"));
        QCOMPARE(actionDescriptionSpy.count(), 1);
        QCOMPARE(
            action.accessibleDescription(),
            QStringLiteral("Open settings"));
        action.setTrailingIndicatorVisible(true);
        QCOMPARE(indicatorSpy.count(), 0);
        action.setTrailingIndicatorVisible(false);
        QCOMPARE(indicatorSpy.count(), 1);
        action.setTrailingIndicatorVisible(false);
        QCOMPARE(indicatorSpy.count(), 1);

        ZzFluentUI::ZzImageCard image;
        QSignalSpy pixmapSpy(
            &image,
            &ZzFluentUI::ZzImageCard::pixmapChanged);
        QSignalSpy imageDescriptionSpy(
            &image,
            &ZzFluentUI::ZzImageCard::descriptionChanged);
        QSignalSpy aspectSpy(
            &image,
            &ZzFluentUI::ZzImageCard::aspectRatioModeChanged);
        QPixmap pixmap(160, 90);
        pixmap.fill(Qt::blue);
        image.setPixmap(pixmap);
        QCOMPARE(pixmapSpy.count(), 1);
        image.setPixmap(pixmap);
        QCOMPARE(pixmapSpy.count(), 1);
        image.setDescription(QStringLiteral("Project preview"));
        QCOMPARE(imageDescriptionSpy.count(), 1);
        image.setDescription(QStringLiteral("Project preview"));
        QCOMPARE(imageDescriptionSpy.count(), 1);
        image.setAspectRatioMode(Qt::KeepAspectRatio);
        QCOMPARE(aspectSpy.count(), 1);
        image.setAspectRatioMode(Qt::KeepAspectRatio);
        QCOMPARE(aspectSpy.count(), 1);
        image.setAspectRatioMode(Qt::IgnoreAspectRatio);
        QCOMPARE(aspectSpy.count(), 2);
        QCOMPARE(
            image.aspectRatioMode(),
            Qt::IgnoreAspectRatio);
        image.setAspectRatioMode(Qt::IgnoreAspectRatio);
        QCOMPARE(aspectSpy.count(), 2);
        image.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
        QCOMPARE(aspectSpy.count(), 3);
        image.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
        QCOMPARE(aspectSpy.count(), 3);
    }

    void preservesNativeKeyboardAndCheckableSemantics()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *action = new ZzFluentUI::ZzActionCard(
            QStringLiteral("Settings"),
            QStringLiteral("Open preferences"),
            &window);
        auto *image = new ZzFluentUI::ZzImageCard(
            QStringLiteral("Project"),
            QStringLiteral("Open project"),
            &window);
        action->setCheckable(true);
        layout->addWidget(action);
        layout->addWidget(image);
        window.show();
        action->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        QSignalSpy actionClickSpy(action, &QAbstractButton::clicked);
        QSignalSpy toggledSpy(action, &QAbstractButton::toggled);
        QSignalSpy imageClickSpy(image, &QAbstractButton::clicked);

        QTest::keyClick(action, Qt::Key_Space);
        QCOMPARE(actionClickSpy.count(), 1);
        QCOMPARE(toggledSpy.count(), 1);
        QVERIFY(action->isChecked());
        QTest::keyClick(action, Qt::Key_Return);
        QCOMPARE(actionClickSpy.count(), 2);
        QCOMPARE(toggledSpy.count(), 2);
        QVERIFY(!action->isChecked());

        image->setEnabled(false);
        QTest::keyClick(image, Qt::Key_Return);
        QCOMPARE(imageClickSpy.count(), 0);
    }

    void rendersLongTextAspectModesAndRtlSafely()
    {
        const QString longText(400, QLatin1Char('W'));
        ZzFluentUI::ZzActionCard action(longText, longText);
        action.setLayoutDirection(Qt::RightToLeft);
        action.setIcon(action.style()->standardIcon(QStyle::SP_FileIcon));
        action.resize(action.minimumSizeHint());

        ZzFluentUI::ZzImageCard image(longText, longText);
        image.setLayoutDirection(Qt::RightToLeft);
        image.resize(image.minimumSizeHint());
        QPixmap onePixel(1, 1);
        onePixel.fill(Qt::red);

        QImage target(
            QSize(400, 360),
            QImage::Format_ARGB32_Premultiplied);
        for (Qt::AspectRatioMode mode : {
                 Qt::IgnoreAspectRatio,
                 Qt::KeepAspectRatio,
                 Qt::KeepAspectRatioByExpanding}) {
            image.setAspectRatioMode(mode);
            image.setPixmap(onePixel);
            target.fill(Qt::transparent);
            QPainter painter(&target);
            action.render(&painter, QPoint(0, 0));
            image.render(&painter, QPoint(0, 100));
        }
        image.setPixmap({});
        QPainter painter(&target);
        image.render(&painter, QPoint(0, 100));
        painter.end();
        QVERIFY(!target.isNull());
    }

    void repeatedUpdatesAndRenderingDoNotAllocateObjects()
    {
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        auto *action = new ZzFluentUI::ZzActionCard(
            QStringLiteral("Settings"),
            QStringLiteral("Open preferences"),
            &host);
        auto *image = new ZzFluentUI::ZzImageCard(
            QStringLiteral("Project"),
            QStringLiteral("Open project"),
            &host);
        QPixmap pixmap(160, 90);
        pixmap.fill(Qt::green);
        image->setPixmap(pixmap);
        layout->addWidget(action);
        layout->addWidget(image);
        host.resize(360, 400);
        host.show();
        QCoreApplication::processEvents();
        const qsizetype initialActionDescendants =
            action->findChildren<QObject *>().size();
        const qsizetype initialImageDescendants =
            image->findChildren<QObject *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qint64 pixmapCacheKey = image->pixmap().cacheKey();
        QImage target(
            host.size(),
            QImage::Format_ARGB32_Premultiplied);

        for (int iteration = 0; iteration < 1000; ++iteration) {
            action->setChecked((iteration % 2) != 0);
            action->setLayoutDirection(
                (iteration % 2) == 0
                    ? Qt::LeftToRight
                    : Qt::RightToLeft);
            image->setAspectRatioMode(
                static_cast<Qt::AspectRatioMode>(iteration % 3));
            target.fill(Qt::transparent);
            QPainter painter(&target);
            host.render(&painter);
        }

        QCOMPARE(
            action->findChildren<QObject *>().size(),
            initialActionDescendants);
        QCOMPARE(
            image->findChildren<QObject *>().size(),
            initialImageDescendants);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QCOMPARE(image->pixmap().cacheKey(), pixmapCacheKey);
    }
};

QTEST_MAIN(ZzCardControlsTest)

#include "ZzCardControlsTest.moc"
