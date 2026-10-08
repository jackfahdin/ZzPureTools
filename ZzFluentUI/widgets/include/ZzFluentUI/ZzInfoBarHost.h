#pragma once

#include <memory>

#include <QtCore/QObject>
#include <ZzFluentUI/ZzInfoBar.h>

namespace ZzFluentUI {

class ZzInfoBarHostPrivate;

/** @brief 在目标 QWidget 的内容区域内堆叠、排队并计时展示浮动信息栏。 */
class ZZ_FLUENT_UI_EXPORT ZzInfoBarHost final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzInfoBarHost)

public:
    /** @brief 六个独立的浮动通知锚点。 */
    enum Position { TopLeft, Top, TopRight, BottomLeft, Bottom, BottomRight };
    Q_ENUM(Position)

    Q_PROPERTY(int margin READ margin WRITE setMargin NOTIFY marginChanged)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(int maximumWidth READ maximumWidth WRITE setMaximumWidth NOTIFY maximumWidthChanged)
    Q_PROPERTY(
        int defaultTimeout READ defaultTimeout WRITE setDefaultTimeout NOTIFY defaultTimeoutChanged)

    /** @brief 绑定目标；未指定 QObject 所有者时由目标拥有 Host。 */
    explicit ZzInfoBarHost(QWidget* target, QObject* parent = nullptr);
    ~ZzInfoBarHost() override;

    /** @brief 更换全局默认目标并释放旧默认 Host。 */
    static void setDefaultTarget(QWidget* target);
    [[nodiscard]] static QWidget* defaultTarget();
    [[nodiscard]] static ZzInfoBarHost* defaultHost();
    [[nodiscard]] QWidget* target() const;

    [[nodiscard]] int margin() const;
    void setMargin(int margin);
    [[nodiscard]] int spacing() const;
    void setSpacing(int spacing);
    [[nodiscard]] int maximumWidth() const;
    void setMaximumWidth(int width);
    [[nodiscard]] int defaultTimeout() const;
    void setDefaultTimeout(int milliseconds);

    /** @brief 创建并托管通知；负超时使用默认值，零超时持续展示。 */
    ZzInfoBar* showInfoBar(ZzInfoBar::Severity severity, const QString& title,
        const QString& message, Position position = TopRight, int timeout = -1);
    /** @brief 接管信息栏；关闭后自动销毁。 */
    void addInfoBar(ZzInfoBar* infoBar, Position position = TopRight, int timeout = -1);

public Q_SLOTS:
    /** @brief 关闭所有活动项及等待项。 */
    void dismissAll();
    /** @brief 关闭指定锚点的活动项及等待项。 */
    void dismissAll(ZzInfoBarHost::Position position);

Q_SIGNALS:
    void marginChanged(int margin);
    void spacingChanged(int spacing);
    void maximumWidthChanged(int width);
    void defaultTimeoutChanged(int milliseconds);
    void infoBarShown(ZzInfoBar* infoBar, ZzInfoBarHost::Position position);
    void infoBarClosed(ZzInfoBar* infoBar, ZzInfoBarHost::Position position);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::unique_ptr<ZzInfoBarHostPrivate> d_ptr;
};

} // namespace ZzFluentUI
