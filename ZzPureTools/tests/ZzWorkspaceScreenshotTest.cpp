#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontInfo>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzAnnotatedScrollBar.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandBar.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzExplorerPane.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

namespace {

constexpr QSize zzLogicalSurfaceSize(1200, 800);
constexpr QSize zzNarrowWorkspaceSurfaceSize(480, 540);
constexpr int zzChannelTolerance = 3;

constexpr qreal zzMaximumDifferenceRatio()
{
#if QT_VERSION_MAJOR == ZZ_PURETOOLS_SCREENSHOT_REFERENCE_QT_MAJOR \
    && QT_VERSION_MINOR == ZZ_PURETOOLS_SCREENSHOT_REFERENCE_QT_MINOR
    return 0.005;
#else
    return 0.02;
#endif
}

struct ZzScreenshotArguments final
{
    qreal expectedDpr = 0.0;
    QString baselineSubdirectory;
    std::vector<QByteArray> filteredArguments;
};

struct ZzImageComparison final
{
    qsizetype comparedPixels = 0;
    qsizetype differentPixels = 0;
    QImage difference;
};

[[nodiscard]] std::optional<ZzScreenshotArguments> zzParseArguments(
    int argc,
    char *argv[],
    QString *error)
{
    ZzScreenshotArguments result;
    result.filteredArguments.reserve(static_cast<std::size_t>(argc));
    result.filteredArguments.emplace_back(argv[0]);
    bool hasExpectedDpr = false;
    bool hasBaselineSubdirectory = false;
    for (int index = 1; index < argc; ++index) {
        const QByteArray argument(argv[index]);
        if (argument == QByteArrayLiteral("--expected-dpr")) {
            if (++index >= argc || hasExpectedDpr) {
                *error = QStringLiteral("--expected-dpr 必须且只能提供一次有效值");
                return std::nullopt;
            }
            bool valid = false;
            result.expectedDpr = QByteArray(argv[index]).toDouble(&valid);
            if (!valid || !std::isfinite(result.expectedDpr)
                || result.expectedDpr <= 0.0) {
                *error = QStringLiteral("--expected-dpr 必须是有限正数");
                return std::nullopt;
            }
            hasExpectedDpr = true;
            continue;
        }
        if (argument == QByteArrayLiteral("--baseline-subdir")) {
            if (++index >= argc || hasBaselineSubdirectory) {
                *error = QStringLiteral(
                    "--baseline-subdir 必须且只能提供一次有效值");
                return std::nullopt;
            }
            result.baselineSubdirectory = QString::fromLocal8Bit(argv[index]);
            const bool safeName = !result.baselineSubdirectory.isEmpty()
                && !result.baselineSubdirectory.contains(QLatin1Char('/'))
                && !result.baselineSubdirectory.contains(QLatin1Char('\\'))
                && result.baselineSubdirectory != QStringLiteral(".")
                && result.baselineSubdirectory != QStringLiteral("..");
            if (!safeName) {
                *error = QStringLiteral(
                    "--baseline-subdir 必须是单个安全目录名");
                return std::nullopt;
            }
            hasBaselineSubdirectory = true;
            continue;
        }
        result.filteredArguments.push_back(argument);
    }
    if (!hasExpectedDpr || !hasBaselineSubdirectory) {
        *error = QStringLiteral(
            "必须同时提供 --expected-dpr 和 --baseline-subdir");
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] ZzImageComparison zzCompareImages(
    const QImage &expected,
    const QImage &actual)
{
    ZzImageComparison result;
    result.difference = QImage(actual.size(), QImage::Format_ARGB32_Premultiplied);
    result.difference.fill(Qt::transparent);
    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < actual.width(); ++x) {
            ++result.comparedPixels;
            const QColor expectedColor = expected.pixelColor(x, y);
            const QColor actualColor = actual.pixelColor(x, y);
            const bool differs = std::abs(expectedColor.red() - actualColor.red())
                    > zzChannelTolerance
                || std::abs(expectedColor.green() - actualColor.green())
                    > zzChannelTolerance
                || std::abs(expectedColor.blue() - actualColor.blue())
                    > zzChannelTolerance
                || std::abs(expectedColor.alpha() - actualColor.alpha())
                    > zzChannelTolerance;
            if (differs) {
                ++result.differentPixels;
                result.difference.setPixelColor(x, y, Qt::red);
            }
        }
    }
    return result;
}

