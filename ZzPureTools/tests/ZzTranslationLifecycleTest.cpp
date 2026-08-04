#include <cstring>
#include <memory>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTranslator>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentTitleBar.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#ifndef ZZ_TRANSLATION_TEST_QM
#error "ZZ_TRANSLATION_TEST_QM must name the generated translation file"
#endif

namespace {

constexpr auto ZzTranslationContext = "ZzTranslationLifecycleTest";
constexpr auto ZzOwnedMarker = "Owned marker";
constexpr auto ZzExternalMarker = "External marker";

/** @brief 返回当前进程唯一的 PureTools 应用。 */
[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

/** @brief 只翻译测试外部标记，用于验证 Builder 不卸载外部资源。 */
class ZzExternalTranslator final : public QTranslator
{
public:
    /** @brief 返回外部标记翻译，其他键继续交给更早的 translator。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (context != nullptr && sourceText != nullptr
            && std::strcmp(context, ZzTranslationContext) == 0
            && std::strcmp(sourceText, ZzExternalMarker) == 0) {
            return QStringLiteral("External translated");
        }
        return {};
    }
};

/** @brief 观察应用语言事件并维护与展示层无关的翻译文本状态。 */
class ZzTranslationViewModel final : public QObject
{
    Q_OBJECT

public:
    /** @brief 注册全局语言事件观察并建立初始文本。 */
    ZzTranslationViewModel()
    {
        if (QCoreApplication::instance() != nullptr) {
            QCoreApplication::instance()->installEventFilter(this);
        }
        refreshTranslation();
    }

    /** @brief 取消全局语言事件观察。 */
    ~ZzTranslationViewModel() override
    {
        if (QCoreApplication::instance() != nullptr) {
            QCoreApplication::instance()->removeEventFilter(this);
        }
    }

    /** @brief 返回当前拥有值的动态文本。 */
    [[nodiscard]] QString text() const
    {
        return text_;
    }

Q_SIGNALS:
    /** @brief 动态文本实际变化后通知 Presenter。 */
    void textChanged(const QString &text);

protected:
    /** @brief 收到语言变化时刷新状态，不访问任何 QWidget。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event != nullptr && event->type() == QEvent::LanguageChange) {
            refreshTranslation();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    /** @brief 使用当前 translator 计算文本并按变化发信号。 */
    void refreshTranslation()
    {
        const QString translated = QCoreApplication::translate(
            ZzTranslationContext, ZzOwnedMarker);
        if (text_ == translated) {
            return;
        }
        text_ = translated;
        Q_EMIT textChanged(text_);
    }

    QString text_;
};

/** @brief 单向连接翻译 ViewModel 与页面 QLabel。 */
class ZzTranslationPresenter final : public QObject
{
public:
    /**
     * @brief 建立单向展示绑定。
     * @param viewModel 非空且由页面实例独占的状态观察值。
     * @param label 非空且由页面 View 子树拥有的展示控件。
     */
    ZzTranslationPresenter(
        ZzTranslationViewModel *viewModel,
        QLabel *label)
        : label_(label)
    {
        Q_ASSERT(viewModel != nullptr);
        Q_ASSERT(label != nullptr);
        label_->setText(viewModel->text());
        QObject::connect(
            viewModel,
            &ZzTranslationViewModel::textChanged,
            this,
            [this](const QString &text) {
                if (label_) {
                    label_->setText(text);
                }
            });
    }

private:
    QPointer<QLabel> label_;
};

/** @brief 在启动阶段观察 Builder translator 后返回预设失败。 */
class ZzTranslationFailingModule final
    : public ZzPureTools::ZzApplicationModule
{
public:
    /** @brief 保存用于报告启动期翻译可见性的观察值。 */
    explicit ZzTranslationFailingModule(bool *sawOwnedTranslation)
        : sawOwnedTranslation_(sawOwnedTranslation)
    {
    }

    /** @brief 返回稳定且无依赖的测试模块描述。 */
    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override
    {
        return {
            ZzPureTools::ZzModuleId(
                QStringLiteral("translation-failure")),
            QStringLiteral("1.0.0"),
            {}};
    }

    /** @brief 记录 translator 已安装后返回 Backend 失败。 */
    [[nodiscard]] ZzCore::ZzResult<void> start() override
    {
        if (sawOwnedTranslation_ != nullptr) {
            *sawOwnedTranslation_ =
                QCoreApplication::translate(
                    ZzTranslationContext, ZzOwnedMarker)
                == QStringLiteral("Owned translated");
        }
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("translation test module start failed")));
    }

    /** @brief 启动失败模块没有已启动工作需要请求停止。 */
    void requestStop() noexcept override
    {
    }

    /** @brief 启动失败模块没有已启动资源需要停止。 */
    void stop() noexcept override
    {
    }

private:
    bool *sawOwnedTranslation_;
};

/** @brief 创建动态文本由 ViewModel 驱动的测试页面。 */
[[nodiscard]] ZzPureTools::ZzPageRegistration zzPage()
{
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId =
        ZzPureTools::ZzRouteId(QStringLiteral("home"));
    registration.lifetime =
        ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    registration.factory =
        [](QWidget *pageParent)
        -> ZzCore::ZzResult<std::unique_ptr<
            ZzPureTools::ZzPageInstance>> {
            auto *view = new QWidget(pageParent);
            auto *layout = new QVBoxLayout(view);
            auto *label = new QLabel(view);
            label->setObjectName(QStringLiteral("zzDynamicTranslationLabel"));
            layout->addWidget(label);

            auto viewModel = std::make_unique<ZzTranslationViewModel>();
            auto presenter = std::make_unique<ZzTranslationPresenter>(
                viewModel.get(), label);
            return ZzPureTools::ZzPageInstance::create(
                pageParent,
                view,
                std::move(viewModel),
                std::move(presenter));
        };
    return registration;
}

/** @brief 注册单页、单节点和首路由。 */
[[nodiscard]] bool zzConfigure(
    ZzPureTools::ZzApplicationBuilder &builder)
{
    if (!builder.addPage(zzPage())) {
        return false;
    }
    if (!builder.addNavigationNode({
            ZzPureTools::ZzRouteId(QStringLiteral("home")),
            QStringLiteral("ZzTranslationLifecycleTest"),
            QStringLiteral("Owned marker"),
            {}})) {
        return false;
    }
    const auto initialRouteResult = builder.setInitialRoute(
        ZzPureTools::ZzRouteId(QStringLiteral("home")));
    return static_cast<bool>(initialRouteResult);
}

/** @brief 查找当前唯一的应用窗口；数量不为一时返回空。 */
[[nodiscard]] ZzPureTools::ZzApplicationWindow *zzOnlyWindow(
    ZzPureTools::ZzPureApplication &application)
{
    ZzPureTools::ZzApplicationWindow *result = nullptr;
    for (QWidget *widget : application.topLevelWidgets()) {
        auto *window = qobject_cast<
            ZzPureTools::ZzApplicationWindow *>(widget);
        if (window == nullptr) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = window;
    }
    return result;
}

} // namespace

