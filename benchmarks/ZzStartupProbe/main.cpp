#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

namespace {

/** @brief 保存从进程入口起算且严格递增的启动时间标记。 */
class ZzStartupMarkers final
{
public:
    /** @brief 创建标记集合并固定写入 process-entry=0。 */
    explicit ZzStartupMarkers(const QElapsedTimer *timer)
        : timer_(timer)
    {
        markers_.insert(QStringLiteral("process-entry"), 0);
    }

    /** @brief 记录一个至少比上一标记大一纳秒的新阶段。 */
    void record(const QString &name)
    {
        const qint64 elapsed = timer_ != nullptr
            ? timer_->nsecsElapsed() : 0;
        lastNanoseconds_ = std::max(elapsed, lastNanoseconds_ + 1);
        markers_.insert(name, QJsonValue(lastNanoseconds_));
    }

    /** @brief 返回可直接序列化的标记对象副本。 */
    [[nodiscard]] QJsonObject json() const
    {
        return markers_;
    }

private:
    const QElapsedTimer *timer_ = nullptr;
    QJsonObject markers_;
    qint64 lastNanoseconds_ = 0;
};

/** @brief 在真实模块 start() 点写入启动标记的最小应用模块。 */
class ZzStartupModule final : public ZzPureTools::ZzApplicationModule
{
public:
    /** @brief 保存非拥有标记观察指针。 */
    explicit ZzStartupModule(ZzStartupMarkers *markers)
        : markers_(markers)
    {
    }

    /** @brief 返回稳定且无依赖的探针模块描述。 */
    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override
    {
        return {
            ZzPureTools::ZzModuleId(QStringLiteral("startup-probe")),
            QStringLiteral("1.0.0"),
            {}};
    }

    /** @brief 记录模块运行时已经启动。 */
    [[nodiscard]] ZzCore::ZzResult<void> start() override
    {
        if (markers_ != nullptr) {
            markers_->record(QStringLiteral("modules-started"));
        }
        return ZzCore::ZzResult<void>::success();
    }

    /** @brief 最小探针没有异步工作需要请求停止。 */
    void requestStop() noexcept override
    {
    }

    /** @brief 最小探针没有运行资源需要最终回收。 */
    void stop() noexcept override
    {
    }

private:
    ZzStartupMarkers *markers_ = nullptr;
};

/** @brief 在首个顶层窗口 Paint 后验证窗口可交互并输出标记。 */
class ZzFirstPaintFilter final : public QObject
{
public:
    /** @brief 保存应用、窗口和标记的非拥有观察指针。 */
    ZzFirstPaintFilter(
        ZzPureTools::ZzPureApplication *application,
        ZzPureTools::ZzApplicationWindow *window,
        ZzStartupMarkers *markers)
        : application_(application)
        , window_(window)
        , markers_(markers)
    {
    }

protected:
    /** @brief 只消费首个 Paint 的观测，不阻止窗口正常绘制。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (handled_ || watched != window_.data() || event == nullptr
            || event->type() != QEvent::Paint) {
            return QObject::eventFilter(watched, event);
        }

        handled_ = true;
        if (markers_ != nullptr) {
            markers_->record(QStringLiteral("first-paint"));
        }
        QTimer::singleShot(0, application_, [this] {
            const bool usable = !window_.isNull()
                && window_->isVisible()
                && window_->isEnabled()
                && window_->windowHandle() != nullptr
                && window_->windowHandle()->isExposed()
                && application_ != nullptr
                && application_->focusWidget() != nullptr;
            if (!usable || markers_ == nullptr) {
                QTextStream error(stderr, QIODevice::WriteOnly);
                error << "startup window was not usable after first paint\n";
                error.flush();
                if (application_ != nullptr) {
                    application_->exit(2);
                }
                return;
            }

            QTextStream output(stdout, QIODevice::WriteOnly);
            output << QJsonDocument(markers_->json())
                          .toJson(QJsonDocument::Compact)
                   << '\n';
            output.flush();
            application_->exit(EXIT_SUCCESS);
        });
        return QObject::eventFilter(watched, event);
    }

private:
    ZzPureTools::ZzPureApplication *application_ = nullptr;
    QPointer<ZzPureTools::ZzApplicationWindow> window_;
    ZzStartupMarkers *markers_ = nullptr;
    bool handled_ = false;
};

/** @brief 向标准错误输出 Result 的技术信息并返回失败码。 */
int zzFail(const QString &context, const ZzCore::ZzError &error)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << context << ": " << error.technicalMessage() << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char *argv[])
{
    QElapsedTimer processTimer;
    processTimer.start();
    ZzStartupMarkers markers(&processTimer);

    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return zzFail(QStringLiteral("window bootstrap failed"),
                      bootstrap.error());
    }

