#pragma once

#include <memory>

#include <QtWidgets/QAbstractItemView>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QHideEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

namespace ZzFluentUI {

class ZzCarouselViewPrivate;

/**
 * @brief 以固定绘制复杂度展示当前模型项的 Fluent 轮播视图。
 *
 * 控件只读取 QAbstractItemModel 的展示角色，不取得模型所有权，也不执行
 * 自动播放、图片加载、导航或其他业务命令。普通帧只绘制当前项，过渡帧
 * 最多绘制当前项和前一项。
 */
class ZZ_FLUENT_UI_EXPORT ZzCarouselView final : public QAbstractItemView {
  Q_OBJECT
  Q_PROPERTY(bool wrapAroundEnabled READ isWrapAroundEnabled WRITE
                 setWrapAroundEnabled NOTIFY wrapAroundEnabledChanged)
  Q_PROPERTY(int animationDuration READ animationDuration WRITE
                 setAnimationDuration NOTIFY animationDurationChanged)
  Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY
                 currentRowChanged)
  Q_PROPERTY(bool immersive READ immersive WRITE setImmersive NOTIFY immersiveChanged)
  Q_PROPERTY(qreal borderRadius READ borderRadius WRITE setBorderRadius NOTIFY borderRadiusChanged)
  Q_PROPERTY(bool showNavigationButtons READ showNavigationButtons WRITE setShowNavigationButtons NOTIFY showNavigationButtonsChanged)
  Q_PROPERTY(bool showIndicators READ showIndicators WRITE setShowIndicators NOTIFY showIndicatorsChanged)
  Q_PROPERTY(ZzNavigationButtonTrigger navigationButtonTrigger READ navigationButtonTrigger WRITE setNavigationButtonTrigger NOTIFY navigationButtonTriggerChanged)
  Q_PROPERTY(Qt::AspectRatioMode imageAspectRatioMode READ imageAspectRatioMode WRITE setImageAspectRatioMode NOTIFY imageAspectRatioModeChanged)
  Q_DISABLE_COPY_MOVE(ZzCarouselView)

public:
  /** @brief 默认 delegate 使用的扩展示意数据角色。 */
  enum ZzItemDataRole {
    /** @brief 当前项标题下方的可选说明文字。 */
    DescriptionRole = Qt::UserRole + 1
  };
  Q_ENUM(ZzItemDataRole)

  /** @brief 导航按钮的可见性触发方式。 */
  enum ZzNavigationButtonTrigger { AlwaysVisible, OnHover };
  Q_ENUM(ZzNavigationButtonTrigger)

  /**
   * @brief 创建不拥有外部 model 的空轮播视图。
   * @param parent 可为空的 QWidget 所有者。
   */
  explicit ZzCarouselView(QWidget *parent = nullptr);

  /** @brief 停止过渡并释放固定数量的内部控件。 */
  ~ZzCarouselView() override;

  /**
   * @brief 返回前后导航是否在首尾之间环绕。
   * @return 启用环绕时返回 true。
   */
  [[nodiscard]] bool isWrapAroundEnabled() const noexcept;

  /**
   * @brief 设置前后导航是否在首尾之间环绕。
   * @param enabled 是否启用环绕。
   */
  void setWrapAroundEnabled(bool enabled);

  /**
   * @brief 返回相邻项滑动动画时长。
   * @return 0 到 1000 毫秒范围内的时长。
   */
  [[nodiscard]] int animationDuration() const noexcept;

  /**
   * @brief 设置相邻项滑动动画时长并收敛到安全范围。
   * @param durationMilliseconds 请求时长，最终限制为 0 到 1000 毫秒。
   */
  void setAnimationDuration(int durationMilliseconds);

  /**
   * @brief 返回 root 下当前索引的行号。
   * @return 没有有效当前项时返回 -1。
   */
  [[nodiscard]] int currentRow() const noexcept;

  /**
   * @brief 以程序方式选择 root 下指定行，不检查 enabled flag。
   * @param row 合法行号；越界输入不改变状态。
   */
  void setCurrentRow(int row);

  /** @brief 返回是否以整幅视口呈现沉浸式轮播。 */
  [[nodiscard]] bool immersive() const noexcept;
  /** @brief 切换沉浸式呈现。 */
  void setImmersive(bool immersive);
  /** @brief 返回内容外框圆角半径。 */
  [[nodiscard]] qreal borderRadius() const noexcept;
  /** @brief 设置非负且有限的外框圆角半径。 */
  void setBorderRadius(qreal radius);
  /** @brief 返回是否显示导航按钮。 */
  [[nodiscard]] bool showNavigationButtons() const noexcept;
  /** @brief 设置导航按钮显隐。 */
  void setShowNavigationButtons(bool show);
  /** @brief 返回是否显示分页指示点。 */
  [[nodiscard]] bool showIndicators() const noexcept;
  /** @brief 设置分页指示点显隐。 */
  void setShowIndicators(bool show);
  /** @brief 返回导航按钮触发方式。 */
  [[nodiscard]] ZzNavigationButtonTrigger navigationButtonTrigger() const noexcept;
  /** @brief 设置有效的导航按钮触发方式。 */
  void setNavigationButtonTrigger(ZzNavigationButtonTrigger trigger);
  /** @brief 返回默认图片适配方式。 */
  [[nodiscard]] Qt::AspectRatioMode imageAspectRatioMode() const noexcept;
  /** @brief 设置有效的默认图片适配方式。 */
  void setImageAspectRatioMode(Qt::AspectRatioMode mode);

