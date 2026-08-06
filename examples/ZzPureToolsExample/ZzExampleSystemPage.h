#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include "ZzExampleSystemPageKind.h"

class QAbstractItemModel;

namespace ZzExample {

class ZzExampleSystemPagePrivate;

/** @brief 只展示注入快照并发出设置意图的系统页面 View。 */
class ZzExampleSystemPage final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建平台、设置或关于页面。
     * @param kind 页面种类。
     * @param title 页面标题。
     * @param model 非空且生命周期覆盖 View 的只读快照模型。
     * @param parent 必须由页面宿主提供的 QWidget 父对象。
     */
    explicit ZzExampleSystemPage(
        ZzExampleSystemPageKind kind,
        const QString &title,
        QAbstractItemModel *model,
        QWidget *parent);

    /** @brief 释放私有状态，控件由 Qt 父子树销毁。 */
    ~ZzExampleSystemPage() override;

    /** @brief 禁止复制 QWidget View。 */
    ZzExampleSystemPage(const ZzExampleSystemPage &) = delete;

    /** @brief 禁止复制赋值 QWidget View。 */
    ZzExampleSystemPage &operator=(
        const ZzExampleSystemPage &) = delete;

    /** @brief 禁止移动已经建立 QObject 连接的 View。 */
    ZzExampleSystemPage(ZzExampleSystemPage &&) = delete;

    /** @brief 禁止移动赋值已经建立 QObject 连接的 View。 */
    ZzExampleSystemPage &operator=(ZzExampleSystemPage &&) = delete;

    /**
     * @brief 由 Presenter 同步设置页当前值且不发用户意图。
     * @param themeMode 主题模式索引。
     * @param logLevel 日志等级索引。
     * @param reducedMotion 是否减少动效。
     * @param activityDockVisible 活动 Dock 是否可见。
     */
    void setSettingsSnapshot(
        int themeMode,
        int logLevel,
        bool reducedMotion,
        bool activityDockVisible);

    /** @brief 由 Presenter 更新操作状态。 */
    void setStatusText(const QString &text);

Q_SIGNALS:
    /** @brief 用户请求切换主题模式。 */
    void themeModeRequested(int mode);

    /** @brief 用户请求切换日志等级。 */
    void logLevelRequested(int level);

    /** @brief 用户请求改变减少动效偏好。 */
    void reducedMotionRequested(bool enabled);

    /** @brief 用户请求改变活动 Dock 可见性。 */
    void activityDockVisibilityRequested(bool visible);

private:
    friend class ZzExampleSystemPagePrivate;
    std::unique_ptr<ZzExampleSystemPagePrivate> d_ptr;
};

} // namespace ZzExample