    ZzPureTools::ZzPureApplication application(argc, argv);
    markers.record(QStringLiteral("qt-created"));

    ZzPureTools::ZzApplicationBuilder builder;
    auto moduleResult = builder.addModule(
        std::make_unique<ZzStartupModule>(&markers));
    if (!moduleResult) {
        return zzFail(QStringLiteral("module registration failed"),
                      moduleResult.error());
    }

    const ZzPureTools::ZzRouteId route(QStringLiteral("startup"));
    ZzPureTools::ZzPageRegistration page;
    page.routeId = route;
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::Persistent;
    page.factory = [&markers](QWidget *parent) {
        auto *view = new QWidget(parent);
        view->setObjectName(QStringLiteral("ZzStartupProbePage"));
        view->setFocusPolicy(Qt::StrongFocus);
        auto result = ZzPureTools::ZzPageInstance::create(
            parent,
            view,
            std::make_unique<QObject>(),
            std::make_unique<QObject>());
        if (result) {
            markers.record(QStringLiteral("page-created"));
        }
        return result;
    };
    auto pageResult = builder.addPage(std::move(page));
    if (!pageResult) {
        return zzFail(QStringLiteral("page registration failed"),
                      pageResult.error());
    }

    const ZzPureTools::ZzNavigationNode node{
        route,
        QStringLiteral("ZzStartupProbe"),
        QStringLiteral("Startup"),
        {}};
    auto navigationResult = builder.addNavigationNode(node);
    if (!navigationResult) {
        return zzFail(QStringLiteral("navigation registration failed"),
                      navigationResult.error());
    }
    auto routeResult = builder.setInitialRoute(route);
    if (!routeResult) {
        return zzFail(QStringLiteral("initial route failed"),
                      routeResult.error());
    }
    auto buildResult = builder.build(application);
    if (!buildResult) {
        return zzFail(QStringLiteral("application build failed"),
                      buildResult.error());
    }

    ZzPureTools::ZzApplicationWindow *window = nullptr;
    for (QWidget *candidate : application.topLevelWidgets()) {
        window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(candidate);
        if (window != nullptr) {
            break;
        }
    }
    if (window == nullptr) {
        QTextStream(stderr, QIODevice::WriteOnly)
            << "startup application created no application window\n";
        return EXIT_FAILURE;
    }

    window->resize(900, 640);
    window->setFocusPolicy(Qt::StrongFocus);
    window->raise();
    window->activateWindow();
    if (auto *pageView = window->findChild<QWidget *>(
            QStringLiteral("ZzStartupProbePage"))) {
        pageView->setFocus(Qt::OtherFocusReason);
    } else {
        window->setFocus(Qt::OtherFocusReason);
    }

    ZzFirstPaintFilter paintFilter(&application, window, &markers);
    window->installEventFilter(&paintFilter);
    QTimer::singleShot(5000, &application, [&application] {
        QTextStream(stderr, QIODevice::WriteOnly)
            << "startup probe timed out before first usable paint\n";
        application.exit(2);
    });
    return application.exec();
}
