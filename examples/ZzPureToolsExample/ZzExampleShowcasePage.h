#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

namespace ZzExample {

class ZzExampleShowcasePagePrivate;

/** @brief 展示导航、反馈或图标组件组合的纯 QWidget View。 */
class ZzExampleShowcasePage final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 标识当前 View 创建的组件组合。 */
    enum class ZzPageKind
    {
        Navigation,
        Feedback,
        Icons
    };
    Q_ENUM(ZzPageKind)

    /**
     * @brief 创建指定类型的本地交互展示页。
     * @param kind 页面内容类型。
     * @param title 页面标题。
     * @param parent 必须由页面宿主提供的 QWidget 父对象。
     */
    explicit ZzExampleShowcasePage(
        ZzPageKind kind,
        const QString &title,
        QWidget *parent);

    /** @brief 释放私有状态，控件由 Qt 父子树销毁。 */
    ~ZzExampleShowcasePage() override;

    /** @brief 禁止复制 QWidget View。 */
    ZzExampleShowcasePage(const ZzExampleShowcasePage &) = delete;

    /** @brief 禁止复制赋值 QWidget View。 */
    ZzExampleShowcasePage &operator=(
        const ZzExampleShowcasePage &) = delete;

    /** @brief 禁止移动已经建立 QObject 连接的 View。 */
    ZzExampleShowcasePage(ZzExampleShowcasePage &&) = delete;

    /** @brief 禁止移动赋值已经建立 QObject 连接的 View。 */
    ZzExampleShowcasePage &operator=(
        ZzExampleShowcasePage &&) = delete;

private:
    friend class ZzExampleShowcasePagePrivate;
    std::unique_ptr<ZzExampleShowcasePagePrivate> d_ptr;
};

} // namespace ZzExample
