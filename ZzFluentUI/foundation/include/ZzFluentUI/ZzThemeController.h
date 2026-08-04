#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <ZzFluentUI/ZzFluentFoundationExport.h>
#include <ZzFluentUI/ZzThemeChangeKind.h>
#include <ZzFluentUI/ZzThemeMode.h>

class QEvent;

namespace ZzFluentUI {

class ZzThemeControllerPrivate;
class ZzThemeSnapshot;

/** @brief 在 GUI 线程管理应用级 Fluent 主题快照。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzThemeController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzThemeController)

public:
    /**
     * @brief 构造应用级主题控制器。
     * @param parent 可选 QObject 所有者，不改变快照所有权。
     * @pre 已构造 QGuiApplication，且调用发生在 GUI 线程。
     */
    explicit ZzThemeController(QObject *parent = nullptr);

    /** @brief 销毁控制器并断开系统主题监听。 */
    ~ZzThemeController() override;

    /**
     * @brief 返回请求模式。
     * @return 调用者最后设置的模式。
     * @pre 仅可在控制器线程调用。
     */
    [[nodiscard]] ZzThemeMode mode() const noexcept;

    /**
     * @brief 返回 System 解析后的模式。
     * @return Light、Dark 或显式 HighContrast。
     * @pre 仅可在控制器线程调用。
     */
    [[nodiscard]] ZzThemeMode resolvedMode() const noexcept;

    /**
     * @brief 返回共享只读快照。
     * @return 可复制的不可变快照所有权；取得后可传给其他线程只读使用。
     * @pre 读取控制器当前指针本身必须发生在控制器所属 GUI 线程。
     * @note shared_ptr 延长生命周期且不转移可变所有权。
     */
    [[nodiscard]] std::shared_ptr<const ZzThemeSnapshot> snapshot()
        const noexcept;

    /**
     * @brief 返回当前强调色。
     * @return 已规范化的有效颜色。
     * @pre 仅可在控制器线程调用。
     */
    [[nodiscard]] QColor accentColor() const;

    /**
     * @brief 返回是否减少非必要动效。
     * @return 启用减少动效时返回 true。
     * @pre 仅可在控制器线程调用。
     */
    [[nodiscard]] bool reducedMotion() const noexcept;

    /**
     * @brief 切换主题来源。
     * @param mode 新请求模式；等价值不发信号。
     * @pre 必须在控制器线程调用。
     */
    void setMode(ZzThemeMode mode);

    /**
     * @brief 设置强调色。
     * @param color 新颜色；无效颜色恢复默认蓝色。
     * @pre 必须在控制器线程调用。
     */
    void setAccentColor(const QColor &color);

    /**
     * @brief 设置可访问性动效偏好。
     * @param reducedMotion 是否关闭非必要动效。
     * @pre 必须在控制器线程调用。
     */
    void setReducedMotion(bool reducedMotion);

Q_SIGNALS:
    /**
     * @brief 在完整快照交换后发出。
     * @param revision 新快照版本。
     * @param changes 消费者需要执行的更新类别。
     * @note 信号在控制器所属 GUI 线程同步发出，不转移快照所有权。
     */
    void snapshotChanged(
        quint64 revision,
        ZzThemeChangeKinds changes);

protected:
    /**
     * @brief 监听应用字体变化并重建几何令牌。
     * @param watched 事件来源。
     * @param event 待处理事件。
     * @return QObject 默认过滤结果；调用方不得直接调用。
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<ZzThemeControllerPrivate> d_ptr;
};

} // namespace ZzFluentUI