class ZzWorkspaceScreenshotSurface final
{
public:
    explicit ZzWorkspaceScreenshotSurface(
        QSize surfaceSize = zzLogicalSurfaceSize,
        QString titleBarTitle = {},
        bool denseWorkbench = false,
        bool collapseBottom = false,
        bool twoGroupWorkbench = false,
        bool forceInvalidPanelSetup = false)
        : titleBar(&window)
    {
        window.setObjectName(QStringLiteral("zzWorkspaceScreenshotSurface"));
        window.setWindowTitle(QStringLiteral("Workspace"));
        window.setFixedSize(surfaceSize);
        titleBar.setTitle(std::move(titleBarTitle));
        auto *fileMenu = titleBar.menuBar()->addMenu(QStringLiteral("File"));
        fileMenu->addAction(QStringLiteral("Open"));
        titleBar.menuBar()->addMenu(QStringLiteral("View"));
        window.setMenuWidget(&titleBar);
        auto result = ZzPureTools::ZzWorkspaceShell::create(&window, &titleBar);
        if (!requireResult(result, QStringLiteral("failed to create workspace shell"))) {
            return;
        }
        shell = std::move(result).value();
        window.setCentralWidget(shell->workspaceWidget());
        auto explorer = std::make_unique<ZzFluentUI::ZzExplorerPane>();
        explorerModel.appendRow(new QStandardItem(QStringLiteral("src")));
        explorerModel.appendRow(new QStandardItem(QStringLiteral("README.md")));
        explorer->setModel(&explorerModel);
        const auto sideResult = shell->registerSidePanel(
            ZzPureTools::ZzWorkspacePanelId(forceInvalidPanelSetup
                    ? QString() : QStringLiteral("explorer")),
            QStringLiteral("Explorer"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, explorer.get());
        if (!requireResult(sideResult,
                QStringLiteral("failed to register explorer side panel"))) {
            return;
        }
        static_cast<void>(explorer.release());
        auto dockContent = std::make_unique<QWidget>();
        const auto dockResult = shell->registerDockPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("terminal")),
            QStringLiteral("Terminal"), {}, Qt::BottomDockWidgetArea,
            dockContent.get());
        if (!requireResult(dockResult,
                QStringLiteral("failed to register terminal dock panel"))) {
            return;
        }
        static_cast<void>(dockContent.release());
        shell->tabWidget()->addTab(new QWidget, QStringLiteral("main.cpp"));
        shell->tabWidget()->addTab(new QWidget, QStringLiteral("Preview"));
        commandModel.appendRow(new QStandardItem(QStringLiteral("Build workspace")));
        shell->commandPalette()->setModel(&commandModel);
        if (denseWorkbench) {
            static_cast<void>(configureDenseWorkbench(
                collapseBottom, twoGroupWorkbench));
        }
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return setupError_.isEmpty();
    }

    [[nodiscard]] const QString &setupError() const noexcept
    {
        return setupError_;
    }

    void polish()
    {
        window.show();
        QCoreApplication::processEvents();
    }

    void hide()
    {
        shell.reset();
        window.hide();
    }

    QMainWindow window;
    ZzFluentUI::ZzFluentTitleBar titleBar;
    std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
    QStandardItemModel explorerModel;
    QStandardItemModel commandModel;

