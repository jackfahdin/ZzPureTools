#include "ZzColorPickerPrivate.h"
#include "ZzColorSpectrum.h"
#include "ZzColorGradientSlider.h"
#include "ZzColorPickerMath.h"
#include "ZzColorPickerMetrics.h"

#include <algorithm>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QPointer>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QRegularExpressionValidator>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int ZzMaximumPaletteColors = 256;
constexpr int ZzVisiblePaletteRows = 3;
constexpr int ZzChannelEditorWidth = 84;
constexpr int ZzSectionSpacing = 10;

/** @brief 把有效颜色规范化到 8 位 RGBA。 */
QColor zzNormalizedColor(const QColor &color)
{
    return color.isValid()
        ? QColor::fromRgba(color.rgba())
        : QColor{};
}

/** @brief 返回适合色板和无障碍展示的确定性颜色文本。 */
QString zzColorText(const QColor &color)
{
    const QColor::NameFormat format = color.alpha() < 255
        ? QColor::HexArgb
        : QColor::HexRgb;
    return color.name(format).toUpper();
}

/** @brief 过滤无效和重复 RGBA 值并限制色板大小。 */
QList<QColor> zzNormalizedPalette(QList<QColor> colors)
{
    QList<QColor> result;
    result.reserve(std::min(
        colors.size(),
        static_cast<qsizetype>(ZzMaximumPaletteColors)));
    QSet<QRgb> seen;
    for (const QColor &candidate : std::as_const(colors)) {
        const QColor color = zzNormalizedColor(candidate);
        if (!color.isValid() || seen.contains(color.rgba())) {
            continue;
        }
        seen.insert(color.rgba());
        result.append(color);
        if (result.size() == ZzMaximumPaletteColors) {
            break;
        }
    }
    return result;
}

} // namespace

/** @brief 在原生 HEX 失焦处理前启用安全、有序的颜色通知。 */
class ZzColorHexEditor final : public QLineEdit
{
public:
    /** @brief 绑定选择器状态并保留原生输入与无障碍能力。 */
    explicit ZzColorHexEditor(ZzColorPickerPrivate *owner, QWidget *parent)
        : QLineEdit(parent), owner_(owner)
    {
    }
protected:
    /** @brief 失焦内立即更新颜色，但避免用户回调删除 Qt 正在访问的对象。 */
    void focusOutEvent(QFocusEvent *event) override
    {
        const QPointer<QLineEdit> guard(this);
        owner_->hexFocusOutActive = true;
        owner_->deferColorNotifications();
        QLineEdit::focusOutEvent(event);
        if (guard) {
            owner_->hexFocusOutActive = false;
        }
    }
private:
    ZzColorPickerPrivate *const owner_;
};

/** @brief 在 viewport 宽度变化后调整八列色板的单元尺寸。 */
class ZzColorPaletteView final : public QListView
{
public:
    /** @brief 绑定唯一选择器装配，并保留 QListView 的原生交互。 */
    explicit ZzColorPaletteView(ZzColorPickerPrivate *owner, QWidget *parent)
        : QListView(parent), owner_(owner)
    {
    }
protected:
    /** @brief 重算适配当前 viewport 的固定八列网格。 */
    void resizeEvent(QResizeEvent *event) override
    {
        QListView::resizeEvent(event);
        // 网格尺寸只依赖 viewport 宽度；高度变化不重算，避免与 setFixedHeight 往复
        if (event->size().width() == event->oldSize().width()) {
            return;
        }
        owner_->syncPaletteMetrics();
    }
private:
    ZzColorPickerPrivate *const owner_;
};

/** @brief 保存唯一色板集合并暴露颜色和无障碍展示角色。 */
class ZzColorPaletteModel final : public QAbstractListModel
{
public:
    enum ZzRole : int
    {
        ZzColorRole = Qt::UserRole + 1,
    };

