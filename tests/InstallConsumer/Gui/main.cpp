#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QDate>
#include <QtCore/QTimer>
#include <QtGui/QActionGroup>
#include <QtGui/QImage>
#include <QtGui/QIntValidator>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzCarouselView.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzDrawer.h>
#include <ZzFluentUI/ZzExpander.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzPasswordBox.h>
#include <ZzFluentUI/ZzPasswordRevealMode.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzRoller.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSplitButton.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzSuggestBox.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  ZzFluentUI::ZzThemeController themeController;
  ZzFluentUI::ZzFluentStyle fluentStyle(&themeController);
  QLineEdit lineEdit(QStringLiteral("Alpha"));
  QTextEdit textEdit(QStringLiteral("Beta"));
  QPlainTextEdit plainTextEdit(QStringLiteral("Gamma"));
  lineEdit.setStyle(&fluentStyle);
  textEdit.setStyle(&fluentStyle);
  plainTextEdit.setStyle(&fluentStyle);
  QLCDNumber digitalDisplay;
  digitalDisplay.setStyle(&fluentStyle);
  digitalDisplay.setDigitCount(6);
  digitalDisplay.setSegmentStyle(QLCDNumber::Flat);
  digitalDisplay.display(2048);
  digitalDisplay.resize(160, 56);
  QImage digitalImage(digitalDisplay.size(),
                      QImage::Format_ARGB32_Premultiplied);
  digitalImage.fill(Qt::transparent);
  QPainter digitalPainter(&digitalImage);
  digitalDisplay.render(&digitalPainter);
  digitalPainter.end();
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
  editableSelection.setCompleter(
      new QCompleter(QStringList{QStringLiteral("120"), QStringLiteral("240")},
                     &editableSelection));
  editableSelection.setEditText(QStringLiteral("42"));
  QMenu popupMenu;
  popupMenu.setStyle(&fluentStyle);
  QAction *openAction = popupMenu.addAction(QStringLiteral("&Open"));
  openAction->setShortcut(QKeySequence::Open);
  openAction->setData(71);
  QAction *automaticAction = popupMenu.addAction(QStringLiteral("Automatic"));
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
  QAction *saveAction = fileMenu->addAction(QStringLiteral("&Save"));
  saveAction->setShortcut(QKeySequence::Save);
  QPushButton toolTipHost;
  toolTipHost.setStyle(&fluentStyle);
  toolTipHost.setToolTip(QStringLiteral("Installed tooltip"));
  QMainWindow commandWindow;
  commandWindow.setStyle(&fluentStyle);
  auto *commandToolBar = new QToolBar(QStringLiteral("Commands"));
  commandToolBar->setStyle(&fluentStyle);
  commandWindow.addToolBar(commandToolBar);
  QAction *commandAction = commandToolBar->addAction(
      QStringLiteral("Build"));
  commandAction->setCheckable(true);
  commandAction->trigger();
  auto *commandStatusBar = new QStatusBar;
  commandStatusBar->setStyle(&fluentStyle);
  commandWindow.setStatusBar(commandStatusBar);
  commandStatusBar->showMessage(QStringLiteral("Ready"), 0);
  commandWindow.resize(360, 180);
  commandWindow.show();
  QCoreApplication::processEvents();
  QWidget *commandButton = commandToolBar->widgetForAction(commandAction);
  if (commandButton != nullptr) {
    commandButton->setStyle(&fluentStyle);
  }
  QImage commandImage(
      commandToolBar->size(),
      QImage::Format_ARGB32_Premultiplied);
  commandImage.fill(Qt::transparent);
  QPainter commandPainter(&commandImage);
  commandToolBar->render(&commandPainter);
  commandPainter.end();
  ZzFluentUI::ZzCalendar calendar;
  ZzFluentUI::ZzCalendarPicker picker;
  ZzFluentUI::ZzActionCard actionCard(QStringLiteral("Settings"),
                                      QStringLiteral("Open preferences"));
  ZzFluentUI::ZzImageCard imageCard(QStringLiteral("Project"),
                                    QStringLiteral("Open preview"));
  QStandardItemModel carouselModel;
  auto *carouselItem = new QStandardItem(QStringLiteral("Release notes"));
  QPixmap carouselPixmap(160, 90);
  carouselPixmap.fill(QColor(31, 127, 71));
  carouselItem->setData(carouselPixmap, Qt::DecorationRole);
  carouselItem->setData(QStringLiteral("Installed model consumer"),
                        ZzFluentUI::ZzCarouselView::DescriptionRole);
  carouselModel.appendRow(carouselItem);
  ZzFluentUI::ZzCarouselView carouselView;
  carouselView.setStyle(&fluentStyle);
  carouselView.setAnimationDuration(0);
  carouselView.setModel(&carouselModel);
  carouselView.resize(320, 180);
  QImage carouselImage(carouselView.size(),
                       QImage::Format_ARGB32_Premultiplied);
  carouselImage.fill(Qt::transparent);
  QPainter carouselPainter(&carouselImage);
  carouselView.render(&carouselPainter);
  carouselPainter.end();
  QStandardItemModel navigationModel;
  auto *navigationHome = new QStandardItem(QStringLiteral("Home"));
  navigationHome->setData(
      QStringLiteral("Workspace"),
      static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Section));
  navigationHome->setData(
      QStringLiteral("3"),
      static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge));
  auto *navigationSettings = new QStandardItem(QStringLiteral("Settings"));
  navigationSettings->setData(
      QVariant::fromValue(ZzFluentUI::ZzNavigationPlacement::Footer),
      static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Placement));
  navigationModel.appendRow(navigationHome);
  navigationModel.appendRow(navigationSettings);
  ZzFluentUI::ZzNavigationPane navigationPane;
  navigationPane.setStyle(&fluentStyle);
  navigationPane.setModel(&navigationModel);
  navigationPane.setDisplayMode(
      ZzFluentUI::ZzNavigationDisplayMode::Regular);
  navigationPane.setCurrentSourceIndex(navigationModel.index(1, 0));
  ZzFluentUI::ZzProgressRing progressRing;
  ZzFluentUI::ZzProgressRing busyRing;
  QWidget flowHost;
  auto *flowLayout = new ZzFluentUI::ZzFlowLayout(5, 7, &flowHost);
  flowLayout->setContentsMargins(0, 0, 0, 0);
  auto *flowFirst = new QWidget(&flowHost);
  auto *flowSecond = new QWidget(&flowHost);
  flowFirst->setFixedSize(40, 20);
  flowSecond->setFixedSize(40, 20);
  flowLayout->addWidget(flowFirst);
  flowLayout->addWidget(flowSecond);
  flowLayout->setGeometry(QRect(0, 0, 84, 60));
  ZzFluentUI::ZzScrollArea scrollArea;
  ZzFluentUI::ZzSpinBox integerInput;
  ZzFluentUI::ZzDoubleSpinBox floatingInput;
  ZzFluentUI::ZzSuggestBox suggestBox;
  ZzFluentUI::ZzMultiSelectComboBox multiSelect;
  ZzFluentUI::ZzRoller roller;
  ZzFluentUI::ZzRollerPicker rollerPicker;
  ZzFluentUI::ZzExpander expander;
  ZzFluentUI::ZzPivot pivot;
  ZzFluentUI::ZzPasswordBox passwordBox;
  ZzFluentUI::ZzSplitButton splitButton(QStringLiteral("Installed build"));
  QMenu splitMenu;
  QWidget drawerHost;
  ZzFluentUI::ZzDrawer drawer(&drawerHost);
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
  const QString localSuggestionKey =
      suggestBox.addSuggestion(QStringLiteral("Open local"), 41);
  const QString remoteSuggestionKey = suggestBox.addSuggestion(
      {localSuggestionKey, QStringLiteral("Open remote"), {}, 82, true});
  suggestBox.completer()->setCompletionPrefix(QStringLiteral("remote"));
  multiSelect.setStyle(&fluentStyle);
  multiSelect.setOptions(
      {{QStringLiteral("shared"),
        QStringLiteral("Desktop"),
        {},
        17,
        true,
        false},
       {QStringLiteral("shared"),
        QStringLiteral("Desktop"),
        {},
        29,
        true,
        false},
       {{}, QStringLiteral("Logs, metrics"), {}, 41, false, true}});
  multiSelect.setSelectedKeys(
      {multiSelect.options().at(0).key, multiSelect.options().at(2).key});
  roller.setStyle(&fluentStyle);
  roller.setItems(
      {QStringLiteral("Low"), QStringLiteral("High"), QStringLiteral("High")});
  roller.setWrapping(true);
  roller.setCurrentIndex(2);
  rollerPicker.setStyle(&fluentStyle);
  rollerPicker.setColumns(
      {{QStringLiteral("time"),
        {QStringLiteral("08"), QStringLiteral("09")},
        1,
        true,
        96},
       {QStringLiteral("time"),
        {QStringLiteral("00"), QStringLiteral("30")},
        0,
        false,
        96},
       {{}, {QStringLiteral("AM"), QStringLiteral("PM")}, 1, false, 80}});
  const QList<int> installedPickerIndexes = rollerPicker.currentIndexes();
  rollerPicker.showPopup();
  if (!rollerPicker.setCurrentIndex(0, 0)) {
    return 1;
  }
  rollerPicker.cancelPopup();
  expander.setStyle(&fluentStyle);
  expander.setHeaderText(QStringLiteral("Installed expander"));
  expander.setContentWidget(new QLabel(QStringLiteral("Installed content")));
  expander.setExpanded(true);
  QWidget *const takenExpanderContent = expander.takeContentWidget();
  expander.setContentWidget(takenExpanderContent);
  pivot.setStyle(&fluentStyle);
  pivot.addItem(QStringLiteral("Overview"));
  pivot.addItem(QStringLiteral("Details"));
  pivot.setCurrentIndex(1);
  passwordBox.setStyle(&fluentStyle);
  passwordBox.setText(QStringLiteral("Installed secret"));
  passwordBox.setRevealMode(
      ZzFluentUI::ZzPasswordRevealMode::Visible);
  splitMenu.addAction(QStringLiteral("Installed option"));
  splitButton.setStyle(&fluentStyle);
  splitButton.setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
  splitButton.setMenu(&splitMenu);
  drawerHost.resize(640, 480);
  drawer.setStyle(&fluentStyle);
  drawer.setEdge(ZzFluentUI::ZzDrawerEdge::Right);
  drawer.setModal(false);
  drawer.setWidthHint(240);
  drawer.setContentWidget(new QLabel(QStringLiteral("Installed drawer")));
  QWidget *const installedDrawerContent = drawer.contentWidget();
  drawer.openDrawer();
  drawer.closeDrawer();
  QStyleOptionFrame lineOption;
  lineOption.initFrom(&lineEdit);
  const QSize lineSize = fluentStyle.sizeFromContents(
      QStyle::CT_LineEdit, &lineOption, QSize(8, 8), &lineEdit);
  QStyleOptionComboBox comboOption;
  comboOption.initFrom(&selection);
  const QSize comboSize = fluentStyle.sizeFromContents(
      QStyle::CT_ComboBox, &comboOption, QSize(8, 8), &selection);
  QStyleOptionMenuItem menuItemOption;
  menuItemOption.initFrom(&popupMenu);
  menuItemOption.menuItemType = QStyleOptionMenuItem::Normal;
  menuItemOption.text = openAction->text();
  const QSize menuItemSize = fluentStyle.sizeFromContents(
      QStyle::CT_MenuItem, &menuItemOption, QSize(80, 8), &popupMenu);
  QKeyEvent downPress(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
  QCoreApplication::sendEvent(&selection, &downPress);

  if (commandToolBar->style() != &fluentStyle) {
    return 20;
  }
  if (commandToolBar->orientation() != Qt::Horizontal) {
    return 21;
  }
  if (commandButton == nullptr) {
    return 22;
  }
  if (commandButton->style() != &fluentStyle) {
    return 23;
  }
  if (!commandAction->isChecked()) {
    return 24;
  }
  if (commandStatusBar->style() != &fluentStyle) {
    return 25;
  }
  if (commandStatusBar->currentMessage() != QStringLiteral("Ready")) {
    return 26;
  }
  if (commandImage.pixelColor(commandImage.rect().center()).alpha() == 0) {
    return 27;
  }

  if (lineEdit.style() != &fluentStyle || textEdit.style() != &fluentStyle ||
      plainTextEdit.style() != &fluentStyle ||
      lineEdit.text() != QStringLiteral("Alpha") ||
      textEdit.toPlainText() != QStringLiteral("Beta") ||
      plainTextEdit.toPlainText() != QStringLiteral("Gamma") ||
      digitalDisplay.style() != &fluentStyle ||
      digitalDisplay.digitCount() != 6 || digitalDisplay.intValue() != 2048 ||
      digitalDisplay.segmentStyle() != QLCDNumber::Flat ||
      digitalImage.pixelColor(digitalImage.rect().center()).alpha() == 0 ||
      lineSize.width() < 96 || lineSize.height() < 32 ||
      selection.style() != &fluentStyle || selection.currentIndex() != 1 ||
      selection.currentData().toInt() != 29 || comboSize.width() < 96 ||
      comboSize.height() < 32 || editableSelection.style() != &fluentStyle ||
      editableSelection.lineEdit() != comboEditor ||
      comboEditor->text() != QStringLiteral("42") ||
      !comboEditor->hasAcceptableInput() ||
      editableSelection.completer() == nullptr ||
      editableSelection.insertPolicy() != QComboBox::NoInsert ||
      popupMenu.style() != &fluentStyle ||
      openAction->shortcut() != QKeySequence::Open ||
      openAction->data().toInt() != 71 || !automaticAction->isChecked() ||
      !localAction->isChecked() || remoteAction->isChecked() ||
      popupMenu.defaultAction() != openAction ||
      popupMenu.activeAction() != automaticAction ||
      QMenu::menuInAction(exportMenu->menuAction()) != exportMenu ||
      jsonAction->text() != QStringLiteral("JSON") ||
      menuItemSize.height() < 32 || menuBar.style() != &fluentStyle ||
      menuBar.isNativeMenuBar() ||
      !menuBar.actions().contains(fileMenu->menuAction()) ||
      saveAction->shortcut() != QKeySequence::Save ||
      toolTipHost.toolTip() != QStringLiteral("Installed tooltip") ||
      calendar.selectedDate() != expectedDate ||
      picker.date() != expectedDate || picker.calendar() == nullptr ||
      picker.calendarWidget() != picker.calendar() ||
      actionCard.description() != QStringLiteral("Open preferences") ||
      imageCard.description() != QStringLiteral("Open preview") ||
      carouselView.style() != &fluentStyle ||
      carouselView.model() != &carouselModel ||
      carouselView.currentRow() != 0 ||
      carouselView.currentIndex().data().toString() !=
          QStringLiteral("Release notes") ||
      carouselView.currentIndex()
              .data(ZzFluentUI::ZzCarouselView::DescriptionRole)
              .toString() != QStringLiteral("Installed model consumer") ||
      carouselView.findChildren<QTimer *>().size() != 0 ||
      carouselImage.pixelColor(carouselImage.rect().center()).alpha() == 0 ||
      navigationPane.style() != &fluentStyle ||
      navigationPane.model() != &navigationModel ||
      navigationPane.currentSourceIndex() != navigationModel.index(1, 0) ||
      navigationPane.findChildren<ZzFluentUI::ZzNavigationView *>().size()
          != 2 ||
      progressRing.minimum() != 20 || progressRing.maximum() != 120 ||
      progressRing.value() != 70 || progressRing.ringWidth() != 6 ||
      busyRing.minimum() != 0 || busyRing.maximum() != 0 ||
      flowLayout->count() != 2 || flowLayout->horizontalSpacing() != 5 ||
      flowLayout->verticalSpacing() != 7 ||
      flowFirst->geometry() != QRect(0, 0, 40, 20) ||
      flowSecond->geometry() != QRect(0, 27, 40, 20) ||
      flowLayout->heightForWidth(84) != 47 ||
      horizontalScrollBar->orientation() != Qt::Horizontal ||
      horizontalScrollBar->minimum() != 10 ||
      horizontalScrollBar->maximum() != 110 ||
      horizontalScrollBar->value() != 60 ||
      verticalScrollBar->orientation() != Qt::Vertical ||
      verticalScrollBar->minimum() != 20 ||
      verticalScrollBar->maximum() != 220 ||
      verticalScrollBar->pageStep() != 40 ||
      verticalScrollBar->value() != 120 || integerInput.minimum() != -20 ||
      integerInput.maximum() != 80 || integerInput.value() != 24 ||
      integerInput.buttonSymbols() != QAbstractSpinBox::PlusMinus ||
      floatingInput.minimum() != -10.0 || floatingInput.maximum() != 10.0 ||
      floatingInput.decimals() != 2 || floatingInput.value() != 1.25 ||
      floatingInput.buttonSymbols() != QAbstractSpinBox::PlusMinus ||
      suggestBox.style() != &fluentStyle || suggestBox.suggestionCount() != 2 ||
      localSuggestionKey.isEmpty() || remoteSuggestionKey.isEmpty() ||
      remoteSuggestionKey == localSuggestionKey ||
      suggestBox.filterMode() != Qt::MatchContains ||
      suggestBox.completer() == nullptr ||
      suggestBox.completer()->completionModel()->rowCount() != 1 ||
      suggestBox.suggestions().at(1).data.toInt() != 82 ||
      multiSelect.style() != &fluentStyle || multiSelect.optionCount() != 3 ||
      multiSelect.selectionCount() != 2 || multiSelect.currentIndex() != -1 ||
      multiSelect.options().at(0).key == multiSelect.options().at(1).key ||
      multiSelect.options().at(2).key.isEmpty() ||
      multiSelect.selectedText() != QStringLiteral("Desktop, Logs, metrics") ||
      multiSelect.currentText() != multiSelect.selectedText() ||
      multiSelect.selectedOptions().at(1).data.toInt() != 41 ||
      multiSelect.model()
              ->data(multiSelect.model()->index(0, 0),
                     ZzFluentUI::ZzMultiSelectComboBox::KeyRole)
              .toString() != multiSelect.options().at(0).key ||
      roller.style() != &fluentStyle || roller.itemCount() != 3 ||
      roller.currentIndex() != 2 ||
      roller.currentText() != QStringLiteral("High") || !roller.wrapping() ||
      roller.findChildren<QTimer *>().size() != 0 ||
      rollerPicker.style() != &fluentStyle || rollerPicker.columnCount() != 3 ||
      rollerPicker.currentIndexes() != installedPickerIndexes ||
      rollerPicker.currentText() != QStringLiteral("09 / 00 / PM") ||
      rollerPicker.columns().at(0).key == rollerPicker.columns().at(1).key ||
      rollerPicker.columns().at(2).key.isEmpty() ||
      rollerPicker.isPopupVisible() ||
      expander.style() != &fluentStyle ||
      expander.headerText() != QStringLiteral("Installed expander") ||
      !expander.isExpanded() ||
      expander.contentWidget() != takenExpanderContent ||
      takenExpanderContent == nullptr ||
      takenExpanderContent->parentWidget() == nullptr ||
      pivot.style() != &fluentStyle || pivot.count() != 2 ||
      pivot.itemText(1) != QStringLiteral("Details") ||
      pivot.currentIndex() != 1 ||
      passwordBox.style() != &fluentStyle ||
      passwordBox.text() != QStringLiteral("Installed secret") ||
      passwordBox.revealMode() !=
          ZzFluentUI::ZzPasswordRevealMode::Visible ||
      !passwordBox.isPasswordVisible() ||
      passwordBox.echoMode() != QLineEdit::Normal ||
      splitButton.style() != &fluentStyle ||
      splitButton.appearance() !=
          ZzFluentUI::ZzButtonAppearance::Accent ||
      splitButton.menu() != &splitMenu ||
      splitMenu.parent() != nullptr || splitMenu.actions().size() != 1 ||
      splitButton.sizeHint().width() <=
          QPushButton(QStringLiteral("Installed build")).sizeHint().width() ||
      drawer.style() != &fluentStyle || drawer.isOpen() ||
      drawer.edge() != ZzFluentUI::ZzDrawerEdge::Right ||
      drawer.isModal() || drawer.widthHint() != 240 ||
      drawer.contentWidget() != installedDrawerContent ||
      installedDrawerContent == nullptr ||
      sourceTabs.fluentTabBar() == nullptr ||
      !sourceTabs.transferTabTo(&targetTabs, 0) || sourceTabs.count() != 0 ||
      targetTabs.widget(0) != tabPage) {
    return 1;
  }
  return 0;
}
