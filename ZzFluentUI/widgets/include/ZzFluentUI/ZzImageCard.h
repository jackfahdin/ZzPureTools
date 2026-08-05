#pragma once

#include <memory>

#include <QtCore/Qt>
#include <QtGui/QPixmap>
#include <QtWidgets/QAbstractButton>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QKeyEvent;
class QPaintEvent;

namespace ZzFluentUI {

class ZzImageCardPrivate;

/**
 * @brief 显示本地 pixmap、标题和说明的可操作图片卡片。
 *
 * 控件不读取文件或网络；调用方负责提供 QPixmap，并通过 clicked 信号
 * 执行导航或业务命令。
 */
class ZZ_FLUENT_UI_EXPORT ZzImageCard final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(
        QPixmap pixmap
        READ pixmap
        WRITE setPixmap
        NOTIFY pixmapChanged)
    Q_PROPERTY(
        QString description
        READ description
        WRITE setDescription
        NOTIFY descriptionChanged)
    Q_PROPERTY(
        Qt::AspectRatioMode aspectRatioMode
        READ aspectRatioMode
        WRITE setAspectRatioMode
        NOTIFY aspectRatioModeChanged)
    Q_DISABLE_COPY_MOVE(ZzImageCard)

public:
    /**
     * @brief 创建空图片卡片。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzImageCard(QWidget *parent = nullptr);

    /**
     * @brief 创建带标题和说明的图片卡片。
     * @param text 可本地化的标题。
     * @param description 可本地化的辅助说明。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzImageCard(
        const QString &text,
        const QString &description = {},
        QWidget *parent = nullptr);

    /** @brief 释放隐式共享 pixmap 引用和私有状态。 */
    ~ZzImageCard() override;

    /**
     * @brief 返回当前隐式共享 pixmap。
     * @return 当前 pixmap 的隐式共享副本。
     */
    [[nodiscard]] QPixmap pixmap() const;

    /**
     * @brief 更新本地 pixmap，不执行文件或网络读取。
     * @param pixmap 新 pixmap，可为空。
     */
    void setPixmap(QPixmap pixmap);

    /**
     * @brief 返回图片下方的辅助说明。
     * @return 当前说明文字的隐式共享副本。
     */
    [[nodiscard]] QString description() const;

    /**
     * @brief 更新辅助说明并同步默认可访问描述。
     * @param description 新的可本地化说明。
     */
    void setDescription(QString description);

    /**
     * @brief 返回图片适配策略。
     * @return 当前 Qt 图片长宽比策略。
     */
    [[nodiscard]] Qt::AspectRatioMode aspectRatioMode() const noexcept;

    /**
     * @brief 更新图片适配策略，非法枚举收敛为居中裁剪。
     * @param mode 新图片适配策略。
     */
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    /**
     * @brief 返回包含 16:9 图片区和双行文字区的建议尺寸。
     * @return 设备无关逻辑像素尺寸。
     */
    [[nodiscard]] QSize sizeHint() const override;

    /**
     * @brief 返回仍可辨认图片和标题的最小尺寸。
     * @return 设备无关逻辑像素尺寸。
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /**
     * @brief pixmap 引用变化后发出。
     * @param pixmap 新 pixmap。
     */
    void pixmapChanged(const QPixmap &pixmap);

    /**
     * @brief 说明文字变化后发出。
     * @param description 新说明文字。
     */
    void descriptionChanged(const QString &description);

    /**
     * @brief 图片适配策略变化后发出。
     * @param mode 收敛后的新策略。
     */
    void aspectRatioModeChanged(Qt::AspectRatioMode mode);

protected:
    /** @brief 使用当前 style 和 palette 绘制图片、文字与按钮状态。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 处理 Enter/Return，并保留 QAbstractButton 的 Space 语义。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 在字体、style、palette、布局方向或 DPR 变化后刷新。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzImageCardPrivate> d_ptr;
};

} // namespace ZzFluentUI