    /** @brief 创建空色板模型。 */
    explicit ZzColorPaletteModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回根索引下的色板项数。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(colors_.size());
    }

    /** @brief 返回颜色值和确定性文本角色。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= colors_.size()) {
            return {};
        }
        const QColor color = colors_.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::AccessibleTextRole:
            return zzColorText(color);
        case ZzColorRole:
            return color;
        default:
            return {};
        }
    }

    /** @brief 一次 reset 替换全部颜色，重复集合不发送模型事件。 */
    [[nodiscard]] bool setColors(QList<QColor> colors)
    {
        if (colors_ == colors) {
            return false;
        }
        beginResetModel();
        colors_ = std::move(colors);
        endResetModel();
        return true;
    }

    /** @brief 返回隐式共享颜色列表快照。 */
    [[nodiscard]] QList<QColor> colors() const
    {
        return colors_;
    }

    /** @brief 返回指定模型行颜色。 */
    [[nodiscard]] QColor colorAt(int row) const
    {
        return row >= 0 && row < colors_.size()
            ? colors_.at(row)
            : QColor{};
    }

    /** @brief 按 8 位 RGBA 查找首个完全匹配行。 */
    [[nodiscard]] int rowForColor(const QColor &color) const noexcept
    {
        const QRgb rgba = color.rgba();
        for (int row = 0; row < colors_.size(); ++row) {
            if (colors_.at(row).rgba() == rgba) {
                return row;
            }
        }
        return -1;
    }

private:
    QList<QColor> colors_;
};

/** @brief 只绘制当前可见色板 index 的主题边框和内容色。 */
class ZzColorSwatchDelegate final : public QStyledItemDelegate
{
public:
    /** @brief 绑定非拥有颜色选择器状态。 */
    ZzColorSwatchDelegate(
        ZzColorPickerPrivate *owner,
        QObject *parent)
        : QStyledItemDelegate(parent)
        , owner_(owner)
    {
        Q_ASSERT(owner_ != nullptr);
    }

    /** @brief 绘制内容色块、边框和非纯颜色选择反馈。 */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        if (painter == nullptr || !index.isValid()) {
            return;
        }
        const auto snapshot = owner_->theme.snapshot();
        const int extent = owner_->appearance == ZzColorPicker::Fluent
            ? qMax(1, owner_->paletteView->gridSize().width() - 3) : qMax(
            1,
            qCeil(snapshot->metric(ZzMetricToken::ColorSwatchExtent)));
        const int side = std::min(
            {extent, option.rect.width(), option.rect.height()});
        QRect swatch(
            option.rect.center().x() - side / 2,
            option.rect.center().y() - side / 2,
            side,
            side);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool focused = option.state.testFlag(QStyle::State_HasFocus);
        const qreal radius = snapshot->metric(
            ZzMetricToken::CornerRadiusSmall);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(ZzColorToken::SurfaceSecondary));
        painter->drawRoundedRect(swatch, radius, radius);
        painter->setBrush(
            index.data(ZzColorPaletteModel::ZzColorRole).value<QColor>());
        painter->drawRoundedRect(swatch, radius, radius);
        if (owner_->appearance == ZzColorPicker::Fluent && !selected && !focused) {
            painter->restore();
            return;
        }
        const QColor stroke = selected || focused
            ? snapshot->color(ZzColorToken::FocusStroke)
            : snapshot->color(ZzColorToken::ControlStroke);
        const qreal strokeWidth = selected || focused
            ? snapshot->metric(ZzMetricToken::FocusStrokeWidth)
            : snapshot->metric(ZzMetricToken::StrokeThin);
        QPen pen(stroke, strokeWidth);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        const qreal inset = strokeWidth / 2.0;
        painter->drawRoundedRect(
            QRectF(swatch).adjusted(inset, inset, -inset, -inset),
            radius,
            radius);
        painter->restore();
    }

    /** @brief 返回具名色块边长与间距之和。 */
    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &,
        const QModelIndex &) const override
    {
        const auto snapshot = owner_->theme.snapshot();
        const int side = owner_->appearance == ZzColorPicker::Fluent
            ? owner_->paletteView->gridSize().width() : qMax(
            1,
            qCeil(snapshot->metric(ZzMetricToken::ColorSwatchExtent)
                  + snapshot->metric(ZzMetricToken::ColorSwatchGap)));
        return {side, side};
    }

private:
    ZzColorPickerPrivate *const owner_;
};

