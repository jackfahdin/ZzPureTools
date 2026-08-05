#include "ZzFluentControlsGalleryPrivate.h"

#include <array>

#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtGui/QActionGroup>
#include <QtGui/QFont>
#include <QtGui/QCloseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
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
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzSuggestBox.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

#include "ZzFluentControlsGallery.h"

namespace ZzExamples {

namespace {

/** @brief 由画廊应用拥有、关闭时按策略回填页面的普通顶层宿主。 */
class ZzDetachedTabWindow final : public QWidget
{
public:
    /** @brief 创建尚未接收页面的演示浮窗。 */
    ZzDetachedTabWindow(
        ZzFluentUI::ZzTabWidget *source,
        QWidget *owner)
        : QWidget(owner, Qt::Window)
        , origin(source)
        , tabs(new ZzFluentUI::ZzTabWidget(this))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QStringLiteral("Detached tab"));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        tabs->setTabsClosable(true);
        layout->addWidget(tabs);
        resize(560, 360);
        QObject::connect(
            tabs,
            &QTabWidget::currentChanged,
            this,
            [this](int index) {
                if (index < 0 && !returningPages) {
                    close();
                }
            });
    }

    /** @brief 返回浮窗内的目标标签容器。 */
    [[nodiscard]] ZzFluentUI::ZzTabWidget *tabHost() const noexcept
    {
        return tabs;
    }

protected:
    /** @brief 关闭前把仍存在的页面同步转回来源。 */
    void closeEvent(QCloseEvent *event) override
    {
        if (event == nullptr) {
            return;
        }
        returningPages = true;
        while (!origin.isNull() && tabs->count() > 0) {
            if (!tabs->transferTabTo(origin, 0)) {
                returningPages = false;
                event->ignore();
                return;
            }
        }
        returningPages = false;
        QWidget::closeEvent(event);
    }

private:
    QPointer<ZzFluentUI::ZzTabWidget> origin;
    ZzFluentUI::ZzTabWidget *const tabs;
    bool returningPages = false;
};

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

