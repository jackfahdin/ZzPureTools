#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtGui/QImage>
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include "ZzWidgetTheme.h"

class QVariantAnimation;

namespace ZzFluentUI {

class ZzStackTransitionOverlay;

/** @brief 保留页面稳定身份、快照开关和销毁观察。 */
struct ZzAnimatedStackPage final
{
    QPointer<QWidget> view;
    QWidget *identity = nullptr;
    QMetaObject::Connection destroyed;
    QPointer<QWidget> focus;
    bool animationEnabled = true;
};

/** @brief 管理页面所有权、事务及动效状态，不接触业务数据。 */
class ZzAnimatedStackedWidgetPrivate final : public QObject
{
public:
    using Mode = ZzAnimatedStackedWidget::ZzTransitionMode;
    using Direction = ZzAnimatedStackedWidget::ZzTransitionDirection;
    /** @brief 创建固定内容宿主与布局。 */
    explicit ZzAnimatedStackedWidgetPrivate(ZzAnimatedStackedWidget *owner);
    /** @brief 在页面树析构前解除所有观察。 */
    ~ZzAnimatedStackedWidgetPrivate() override;
    /** @brief 查找稳定页面身份。 */
    int indexOf(QWidget *page) const noexcept;
    /** @brief 校验并接管页面。 */
    int insert(int index, QWidget *page);
    /** @brief 删除注册记录，可选择归还页面。 */
    QWidget *remove(QWidget *page, bool detach);
    /** @brief 提交或排队一次切换。 */
    bool request(QWidget *page, Direction direction);
    /** @brief 环境变化时立即恢复真实页。 */
    void settle();
    /** @brief 在可重入的显隐操作之间检查对象存活。 */
    void synchronizePages();
    /** @brief 完成一次修改后消费最新排队请求。 */
    void endMutation();
    /** @brief 按最后已通知的身份和索引去重，不吞掉结构重入后的有效提交。 */
    void notifyCurrent();
    /** @brief 清理动效；环境中断时直接提交最后待切页。 */
    void finish(bool latest);
    /** @brief 按内容尺寸捕获已布局的页面，不在帧循环中调用。 */
    QImage capture(QWidget *page, QSize pixels);
    /** @brief 判断页面子树能否安全使用普通 QWidget 截图。 */
    bool canCapture(QWidget *page) const;
    /** @brief 观察页面被外部移动。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzAnimatedStackedWidget *const q;
    ZzWidgetTheme theme;
    QWidget *host;
    ZzStackTransitionOverlay *overlay;
    QVariantAnimation *animation;
    QList<ZzAnimatedStackPage> pages;
    QWidget *current = nullptr;
    QWidget *notifiedCurrent = nullptr;
    int notifiedIndex = -1;
    Mode mode = Mode::SlideHorizontal;
    int milliseconds = -1;
    quint64 generation = 0;
    bool running = false;
    bool announced = false;
    bool preparing = false;
    bool settling = false;
    bool restoreFocus = false;
    QWidget *source = nullptr;
    int mutationDepth = 0;
    bool draining = false;
    QPointer<QWidget> pending;
    Direction pendingDirection = Direction::Automatic;
};

} // namespace ZzFluentUI