  /**
   * @brief 安装外部模型并在非空时选择第 0 行。
   * @param model 可为空且继续由调用方拥有的模型。
   */
  void setModel(QAbstractItemModel *model) override;

  /**
   * @brief 返回当前项在 viewport 中的可见矩形。
   * @param index 待查询索引。
   * @return 仅当前有效索引返回内容矩形，其他索引返回空矩形。
   */
  [[nodiscard]] QRect visualRect(const QModelIndex &index) const override;

  /**
   * @brief 将指定索引设为当前可见项。
   * @param index root 下第 0 列的目标索引。
   * @param hint 保留的 Qt 滚动提示；单项视图不区分提示类型。
   */
  void scrollTo(const QModelIndex &index,
                ScrollHint hint = EnsureVisible) override;

  /**
   * @brief 查询内容点命中的当前模型索引。
   * @param point viewport 局部坐标。
   * @return 点位于内容区时返回当前索引，否则返回无效索引。
   */
  [[nodiscard]] QModelIndex indexAt(const QPoint &point) const override;

public Q_SLOTS:
  /**
   * @brief 切换 root index，并为新 root 初始化当前项。
   * @param index 属于当前 model 的 root index，可为空。
   */
  void setRootIndex(const QModelIndex &index) override;

  /** @brief 切换到前一行；边界行为由 wrapAroundEnabled 决定。 */
  void showPrevious();

  /** @brief 切换到后一行；边界行为由 wrapAroundEnabled 决定。 */
  void showNext();

Q_SIGNALS:
  /**
   * @brief 环绕能力实际变化后发出。
   * @param enabled 新状态。
   */
  void wrapAroundEnabledChanged(bool enabled);

  /**
   * @brief 动画时长实际变化后发出。
   * @param durationMilliseconds 收敛后的新时长。
   */
  void animationDurationChanged(int durationMilliseconds);

  /**
   * @brief 派生 current row 实际变化后发出。
   * @param row 新行号；无有效当前项时为 -1。
   * @note 外部直接修改 selectionModel 时，通知延至事件队列；若期间发生本控件
   * 导航，会先按顺序补发外部行变化，再同步通知内部导航的结果。
   */
  void currentRowChanged(int row);
  /** @brief 沉浸模式实际变化。 */
  void immersiveChanged(bool immersive);
  /** @brief 圆角半径实际变化。 */
  void borderRadiusChanged(qreal radius);
  /** @brief 导航按钮显隐实际变化。 */
  void showNavigationButtonsChanged(bool show);
  /** @brief 指示点显隐实际变化。 */
  void showIndicatorsChanged(bool show);
  /** @brief 按钮触发方式实际变化。 */
  void navigationButtonTriggerChanged(ZzNavigationButtonTrigger trigger);
  /** @brief 图片适配方式实际变化。 */
  void imageAspectRatioModeChanged(Qt::AspectRatioMode mode);

protected:
  /** @brief 按键导航只返回直接相邻或首尾目标，不扫描完整 model。 */
  [[nodiscard]] QModelIndex
  moveCursor(CursorAction cursorAction,
             Qt::KeyboardModifiers modifiers) override;

  /** @brief 单项视图没有水平滚动偏移。 */
  [[nodiscard]] int horizontalOffset() const override;

  /** @brief 单项视图没有垂直滚动偏移。 */
  [[nodiscard]] int verticalOffset() const override;

  /** @brief 只有当前 root 下第 0 列索引可见。 */
  [[nodiscard]] bool isIndexHidden(const QModelIndex &index) const override;

  /** @brief 将命中当前内容矩形的选择应用到 selection model。 */
  void setSelection(const QRect &rect,
                    QItemSelectionModel::SelectionFlags flags) override;

  /** @brief 将包含当前项的选择映射为唯一内容区域。 */
  [[nodiscard]] QRegion
  visualRegionForSelection(const QItemSelection &selection) const override;

  /** @brief 同步 row 信号并启动唯一相邻项过渡。 */
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override;

  /** @brief 通过当前 delegate 绘制最多两个模型项和有界指示器。 */
  void paintEvent(QPaintEvent *event) override;

  /** @brief 只更新两个固定箭头按钮的几何。 */
  void resizeEvent(QResizeEvent *event) override;

  /** @brief 将有效滚轮步进转换为一次前后导航。 */
  void wheelEvent(QWheelEvent *event) override;

  /** @brief 统一 Enter 与 Return 对当前可用项的键盘激活语义。 */
  void keyPressEvent(QKeyEvent *event) override;

  /** @brief 响应语言、布局方向、style、palette 和 enabled 变化。 */
  void changeEvent(QEvent *event) override;

  /** @brief 隐藏时停止动画并同步到当前项终态。 */
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

  /** @brief 让隐藏的导航按钮响应视图焦点与悬停。 */
  bool viewportEvent(QEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

  /** @brief 在模型项点击之前处理可见分页点。 */
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  friend class ZzCarouselViewPrivate;
  std::unique_ptr<ZzCarouselViewPrivate> d_ptr;
};

} // namespace ZzFluentUI
