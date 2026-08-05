#include <QtCore/QDate>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtGui/QActionGroup>
#include <QtGui/QIntValidator>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTextEdit>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSuggestBox.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzThemeController themeController;
    ZzFluentUI::ZzFluentStyle fluentStyle(&themeController);
    QLineEdit lineEdit(QStringLiteral("Alpha"));
    QTextEdit textEdit(QStringLiteral("Beta"));
    QPlainTextEdit plainTextEdit(QStringLiteral("Gamma"));
    lineEdit.setStyle(&fluentStyle);
    textEdit.setStyle(&fluentStyle);
    plainTextEdit.setStyle(&fluentStyle);
    QComboBox selection;
    selection.setStyle(&fluentStyle);
    selection.addItem(QStringLiteral("Local"), 17);
    selection.addItem(QStringLiteral("Remote"), 29);
    selection.setCurrentIndex(0);
    QComboBox editableSelection;
    editableSelection.setStyle(&fluentStyle);
    editableSelection.setEditable(true);
    editableSelection.setInsertPolicy(QComboBox::NoInsert);
    editableSelection.addItems({QStringLiteral("12"), QStringLiteral("24")});
    QLineEdit *comboEditor = editableSelection.lineEdit();
    if (comboEditor == nullptr) {
        return 1;
    }
    comboEditor->setValidator(new QIntValidator(0, 999, comboEditor));
    editableSelection.setCompleter(new QCompleter(
        QStringList{QStringLiteral("120"), QStringLiteral("240")},
        &editableSelection));
    editableSelection.setEditText(QStringLiteral("42"));
    QMenu popupMenu;
    popupMenu.setStyle(&fluentStyle);
    QAction *openAction = popupMenu.addAction(QStringLiteral("&Open"));
    openAction->setShortcut(QKeySequence::Open);
    openAction->setData(71);
    QAction *automaticAction = popupMenu.addAction(
        QStringLiteral("Automatic"));
    automaticAction->setCheckable(true);
    automaticAction->setChecked(true);
    auto *modeGroup = new QActionGroup(&popupMenu);
    modeGroup->setExclusive(true);
    QAction *localAction = popupMenu.addAction(QStringLiteral("Local"));
    QAction *remoteAction = popupMenu.addAction(QStringLiteral("Remote"));
    localAction->setCheckable(true);
    remoteAction->setCheckable(true);
    localAction->setChecked(true);
    modeGroup->addAction(localAction);
    modeGroup->addAction(remoteAction);
    QMenu *exportMenu = popupMenu.addMenu(QStringLiteral("Export"));
    QAction *jsonAction = exportMenu->addAction(QStringLiteral("JSON"));
    popupMenu.setDefaultAction(openAction);
    popupMenu.setActiveAction(automaticAction);
    QMenuBar menuBar;
    menuBar.setNativeMenuBar(false);
    menuBar.setStyle(&fluentStyle);
    QMenu *fileMenu = menuBar.addMenu(QStringLiteral("&File"));
    QAction *saveAction = fileMenu->addAction(
        QStringLiteral("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    QPushButton toolTipHost;
    toolTipHost.setStyle(&fluentStyle);
    toolTipHost.setToolTip(QStringLiteral("Installed tooltip"));
    ZzFluentUI::ZzCalendar calendar;
    ZzFluentUI::ZzCalendarPicker picker;
    ZzFluentUI::ZzActionCard actionCard(
        QStringLiteral("Settings"),
        QStringLiteral("Open preferences"));
    ZzFluentUI::ZzImageCard imageCard(
        QStringLiteral("Project"),
        QStringLiteral("Open preview"));
    ZzFluentUI::ZzProgressRing progressRing;
    ZzFluentUI::ZzProgressRing busyRing;
    ZzFluentUI::ZzScrollArea scrollArea;
    ZzFluentUI::ZzSpinBox integerInput;
    ZzFluentUI::ZzDoubleSpinBox floatingInput;
    ZzFluentUI::ZzSuggestBox suggestBox;
    ZzFluentUI::ZzMultiSelectComboBox multiSelect;
    ZzFluentUI::ZzTabWidget sourceTabs;
    ZzFluentUI::ZzTabWidget targetTabs;
    QWidget *tabPage = new QWidget;
    sourceTabs.addTab(tabPage, QStringLiteral("Overview"));
    const QDate expectedDate(2026, 8, 5);
    calendar.setSelectedDate(expectedDate);
    picker.setDate(expectedDate);
    progressRing.setRange(20, 120);
    progressRing.setValue(70);
    progressRing.setRingWidth(6);
    busyRing.setRange(0, 0);
    ZzFluentUI::ZzScrollBar *horizontalScrollBar =
        scrollArea.fluentHorizontalScrollBar();
    ZzFluentUI::ZzScrollBar *verticalScrollBar =
        scrollArea.fluentVerticalScrollBar();
    if (horizontalScrollBar == nullptr || verticalScrollBar == nullptr) {
        return 1;
    }
    horizontalScrollBar->setRange(10, 110);
    horizontalScrollBar->setValue(60);
    verticalScrollBar->setRange(20, 220);
    verticalScrollBar->setPageStep(40);
    verticalScrollBar->setValue(120);
    integerInput.setRange(-20, 80);
    integerInput.setValue(24);
    floatingInput.setRange(-10.0, 10.0);
    floatingInput.setDecimals(2);
    floatingInput.setValue(1.25);
    suggestBox.setStyle(&fluentStyle);
    const QString localSuggestionKey = suggestBox.addSuggestion(
        QStringLiteral("Open local"), 41);
    const QString remoteSuggestionKey = suggestBox.addSuggestion({
        localSuggestionKey,
        QStringLiteral("Open remote"),
        {},
        82,
        true});
    suggestBox.completer()->setCompletionPrefix(QStringLiteral("remote"));
    multiSelect.setStyle(&fluentStyle);
    multiSelect.setOptions({
        {QStringLiteral("shared"), QStringLiteral("Desktop"), {}, 17,
         true, false},
        {QStringLiteral("shared"), QStringLiteral("Desktop"), {}, 29,
         true, false},
        {{}, QStringLiteral("Logs, metrics"), {}, 41, false, true}});
    multiSelect.setSelectedKeys({
        multiSelect.options().at(0).key,
        multiSelect.options().at(2).key});
    QStyleOptionFrame lineOption;
    lineOption.initFrom(&lineEdit);
    const QSize lineSize = fluentStyle.sizeFromContents(
        QStyle::CT_LineEdit,
        &lineOption,
        QSize(8, 8),
        &lineEdit);
    QStyleOptionComboBox comboOption;
    comboOption.initFrom(&selection);
    const QSize comboSize = fluentStyle.sizeFromContents(
        QStyle::CT_ComboBox,
        &comboOption,
        QSize(8, 8),
        &selection);
    QStyleOptionMenuItem menuItemOption;
    menuItemOption.initFrom(&popupMenu);
    menuItemOption.menuItemType = QStyleOptionMenuItem::Normal;
    menuItemOption.text = openAction->text();
    const QSize menuItemSize = fluentStyle.sizeFromContents(
        QStyle::CT_MenuItem,
        &menuItemOption,
        QSize(80, 8),
        &popupMenu);
    QKeyEvent downPress(
        QEvent::KeyPress,
        Qt::Key_Down,
        Qt::NoModifier);
    QCoreApplication::sendEvent(&selection, &downPress);

    if (lineEdit.style() != &fluentStyle
        || textEdit.style() != &fluentStyle
        || plainTextEdit.style() != &fluentStyle
        || lineEdit.text() != QStringLiteral("Alpha")
        || textEdit.toPlainText() != QStringLiteral("Beta")
        || plainTextEdit.toPlainText() != QStringLiteral("Gamma")
        || lineSize.width() < 96
        || lineSize.height() < 32
        || selection.style() != &fluentStyle
        || selection.currentIndex() != 1
        || selection.currentData().toInt() != 29
        || comboSize.width() < 96
        || comboSize.height() < 32
        || editableSelection.style() != &fluentStyle
        || editableSelection.lineEdit() != comboEditor
        || comboEditor->text() != QStringLiteral("42")
        || !comboEditor->hasAcceptableInput()
        || editableSelection.completer() == nullptr
        || editableSelection.insertPolicy() != QComboBox::NoInsert
        || popupMenu.style() != &fluentStyle
        || openAction->shortcut() != QKeySequence::Open
        || openAction->data().toInt() != 71
        || !automaticAction->isChecked()
        || !localAction->isChecked()
        || remoteAction->isChecked()
        || popupMenu.defaultAction() != openAction
        || popupMenu.activeAction() != automaticAction
        || QMenu::menuInAction(exportMenu->menuAction()) != exportMenu
        || jsonAction->text() != QStringLiteral("JSON")
        || menuItemSize.height() < 32
        || menuBar.style() != &fluentStyle
        || menuBar.isNativeMenuBar()
        || !menuBar.actions().contains(fileMenu->menuAction())
        || saveAction->shortcut() != QKeySequence::Save
        || toolTipHost.toolTip() != QStringLiteral("Installed tooltip")
        || calendar.selectedDate() != expectedDate
        || picker.date() != expectedDate
        || picker.calendar() == nullptr
        || picker.calendarWidget() != picker.calendar()
        || actionCard.description() != QStringLiteral("Open preferences")
        || imageCard.description() != QStringLiteral("Open preview")
        || progressRing.minimum() != 20
        || progressRing.maximum() != 120
        || progressRing.value() != 70
        || progressRing.ringWidth() != 6
        || busyRing.minimum() != 0
        || busyRing.maximum() != 0
        || horizontalScrollBar->orientation() != Qt::Horizontal
        || horizontalScrollBar->minimum() != 10
        || horizontalScrollBar->maximum() != 110
        || horizontalScrollBar->value() != 60
        || verticalScrollBar->orientation() != Qt::Vertical
        || verticalScrollBar->minimum() != 20
        || verticalScrollBar->maximum() != 220
        || verticalScrollBar->pageStep() != 40
        || verticalScrollBar->value() != 120
        || integerInput.minimum() != -20
        || integerInput.maximum() != 80
        || integerInput.value() != 24
        || integerInput.buttonSymbols() != QAbstractSpinBox::PlusMinus
        || floatingInput.minimum() != -10.0
        || floatingInput.maximum() != 10.0
        || floatingInput.decimals() != 2
        || floatingInput.value() != 1.25
        || floatingInput.buttonSymbols() != QAbstractSpinBox::PlusMinus
        || suggestBox.style() != &fluentStyle
        || suggestBox.suggestionCount() != 2
        || localSuggestionKey.isEmpty()
        || remoteSuggestionKey.isEmpty()
        || remoteSuggestionKey == localSuggestionKey
        || suggestBox.filterMode() != Qt::MatchContains
        || suggestBox.completer() == nullptr
        || suggestBox.completer()->completionModel()->rowCount() != 1
        || suggestBox.suggestions().at(1).data.toInt() != 82
        || multiSelect.style() != &fluentStyle
        || multiSelect.optionCount() != 3
        || multiSelect.selectionCount() != 2
        || multiSelect.currentIndex() != -1
        || multiSelect.options().at(0).key
            == multiSelect.options().at(1).key
        || multiSelect.options().at(2).key.isEmpty()
        || multiSelect.selectedText()
            != QStringLiteral("Desktop, Logs, metrics")
        || multiSelect.currentText() != multiSelect.selectedText()
        || multiSelect.selectedOptions().at(1).data.toInt() != 41
        || multiSelect.model()->data(
               multiSelect.model()->index(0, 0),
               ZzFluentUI::ZzMultiSelectComboBox::KeyRole).toString()
            != multiSelect.options().at(0).key
        || sourceTabs.fluentTabBar() == nullptr
        || !sourceTabs.transferTabTo(&targetTabs, 0)
        || sourceTabs.count() != 0
        || targetTabs.widget(0) != tabPage) {
        return 1;
    }
    return 0;
}