/** @brief 使用当前 palette 创建无文件依赖的确定性卡片预览图。 */
QPixmap zzCardPreviewPixmap(const QPalette &palette)
{
    QPixmap pixmap(640, 360);
    pixmap.fill(palette.color(QPalette::AlternateBase));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Highlight));
    painter.drawRect(QRect(0, 0, 250, 360));
    painter.setBrush(palette.color(QPalette::Button));
    painter.drawEllipse(QPoint(455, 180), 105, 105);
    painter.setBrush(palette.color(QPalette::Base));
    painter.drawRoundedRect(QRect(285, 78, 310, 62), 6, 6);
    painter.drawRoundedRect(QRect(285, 166, 235, 40), 6, 6);
    return pixmap;
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

    auto *scrollArea = new ZzFluentUI::ZzScrollArea(q_ptr);
    scrollArea->setWidgetResizable(true);
    auto *content = new QWidget(scrollArea);
    content->setMinimumSize(1120, 980);
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
    icon->setToolTip(QStringLiteral(
        "Refresh the current workspace snapshot"));
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
    name->setPlaceholderText(QStringLiteral("Workspace name"));
    name->setClearButtonEnabled(true);
    auto *password = new QLineEdit(container);
    password->setText(QStringLiteral("Fluent-2026"));
    password->setEchoMode(QLineEdit::Password);
    auto *notes = new QTextEdit(container);
    notes->setHtml(QStringLiteral(
        "<b>Fluent controls</b><br>Cross-platform UI"));
    notes->setFixedHeight(72);
    auto *output = new QPlainTextEdit(container);
    output->setPlainText(QStringLiteral("configure: ok\nbuild: ready"));
    output->setFixedHeight(72);
    auto *rightToLeftText = new QLineEdit(container);
    rightToLeftText->setLayoutDirection(Qt::RightToLeft);
    rightToLeftText->setText(QStringLiteral("RTL input"));
    auto *readOnlyText = new QLineEdit(container);
    readOnlyText->setText(QStringLiteral("Read-only value"));
    readOnlyText->setReadOnly(true);
    auto *disabledText = new QLineEdit(container);
    disabledText->setText(QStringLiteral("Disabled value"));
    disabledText->setEnabled(false);
    auto *density = new QComboBox(container);
    density->addItems({
        QStringLiteral("Balanced"),
        QStringLiteral("Compact"),
        QStringLiteral("Comfortable")});
    auto *environment = new QComboBox(container);
    environment->setPlaceholderText(QStringLiteral("Select environment"));
    environment->addItems({
        QStringLiteral("Local"),
        QStringLiteral("Staging"),
        QStringLiteral("Production")});
    environment->setCurrentIndex(-1);
    auto *storage = new QComboBox(container);
    storage->addItem(
        storage->style()->standardIcon(QStyle::SP_DriveHDIcon),
        QStringLiteral("Local disk"));
    storage->addItem(
        storage->style()->standardIcon(QStyle::SP_DriveNetIcon),
        QStringLiteral("Network share"));
    auto *editableTarget = new QComboBox(container);
    editableTarget->setEditable(true);
    editableTarget->setInsertPolicy(QComboBox::NoInsert);
    editableTarget->addItems({
        QStringLiteral("Debug"),
        QStringLiteral("Release"),
        QStringLiteral("RelWithDebInfo")});
    editableTarget->setCompleter(new QCompleter(
        QStringList{
            QStringLiteral("Debug"),
            QStringLiteral("Release"),
            QStringLiteral("RelWithDebInfo")},
        editableTarget));
    auto *disabledChoice = new QComboBox(container);
    disabledChoice->addItem(QStringLiteral("Unavailable"));
    disabledChoice->setEnabled(false);
    auto *rightToLeftChoice = new QComboBox(container);
    rightToLeftChoice->setLayoutDirection(Qt::RightToLeft);
    rightToLeftChoice->addItems({
        QStringLiteral("Primary"),
        QStringLiteral("Secondary")});
    auto *suggestBox = new ZzFluentUI::ZzSuggestBox(container);
    suggestBox->setAccessibleName(QStringLiteral("Command search"));
    suggestBox->setPlaceholderText(QStringLiteral("Search commands"));
    suggestBox->setSuggestions({
        {QStringLiteral("open-local"), QStringLiteral("Open workspace"),
         suggestBox->style()->standardIcon(QStyle::SP_DirOpenIcon),
         QStringLiteral("local"), true},
        {QStringLiteral("open-remote"), QStringLiteral("Open workspace"),
         suggestBox->style()->standardIcon(QStyle::SP_DriveNetIcon),
         QStringLiteral("remote"), true},
        {QStringLiteral("clean"), QStringLiteral("Clean build output"),
         {}, QStringLiteral("clean"), true},
        {QStringLiteral("unavailable"),
         QStringLiteral("Deploy to unavailable target"), {},
         QStringLiteral("deploy"), false}});
    auto *suggestResult = new QLabel(QStringLiteral("No command selected"),
                                     container);
    QObject::connect(
        suggestBox,
        &ZzFluentUI::ZzSuggestBox::suggestionActivated,
        suggestResult,
        [suggestResult](const ZzFluentUI::ZzSuggestion &suggestion) {
            suggestResult->setText(QStringLiteral("%1 [%2]")
                                       .arg(suggestion.text,
                                            suggestion.data.toString()));
        });
    auto *multiSelect =
        new ZzFluentUI::ZzMultiSelectComboBox(container);
    multiSelect->setAccessibleName(QStringLiteral("Deployment scopes"));
    multiSelect->setPlaceholderText(QStringLiteral("Select scopes"));
    multiSelect->setOptions({
        {QStringLiteral("local"), QStringLiteral("Desktop"),
         multiSelect->style()->standardIcon(QStyle::SP_ComputerIcon),
         QStringLiteral("local"), true, true},
        {QStringLiteral("remote"), QStringLiteral("Desktop"),
         multiSelect->style()->standardIcon(QStyle::SP_DriveNetIcon),
         QStringLiteral("remote"), true, false},
        {QStringLiteral("telemetry"), QStringLiteral("Logs, metrics"),
         {}, QStringLiteral("telemetry"), true, true},
        {QStringLiteral("retired"), QStringLiteral("Retired target"),
         {}, QStringLiteral("retired"), false, false}});
    auto *multiSelectResult = new QLabel(multiSelect->selectedText(),
                                         container);
    QObject::connect(
        multiSelect,
        &ZzFluentUI::ZzMultiSelectComboBox::selectionChanged,
        multiSelectResult,
        [multiSelect, multiSelectResult]() {
            multiSelectResult->setText(multiSelect->selectedText());
        });
    QObject::connect(
        multiSelect,
        &ZzFluentUI::ZzMultiSelectComboBox::optionToggled,
        multiSelectResult,
        [multiSelectResult](
            const ZzFluentUI::ZzMultiSelectOption &option,
            bool selected) {
            multiSelectResult->setToolTip(
                QStringLiteral("%1 [%2]")
                    .arg(option.key,
                         selected ? QStringLiteral("selected")
                                  : QStringLiteral("cleared")));
        });
    datePicker = new ZzFluentUI::ZzCalendarPicker(container);
    datePicker->setAccessibleName(QStringLiteral("Due date"));
    datePicker->setLocale(QLocale::c());
    datePicker->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    datePicker->setDateRange(QDate(2026, 1, 1), QDate(2026, 12, 31));
    datePicker->setDate(QDate(2026, 8, 5));
    form->addRow(QStringLiteral("Name"), name);
    form->addRow(QStringLiteral("Password"), password);
    form->addRow(QStringLiteral("Notes"), notes);
    form->addRow(QStringLiteral("Output"), output);
    form->addRow(QStringLiteral("RTL text"), rightToLeftText);
    form->addRow(QStringLiteral("Read only"), readOnlyText);
    form->addRow(QStringLiteral("Disabled"), disabledText);
    form->addRow(QStringLiteral("Density"), density);
    form->addRow(QStringLiteral("Environment"), environment);
    form->addRow(QStringLiteral("Storage"), storage);
    form->addRow(QStringLiteral("Build target"), editableTarget);
    form->addRow(QStringLiteral("Unavailable"), disabledChoice);
    form->addRow(QStringLiteral("RTL choice"), rightToLeftChoice);
    form->addRow(QStringLiteral("Command"), suggestBox);
    form->addRow(QStringLiteral("Selection"), suggestResult);
    form->addRow(QStringLiteral("Deployment scopes"), multiSelect);
    form->addRow(QStringLiteral("Selected scopes"), multiSelectResult);
    form->addRow(QStringLiteral("Due date"), datePicker);

    auto *workers = new ZzFluentUI::ZzSpinBox(container);
    workers->setRange(1, 64);
    workers->setValue(8);
    workers->setSuffix(QStringLiteral(" threads"));
    auto *threshold = new ZzFluentUI::ZzDoubleSpinBox(container);
    threshold->setRange(0.0, 10.0);
    threshold->setDecimals(2);
    threshold->setSingleStep(0.25);
    threshold->setValue(1.25);
    threshold->setSuffix(QStringLiteral(" ms"));
    auto *rightToLeftInput = new ZzFluentUI::ZzSpinBox(container);
    rightToLeftInput->setLayoutDirection(Qt::RightToLeft);
    rightToLeftInput->setRange(-100, 100);
    rightToLeftInput->setValue(24);
    auto *readOnlyInput = new ZzFluentUI::ZzDoubleSpinBox(container);
    readOnlyInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    readOnlyInput->setReadOnly(true);
    readOnlyInput->setValue(3.14);
    auto *disabledInput = new ZzFluentUI::ZzSpinBox(container);
    disabledInput->setValue(12);
    disabledInput->setEnabled(false);
    form->addRow(QStringLiteral("Workers"), workers);
    form->addRow(QStringLiteral("Threshold"), threshold);
    form->addRow(QStringLiteral("RTL value"), rightToLeftInput);
    form->addRow(QStringLiteral("Read only"), readOnlyInput);
    form->addRow(QStringLiteral("Disabled"), disabledInput);
    layout->addLayout(form);

    auto *slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(0, 100);
    slider->setValue(62);
    layout->addWidget(slider);
    auto *progress = new QProgressBar(container);
    progress->setValue(68);
    progress->setFormat(QStringLiteral("68% complete"));
    layout->addWidget(progress);

    layout->addWidget(zzSectionTitle(QStringLiteral("Scrolling"), container));
    auto *scrollRow = new QHBoxLayout;
    scrollRow->setContentsMargins(0, 0, 0, 0);
    scrollRow->setSpacing(12);
    auto *horizontalBars = new QVBoxLayout;
    horizontalBars->setContentsMargins(0, 0, 0, 0);
    horizontalBars->setSpacing(10);

    auto *horizontal = new ZzFluentUI::ZzScrollBar(
        Qt::Horizontal,
        container);
    horizontal->setRange(0, 100);
    horizontal->setPageStep(12);
    horizontal->setValue(36);
    horizontal->setFixedHeight(12);
    horizontalBars->addWidget(horizontal);

    auto *rightToLeft = new ZzFluentUI::ZzScrollBar(
        Qt::Horizontal,
        container);
    rightToLeft->setLayoutDirection(Qt::RightToLeft);
    rightToLeft->setRange(0, 100);
    rightToLeft->setPageStep(42);
    rightToLeft->setValue(68);
    rightToLeft->setFixedHeight(12);
    horizontalBars->addWidget(rightToLeft);

    auto *disabled = new ZzFluentUI::ZzScrollBar(
        Qt::Horizontal,
        container);
    disabled->setRange(0, 100);
    disabled->setPageStep(24);
    disabled->setValue(52);
    disabled->setEnabled(false);
    disabled->setFixedHeight(12);
    horizontalBars->addWidget(disabled);
    scrollRow->addLayout(horizontalBars, 1);

    auto *shortVertical = new ZzFluentUI::ZzScrollBar(
        Qt::Vertical,
        container);
    shortVertical->setRange(0, 100);
    shortVertical->setPageStep(8);
    shortVertical->setValue(24);
    shortVertical->setFixedSize(12, 92);
    scrollRow->addWidget(shortVertical);

    auto *longVertical = new ZzFluentUI::ZzScrollBar(
        Qt::Vertical,
        container);
    longVertical->setRange(0, 100);
    longVertical->setPageStep(55);
    longVertical->setValue(64);
    longVertical->setFixedSize(12, 92);
    scrollRow->addWidget(longVertical);
    layout->addLayout(scrollRow);

    auto *ringRow = new QHBoxLayout;
    ringRow->setContentsMargins(0, 0, 0, 0);
    ringRow->setSpacing(10);
    const auto addDeterminateRing = [container, ringRow](
                                          int value,
                                          int width = 4) {
        auto *ring = new ZzFluentUI::ZzProgressRing(container);
        ring->setAccessibleName(
            QStringLiteral("Progress %1 percent").arg(value));
        ring->setValue(value);
        ring->setRingWidth(width);
        ringRow->addWidget(ring);
    };
    addDeterminateRing(25);
    addDeterminateRing(72, 6);
    addDeterminateRing(100);
    auto *busyRing = new ZzFluentUI::ZzProgressRing(container);
    busyRing->setAccessibleName(QStringLiteral("Progress in progress"));
    busyRing->setTextVisible(false);
    busyRing->setRange(0, 0);
    ringRow->addWidget(busyRing);
    auto *disabledBusyRing = new ZzFluentUI::ZzProgressRing(container);
    disabledBusyRing->setAccessibleName(
        QStringLiteral("Disabled progress"));
    disabledBusyRing->setTextVisible(false);
    disabledBusyRing->setRange(0, 0);
    disabledBusyRing->setEnabled(false);
    ringRow->addWidget(disabledBusyRing);
    ringRow->addStretch(1);
    layout->addLayout(ringRow);

    auto *message = new ZzFluentUI::ZzMessageBar(container);
    message->setText(QStringLiteral("Settings saved successfully"));
    message->setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
    QObject::connect(
        message,
        &ZzFluentUI::ZzMessageBar::closeRequested,
        message,
        &QWidget::hide);
    layout->addWidget(message);

    layout->addWidget(zzSectionTitle(QStringLiteral("Tabs"), container));
    auto *tabRow = new QHBoxLayout;
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(8);
    auto *primaryTabs = new ZzFluentUI::ZzTabWidget(container);
    auto *secondaryTabs = new ZzFluentUI::ZzTabWidget(container);
    primaryTabs->setTabsClosable(true);
    secondaryTabs->setTabsClosable(true);
    for (const QString &text : {
             QStringLiteral("Overview"),
             QStringLiteral("Details"),
             QStringLiteral("History")}) {
        auto *page = new QLabel(text, primaryTabs);
        page->setAlignment(Qt::AlignCenter);
        primaryTabs->addTab(page, text);
    }
    for (const QString &text : {
             QStringLiteral("Output"),
             QStringLiteral("Problems")}) {
        auto *page = new QLabel(text, secondaryTabs);
        page->setAlignment(Qt::AlignCenter);
        secondaryTabs->addTab(page, text);
    }
    primaryTabs->setCurrentIndex(1);
    primaryTabs->setFixedHeight(150);
    secondaryTabs->setFixedHeight(150);
    bindTabHost(primaryTabs);
    bindTabHost(secondaryTabs);
    tabRow->addWidget(primaryTabs, 1);
    tabRow->addWidget(secondaryTabs, 1);
    layout->addLayout(tabRow);

    layout->addWidget(zzSectionTitle(QStringLiteral("Cards"), container));
    auto *actionCard = new ZzFluentUI::ZzActionCard(
        QStringLiteral("Workspace settings"),
        QStringLiteral("Review local appearance and behavior"),
        container);
    actionCard->setIcon(
        actionCard->style()->standardIcon(QStyle::SP_ComputerIcon));
    actionCard->setCheckable(true);
    actionCard->setFixedHeight(80);
    layout->addWidget(actionCard);

    auto *imageCard = new ZzFluentUI::ZzImageCard(
        QStringLiteral("Project Aurora"),
        QStringLiteral("Local preview image"),
        container);
    imageCard->setPixmap(zzCardPreviewPixmap(imageCard->palette()));
    imageCard->setCheckable(true);
    imageCard->setFixedHeight(220);
    layout->addWidget(imageCard);
    QObject::connect(
        actionCard,
        &QAbstractButton::clicked,
        message,
        [message](bool checked) {
            message->setText(
                checked
                    ? QStringLiteral("Workspace card selected")
                    : QStringLiteral("Workspace card cleared"));
        });
    QObject::connect(
        imageCard,
        &QAbstractButton::clicked,
        message,
        [message](bool checked) {
            message->setText(
                checked
                    ? QStringLiteral("Project preview selected")
                    : QStringLiteral("Project preview cleared"));
        });
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
    table->setFixedHeight(150);
    layout->addWidget(table);

    layout->addWidget(zzSectionTitle(QStringLiteral("Calendar"), container));
    calendar = new ZzFluentUI::ZzCalendar(container);
    calendar->setAccessibleName(QStringLiteral("Release calendar"));
    calendar->setLocale(QLocale::c());
    calendar->setFirstDayOfWeek(Qt::Monday);
    calendar->setDateRange(QDate(2026, 1, 1), QDate(2026, 12, 31));
    calendar->setSelectedDate(QDate(2026, 8, 5));
    calendar->setFixedHeight(230);
    layout->addWidget(calendar);
    if (datePicker != nullptr) {
        QObject::connect(
            datePicker,
            &QDateEdit::dateChanged,
            calendar,
            &QCalendarWidget::setSelectedDate);
        QObject::connect(
            calendar,
            &QCalendarWidget::selectionChanged,
            datePicker,
            [this] {
                if (calendar != nullptr && datePicker != nullptr) {
                    datePicker->setDate(calendar->selectedDate());
                }
            });
    }

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
    tree->setMinimumHeight(150);
    layout->addWidget(tree, 1);

    auto *menuBar = new QMenuBar(container);
    menuBar->setNativeMenuBar(false);
    QMenu *fileMenu = menuBar->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&Open"), QKeySequence::Open);
    fileMenu->addAction(QStringLiteral("&Save"), QKeySequence::Save);
    QMenu *viewMenu = menuBar->addMenu(QStringLiteral("&View"));
    QAction *compact = viewMenu->addAction(QStringLiteral("Compact mode"));
    compact->setCheckable(true);
    compact->setChecked(true);
    QAction *help = menuBar->addAction(QStringLiteral("Help"));
    help->setEnabled(false);
    layout->addWidget(menuBar);

    auto *commandRow = new QHBoxLayout;
    auto *menuButton = new QPushButton(QStringLiteral("Menu"), container);
    auto *menu = new QMenu(menuButton);
    menu->addSection(QStringLiteral("Workspace"));
    QAction *openWorkspace = menu->addAction(
        menuButton->style()->standardIcon(QStyle::SP_DirIcon),
        QStringLiteral("Open workspace"));
    openWorkspace->setShortcut(QKeySequence::Open);
    QAction *automaticSync = menu->addAction(
        QStringLiteral("Automatic sync"));
    automaticSync->setCheckable(true);
    automaticSync->setChecked(true);
    menu->addSeparator();
    auto *modeGroup = new QActionGroup(menu);
    modeGroup->setExclusive(true);
    QAction *local = menu->addAction(QStringLiteral("Local mode"));
    QAction *remote = menu->addAction(QStringLiteral("Remote mode"));
    local->setCheckable(true);
    remote->setCheckable(true);
    local->setChecked(true);
    modeGroup->addAction(local);
    modeGroup->addAction(remote);
    QMenu *exportMenu = menu->addMenu(QStringLiteral("Export"));
    exportMenu->addAction(QStringLiteral("JSON"));
    exportMenu->addAction(QStringLiteral("CSV"));
    QMenu *rtlMenu = menu->addMenu(QStringLiteral("RTL preview"));
    rtlMenu->setLayoutDirection(Qt::RightToLeft);
    rtlMenu->addAction(QStringLiteral("Right to left"));
    QAction *unavailable = menu->addAction(QStringLiteral("Unavailable"));
    unavailable->setEnabled(false);
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

