#include <array>
#include <utility>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QIntValidator>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSuggestBox.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace {

/** @brief 处理事件和延迟销毁，使 popup 与对象计数进入稳定状态。 */
void zzFlushSuggestEvents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

/** @brief 设置 completion prefix 并返回当前匹配行数。 */
int zzCompletionCount(
    ZzFluentUI::ZzSuggestBox *box,
    const QString &prefix)
{
    QCompleter *completer = box->completer();
    Q_ASSERT(completer != nullptr);
    completer->setCompletionPrefix(prefix);
    return completer->completionModel()->rowCount();
}

/** @brief 从信号参数中读取 ZzSuggestion 值快照。 */
ZzFluentUI::ZzSuggestion zzSuggestionArgument(
    const QList<QVariant> &arguments)
{
    if (arguments.isEmpty()) {
        return {};
    }
    return qvariant_cast<ZzFluentUI::ZzSuggestion>(arguments.constFirst());
}

} // namespace

/** @brief 验证搜索建议框的集合、过滤、输入、popup 和对象预算。 */
class ZzSuggestBoxTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableDefaultsAndValueCollection()
    {
        ZzFluentUI::ZzSuggestBox box;
        QCompleter *completer = box.completer();
        QVERIFY(completer != nullptr);
        QCOMPARE(completer->completionMode(), QCompleter::PopupCompletion);
        QCOMPARE(completer->modelSorting(), QCompleter::UnsortedModel);
        QCOMPARE(box.caseSensitivity(), Qt::CaseInsensitive);
        QCOMPARE(box.filterMode(), Qt::MatchContains);
        QCOMPARE(box.maximumVisibleItems(), 8);
        QCOMPARE(box.suggestionCount(), 0);
        QVERIFY(box.isClearButtonEnabled());

        QPixmap iconPixmap(8, 8);
        iconPixmap.fill(Qt::green);
        const QIcon icon(iconPixmap);
        QSignalSpy changedSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::suggestionsChanged);
        box.setSuggestions({
            {QStringLiteral("stable"), QStringLiteral("Alpha"), icon,
             17, true},
            {{}, QStringLiteral("Beta"), {}, QStringLiteral("payload"),
             false},
            {QStringLiteral("stable"), QStringLiteral("Alpha"), {},
             29, true}});
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(box.suggestionCount(), 3);
        const QList<ZzFluentUI::ZzSuggestion> items = box.suggestions();
        QCOMPARE(items.size(), 3);
        QCOMPARE(items.at(0).key, QStringLiteral("stable"));
        QVERIFY(!items.at(1).key.isEmpty());
        QVERIFY(!items.at(2).key.isEmpty());
        QVERIFY(items.at(2).key != QStringLiteral("stable"));
        QSet<QString> keys;
        for (const ZzFluentUI::ZzSuggestion &item : items) {
            keys.insert(item.key);
        }
        QCOMPARE(keys.size(), 3);
        QVERIFY(!items.at(0).icon.isNull());
        QCOMPARE(items.at(0).data.toInt(), 17);
        QVERIFY(!items.at(1).enabled);

        const QString generated = box.addSuggestion(
            QStringLiteral("Gamma"), QVariantMap{{QStringLiteral("id"), 3}});
        QVERIFY(!generated.isEmpty());
        QCOMPARE(changedSpy.count(), 2);
        const QString supplied = box.addSuggestion({
            QStringLiteral("supplied"), QStringLiteral("Delta"), {}, {},
            true});
        QCOMPARE(supplied, QStringLiteral("supplied"));
        const QString duplicate = box.addSuggestion({
            QStringLiteral("supplied"), QStringLiteral("Epsilon"), {}, {},
            true});
        QVERIFY(!duplicate.isEmpty());
        QVERIFY(duplicate != supplied);
        QCOMPARE(changedSpy.count(), 4);

        QVERIFY(!box.removeSuggestionAt(-1));
        QVERIFY(!box.removeSuggestionAt(box.suggestionCount()));
        QVERIFY(!box.removeSuggestion(QStringLiteral("missing")));
        QCOMPARE(changedSpy.count(), 4);
        QVERIFY(box.removeSuggestion(generated));
        QCOMPARE(changedSpy.count(), 5);
        QVERIFY(box.removeSuggestionAt(0));
        QCOMPARE(changedSpy.count(), 6);
        box.clearSuggestions();
        QCOMPARE(changedSpy.count(), 7);
        QCOMPARE(box.suggestionCount(), 0);
        box.clearSuggestions();
        QCOMPARE(changedSpy.count(), 7);
    }

    void filtersWithoutDuplicatingModelState()
    {
        ZzFluentUI::ZzSuggestBox box;
        box.setSuggestions({
            {QStringLiteral("a"), QStringLiteral("Alpha"), {}, 1, true},
            {QStringLiteral("b"), QStringLiteral("alphabet"), {}, 2, true},
            {QStringLiteral("c"), QStringLiteral("BETA"), {}, 3, false},
            {QStringLiteral("d"), QStringLiteral("omega"), {}, 4, true}});

        QCOMPARE(zzCompletionCount(&box, QStringLiteral("ph")), 2);
        QCOMPARE(zzCompletionCount(&box, QStringLiteral("AL")), 2);
        QSignalSpy caseSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::caseSensitivityChanged);
        box.setCaseSensitivity(Qt::CaseSensitive);
        QCOMPARE(caseSpy.count(), 1);
        QCOMPARE(zzCompletionCount(&box, QStringLiteral("AL")), 0);
        QCOMPARE(zzCompletionCount(&box, QStringLiteral("Al")), 1);

        QSignalSpy modeSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::filterModeChanged);
        box.setCaseSensitivity(Qt::CaseInsensitive);
        box.setFilterMode(Qt::MatchStartsWith);
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(zzCompletionCount(&box, QStringLiteral("a")), 2);
        box.setFilterMode(Qt::MatchEndsWith);
        QCOMPARE(modeSpy.count(), 2);
        QCOMPARE(zzCompletionCount(&box, QStringLiteral("ga")), 1);
        box.setFilterMode(Qt::MatchExactly);
        QCOMPARE(box.filterMode(), Qt::MatchEndsWith);
        QCOMPARE(modeSpy.count(), 2);

        QSignalSpy maximumSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::maximumVisibleItemsChanged);
        box.setMaximumVisibleItems(0);
        QCOMPARE(box.maximumVisibleItems(), 1);
        box.setMaximumVisibleItems(200);
        QCOMPARE(box.maximumVisibleItems(), 100);
        QCOMPARE(maximumSpy.count(), 2);

        QAbstractItemModel *sourceModel = box.completer()->model();
        QVERIFY(sourceModel != nullptr);
        QCOMPARE(sourceModel->rowCount(), 4);
        const QModelIndex disabled = sourceModel->index(2, 0);
        QVERIFY(disabled.isValid());
        QVERIFY(!(disabled.flags() & Qt::ItemIsEnabled));
        QVERIFY(!(disabled.flags() & Qt::ItemIsSelectable));
        QCOMPARE(sourceModel->rowCount(sourceModel->index(0, 0)), 0);
        QVERIFY(!sourceModel->data(QModelIndex(), Qt::DisplayRole).isValid());

        QPersistentModelIndex persistent(sourceModel->index(0, 0));
        QVERIFY(persistent.isValid());
        box.setSuggestions({
            {QStringLiteral("replacement"), QStringLiteral("Replacement"),
             {}, 9, true}});
        QVERIFY(!persistent.isValid());
        QCOMPARE(sourceModel->rowCount(), 1);
    }

    void returnsExactDuplicateTextPayloadByCompletionIndex()
    {
        ZzFluentUI::ZzSuggestBox box;
        QPixmap iconPixmap(8, 8);
        iconPixmap.fill(Qt::red);
        box.setSuggestions({
            {QStringLiteral("first"), QStringLiteral("Same"),
             QIcon(iconPixmap), 11, true},
            {QStringLiteral("second"), QStringLiteral("Same"), {}, 22,
             true}});
        QCompleter *completer = box.completer();
        completer->setCompletionPrefix(QStringLiteral("Same"));
        QAbstractItemModel *completionModel = completer->completionModel();
        QCOMPARE(completionModel->rowCount(), 2);

        QSignalSpy activatedSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::suggestionActivated);
        QSignalSpy highlightedSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::suggestionHighlighted);
        Q_EMIT completer->activated(completionModel->index(1, 0));
        QCOMPARE(activatedSpy.count(), 1);
        const ZzFluentUI::ZzSuggestion activated =
            zzSuggestionArgument(activatedSpy.takeFirst());
        QCOMPARE(activated.key, QStringLiteral("second"));
        QCOMPARE(activated.text, QStringLiteral("Same"));
        QCOMPARE(activated.data.toInt(), 22);
        QVERIFY(activated.enabled);

        Q_EMIT completer->highlighted(completionModel->index(0, 0));
        QCOMPARE(highlightedSpy.count(), 1);
        const ZzFluentUI::ZzSuggestion highlighted =
            zzSuggestionArgument(highlightedSpy.takeFirst());
        QCOMPARE(highlighted.key, QStringLiteral("first"));
        QCOMPARE(highlighted.data.toInt(), 11);
        QVERIFY(!highlighted.icon.isNull());
    }

    void preservesLineEditInputAndAccessibilitySemantics()
    {
        ZzFluentUI::ZzSuggestBox box;
        box.setAccessibleName(QStringLiteral("Command search"));
        auto *validator = new QIntValidator(0, 999, &box);
        box.setValidator(validator);
        box.setText(QStringLiteral("42"));
        QVERIFY(box.hasAcceptableInput());
        box.selectAll();
        QTest::keyClicks(&box, QStringLiteral("17"));
        QCOMPARE(box.text(), QStringLiteral("17"));
        box.undo();
        QCOMPARE(box.text(), QStringLiteral("42"));
        box.redo();
        QCOMPARE(box.text(), QStringLiteral("17"));

        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString(QStringLiteral("5"));
        box.selectAll();
        QVERIFY(QCoreApplication::sendEvent(&box, &inputMethodEvent));
        QCOMPARE(box.text(), QStringLiteral("5"));
        QCOMPARE(box.inputMethodQuery(Qt::ImEnabled).toBool(), true);

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&box);
        if (interface == nullptr) {
            QFAIL("搜索建议框缺少标准QLineEdit无障碍接口");
            return;
        }
        QCOMPARE(interface->role(), QAccessible::EditableText);
        QCOMPARE(interface->text(QAccessible::Name),
                 QStringLiteral("Command search"));
        QCOMPARE(interface->text(QAccessible::Value), QStringLiteral("5"));
        QVERIFY(interface->state().focusable);

        box.setReadOnly(true);
        QVERIFY(interface->state().readOnly);
        box.setEnabled(false);
        QVERIFY(interface->state().disabled);
    }

    void usesNativePopupKeyboardAndMousePaths()
    {
        ZzFluentUI::ZzSuggestBox box;
        box.setSuggestions({
            {QStringLiteral("alpha"), QStringLiteral("Alpha"), {}, 1,
             true},
            {QStringLiteral("alpine"), QStringLiteral("Alpine"), {}, 2,
             true},
            {QStringLiteral("disabled"), QStringLiteral("Albatross"), {},
             3, false}});
        box.resize(240, 36);
        box.move(32, 32);
        const Qt::WindowFlags originalFlags = box.windowFlags();
        const QPoint originalPosition = box.pos();
        box.show();
        box.setFocus();
        box.setText(QStringLiteral("Al"));
        box.showSuggestions();
        ZZ_VERIFY_EVENTUALLY(box.isSuggestionPopupVisible());
        QAbstractItemView *popup = box.completer()->popup();
        QVERIFY(popup != nullptr);
        auto *popupList = qobject_cast<QListView *>(popup);
        if (popupList == nullptr) {
            QFAIL("搜索建议popup不是预期的QListView");
        }
        QCOMPARE(popup->selectionMode(),
                 QAbstractItemView::SingleSelection);
        QCOMPARE(popup->editTriggers(), QAbstractItemView::NoEditTriggers);
        QVERIFY(popupList->uniformItemSizes());

        QSignalSpy activatedSpy(
            &box,
            &ZzFluentUI::ZzSuggestBox::suggestionActivated);
        QVERIFY(box.completer()->setCurrentRow(0));
        popup->setCurrentIndex(box.completer()->currentIndex());
        QTest::keyClick(popup, Qt::Key_Return);
        ZZ_COMPARE_EVENTUALLY(activatedSpy.count(), 1);
        const ZzFluentUI::ZzSuggestion keyboardActivation =
            zzSuggestionArgument(activatedSpy.takeFirst());
        QCOMPARE(keyboardActivation.key, QStringLiteral("alpha"));
        QCOMPARE(keyboardActivation.text, QStringLiteral("Alpha"));

        box.setText(QStringLiteral("Al"));
        box.showSuggestions();
        ZZ_VERIFY_EVENTUALLY(box.isSuggestionPopupVisible());
        const QModelIndex second = popup->model()->index(1, 0);
        QVERIFY(second.isValid());
        const QRect secondRect = popup->visualRect(second);
        QVERIFY(secondRect.isValid());
        QTest::mouseClick(
            popup->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            secondRect.center());
        ZZ_COMPARE_EVENTUALLY(activatedSpy.count(), 1);
        QCOMPARE(zzSuggestionArgument(activatedSpy.takeFirst()).key,
                 QStringLiteral("alpine"));

        box.setText(QStringLiteral("Al"));
        box.showSuggestions();
        ZZ_VERIFY_EVENTUALLY(box.isSuggestionPopupVisible());
        QTest::keyClick(popup, Qt::Key_Escape);
        ZZ_VERIFY_EVENTUALLY(!box.isSuggestionPopupVisible());
        QCOMPARE(box.windowFlags(), originalFlags);
        QCOMPARE(box.pos(), originalPosition);
        box.hideSuggestions();
    }

    void rendersThroughApplicationStyleAcrossThemesAndDirections()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzSuggestBox box;
        box.setStyle(&style);
        box.setSuggestions({
            {QStringLiteral("long"),
             QStringLiteral("A long suggestion that should elide safely"),
             {}, {}, true}});
        box.resize(220, 36);

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            box.setPalette(style.standardPalette());
            for (const Qt::LayoutDirection direction : {
                     Qt::LeftToRight,
                     Qt::RightToLeft}) {
                box.setLayoutDirection(direction);
                QPixmap rendered(box.size());
                rendered.fill(Qt::transparent);
                box.render(&rendered);
                QVERIFY(!rendered.isNull());
                QCOMPARE(box.style(), &style);
                QCOMPARE(box.layoutDirection(), direction);
            }
        }
    }

    void keepsObjectsStableAcrossLargeCollectionAndStateChanges()
    {
        constexpr int boxCount = 100;
        constexpr int suggestionsPerBox = 20;
        constexpr int rounds = 1000;
        QWidget host;
        std::vector<ZzFluentUI::ZzSuggestBox *> boxes;
        boxes.reserve(boxCount);
        QList<ZzFluentUI::ZzSuggestion> suggestions;
        suggestions.reserve(suggestionsPerBox);
        for (int item = 0; item < suggestionsPerBox; ++item) {
            suggestions.append({
                QStringLiteral("key-%1").arg(item),
                QStringLiteral("Suggestion %1").arg(item),
                {}, item, true});
        }
        for (int index = 0; index < boxCount; ++index) {
            auto *box = new ZzFluentUI::ZzSuggestBox(&host);
            box->setSuggestions(suggestions);
            box->setText(QStringLiteral("warmup"));
            box->setText({});
            boxes.push_back(box);
        }
        zzFlushSuggestEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();

        for (int round = 0; round < rounds; ++round) {
            const int index = round % boxCount;
            ZzFluentUI::ZzSuggestBox *box =
                boxes.at(static_cast<std::size_t>(index));
            box->setText(QString::number(round));
            box->setCaseSensitivity(Qt::CaseSensitive);
            box->setFilterMode(Qt::MatchStartsWith);
            box->setMaximumVisibleItems((round % 120) + 1);
            box->setLayoutDirection(Qt::RightToLeft);
            const QString temporaryKey = box->addSuggestion(
                QStringLiteral("Temporary"), round);
            QVERIFY(box->removeSuggestion(temporaryKey));
            box->setText({});
            box->setCaseSensitivity(Qt::CaseInsensitive);
            box->setFilterMode(Qt::MatchContains);
            box->setMaximumVisibleItems(8);
            box->setLayoutDirection(Qt::LeftToRight);
        }
        zzFlushSuggestEvents();
        QCOMPARE(host.findChildren<QObject *>().size(), initialDescendants);
        QCOMPARE(host.findChildren<QAbstractAnimation *>().size(),
                 initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        for (const ZzFluentUI::ZzSuggestBox *box : boxes) {
            QCOMPARE(box->suggestionCount(), suggestionsPerBox);
        }

        ZzFluentUI::ZzSuggestBox large;
        const qsizetype beforeItems =
            large.findChildren<QObject *>().size();
        QList<ZzFluentUI::ZzSuggestion> many;
        many.reserve(10000);
        for (int index = 0; index < 10000; ++index) {
            many.append({
                QStringLiteral("large-%1").arg(index),
                QStringLiteral("Entry %1").arg(index),
                {}, index, true});
        }
        large.setSuggestions(std::move(many));
        QCOMPARE(large.suggestionCount(), 10000);
        QCOMPARE(large.findChildren<QObject *>().size(), beforeItems);
    }
};

QTEST_MAIN(ZzSuggestBoxTest)

#include "ZzSuggestBoxTest.moc"