/** @brief 验证 translator 的窗口刷新、失败回滚和外部资源边界。 */
class ZzTranslationLifecycleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void languageChangeRefreshesStaticAndDynamicText()
    {
        auto &application = zzApplication();
        QCOMPARE(
            QCoreApplication::translate(
                ZzTranslationContext, ZzOwnedMarker),
            QStringLiteral("Owned marker"));

        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(zzConfigure(builder));
        QVERIFY(builder.build(application));
        auto *first = zzOnlyWindow(application);
        QVERIFY(first != nullptr);
        auto secondResult = application.createWindow();
        QVERIFY(secondResult);
        auto *second = std::move(secondResult).value();
        QVERIFY(second != nullptr);

        auto *firstLabel = first->findChild<QLabel *>(
            QStringLiteral("zzDynamicTranslationLabel"));
        auto *secondLabel = second->findChild<QLabel *>(
            QStringLiteral("zzDynamicTranslationLabel"));
        auto *firstTitleBar =
            first->findChild<ZzFluentUI::ZzFluentTitleBar *>();
        auto *secondTitleBar =
            second->findChild<ZzFluentUI::ZzFluentTitleBar *>();
        QVERIFY(firstLabel != nullptr);
        QVERIFY(secondLabel != nullptr);
        QVERIFY(firstTitleBar != nullptr);
        QVERIFY(secondTitleBar != nullptr);
        QCOMPARE(firstLabel->text(), QStringLiteral("Owned marker"));
        QCOMPARE(secondLabel->text(), QStringLiteral("Owned marker"));
        QCOMPARE(
            first->navigationModel()->data(
                first->navigationModel()->index(0, 0), Qt::DisplayRole),
            QVariant(QStringLiteral("Owned marker")));

        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(ZZ_TRANSLATION_TEST_QM)));
        QVERIFY(application.installTranslator(&translator));

        QTRY_COMPARE(
            first->windowTitle(),
            QStringLiteral("ZzPureTools translated"));
        QCOMPARE(
            second->windowTitle(),
            QStringLiteral("ZzPureTools translated"));
        QCOMPARE(firstTitleBar->title(), first->windowTitle());
        QCOMPARE(secondTitleBar->title(), second->windowTitle());
        QCOMPARE(firstLabel->text(), QStringLiteral("Owned translated"));
        QCOMPARE(secondLabel->text(), QStringLiteral("Owned translated"));
        QCOMPARE(
            first->navigationModel()->data(
                first->navigationModel()->index(0, 0), Qt::DisplayRole),
            QVariant(QStringLiteral("Owned translated")));
        QCOMPARE(
            second->navigationModel()->data(
                second->navigationModel()->index(0, 0), Qt::DisplayRole),
            QVariant(QStringLiteral("Owned translated")));

        QVERIFY(application.removeTranslator(&translator));
        QTRY_COMPARE(firstLabel->text(), QStringLiteral("Owned marker"));
        QCOMPARE(secondLabel->text(), QStringLiteral("Owned marker"));
        QTRY_COMPARE(first->windowTitle(), QStringLiteral("ZzPureTools"));
        QCOMPARE(second->windowTitle(), QStringLiteral("ZzPureTools"));
        QCOMPARE(firstTitleBar->title(), first->windowTitle());
        QCOMPARE(secondTitleBar->title(), second->windowTitle());
        QCOMPARE(
            first->navigationModel()->data(
                first->navigationModel()->index(0, 0), Qt::DisplayRole),
            QVariant(QStringLiteral("Owned marker")));
        QCOMPARE(
            second->navigationModel()->data(
                second->navigationModel()->index(0, 0), Qt::DisplayRole),
            QVariant(QStringLiteral("Owned marker")));
        application.beginShutdown();
    }

    void failedTranslatorLoadLeavesApplicationUnbuilt()
    {
        auto &application = zzApplication();
        ZzExternalTranslator externalTranslator;
        QVERIFY(application.installTranslator(&externalTranslator));
        QCOMPARE(
            QCoreApplication::translate(
                ZzTranslationContext, ZzExternalMarker),
            QStringLiteral("External translated"));

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(zzConfigure(builder));
        QVERIFY(builder.addTranslatorResource(
            temporaryDirectory.filePath(QStringLiteral("missing.qm"))));

        const auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::NotFound);
        QCOMPARE(application.windowCount(), 0);
        QVERIFY(!application.createWindow());
        QCOMPARE(
            QCoreApplication::translate(
                ZzTranslationContext, ZzExternalMarker),
            QStringLiteral("External translated"));
        QVERIFY(application.removeTranslator(&externalTranslator));
        application.beginShutdown();
    }

    void runtimeFailureRollsBackInstalledTranslators()
    {
        auto &application = zzApplication();
        ZzExternalTranslator externalTranslator;
        QVERIFY(application.installTranslator(&externalTranslator));
        bool sawOwnedTranslation = false;

        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addModule(
            std::make_unique<ZzTranslationFailingModule>(
                &sawOwnedTranslation)));
        QVERIFY(zzConfigure(builder));
        QVERIFY(builder.addTranslatorResource(
            QStringLiteral(ZZ_TRANSLATION_TEST_QM)));

        const auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Backend);
        QVERIFY(sawOwnedTranslation);
        QCOMPARE(application.windowCount(), 0);
        QCOMPARE(
            QCoreApplication::translate(
                ZzTranslationContext, ZzOwnedMarker),
            QStringLiteral("Owned marker"));
        QCOMPARE(
            QCoreApplication::translate(
                ZzTranslationContext, ZzExternalMarker),
            QStringLiteral("External translated"));
        QVERIFY(application.removeTranslator(&externalTranslator));
        application.beginShutdown();
    }
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return 1;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzTranslationLifecycleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzTranslationLifecycleTest.moc"
