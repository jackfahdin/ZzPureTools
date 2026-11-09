#include <cstring>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtGui/QKeySequence>

// Qt documents this function but requires callers to declare it explicitly.
QT_BEGIN_NAMESPACE
Q_GUI_EXPORT void qt_set_sequence_auto_mnemonic(bool enabled);
QT_END_NAMESPACE

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzExpander.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/** @brief 为 Expander 可访问操作文本提供确定的 LanguageChange 翻译。 */
class ZzExpanderTranslator final : public QTranslator
{
public:
    /** @brief 翻译展开和折叠两个内部动作文本。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (sourceText != nullptr
            && std::strcmp(sourceText, "展开内容") == 0) {
            return QStringLiteral("Translated expand");
        }
        if (sourceText != nullptr
            && std::strcmp(sourceText, "折叠内容") == 0) {
            return QStringLiteral("Translated collapse");
        }
        return {};
    }
};

/** @brief 验证 Expander 的状态、所有权、输入、动效和可访问性契约。 */
class ZzExpanderTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 返回固定 header 子控件。 */
    static QToolButton *headerButton(ZzFluentUI::ZzExpander *expander)
    {
        auto *button = expander->findChild<QToolButton *>(
            QStringLiteral("zzExpanderHeaderButton"));
        Q_ASSERT(button != nullptr);
        return button;
    }

    /** @brief 返回固定内容宿主。 */
    static QWidget *contentHost(ZzFluentUI::ZzExpander *expander)
    {
        auto *host = expander->findChild<QWidget *>(
            QStringLiteral("zzExpanderContentHost"));
        Q_ASSERT(host != nullptr);
        return host;
    }

private Q_SLOTS:
    void paintsContinuousCard_data()
    {
        QTest::addColumn<ZzFluentUI::ZzThemeMode>("mode");
        QTest::newRow("light") << ZzFluentUI::ZzThemeMode::Light;
        QTest::newRow("dark") << ZzFluentUI::ZzThemeMode::Dark;
        QTest::newRow("contrast") << ZzFluentUI::ZzThemeMode::HighContrast;
    }

    void paintsContinuousCard()
    {
        QFETCH(ZzFluentUI::ZzThemeMode, mode);
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(mode);
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzExpander expander;
        expander.setStyle(&style);
        auto *content = new QWidget;
        content->setMinimumHeight(80);
        expander.setContentWidget(content);
        expander.setExpanded(true);
        expander.resize(300, expander.sizeHint().height());
        expander.show();
        QCoreApplication::processEvents();
        headerButton(&expander)->clearFocus();
        const QImage image = expander.grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        const auto colorAt = [&](QPoint point) {
            return image.pixelColor(qRound(point.x() * dpr), qRound(point.y() * dpr));
        };
        const QColor surface = controller.snapshot()->color(ZzFluentUI::ZzColorToken::SurfaceSecondary);
        QCOMPARE(colorAt(QPoint(100, headerButton(&expander)->height() / 2)), surface);
        QCOMPARE(colorAt(contentHost(&expander)->geometry().center()), surface);
        // The join has a straight outer edge, not two separate rounded cards.
        const int join = contentHost(&expander)->y();
        QCOMPARE(colorAt(QPoint(3, join - 3)), surface);
        QCOMPARE(colorAt(QPoint(3, join + 3)), surface);
    }