private:
    template<typename ZzValue>
    [[nodiscard]] bool requireResult(
        const ZzValue &result,
        QString failure)
    {
        if (result) {
            return true;
        }
        setupError_ = std::move(failure);
        const QString technicalMessage = result.error().technicalMessage();
        if (!technicalMessage.isEmpty()) {
            setupError_ += QStringLiteral(": ") + technicalMessage;
        }
        return false;
    }

    [[nodiscard]] bool requireCondition(bool condition, QString failure)
    {
        if (condition) {
            return true;
        }
        setupError_ = std::move(failure);
        return false;
    }

    [[nodiscard]] bool configureDenseWorkbench(
        bool collapseBottom,
        bool twoGroupWorkbench)
    {
        auto registerSide = [this](const char *id, const QString &title,
                                ZzFluentUI::ZzActivityArea area) {
            auto panel = std::make_unique<QLabel>(title);
            panel->setAlignment(Qt::AlignCenter);
            const auto result = shell->registerSidePanel(
                ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(id)),
                title, {}, area, panel.get());
            if (!requireResult(result,
                    QStringLiteral("failed to register %1 side panel")
                        .arg(title))) {
                return false;
            }
            static_cast<void>(panel.release());
            return true;
        };
        if (!registerSide("search", QStringLiteral("Search"),
                ZzFluentUI::ZzActivityArea::LeftSecondary)
            || !registerSide("properties", QStringLiteral("Properties"),
                ZzFluentUI::ZzActivityArea::RightPrimary)
            || !registerSide("tasks", QStringLiteral("Tasks"),
                ZzFluentUI::ZzActivityArea::RightSecondary)) {
            return false;
        }

        auto terminal = std::make_unique<QPlainTextEdit>();
        terminal->setPlainText(QStringLiteral("$ build\nBuild completed\n$ _"));
        terminal->setReadOnly(true);
        const auto terminalResult = shell->registerBottomPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("bottom-terminal")),
            QStringLiteral("Terminal"), {}, terminal.get());
        if (!requireResult(terminalResult,
                QStringLiteral("failed to register terminal bottom panel"))) {
            return false;
        }
        static_cast<void>(terminal.release());
        auto problems = std::make_unique<QLabel>(
            QStringLiteral("0 errors   1 warning"));
        problems->setAlignment(Qt::AlignCenter);
        const auto problemsResult = shell->registerBottomPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("bottom-problems")),
            QStringLiteral("Problems"), {}, problems.get());
        if (!requireResult(problemsResult,
                QStringLiteral("failed to register problems bottom panel"))) {
            return false;
        }
        static_cast<void>(problems.release());
        auto commands = std::make_unique<QWidget>();
        auto *commandLayout = new QVBoxLayout(commands.get());
        commandLayout->setContentsMargins(6, 4, 6, 4);
        const auto addPrimary = [this](ZzFluentUI::ZzCommandBar *bar,
                                    QStyle::StandardPixmap icon,
                                    const QString &text) {
            return requireCondition(bar->addPrimaryAction(
                bar->style()->standardIcon(icon), text) != nullptr,
                QStringLiteral("failed to add %1 primary action").arg(text));
        };
        const auto addSecondary = [this](ZzFluentUI::ZzCommandBar *bar,
                                      QStyle::StandardPixmap icon,
                                      const QString &text) {
            return requireCondition(bar->addSecondaryAction(
                bar->style()->standardIcon(icon), text) != nullptr,
                QStringLiteral("failed to add %1 secondary action").arg(text));
        };
        auto *expanded = new ZzFluentUI::ZzCommandBar(commands.get());
        expanded->setObjectName(QStringLiteral("zzDenseExpandedCommandBar"));
        expanded->setFixedWidth(300);
        expanded->setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Expanded);
        if (!addPrimary(expanded, QStyle::SP_ComputerIcon,
                QStringLiteral("Build"))
            || !addPrimary(expanded, QStyle::SP_DialogApplyButton,
                QStringLiteral("Test"))
            || !addSecondary(expanded, QStyle::SP_FileDialogDetailedView,
                QStringLiteral("Artifacts"))) {
            return false;
        }
        commandLayout->addWidget(expanded, 0, Qt::AlignLeft);

        auto *compact = new ZzFluentUI::ZzCommandBar(commands.get());
        compact->setObjectName(QStringLiteral("zzDenseCompactCommandBar"));
        compact->setFixedWidth(180);
        compact->setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Compact);
        if (!addPrimary(compact, QStyle::SP_MediaPlay, QStringLiteral("Run"))
            || !addPrimary(compact, QStyle::SP_BrowserReload,
                QStringLiteral("Restart"))
            || !addPrimary(compact, QStyle::SP_MediaStop,
                QStringLiteral("Stop"))) {
            return false;
        }
        commandLayout->addWidget(compact, 0, Qt::AlignLeft);

        auto *automatic = new ZzFluentUI::ZzCommandBar(commands.get());
        automatic->setObjectName(QStringLiteral("zzDenseAutoCommandBar"));
        automatic->setFixedWidth(230);
        automatic->setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Auto);
        if (!addPrimary(automatic, QStyle::SP_ArrowForward,
                QStringLiteral("Publish"))
            || !addPrimary(automatic, QStyle::SP_DialogSaveButton,
                QStringLiteral("Snapshot"))
            || !addSecondary(automatic, QStyle::SP_FileDialogContentsView,
                QStringLiteral("Logs"))
            || !addSecondary(automatic, QStyle::SP_FileDialogInfoView,
                QStringLiteral("Details"))) {
            return false;
        }
        commandLayout->addWidget(automatic, 0, Qt::AlignLeft);
        const auto commandsResult = shell->registerBottomPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("bottom-commands")),
            QStringLiteral("Commands"), {}, commands.get());
        if (!requireResult(commandsResult,
                QStringLiteral("failed to register commands bottom panel"))) {
            return false;
        }
        static_cast<void>(commands.release());
        const auto showCommandsResult = shell->showPanel(
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("bottom-commands")));
        if (!requireResult(showCommandsResult,
                QStringLiteral("failed to show commands bottom panel"))) {
            return false;
        }
        shell->bottomPane()->setPaneHeight(210);
        shell->bottomPane()->setCollapsed(collapseBottom);

        auto *editor = new QPlainTextEdit;
        QStringList editorLines;
        editorLines.reserve(96);
        for (int line = 1; line <= 96; ++line) {
            editorLines.append(QStringLiteral(
                "%1  const int value%2 = %2;").arg(
                line, 3, 10, QLatin1Char('0')).arg(
                line, 3, 10, QLatin1Char('0')));
        }
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setPlainText(editorLines.join(u'\n'));
        auto *markers = new QStandardItemModel(editor);
        for (const auto &[position, kind] : std::array<std::pair<qreal,
                 ZzFluentUI::ZzScrollMarkerKind>, 3>{{
                 {0.2, ZzFluentUI::ZzScrollMarkerKind::Information},
                 {0.5, ZzFluentUI::ZzScrollMarkerKind::Warning},
                 {0.8, ZzFluentUI::ZzScrollMarkerKind::Error}}}) {
            auto *item = new QStandardItem;
            item->setData(position,
                static_cast<int>(ZzFluentUI::ZzScrollMarkerRole::Position));
            item->setData(static_cast<int>(kind),
                static_cast<int>(ZzFluentUI::ZzScrollMarkerRole::Kind));
            markers->appendRow(item);
        }
        auto *scroll = new ZzFluentUI::ZzAnnotatedScrollBar(editor);
        scroll->setMarkerModel(markers);
        editor->setVerticalScrollBar(scroll);
        shell->tabWidget()->addTab(editor, QStringLiteral("main.cpp"));
        shell->tabWidget()->addTab(new QLabel(QStringLiteral("Preview")),
            QStringLiteral("Preview"));
        shell->tabWidget()->addTab(new QLabel(QStringLiteral("Trace")),
            QStringLiteral("Trace"));
        const auto root = shell->splitWorkspace()->groupIds().constFirst();
        if (twoGroupWorkbench) {
            return requireCondition(
                shell->splitWorkspace()->moveTabToDropZone(
                    root, 0, root, ZzFluentUI::ZzWorkspaceDropZone::Top),
                QStringLiteral("failed to create top workspace group"))
                && requireCondition(
                    shell->splitWorkspace()->groupIds().size() == 2,
                    QStringLiteral("two-group workspace has unexpected group count"));
        }
        return requireCondition(shell->splitWorkspace()->moveTabToDropZone(
                root, 0, root, ZzFluentUI::ZzWorkspaceDropZone::Right),
                QStringLiteral("failed to create right workspace group"))
            && requireCondition(shell->splitWorkspace()->moveTabToDropZone(
                root, 0, root, ZzFluentUI::ZzWorkspaceDropZone::Bottom),
                QStringLiteral("failed to create bottom workspace group"))
            && requireCondition(shell->splitWorkspace()->moveTabToDropZone(
                root, 0, root, ZzFluentUI::ZzWorkspaceDropZone::Left),
                QStringLiteral("failed to create left workspace group"))
            && requireCondition(
                shell->splitWorkspace()->groupIds().size() == 4,
                QStringLiteral("dense workspace has unexpected group count"));
    }

    QString setupError_;
};

