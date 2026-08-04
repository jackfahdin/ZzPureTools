#include "ZzFluentControlsGalleryPrivate.h"

#include <array>

#include <QtGui/QFont>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

#include "ZzFluentControlsGallery.h"

namespace ZzExamples {

namespace {

/** @brief 创建用于分隔控件组的紧凑标题。 */
QLabel *zzSectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

/** @brief 为展示列创建统一、无额外边框的垂直布局。 */
QVBoxLayout *zzColumnLayout(QWidget *container)
{
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 8, 12, 12);
    layout->setSpacing(10);
    return layout;
}

} // namespace

ZzFluentControlsGalleryPrivate::ZzFluentControlsGalleryPrivate(
    ZzFluentControlsGallery *q,
    ZzFluentUI::ZzThemeController *themeController)
    : q_ptr(q)
    , controller(themeController)
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(controller != nullptr);
    auto *rootLayout = new QVBoxLayout(q_ptr);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(q_ptr);
    titleBar->setTitle(QStringLiteral("ZzFluentUI Controls"));
    titleBar->setFixedHeight(40);
    rootLayout->addWidget(titleBar);
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::minimizeRequested,
        q_ptr,
        &QWidget::showMinimized);
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::maximizeRestoreRequested,
        q_ptr,
        [this, titleBar] {
            const bool maximize = !q_ptr->isMaximized();
            if (maximize) {
                q_ptr->showMaximized();
            } else {
                q_ptr->showNormal();
            }
            titleBar->setMaximized(maximize);
        });
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::closeRequested,
        q_ptr,
        &QWidget::close);

    buildThemeSelector(rootLayout);

    auto *scrollArea = new QScrollArea(q_ptr);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scrollArea);
    content->setMinimumSize(1120, 650);
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(12, 8, 12, 12);
    contentLayout->setSpacing(0);
    auto *splitter = new QSplitter(Qt::Horizontal, content);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildNavigationColumn(splitter));
    splitter->addWidget(buildControlsColumn(splitter));
    splitter->addWidget(buildDataColumn(splitter));
    splitter->setSizes({250, 450, 420});
    contentLayout->addWidget(splitter);
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea, 1);
}

void ZzFluentControlsGalleryPrivate::buildThemeSelector(
    QVBoxLayout *rootLayout)
{
    auto *host = new QWidget(q_ptr);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(6);
    layout->addWidget(new QLabel(QStringLiteral("Theme"), host));

    auto *group = new QButtonGroup(host);
    group->setExclusive(true);
    const std::array<std::pair<QString, ZzFluentUI::ZzThemeMode>, 4> modes{{
        {QStringLiteral("System"), ZzFluentUI::ZzThemeMode::System},
        {QStringLiteral("Light"), ZzFluentUI::ZzThemeMode::Light},
        {QStringLiteral("Dark"), ZzFluentUI::ZzThemeMode::Dark},
        {QStringLiteral("High contrast"),
         ZzFluentUI::ZzThemeMode::HighContrast}}};
    for (const auto &[text, mode] : modes) {
        auto *button = new ZzFluentUI::ZzPushButton(text, host);
        button->setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
        button->setCheckable(true);
        button->setMinimumWidth(96);
        const int identifier = static_cast<int>(mode);
        group->addButton(button, identifier);
        layout->addWidget(button);
        if (mode == controller->mode()) {
            button->setChecked(true);
        }
    }
    layout->addStretch(1);
    QObject::connect(
        group,
        &QButtonGroup::idClicked,
        q_ptr,
        [this](int identifier) {
            if (controller != nullptr) {
                controller->setMode(
                    static_cast<ZzFluentUI::ZzThemeMode>(identifier));
            }
        });
    rootLayout->addWidget(host);
}