    void hoverFeedbackStaysAtTrailingChevron_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::newRow("ltr") << false;
        QTest::newRow("rtl") << true;
    }

    void hoverFeedbackStaysAtTrailingChevron()
    {
        QFETCH(bool, rtl);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzExpander expander;
        expander.setStyle(&style);
        expander.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        expander.resize(300, expander.sizeHint().height());
        expander.show();
        QCoreApplication::processEvents();
        auto *button = headerButton(&expander);
        button->clearFocus();
        button->setAttribute(Qt::WA_UnderMouse, false);
        const QImage idle = button->grab().toImage();
        button->setAttribute(Qt::WA_UnderMouse, true);
        const QImage hovered = button->grab().toImage();
        QRect changed;
        for (int y = 0; y < idle.height(); ++y) {
            for (int x = 0; x < idle.width(); ++x) {
                if (idle.pixel(x, y) != hovered.pixel(x, y)) {
                    changed |= QRect(x, y, 1, 1);
                }
            }
        }
        QVERIFY(!changed.isEmpty());
        QVERIFY(changed.width() < idle.width() / 3);
        QVERIFY(rtl ? changed.center().x() < idle.width() / 2
                    : changed.center().x() > idle.width() / 2);
        button->setEnabled(false);
        const QImage disabledHovered = button->grab().toImage();
        button->setAttribute(Qt::WA_UnderMouse, false);
        QCOMPARE(button->grab().toImage(), disabledHovered);
    }

    void defaultsAndPropertiesAreIdempotent()
    {
        ZzFluentUI::ZzExpander expander;
        QSignalSpy headerSpy(
            &expander, &ZzFluentUI::ZzExpander::headerTextChanged);
        QSignalSpy expandedSpy(
            &expander, &ZzFluentUI::ZzExpander::expandedChanged);

        QCOMPARE(expander.headerText(), QString());
        QVERIFY(!expander.isExpanded());
        QCOMPARE(expander.contentWidget(), nullptr);
        QCOMPARE(headerButton(&expander)->arrowType(), Qt::RightArrow);
        QVERIFY(!contentHost(&expander)->isVisible());

        expander.setHeaderText(QStringLiteral("Details"));
        expander.setHeaderText(QStringLiteral("Details"));
        expander.setExpanded(true);
        expander.setExpanded(true);

        QCOMPARE(expander.headerText(), QStringLiteral("Details"));
        QVERIFY(expander.isExpanded());
        QCOMPARE(headerSpy.count(), 1);
        QCOMPARE(expandedSpy.count(), 1);
        QCOMPARE(headerButton(&expander)->text(), QStringLiteral("Details"));
        QCOMPARE(headerButton(&expander)->arrowType(), Qt::DownArrow);
    }

    void chevronTracksAnimationAndReversal()
    {
        ZzFluentUI::ZzExpander expander;
        auto *content = new QWidget;
        content->setMinimumHeight(120);
        expander.setContentWidget(content);
        expander.resize(300, expander.sizeHint().height());
        expander.show();
        QCoreApplication::processEvents();
        auto *button = headerButton(&expander);
        button->clearFocus();
        button->setAttribute(Qt::WA_UnderMouse, false);
        const QImage collapsed = button->grab().toImage();
        auto *animation = expander.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        expander.setExpanded(true);
        animation->setCurrentTime(animation->duration() / 2);
        const QImage intermediate = button->grab().toImage();
        QVERIFY(intermediate != collapsed);
        expander.setExpanded(false);
        QCOMPARE(button->grab().toImage(), intermediate);
        animation->setCurrentTime(animation->duration());
        QCOMPARE(button->grab().toImage(), collapsed);
        expander.setExpanded(true);
        animation->setCurrentTime(animation->duration());
        QVERIFY(button->grab().toImage() != collapsed);
        QCOMPARE(expander.findChildren<QAbstractAnimation *>().size(), 1);
    }

    void longHeaderCanShrinkAndKeepsAccessibleName()
    {
        ZzFluentUI::ZzExpander expander;
        const QString title = QStringLiteral("Long header ").repeated(30);
        expander.setHeaderText(title);
        expander.resize(160, expander.sizeHint().height());
        expander.show();
        QCoreApplication::processEvents();
        QCOMPARE(expander.width(), 160);
        QCOMPARE(headerButton(&expander)->accessibleName(), title);
        QFont large = headerButton(&expander)->font();
        large.setPixelSize(40);
        headerButton(&expander)->setFont(large);
        QVERIFY(headerButton(&expander)->sizeHint().height() >= 40);
        for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            expander.setLayoutDirection(direction);
            QVERIFY(!expander.grab().isNull());
        }
    }

    void destroyingContentSettlesChevron_data()
    {
        QTest::addColumn<bool>("expanding");
        QTest::newRow("expanding") << true;
        QTest::newRow("collapsing") << false;
    }

    void headerPreservesMnemonicSizing_data()
    {
        QTest::addColumn<bool>("disableMnemonics");
        QTest::newRow("platform-default") << false;
        QTest::newRow("disabled-mnemonics") << true;
    }

    void headerPreservesMnemonicSizing()
    {
        QFETCH(bool, disableMnemonics);
        const bool mnemonicsEnabled = !QKeySequence::mnemonic(QStringLiteral("&A")).isEmpty();
        const auto restoreMnemonics = qScopeGuard([mnemonicsEnabled] {
            qt_set_sequence_auto_mnemonic(mnemonicsEnabled);
        });
        if (disableMnemonics) qt_set_sequence_auto_mnemonic(false);
        ZzFluentUI::ZzExpander expander;
        expander.setHeaderText(QStringLiteral("Advanced"));
        const QSize plain = headerButton(&expander)->sizeHint();
        expander.setHeaderText(QStringLiteral("&Advanced"));
        QCOMPARE(headerButton(&expander)->sizeHint(), plain);
        // Qt disables automatic mnemonics by default on macOS.
        QCOMPARE(headerButton(&expander)->shortcut(),
            QKeySequence::mnemonic(QStringLiteral("&Advanced")));
        expander.setHeaderText(QStringLiteral("Save  Close"));
        const int noAmpersand = headerButton(&expander)->sizeHint().width();
        expander.setHeaderText(QStringLiteral("Save && Close"));
        QCOMPARE(headerButton(&expander)->sizeHint().width() - noAmpersand,
            headerButton(&expander)->fontMetrics().horizontalAdvance(QLatin1Char('&')));
    }

    void destroyingContentSettlesChevron()
    {
        QFETCH(bool, expanding);
        ZzFluentUI::ZzExpander expander;
        auto *content = new QWidget;
        content->setMinimumHeight(120);
        expander.setContentWidget(content);
        expander.resize(300, expander.sizeHint().height());
        expander.show();
        QCoreApplication::processEvents();
        auto *button = headerButton(&expander);
        button->clearFocus();
        button->setAttribute(Qt::WA_UnderMouse, false);
        auto *animation = expander.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        if (!expanding) {
            expander.setExpanded(true);
            animation->setCurrentTime(animation->duration());
        }
        expander.setExpanded(expanding);
        animation->setCurrentTime(animation->duration() / 2);
        delete content;
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        const QImage afterDelete = button->grab().toImage();
        expander.setExpanded(!expanding);
        expander.setExpanded(expanding);
        QCOMPARE(button->grab().toImage(), afterDelete);
    }

    // Qt parent 接管嵌套控件，运行时 QPointer 断言覆盖真实析构路径。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void transfersReplacesAndObservesContentOwnership()
    {
        QPointer<QWidget> ownedGuard;
        {
            ZzFluentUI::ZzExpander expander;
            QSignalSpy contentSpy(
                &expander,
                &ZzFluentUI::ZzExpander::contentWidgetChanged);
            auto *first = new QWidget;
            auto *nested = new QLabel(QStringLiteral("Nested"), first);
            QPointer<QWidget> firstGuard(first);
            expander.setContentWidget(first);
            expander.setContentWidget(first);
            QCOMPARE(contentSpy.count(), 1);

            expander.setContentWidget(nested);
            QVERIFY(firstGuard.isNull());
            QCOMPARE(expander.contentWidget(), nested);
            QCOMPARE(nested->parentWidget(), contentHost(&expander));
            QCOMPARE(contentSpy.count(), 2);

            std::unique_ptr<QWidget> taken(expander.takeContentWidget());
            QCOMPARE(taken.get(), nested);
            QCOMPARE(taken->parentWidget(), nullptr);
            QCOMPARE(contentSpy.count(), 3);

            auto *externallyDestroyed = new QLabel(QStringLiteral("Owned"));
            expander.setContentWidget(externallyDestroyed);
            delete externallyDestroyed;
            QCOMPARE(expander.contentWidget(), nullptr);
            QCOMPARE(contentSpy.count(), 5);

            auto *owned = new QLabel(QStringLiteral("Destroyed with owner"));
            ownedGuard = owned;
            expander.setContentWidget(owned);
        }
        QVERIFY(ownedGuard.isNull());
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void supportsMouseSpaceEnterAndReturn()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzExpander expander;
        expander.setStyle(&style);
        expander.setContentWidget(new QLabel(QStringLiteral("Content")));
        expander.show();
        QCoreApplication::processEvents();
        QToolButton *const button = headerButton(&expander);

        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(expander.isExpanded());
        QTest::keyClick(button, Qt::Key_Space);
        QVERIFY(!expander.isExpanded());
        QTest::keyClick(button, Qt::Key_Enter);
        QVERIFY(expander.isExpanded());
        QTest::keyClick(button, Qt::Key_Return);
        QVERIFY(!expander.isExpanded());
    }

    void updatesStableSizeHintAndRestoresHeaderFocus()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *expander = new ZzFluentUI::ZzExpander(&window);
        expander->setStyle(&style);
        auto *editor = new QLineEdit;
        editor->setMinimumHeight(72);
        expander->setContentWidget(editor);
        layout->addWidget(expander);
        window.show();
        QCoreApplication::processEvents();

        const int collapsedHeight = expander->sizeHint().height();
        expander->setExpanded(true);
        QCoreApplication::processEvents();
        const int expandedHeight = expander->sizeHint().height();
        QVERIFY(expandedHeight > collapsedHeight);
        QVERIFY(contentHost(expander)->isVisible());

        window.activateWindow();
        editor->setFocus(Qt::OtherFocusReason);
        ZZ_COMPARE_EVENTUALLY(QApplication::focusWidget(), editor);
        expander->setExpanded(false);
        QCOMPARE(QApplication::focusWidget(), headerButton(expander));
        QVERIFY(!contentHost(expander)->isVisible());
        QCOMPARE(expander->sizeHint().height(), collapsedHeight);
    }

    void reversesRunningAnimationFromCurrentHeight()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *expander = new ZzFluentUI::ZzExpander(&window);
        auto *content = new QLabel(QStringLiteral("Animated content"));
        content->setMinimumHeight(160);
        expander->setContentWidget(content);
        layout->addWidget(expander);
        window.show();
        QCoreApplication::processEvents();
        auto *animation = expander->findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        expander->setExpanded(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        ZZ_VERIFY_EVENTUALLY(contentHost(expander)->maximumHeight() > 0);
        const int expandingHeight = contentHost(expander)->maximumHeight();
        QVERIFY(expandingHeight < QWIDGETSIZE_MAX);

        expander->setExpanded(false);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(contentHost(expander)->maximumHeight(), expandingHeight);
        ZZ_VERIFY_EVENTUALLY(contentHost(expander)->maximumHeight() < expandingHeight);
        const int collapsingHeight = contentHost(expander)->maximumHeight();
        expander->setExpanded(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(contentHost(expander)->maximumHeight(), collapsingHeight);

        ZZ_COMPARE_EVENTUALLY(animation->state(), QAbstractAnimation::Stopped);
        QCOMPARE(contentHost(expander)->maximumHeight(), QWIDGETSIZE_MAX);
        QVERIFY(contentHost(expander)->isVisible());
    }

    void reducedMotionSettlesSynchronously()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzExpander expander;
        expander.setStyle(&style);
        expander.setContentWidget(new QLabel(QStringLiteral("Content")));
        auto *animation = expander.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        expander.setExpanded(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QCOMPARE(contentHost(&expander)->maximumHeight(), QWIDGETSIZE_MAX);
        expander.setExpanded(false);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QCOMPARE(contentHost(&expander)->maximumHeight(), 0);
        QVERIFY(!contentHost(&expander)->isVisible());
    }

    void changingToReducedMotionStopsRunningTransition()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *expander = new ZzFluentUI::ZzExpander(&window);
        expander->setStyle(&style);
        auto *content = new QLabel(QStringLiteral("Animated content"));
        content->setMinimumHeight(160);
        expander->setContentWidget(content);
        layout->addWidget(expander);
        window.show();
        QCoreApplication::processEvents();
        auto *animation = expander->findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        expander->setExpanded(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QCoreApplication::processEvents();

        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QCOMPARE(contentHost(expander)->maximumHeight(), QWIDGETSIZE_MAX);
        QVERIFY(contentHost(expander)->isVisible());
    }

    void refreshesLanguageAndRtlPresentation()
    {
        ZzFluentUI::ZzExpander expander;
        expander.setHeaderText(QStringLiteral("Advanced"));
        QToolButton *const button = headerButton(&expander);
        QCOMPARE(button->accessibleName(), QStringLiteral("Advanced"));
        QCOMPARE(button->accessibleDescription(), QStringLiteral("展开内容"));

        expander.setLayoutDirection(Qt::RightToLeft);
        QCOMPARE(button->arrowType(), Qt::LeftArrow);
        expander.setExpanded(true);
        QCOMPARE(button->arrowType(), Qt::DownArrow);

        ZzExpanderTranslator translator;
        QCoreApplication::installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&expander, &languageChange);
        QCOMPARE(
            button->accessibleDescription(),
            QStringLiteral("Translated collapse"));
        QCOMPARE(button->toolTip(), QStringLiteral("Translated collapse"));
        QCoreApplication::removeTranslator(&translator);
    }

    void keepsAnimationAndObjectBudgetsFixed()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzExpander expander;
        expander.setStyle(&style);
        expander.setContentWidget(new QLabel(QStringLiteral("Content")));
        QVariantAnimation *const animation =
            expander.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        const qsizetype animationCount =
            expander.findChildren<QAbstractAnimation *>().size();
        const qsizetype timerCount = expander.findChildren<QTimer *>().size();
        const qsizetype objectCount = expander.findChildren<QObject *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            expander.setExpanded(!expander.isExpanded());
        }

        QCOMPARE(expander.findChild<QVariantAnimation *>(), animation);
        QCOMPARE(
            expander.findChildren<QAbstractAnimation *>().size(),
            animationCount);
        QCOMPARE(expander.findChildren<QTimer *>().size(), timerCount);
        QCOMPARE(expander.findChildren<QObject *>().size(), objectCount);
    }
};

QTEST_MAIN(ZzExpanderTest)

#include "ZzExpanderTest.moc"