[[nodiscard]] QImage zzRenderWorkspaceSurface(
    ZzWorkspaceScreenshotSurface *surface,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(surface->window.width() * dpr),
        qRound(surface->window.height() * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface->window.render(&painter);
    return image;
}

[[nodiscard]] QRect zzMapToSurface(
    const QWidget *widget,
    const QRect &rect,
    QWidget *surface)
{
    return QRect(widget->mapTo(surface, rect.topLeft()), rect.size());
}

void zzVerifyNarrowWorkspaceGeometry(ZzWorkspaceScreenshotSurface &surface)
{
    const auto *title = surface.titleBar.findChild<QLabel *>(
        QStringLiteral("zzTitleBarTitle"));
    const auto *compactMenu = surface.titleBar.findChild<QToolButton *>(
        QStringLiteral("zzTitleBarCompactMenuButton"));
    const auto *theme = surface.titleBar.findChild<QToolButton *>(
        QStringLiteral("zzTitleBarThemeButton"));
    const auto *alwaysOnTop = surface.titleBar.findChild<QToolButton *>(
        QStringLiteral("zzTitleBarAlwaysOnTopButton"));
    QVERIFY(title != nullptr);
    QVERIFY(compactMenu != nullptr);
    QVERIFY(theme != nullptr);
    QVERIFY(alwaysOnTop != nullptr);
    if (title == nullptr || compactMenu == nullptr || theme == nullptr
        || alwaysOnTop == nullptr) {
        return;
    }
    QVERIFY(compactMenu->isVisible());
    QVERIFY(!surface.titleBar.menuBar()->isVisible());
    const std::array<const QWidget *, 8> titleBarWidgets{
        compactMenu, title, theme, alwaysOnTop,
        surface.titleBar.minimizeButton(), surface.titleBar.maximizeButton(),
        surface.titleBar.closeButton(), surface.titleBar.windowIconWidget()};
    for (const QWidget *widget : titleBarWidgets) {
        QVERIFY(widget != nullptr);
        if (widget == nullptr) {
            return;
        }
        QVERIFY(widget->isVisible());
        QVERIFY(!widget->geometry().isEmpty());
    }
    for (std::size_t first = 0; first < titleBarWidgets.size(); ++first) {
        for (std::size_t second = first + 1;
             second < titleBarWidgets.size(); ++second) {
            QVERIFY2(!titleBarWidgets.at(first)->geometry().intersects(
                          titleBarWidgets.at(second)->geometry()),
                "narrow title bar widgets overlap");
        }
    }
    auto *activity = surface.shell->activityBar(ZzFluentUI::ZzSidePaneEdge::Left);
    auto *tabs = surface.shell->tabWidget();
    auto *dock = surface.window.findChild<ZzFluentUI::ZzDockPanel *>();
    QVERIFY(activity != nullptr);
    QVERIFY(tabs != nullptr);
    QVERIFY(dock != nullptr);
    if (activity == nullptr || tabs == nullptr || dock == nullptr) {
        return;
    }
    QVERIFY(activity->isVisible());
    QVERIFY(tabs->isVisible());
    QVERIFY(dock->isVisible());
    const QRect activityRect = zzMapToSurface(activity, activity->rect(), &surface.window);
    const QRect tabRect = zzMapToSurface(tabs, tabs->rect(), &surface.window);
    const QRect dockRect = zzMapToSurface(dock, dock->rect(), &surface.window);
    QVERIFY(!activityRect.isEmpty());
    QVERIFY(!tabRect.isEmpty());
    QVERIFY(!dockRect.isEmpty());
    QVERIFY(!activityRect.intersects(tabRect));
    QVERIFY(!activityRect.intersects(dockRect));
    QVERIFY(!tabRect.intersects(dockRect));
}

} // namespace