QWidget *ZzFluentControlsGalleryPrivate::buildNavigationColumn(
    QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = zzColumnLayout(container);
    layout->addWidget(zzSectionTitle(QStringLiteral("Navigation"), container));

    navigationModel = new QStandardItemModel(container);
    for (const QString &text : {
             QStringLiteral("Home"),
             QStringLiteral("Activity"),
             QStringLiteral("Projects"),
             QStringLiteral("Settings")}) {
        navigationModel->appendRow(new QStandardItem(text));
    }
    auto *navigation = new ZzFluentUI::ZzNavigationView(container);
    navigation->setModel(navigationModel);
    navigation->setCurrentIndex(navigationModel->index(0, 0));
    navigation->setMinimumHeight(190);
    layout->addWidget(navigation);

    auto *breadcrumb = new ZzFluentUI::ZzBreadcrumbBar(container);
    breadcrumb->setItems({
        QStringLiteral("Home"),
        QStringLiteral("Library"),
        QStringLiteral("Current")});
    breadcrumb->setCurrentIndex(2);
    breadcrumb->setFixedHeight(40);
    layout->addWidget(breadcrumb);

    layout->addWidget(zzSectionTitle(QStringLiteral("List"), container));
    listModel = new QStandardItemModel(container);
    for (const QString &text : {
             QStringLiteral("Design notes"),
             QStringLiteral("Release checklist"),
             QStringLiteral("Performance report"),
             QStringLiteral("Theme tokens")}) {
        listModel->appendRow(new QStandardItem(text));
    }
    auto *list = new QListView(container);
    list->setModel(listModel);
    list->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(list));
    list->setCurrentIndex(listModel->index(1, 0));
    layout->addWidget(list, 1);
    return container;
}

QWidget *ZzFluentControlsGalleryPrivate::buildControlsColumn(
    QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = zzColumnLayout(container);
    layout->addWidget(zzSectionTitle(QStringLiteral("Actions"), container));

    auto *buttonRow = new QHBoxLayout;
    auto *standard = new ZzFluentUI::ZzPushButton(
        QStringLiteral("Standard"), container);
    auto *accent = new ZzFluentUI::ZzPushButton(
        QStringLiteral("Accent"), container);
    accent->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
    auto *subtle = new ZzFluentUI::ZzPushButton(
        QStringLiteral("Subtle"), container);
    subtle->setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
    auto *icon = new ZzFluentUI::ZzIconButton(container);
    icon->setAccessibleName(QStringLiteral("Refresh"));
    icon->setToolTip(QStringLiteral("Refresh"));
    icon->setIconDescriptor({
        QStringLiteral(":/zzfluent/gallery/ZzGalleryRefresh.svg"),
        true});
    icon->setFixedSize(36, 36);
    buttonRow->addWidget(standard);
    buttonRow->addWidget(accent);
    buttonRow->addWidget(subtle);
    buttonRow->addWidget(icon);
    layout->addLayout(buttonRow);

    auto *choiceRow = new QHBoxLayout;
    auto *toggle = new ZzFluentUI::ZzToggleSwitch(
        QStringLiteral("Sync"), container);
    toggle->setChecked(true);
    auto *checkBox = new QCheckBox(QStringLiteral("Backup"), container);
    checkBox->setChecked(true);
    auto *radioButton = new QRadioButton(QStringLiteral("Local"), container);
    radioButton->setChecked(true);
    choiceRow->addWidget(toggle);
    choiceRow->addWidget(checkBox);
    choiceRow->addWidget(radioButton);
    layout->addLayout(choiceRow);

    layout->addWidget(zzSectionTitle(QStringLiteral("Input"), container));
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setVerticalSpacing(8);
    auto *name = new QLineEdit(container);
    name->setText(QStringLiteral("Workspace"));
    auto *notes = new QTextEdit(container);
    notes->setPlainText(QStringLiteral("Fluent controls\nCross-platform UI"));
    notes->setFixedHeight(72);
    auto *density = new QComboBox(container);
    density->addItems({
        QStringLiteral("Balanced"),
        QStringLiteral("Compact"),
        QStringLiteral("Comfortable")});
    form->addRow(QStringLiteral("Name"), name);
    form->addRow(QStringLiteral("Notes"), notes);
    form->addRow(QStringLiteral("Density"), density);
    layout->addLayout(form);

    auto *slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(0, 100);
    slider->setValue(62);
    layout->addWidget(slider);
    auto *progress = new QProgressBar(container);
    progress->setValue(68);
    progress->setFormat(QStringLiteral("68% complete"));
    layout->addWidget(progress);

    auto *message = new ZzFluentUI::ZzMessageBar(container);
    message->setText(QStringLiteral("Settings saved successfully"));
    message->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
    QObject::connect(
        message,
        &ZzFluentUI::ZzMessageBar::closeRequested,
        message,
        &QWidget::hide);
    layout->addWidget(message);

    auto *tabs = new QTabBar(container);
    tabs->addTab(QStringLiteral("Overview"));
    tabs->addTab(QStringLiteral("Details"));
    tabs->addTab(QStringLiteral("History"));
    tabs->setCurrentIndex(1);
    layout->addWidget(tabs);
    layout->addStretch(1);
    return container;
}

