#include <cstring>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzExpander.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

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

        editor->setFocus(Qt::OtherFocusReason);
        QCOMPARE(QApplication::focusWidget(), editor);
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
        QTRY_VERIFY(contentHost(expander)->maximumHeight() > 0);
        const int expandingHeight = contentHost(expander)->maximumHeight();
        QVERIFY(expandingHeight < QWIDGETSIZE_MAX);

        expander->setExpanded(false);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(contentHost(expander)->maximumHeight(), expandingHeight);
        QTRY_VERIFY(contentHost(expander)->maximumHeight() < expandingHeight);
        const int collapsingHeight = contentHost(expander)->maximumHeight();
        expander->setExpanded(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(contentHost(expander)->maximumHeight(), collapsingHeight);

        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
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