void ZzFluentControlsGalleryPrivate::bindTabHost(
    ZzFluentUI::ZzTabWidget *tabs)
{
    if (tabs == nullptr) {
        return;
    }
    QObject::connect(
        tabs,
        &QTabWidget::tabCloseRequested,
        q_ptr,
        [tabs](int index) {
            QWidget *page = tabs->widget(index);
            if (page == nullptr) {
                return;
            }
            tabs->removeTab(index);
            page->deleteLater();
        });
    QObject::connect(
        tabs,
        &ZzFluentUI::ZzTabWidget::tearOffRequested,
        q_ptr,
        [this, tabs](
            int index,
            QWidget *,
            const QPoint &globalPosition) {
            showDetachedTab(tabs, index, globalPosition);
        });
}

void ZzFluentControlsGalleryPrivate::showDetachedTab(
    ZzFluentUI::ZzTabWidget *source,
    int index,
    const QPoint &globalPosition)
{
    if (source == nullptr || index < 0 || index >= source->count()) {
        return;
    }
    const QString title = source->tabText(index);
    auto *window = new ZzDetachedTabWindow(source, q_ptr);
    ZzFluentUI::ZzTabWidget *target = window->tabHost();
    bindTabHost(target);
    if (!source->transferTabTo(target, index)) {
        delete window;
        return;
    }
    window->setWindowTitle(title);
    window->move(globalPosition - QPoint(window->width() / 2, 20));
    window->show();
}

} // namespace ZzExamples