class ZzWorkspaceScreenshotTest final : public QObject
{
    Q_OBJECT

public:
    ZzWorkspaceScreenshotTest(qreal expectedDpr, QString baselineSubdirectory)
        : expectedDpr_(expectedDpr)
        , baselineSubdirectory_(std::move(baselineSubdirectory))
    {
    }

private Q_SLOTS:
    // QApplication 接管 setStyle() 传入对象；静态分析器不了解 Qt 所有权。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void initTestCase()
    {
        QLocale::setDefault(QLocale::c());
        QApplication::setLayoutDirection(Qt::LeftToRight);
        QVERIFY(QFontDatabase::families().contains(QStringLiteral("DejaVu Sans")));
        QApplication::setFont(QFont(QStringLiteral("DejaVu Sans"), 10));
        QCOMPARE(QFontInfo(QApplication::font()).family(), QStringLiteral("DejaVu Sans"));
        QScreen *screen = QApplication::primaryScreen();
        QVERIFY(screen != nullptr);
        actualDpr_ = screen->devicePixelRatio();
        QVERIFY(std::abs(actualDpr_ - expectedDpr_) <= 0.01);
        QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"));
        QVERIFY(fusion != nullptr);
        controller_ = std::make_unique<ZzFluentUI::ZzThemeController>();
        controller_->setReducedMotion(true);
        QApplication::setStyle(new ZzFluentUI::ZzFluentStyle(controller_.get(), fusion));
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void rendersWorkspaceThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("workspace-light") << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
                                           << QStringLiteral("workspace-light");
        QTest::newRow("workspace-dark") << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
                                          << QStringLiteral("workspace-dark");
        QTest::newRow("workspace-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("workspace-high-contrast");
    }

    void rendersWorkspaceThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));
        ZzWorkspaceScreenshotSurface surface;
        QVERIFY2(surface.isValid(), qPrintable(surface.setupError()));
        surface.polish();
        const QImage actual = zzRenderWorkspaceSurface(&surface, actualDpr_);
        QCOMPARE(actual.size(), QSize(qRound(zzLogicalSurfaceSize.width() * expectedDpr_),
                                      qRound(zzLogicalSurfaceSize.height() * expectedDpr_)));
        surface.hide();
        verifyScreenshot(fileStem, actual);
    }

    void rendersNarrowWorkspaceThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("workspace-narrow-light") << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
                                                  << QStringLiteral("workspace-narrow-light");
        QTest::newRow("workspace-narrow-dark") << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
                                                 << QStringLiteral("workspace-narrow-dark");
        QTest::newRow("workspace-narrow-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("workspace-narrow-high-contrast");
    }

    void rendersNarrowWorkspaceThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));
        ZzWorkspaceScreenshotSurface surface(zzNarrowWorkspaceSurfaceSize,
            QStringLiteral("Narrow workspace title"));
        QVERIFY2(surface.isValid(), qPrintable(surface.setupError()));
        surface.polish();
        zzVerifyNarrowWorkspaceGeometry(surface);
        const QImage actual = zzRenderWorkspaceSurface(&surface, actualDpr_);
        QCOMPARE(actual.size(), QSize(qRound(zzNarrowWorkspaceSurfaceSize.width() * expectedDpr_),
                                      qRound(zzNarrowWorkspaceSurfaceSize.height() * expectedDpr_)));
        surface.hide();
        verifyScreenshot(fileStem, actual);
    }

    void rendersDenseWorkbenchThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::addColumn<bool>("collapseBottom");
        QTest::newRow("workspace-dense-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("workspace-dense-light") << false;
        QTest::newRow("workspace-dense-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("workspace-dense-dark") << false;
        QTest::newRow("workspace-dense-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("workspace-dense-high-contrast") << false;
        QTest::newRow("workspace-dense-collapsed-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("workspace-dense-collapsed-light") << true;
        QTest::newRow("workspace-dense-collapsed-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("workspace-dense-collapsed-dark") << true;
        QTest::newRow("workspace-dense-collapsed-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("workspace-dense-collapsed-high-contrast") << true;
    }

    void rendersDenseWorkbenchThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        QFETCH(bool, collapseBottom);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));
        ZzWorkspaceScreenshotSurface surface(
            zzLogicalSurfaceSize, QStringLiteral("Dense workspace"), true,
            collapseBottom);
        QVERIFY2(surface.isValid(), qPrintable(surface.setupError()));
        surface.polish();
        const QImage actual = zzRenderWorkspaceSurface(&surface, actualDpr_);
        surface.hide();
        verifyScreenshot(fileStem, actual);
    }

    void rendersTwoGroupWorkbenchThemes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("fileStem");
        QTest::newRow("workspace-two-group-light")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Light)
            << QStringLiteral("workspace-two-group-light");
        QTest::newRow("workspace-two-group-dark")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)
            << QStringLiteral("workspace-two-group-dark");
        QTest::newRow("workspace-two-group-high-contrast")
            << static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
            << QStringLiteral("workspace-two-group-high-contrast");
    }

    void rendersTwoGroupWorkbenchThemes()
    {
        QFETCH(int, mode);
        QFETCH(QString, fileStem);
        controller_->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));
        ZzWorkspaceScreenshotSurface surface(
            zzLogicalSurfaceSize, QStringLiteral("Two group workspace"), true,
            false, true);
        QVERIFY2(surface.isValid(), qPrintable(surface.setupError()));
        surface.polish();
        const QImage actual = zzRenderWorkspaceSurface(&surface, actualDpr_);
        surface.hide();
        verifyScreenshot(fileStem, actual);
    }

    void reportsWorkspaceSetupFailure()
    {
        ZzWorkspaceScreenshotSurface surface(
            zzLogicalSurfaceSize, {}, false, false, false, true);

        QVERIFY(!surface.isValid());
        QCOMPARE(surface.setupError(), QStringLiteral(
            "failed to register explorer side panel: "
            "Invalid side panel registration"));
    }

    void cleanupTestCase()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        controller_.reset();
    }