QWidget *ZzFluentControlsGalleryPrivate::buildDataColumn(QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = zzColumnLayout(container);
    layout->addWidget(zzSectionTitle(QStringLiteral("Table"), container));

    tableModel = new QStandardItemModel(4, 3, container);
    for (int row = 0; row < tableModel->rowCount(); ++row) {
        for (int column = 0; column < tableModel->columnCount(); ++column) {
            tableModel->setData(
                tableModel->index(row, column),
                QStringLiteral("R%1 C%2").arg(row + 1).arg(column + 1));
        }
    }
    auto *table = new QTableView(container);
    table->setModel(tableModel);
    table->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(table));
    table->horizontalHeader()->hide();
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setCurrentIndex(tableModel->index(1, 1));
    table->setFixedHeight(200);
    layout->addWidget(table);

    layout->addWidget(zzSectionTitle(QStringLiteral("Tree"), container));
    treeModel = new QStandardItemModel(container);
    auto *workspace = new QStandardItem(QStringLiteral("Workspace"));
    workspace->appendRow(new QStandardItem(QStringLiteral("Sources")));
    workspace->appendRow(new QStandardItem(QStringLiteral("Resources")));
    auto *tests = new QStandardItem(QStringLiteral("Tests"));
    tests->appendRow(new QStandardItem(QStringLiteral("Accessibility")));
    tests->appendRow(new QStandardItem(QStringLiteral("Screenshots")));
    treeModel->appendRow(workspace);
    treeModel->appendRow(tests);
    auto *tree = new QTreeView(container);
    tree->setModel(treeModel);
    tree->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(tree));
    tree->header()->hide();
    tree->expandAll();
    tree->setCurrentIndex(workspace->index());
    tree->setMinimumHeight(220);
    layout->addWidget(tree, 1);

    auto *commandRow = new QHBoxLayout;
    auto *menuButton = new QPushButton(QStringLiteral("Menu"), container);
    auto *menu = new QMenu(menuButton);
    menu->addAction(QStringLiteral("Open workspace"));
    menu->addAction(QStringLiteral("Save snapshot"));
    menu->addSeparator();
    menu->addAction(QStringLiteral("Close"));
    menuButton->setMenu(menu);
    auto *dialogButton = new ZzFluentUI::ZzPushButton(
        QStringLiteral("Dialog"), container);
    QObject::connect(
        dialogButton,
        &QPushButton::clicked,
        q_ptr,
        [this] { showDialog(); });
    commandRow->addWidget(menuButton);
    commandRow->addWidget(dialogButton);
    commandRow->addStretch(1);
    layout->addLayout(commandRow);
    return container;
}

void ZzFluentControlsGalleryPrivate::showDialog()
{
    auto *dialog = new QDialog(q_ptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("ZzFluentUI Dialog"));
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(new QLabel(QStringLiteral("Workspace settings"), dialog));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        dialog,
        &QDialog::reject);
    layout->addWidget(buttons);
    dialog->resize(360, 140);
    dialog->show();
}

} // namespace ZzExamples
