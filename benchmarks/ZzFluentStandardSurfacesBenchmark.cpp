#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QTextStream>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 100;
constexpr int zzToggleIterations = 1000;
constexpr int zzWindowWidth = 960;
constexpr int zzWindowHeight = 720;

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed()
        : QString{};
}

/** @brief 输出标准表面基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 将纳秒计时转换为性能报告使用的毫秒。 */
double zzMilliseconds(qint64 nanoseconds)
{
    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    return static_cast<double>(nanoseconds) / nanosecondsPerMillisecond;
}

/** @brief 判断固定渲染结果是否包含可见且非单色内容。 */
bool zzHasMeaningfulPixels(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }
    const QColor first = image.pixelColor(0, 0);
    bool hasVisiblePixel = first.alpha() != 0;
    bool hasDifferentPixel = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            hasVisiblePixel = hasVisiblePixel || color.alpha() != 0;
            hasDifferentPixel = hasDifferentPixel || color != first;
            if (hasVisiblePixel && hasDifferentPixel) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 对固定窗口执行一次完整绘制并返回耗时。 */
qint64 zzRenderWindow(QMainWindow *window, QImage *target)
{
    target->fill(Qt::transparent);
    QElapsedTimer timer;
    timer.start();
    window->render(target);
    return timer.nsecsElapsed();
}