private:
    void verifyScreenshot(const QString &fileStem, const QImage &actual)
    {
        const QString baselineDirectory = QDir(
            QStringLiteral(ZZ_PURETOOLS_WORKSPACE_SCREENSHOT_BASELINE_DIR))
                                              .filePath(baselineSubdirectory_);
        const QString baselinePath = QDir(baselineDirectory).filePath(
            fileStem + QStringLiteral(".png"));
        if (qEnvironmentVariableIntValue("ZZ_UPDATE_SCREENSHOTS") == 1) {
            QVERIFY(QDir().mkpath(baselineDirectory));
            QVERIFY(actual.save(baselinePath, "PNG"));
            return;
        }
        const QImage expected(baselinePath);
        QVERIFY2(!expected.isNull(), qPrintable(QStringLiteral("缺少 baseline：%1").arg(baselinePath)));
        QCOMPARE(expected.size(), actual.size());
        const ZzImageComparison comparison = zzCompareImages(expected, actual);
        QVERIFY(comparison.comparedPixels > 0);
        const qreal ratio = static_cast<qreal>(comparison.differentPixels)
            / static_cast<qreal>(comparison.comparedPixels);
        if (ratio <= zzMaximumDifferenceRatio()) {
            return;
        }
        const QString reportDirectory = QDir(
            QStringLiteral(ZZ_PURETOOLS_WORKSPACE_SCREENSHOT_REPORT_DIR))
                                              .filePath(baselineSubdirectory_);
        QVERIFY(QDir().mkpath(reportDirectory));
        const QString actualPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-actual.png"));
        const QString diffPath = QDir(reportDirectory).filePath(
            fileStem + QStringLiteral("-diff.png"));
        QVERIFY(actual.save(actualPath, "PNG"));
        QVERIFY(comparison.difference.save(diffPath, "PNG"));
        QFAIL(qPrintable(QStringLiteral("workspace screenshot differs: ratio=%1 actual=%2 diff=%3")
            .arg(ratio, 0, 'f', 6).arg(actualPath, diffPath)));
    }

    qreal expectedDpr_ = 1.0;
    QString baselineSubdirectory_;
    qreal actualDpr_ = 1.0;
    std::unique_ptr<ZzFluentUI::ZzThemeController> controller_;
};

namespace {

int zzRunWorkspaceScreenshotTest(int argc, char *argv[])
{
    QString parseError;
    const auto arguments = zzParseArguments(argc, argv, &parseError);
    if (!arguments) {
        std::fprintf(stderr, "%s\n", qPrintable(parseError));
        return EXIT_FAILURE;
    }
    std::vector<char *> filteredPointers;
    filteredPointers.reserve(arguments->filteredArguments.size());
    for (const QByteArray &argument : arguments->filteredArguments) {
        filteredPointers.push_back(const_cast<char *>(argument.constData()));
    }
    int filteredArgc = static_cast<int>(filteredPointers.size());
    QApplication application(filteredArgc, filteredPointers.data());
    ZzWorkspaceScreenshotTest test(
        arguments->expectedDpr, arguments->baselineSubdirectory);
    return QTest::qExec(&test, filteredArgc, filteredPointers.data());
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        return zzRunWorkspaceScreenshotTest(argc, argv);
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "%s\n", exception.what());
        return EXIT_FAILURE;
    }
}

#include "ZzWorkspaceScreenshotTest.moc"