/** @brief 用主题棋盘和唯一当前颜色展示 alpha 合成结果。 */
class ZzColorPreviewWidget final : public QWidget
{
public:
    /** @brief 绑定非拥有颜色选择器状态。 */
    ZzColorPreviewWidget(
        ZzColorPickerPrivate *owner,
        QWidget *parent)
        : QWidget(parent)
        , owner_(owner)
    {
        Q_ASSERT(owner_ != nullptr);
        setObjectName(QStringLiteral("zzColorPreview"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    /** @brief 返回不随内容变化的稳定预览尺寸。 */
    [[nodiscard]] QSize sizeHint() const override
    {
        return {160, 52};
    }

protected:
    /** @brief 以矩形块绘制主题棋盘、当前颜色和外框。 */
    void paintEvent(QPaintEvent *) override
    {
        const auto snapshot = owner_->theme.snapshot();
        QPainter painter(this);
        const QRect content = rect().adjusted(1, 1, -1, -1);
        const bool fluent = owner_->appearance == ZzColorPicker::Fluent;
        if (fluent) {
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addRoundedRect(content, 4, 4);
            painter.setClipPath(clip);
        }
        constexpr int tileExtent = 8;
        for (int y = content.top(); y <= content.bottom(); y += tileExtent) {
            for (int x = content.left(); x <= content.right(); x += tileExtent) {
                const bool alternate =
                    ((x - content.left()) / tileExtent
                     + (y - content.top()) / tileExtent)
                    % 2 != 0;
                painter.fillRect(
                    QRect(x, y, tileExtent, tileExtent).intersected(content),
                    snapshot->color(
                        alternate
                            ? ZzColorToken::SurfaceSecondary
                            : ZzColorToken::Surface));
            }
        }
        painter.fillRect(content, owner_->currentColor);
        QPen pen(
            snapshot->color(ZzColorToken::ControlStroke),
            snapshot->metric(ZzMetricToken::StrokeThin));
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        if (fluent) {
            painter.drawRoundedRect(QRectF(content).adjusted(0.5, 0.5, -0.5, -0.5),
                                    ZzColorPickerCornerRadius, ZzColorPickerCornerRadius);
        } else {
            painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
        }
    }

private:
    ZzColorPickerPrivate *const owner_;
};

ZzColorPickerPrivate::ZzColorPickerPrivate(ZzColorPicker *q)
    : q_ptr(q)
    , theme(q)
    , paletteModel(new ZzColorPaletteModel(q))
    , paletteView(new ZzColorPaletteView(this, q))
    , swatchDelegate(new ZzColorSwatchDelegate(this, paletteView))
    , preview(new ZzColorPreviewWidget(this, q))
    , redLabel(new QLabel(q))
    , greenLabel(new QLabel(q))
    , blueLabel(new QLabel(q))
    , alphaLabel(new QLabel(q))
    , hexLabel(new QLabel(q))
    , redSpinBox(new ZzSpinBox(q))
    , greenSpinBox(new ZzSpinBox(q))
    , blueSpinBox(new ZzSpinBox(q))
    , alphaSpinBox(new ZzSpinBox(q))
    , hexEditor(new ZzColorHexEditor(this, q))
    , hexValidator(new QRegularExpressionValidator(q))
{
    Q_ASSERT(q_ptr != nullptr);
    paletteView->setObjectName(QStringLiteral("zzColorPaletteView"));
    redSpinBox->setObjectName(QStringLiteral("zzRedSpinBox"));
    greenSpinBox->setObjectName(QStringLiteral("zzGreenSpinBox"));
    blueSpinBox->setObjectName(QStringLiteral("zzBlueSpinBox"));
    alphaSpinBox->setObjectName(QStringLiteral("zzAlphaSpinBox"));
    hexEditor->setObjectName(QStringLiteral("zzHexColorEditor"));

    paletteView->setModel(paletteModel);
    paletteView->setItemDelegate(swatchDelegate);
    paletteView->setViewMode(QListView::IconMode);
    paletteView->setFlow(QListView::LeftToRight);
    paletteView->setWrapping(true);
    paletteView->setResizeMode(QListView::Adjust);
    paletteView->setMovement(QListView::Static);
    paletteView->setUniformItemSizes(true);
    paletteView->setSelectionMode(QAbstractItemView::SingleSelection);
    paletteView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    paletteView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    for (ZzSpinBox *spinBox : {
             redSpinBox,
             greenSpinBox,
             blueSpinBox,
             alphaSpinBox}) {
        spinBox->setRange(0, 255);
        spinBox->setFixedWidth(ZzChannelEditorWidth);
        QObject::connect(
            spinBox,
            &QSpinBox::valueChanged,
            q_ptr,
            [this, spinBox] {
                commitChannelEditor(spinBox);
            });
    }
    hexEditor->setValidator(hexValidator);
    hexEditor->setClearButtonEnabled(false);
    QObject::connect(
        hexEditor,
        &QLineEdit::editingFinished,
        q_ptr,
        [this] {
            commitHexEditor();
        });

    const auto applyPaletteIndex = [this](const QModelIndex &index) {
        if (syncing || !index.isValid()) {
            return;
        }
        q_ptr->setCurrentColor(paletteModel->colorAt(index.row()));
    };
    QObject::connect(
        paletteView,
        &QListView::clicked,
        q_ptr,
        applyPaletteIndex);
    QObject::connect(
        paletteView,
        &QListView::activated,
        q_ptr,
        applyPaletteIndex);
    QObject::connect(
        paletteView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        q_ptr,
        [applyPaletteIndex](
            const QModelIndex &current,
            const QModelIndex &) {
            applyPaletteIndex(current);
        });

    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(ZzSectionSpacing);
    compactHost = new QWidget(q_ptr);
    compactLayout = new QVBoxLayout(compactHost);
    compactLayout->setContentsMargins(0, 0, 0, 0);
    compactLayout->setSpacing(ZzSectionSpacing);
    layout->addWidget(compactHost);
    compactLayout->addWidget(preview);
    compactLayout->addWidget(paletteView);
    editorHost = new QWidget(compactHost);
    editorLayout = new QGridLayout(editorHost);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setHorizontalSpacing(8);
    editorLayout->setVerticalSpacing(4);
    editorLayout->addWidget(redLabel, 0, 0);
    editorLayout->addWidget(greenLabel, 0, 1);
    editorLayout->addWidget(blueLabel, 0, 2);
    editorLayout->addWidget(alphaLabel, 0, 3);
    editorLayout->addWidget(redSpinBox, 1, 0);
    editorLayout->addWidget(greenSpinBox, 1, 1);
    editorLayout->addWidget(blueSpinBox, 1, 2);
    editorLayout->addWidget(alphaSpinBox, 1, 3);
    editorLayout->setColumnStretch(4, 1);
    compactLayout->addWidget(editorHost);
    editorLayout->addWidget(hexLabel, 2, 0, 1, 5);
    editorLayout->addWidget(hexEditor, 3, 0, 1, 5);

    buildFluentPresentation();
    syncAppearance();

    static_cast<void>(paletteModel->setColors(defaultPaletteColors()));
    refreshAccessibleText();
    syncAlphaPresentation();
    refreshTheme();
}

ZzColorPickerPrivate::~ZzColorPickerPrivate() = default;

bool ZzColorPickerPrivate::applyCurrentColor(QColor color)
{
    color = zzNormalizedColor(color);
    if (!color.isValid() || color.rgba() == currentColor.rgba()) {
        return false;
    }
    const bool rgbChanged = color.rgb() != currentColor.rgb();
    currentColor = color;
    if (rgbChanged) {
        if (color.hsvHueF() >= 0 && color.hsvSaturationF() > 0) {
            hue = color.hsvHueF();
        }
        if (color.value() > 0) {
            saturation = color.hsvSaturationF();
        }
        value = color.valueF();
    }
    syncDerivedState();
    return true;
}

bool ZzColorPickerPrivate::applyPaletteColors(QList<QColor> colors)
{
    const bool changed = paletteModel->setColors(
        zzNormalizedPalette(std::move(colors)));
    if (changed) {
        syncPaletteMetrics();
        syncDerivedState();
    }
    return changed;
}

QList<QColor> ZzColorPickerPrivate::paletteColors() const
{
    return paletteModel->colors();
}

int ZzColorPickerPrivate::paletteColorCount() const noexcept
{
    return paletteModel->rowCount();
}

QList<QColor> ZzColorPickerPrivate::defaultPaletteColors()
{
    static const QList<QColor> colors{
        QColor::fromRgb(0, 120, 212),
        QColor::fromRgb(0, 90, 158),
        QColor::fromRgb(43, 136, 216),
        QColor::fromRgb(80, 230, 255),
        QColor::fromRgb(16, 124, 16),
        QColor::fromRgb(73, 130, 5),
        QColor::fromRgb(0, 183, 195),
        QColor::fromRgb(3, 131, 135),
        QColor::fromRgb(255, 185, 0),
        QColor::fromRgb(247, 99, 12),
        QColor::fromRgb(209, 52, 56),
        QColor::fromRgb(232, 17, 35),
        QColor::fromRgb(136, 23, 152),
        QColor::fromRgb(194, 57, 179),
        QColor::fromRgb(135, 100, 184),
        QColor::fromRgb(92, 45, 145),
        QColor::fromRgb(142, 86, 46),
        QColor::fromRgb(202, 80, 16),
        QColor::fromRgb(105, 121, 126),
        QColor::fromRgb(76, 74, 72),
        QColor::fromRgb(255, 255, 255),
        QColor::fromRgb(210, 208, 206),
        QColor::fromRgb(96, 94, 92),
        QColor::fromRgb(0, 0, 0)};
    return colors;
}

void ZzColorPickerPrivate::syncDerivedState()
{
    const bool wasSyncing = syncing;
    syncing = true;
    const QSignalBlocker redBlocker(redSpinBox);
    const QSignalBlocker greenBlocker(greenSpinBox);
    const QSignalBlocker blueBlocker(blueSpinBox);
    const QSignalBlocker alphaBlocker(alphaSpinBox);
    const QSignalBlocker hexBlocker(hexEditor);
    const bool hsv = appearance == ZzColorPicker::Fluent && representation == ZzColorPicker::Hsva;
    redSpinBox->setRange(0, hsv ? 360 : 255);
    greenSpinBox->setRange(0, hsv ? 100 : 255);
    blueSpinBox->setRange(0, hsv ? 100 : 255);
    redSpinBox->setValue(currentColor.red());
    greenSpinBox->setValue(currentColor.green());
    blueSpinBox->setValue(currentColor.blue());
    alphaSpinBox->setValue(currentColor.alpha());
    hexEditor->setText(currentColor.name(
        alphaEnabled ? QColor::HexArgb : QColor::HexRgb).toUpper());

    const int row = paletteModel->rowForColor(currentColor);
    if (row >= 0) {
        paletteView->setCurrentIndex(paletteModel->index(row, 0));
    } else {
        paletteView->clearSelection();
        paletteView->setCurrentIndex({});
    }
    preview->update();
    paletteView->viewport()->update();
    syncFluentState();
    syncing = wasSyncing;
}

void ZzColorPickerPrivate::commitChannelEditor(ZzSpinBox *editor)
{
    if (syncing) {
        return;
    }
    if (editor == alphaSpinBox) {
        if (alphaEnabled) {
            QColor color = currentColor;
            color.setAlpha(editor->value());
            q_ptr->setCurrentColor(color);
        }
        return;
    }
    if (appearance == ZzColorPicker::Fluent && representation == ZzColorPicker::Hsva) {
        commitHsv(editor == redSpinBox ? editor->value() / 360.0 : hue,
                  editor == greenSpinBox ? editor->value() / 100.0 : saturation,
                  editor == blueSpinBox ? editor->value() / 100.0 : value);
        return;
    }
    QColor color = currentColor;
    if (editor == redSpinBox) {
        color.setRed(editor->value());
    } else if (editor == greenSpinBox) {
        color.setGreen(editor->value());
    } else if (editor == blueSpinBox) {
        color.setBlue(editor->value());
    }
    q_ptr->setCurrentColor(color);
}

void ZzColorPickerPrivate::commitHexEditor()
{
    if (syncing) {
        return;
    }
    const QString text = hexEditor->text();
    const int expectedLength = alphaEnabled ? 9 : 7;
    const QColor parsed = QColor::fromString(text);
    if (text.size() != expectedLength || !parsed.isValid()) {
        syncDerivedState();
        return;
    }
    QColor color = parsed;
    if (!alphaEnabled) {
        color.setAlpha(currentColor.alpha());
    }
    const QPointer<ZzColorPicker> guard(q_ptr);
    q_ptr->setCurrentColor(color);
    if (guard) {
        syncDerivedState();
    }
}

void ZzColorPickerPrivate::deferColorNotifications()
{
    if (notificationsDeferred) {
        return;
    }
    notificationsDeferred = true;
    QMetaObject::invokeMethod(q_ptr, [this] { flushColorNotifications(); }, Qt::QueuedConnection);
}

void ZzColorPickerPrivate::notifyCurrentColorChanged()
{
    const QColor snapshot = currentColor;
    if (notificationsDeferred) {
        pendingColorNotifications.append(snapshot);
        return;
    }
    Q_EMIT q_ptr->currentColorChanged(snapshot);
}

void ZzColorPickerPrivate::flushColorNotifications()
{
    if (hexFocusOutActive) {
        return;
    }
    notificationsDeferred = false;
    const auto notifications = std::exchange(pendingColorNotifications, QList<QColor>{});
    const QPointer<ZzColorPicker> guard(q_ptr);
    for (const QColor &snapshot : notifications) {
        Q_EMIT q_ptr->currentColorChanged(snapshot);
        if (!guard) {
            return;
        }
    }
}

void ZzColorPickerPrivate::syncAlphaPresentation()
{
    alphaLabel->setVisible(alphaEnabled);
    alphaSpinBox->setVisible(alphaEnabled);
    hexValidator->setRegularExpression(QRegularExpression(
        alphaEnabled
            ? QStringLiteral("^#[0-9A-Fa-f]{8}$")
            : QStringLiteral("^#[0-9A-Fa-f]{6}$")));
    syncDerivedState();
    syncVisibility();
}

void ZzColorPickerPrivate::refreshAccessibleText()
{
    redLabel->setText(ZzColorPicker::tr("红色"));
    greenLabel->setText(ZzColorPicker::tr("绿色"));
    blueLabel->setText(ZzColorPicker::tr("蓝色"));
    alphaLabel->setText(ZzColorPicker::tr("透明度"));
    hexLabel->setText(ZzColorPicker::tr("十六进制颜色"));
    paletteView->setAccessibleName(ZzColorPicker::tr("颜色色板"));
    preview->setAccessibleName(ZzColorPicker::tr("当前颜色预览"));
    redSpinBox->setAccessibleName(redLabel->text());
    greenSpinBox->setAccessibleName(greenLabel->text());
    blueSpinBox->setAccessibleName(blueLabel->text());
    alphaSpinBox->setAccessibleName(alphaLabel->text());
    hexEditor->setAccessibleName(hexLabel->text());
    syncFluentState();
}

void ZzColorPickerPrivate::refreshTheme()
{
    theme.refreshFallback();
    syncPaletteMetrics();
    preview->update();
    paletteView->viewport()->update();
    if (spectrum) {
        spectrum->update();
        syncFluentState();
    }
}

void ZzColorPickerPrivate::syncPaletteMetrics()
{
    // setFixedHeight/doItemsLayout 会再触发 resizeEvent，入口守卫防止同步重入
    if (syncingPaletteMetrics) {
        return;
    }
    syncingPaletteMetrics = true;
    const auto snapshot = theme.snapshot();
    const bool fluent = appearance == ZzColorPicker::Fluent;
    // QListView keeps a small flow inset even with a frameless viewport.
    const int gridExtent = fluent ? qMax(1, (paletteView->viewport()->width() - 2) / 8) : qMax(
        1,
        qCeil(snapshot->metric(ZzMetricToken::ColorSwatchExtent)
              + snapshot->metric(ZzMetricToken::ColorSwatchGap)));
    paletteView->setGridSize({gridExtent, gridExtent});
    paletteView->setFrameShape(fluent ? QFrame::NoFrame : QFrame::StyledPanel);
    paletteView->setVerticalScrollBarPolicy(fluent && paletteColorCount() <= 48
        ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    paletteView->setFixedHeight(
        (appearance == ZzColorPicker::Fluent ? 6 : ZzVisiblePaletteRows) * gridExtent
        + 2 * paletteView->frameWidth());
    paletteView->doItemsLayout();
    syncingPaletteMetrics = false;
}

} // namespace ZzFluentUI