/** @brief 为标准 item view 填充固定、无业务依赖的展示模型。 */
void zzPopulateModel(QStandardItemModel *model)
{
    for (int row = 0; row < 4; ++row) {
        QList<QStandardItem *> items;
        items.reserve(3);
        for (int column = 0; column < 3; ++column) {
            items.append(new QStandardItem(
                QStringLiteral("Surface %1/%2").arg(row).arg(column)));
        }
        model->appendRow(items);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    ZzFluentUI::ZzThemeController controller;
    controller.setReducedMotion(true);
    ZzFluentUI::ZzFluentStyle style(&controller);
    QMainWindow window;
    window.setObjectName(QStringLiteral("ZzFluentStandardSurfacesWindow"));
    window.setStyle(&style);
    window.resize(zzWindowWidth, zzWindowHeight);

    auto *menuBar = new QMenuBar(&window);
    menuBar->setNativeMenuBar(false);
    menuBar->setStyle(&style);
    auto *fileMenu = menuBar->addMenu(QStringLiteral("File"));
    fileMenu->setStyle(&style);
    fileMenu->addAction(QStringLiteral("Open"));
    window.setMenuBar(menuBar);

    auto *toolBar = new QToolBar(QStringLiteral("Commands"), &window);
    toolBar->setStyle(&style);
    auto *toolbarAction = toolBar->addAction(QStringLiteral("Build"));
    toolbarAction->setCheckable(true);
    window.addToolBar(toolBar);

    auto *statusBar = new QStatusBar(&window);
    statusBar->setStyle(&style);
    statusBar->showMessage(QStringLiteral("Ready"));
    window.setStatusBar(statusBar);

    auto *central = new QWidget(&window);
    central->setStyle(&style);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto *commandRow = new QHBoxLayout;
    commandRow->setSpacing(8);
    auto *checkableButton = new ZzFluentUI::ZzPushButton(
        QStringLiteral("Keep preview"), central);
    checkableButton->setStyle(&style);
    checkableButton->setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
    checkableButton->setCheckable(true);
    auto *checkBox = new QCheckBox(QStringLiteral("Diagnostics"), central);
    checkBox->setStyle(&style);
    auto *radioButton = new QRadioButton(QStringLiteral("Balanced"), central);
    radioButton->setStyle(&style);
    auto *iconButton = new ZzFluentUI::ZzIconButton(central);
    iconButton->setStyle(&style);
    iconButton->setAccessibleName(QStringLiteral("Refresh"));
    iconButton->setFixedSize(40, 40);
    iconButton->setIconDescriptor({
        QStringLiteral(
            ":/zzfluent/standard-surfaces/ZzFluentTestSquare.svg"),
        true});
    commandRow->addWidget(checkableButton);
    commandRow->addWidget(checkBox);
    commandRow->addWidget(radioButton);
    commandRow->addWidget(iconButton);
    layout->addLayout(commandRow);

    auto *form = new QFormLayout;
    auto *lineEdit = new QLineEdit(QStringLiteral("Surface input"), central);
    lineEdit->setStyle(&style);
    auto *plainTextEdit = new QPlainTextEdit(
        QStringLiteral("Surface notes"), central);
    plainTextEdit->setStyle(&style);
    plainTextEdit->setFixedHeight(56);
    auto *comboBox = new QComboBox(central);
    comboBox->setStyle(&style);
    comboBox->addItems({QStringLiteral("Local"), QStringLiteral("Remote")});
    form->addRow(QStringLiteral("Text"), lineEdit);
    form->addRow(QStringLiteral("Notes"), plainTextEdit);
    form->addRow(QStringLiteral("Mode"), comboBox);
    layout->addLayout(form);

    auto *progressRow = new QHBoxLayout;
    auto *slider = new QSlider(Qt::Horizontal, central);
    slider->setStyle(&style);
    slider->setRange(0, 100);
    slider->setValue(68);
    auto *progress = new QProgressBar(central);
    progress->setStyle(&style);
    progress->setRange(0, 100);
    progress->setValue(68);
    progressRow->addWidget(slider, 1);
    progressRow->addWidget(progress, 1);
    layout->addLayout(progressRow);

    auto *model = new QStandardItemModel(central);
    zzPopulateModel(model);
    auto *views = new QHBoxLayout;
    auto *listView = new QListView(central);
    auto *tableView = new QTableView(central);
    auto *treeView = new QTreeView(central);
    const std::array<QAbstractItemView *, 3> itemViews{
        listView,
        tableView,
        treeView};
    for (QAbstractItemView *view : itemViews) {
        view->setStyle(&style);
        view->setModel(model);
        view->setAlternatingRowColors(true);
        view->setMinimumHeight(180);
        views->addWidget(view, 1);
    }
    listView->setCurrentIndex(model->index(1, 0));
    tableView->setCurrentIndex(model->index(0, 0));
    treeView->setCurrentIndex(model->index(0, 0));
    layout->addLayout(views);

    auto *digitalDisplay = new QLCDNumber(central);
    digitalDisplay->setStyle(&style);
    digitalDisplay->setDigitCount(6);
    digitalDisplay->display(2048);
    digitalDisplay->setFixedHeight(48);
    layout->addWidget(digitalDisplay);
    window.setCentralWidget(central);

    window.show();
    window.ensurePolished();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QImage target(window.size(), QImage::Format_ARGB32_Premultiplied);
    target.setDevicePixelRatio(window.devicePixelRatioF());
    target.fill(Qt::transparent);
    window.render(&target);
    if (!zzHasMeaningfulPixels(target)) {
        return zzFail(QStringLiteral("standard surface render is blank"));
    }

    const qsizetype initialObjects = window.findChildren<QObject *>().size();
    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("fluent-standard-surfaces"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, window.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    for (int iteration = 0;
         iteration < zzWarmupIterations + zzMeasuredIterations;
         ++iteration) {
        const bool checked = (iteration % 2) == 0;
        QElapsedTimer stateTimer;
        stateTimer.start();
        checkableButton->setChecked(checked);
        checkBox->setChecked(checked);
        radioButton->setChecked(!checked);
        slider->setValue((iteration * 7) % 101);
        progress->setValue(slider->value());
        comboBox->setCurrentIndex(iteration % 2);
        toolbarAction->setChecked(checked);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        const qint64 stateElapsed = stateTimer.nsecsElapsed();
        const qint64 renderElapsed = zzRenderWindow(&window, &target);

        if (iteration >= zzWarmupIterations) {
            reporter.addSample({
                QStringLiteral("state-update-time"),
                QStringLiteral("ms"),
                zzMilliseconds(stateElapsed)});
            reporter.addSample({
                QStringLiteral("render-time"),
                QStringLiteral("ms"),
                zzMilliseconds(renderElapsed)});
            reporter.addSample({
                QStringLiteral("object-count"),
                QStringLiteral("objects"),
                static_cast<double>(window.findChildren<QObject *>().size())});
            reporter.addSample({
                QStringLiteral("style-cache-bytes"),
                QStringLiteral("bytes"),
                static_cast<double>(style.iconCacheBytes())});
        }
    }

    for (int iteration = 0; iteration < zzToggleIterations; ++iteration) {
        checkableButton->setChecked((iteration % 2) == 0);
    }
    if (window.findChildren<QObject *>().size() != initialObjects
        || checkableButton->isChecked()) {
        return zzFail(QStringLiteral(
            "checkable state toggles changed object count or final state"));
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
