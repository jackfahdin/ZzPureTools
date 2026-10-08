#include "ZzExampleShowcasePagePrivate.h"

#include <QCoreApplication>
#include <QPainter>
#include <QScreen>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <QToolTip>
#include <ZzFluentUI/ZzBorderBeam.h>
#include <ZzFluentUI/ZzBorderBeamButton.h>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <functional>

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ZzExample {
using ZzFluentUI::ZzBorderBeam;
using ZzFluentUI::ZzBorderBeamButton;
namespace {
QString zzBeamText(const char *text) { return QCoreApplication::translate("ZzPureToolsExample", text); }

/** @brief 复用现有颜色编辑器，使用参考页的紧凑色块按钮。 */
class ZzBeamColorButton final : public QToolButton
{
    Q_OBJECT
public:
    explicit ZzBeamColorButton(QWidget *parent)
        : QToolButton(parent)
    {
        setFixedSize(68, 32);
        QObject::connect(this, &QToolButton::clicked, this, [this] {
            if (!popup_) {
                popup_ = new QWidget(this, Qt::Popup);
                auto *layout = new QVBoxLayout(popup_);
                picker_ = new ZzFluentUI::ZzColorPicker(popup_);
                picker_->setAlphaEnabled(true);
                layout->addWidget(picker_);
                QObject::connect(picker_, &ZzFluentUI::ZzColorPicker::currentColorChanged, this,
                    &ZzBeamColorButton::setSelectedColor);
            }
            {
                const QSignalBlocker blocker(picker_);
                picker_->setCurrentColor(selectedColor_);
            }
            popup_->adjustSize();
            const QRect available = screen()->availableGeometry();
            QPoint position = mapToGlobal(QPoint(0, height()));
            position.setX(
                qMax(available.left(), qMin(position.x(), available.right() - popup_->width() + 1)));
            if (position.y() + popup_->height() > available.bottom())
                position.setY(qMax(available.top(), mapToGlobal(QPoint()).y() - popup_->height()));
            popup_->move(position);
            popup_->show();
        });
    }
    void setSelectedColor(const QColor &color)
    {
        if (selectedColor_ == color)
            return;
        selectedColor_ = color;
        setToolTip(color.name(QColor::HexArgb));
        update();
        Q_EMIT selectedColorChanged(color);
    }
    std::function<QColor()> themeColor;
Q_SIGNALS:
    void selectedColorChanged(const QColor &color);

protected:
    void changeEvent(QEvent *event) override
    {
        QToolButton::changeEvent(event);
        if (themeColor && (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)) {
            const QSignalBlocker blocker(this);
            setSelectedColor(themeColor());
        }
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QStyleOptionToolButton option;
        initStyleOption(&option);
        style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, this);
        const int arrowWidth = style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &option, this);
        const QRect arrow(rect().right() - arrowWidth - 2, 0, arrowWidth, height());
        QRect swatch = rect().adjusted(8, 5, -8, -5);
        swatch.setRight(arrow.left() - 5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(selectedColor_);
        painter.drawRoundedRect(swatch, 3, 3);
        painter.setFont(ZzFluentUI::ZzSegoeIconFont::font(11));
        painter.setPen(
            palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
        painter.drawText(arrow.translated(0, isDown() ? 2 : 0), Qt::AlignCenter, QChar(0xe70d));
    }

private:
    QColor selectedColor_;
    QWidget *popup_ = nullptr;
    ZzFluentUI::ZzColorPicker *picker_ = nullptr;
};

class ZzBeamCard final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const auto *fluent = qobject_cast<const ZzFluentUI::ZzFluentStyle *>(style());
        const bool hc = fluent && fluent->themeSnapshot()->mode() == ZzFluentUI::ZzThemeMode::HighContrast;
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(hc ? palette().color(QPalette::WindowText) : QColor(dark ? "#252525" : "#e9e9e9"));
        QColor base = palette().color(QPalette::Base);
        if (!base.alpha())
            base = palette().color(QPalette::Window);
        const qreal alpha = (dark ? 13.0 : 179.0) / 255.0;
        painter.setBrush(hc ? base
                            : QColor(qRound(base.red() * (1 - alpha) + 255 * alpha),
                                  qRound(base.green() * (1 - alpha) + 255 * alpha),
                                  qRound(base.blue() * (1 - alpha) + 255 * alpha)));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
};

QWidget *makeCard(QWidget *parent)
{
    auto *card = new ZzBeamCard(parent);
    card->setProperty("isCard", true);
    card->setAttribute(Qt::WA_StyledBackground, true);
    return card;
}

QLabel *makeSectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    font.setPixelSize(14);
    label->setFont(font);
    return label;
}

/** @brief 在示例中将整数滑块刻度转换为光束的实际小数属性。 */
class ZzBeamPropertySlider final : public QSlider
{
public:
    explicit ZzBeamPropertySlider(QWidget *parent)
        : QSlider(Qt::Horizontal, parent)
    {
    }

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QSlider::event(event);
        const auto type = event->type();
        if (isEnabled()
            && (type == QEvent::KeyPress || type == QEvent::Wheel
                || ((type == QEvent::MouseMove || type == QEvent::MouseButtonPress) && isSliderDown()))) {
            const int scale = qMax(1, property("scale").toInt());
            const int precision = property("precision").toInt();
            QToolTip::showText(mapToGlobal(QPoint(width() / 2, 0)),
                QString::number(qreal(value()) / scale, 'f', precision), this);
        } else if (type == QEvent::MouseButtonRelease || type == QEvent::Hide || type == QEvent::FocusOut) {
            QToolTip::hideText();
        }
        return handled;
    }
};

QSlider *makeSlider(QWidget *parent, int minimum, int maximum, int value)
{
    auto *slider = new ZzBeamPropertySlider(parent);
    slider->setRange(minimum, maximum);
    slider->setValue(value);
    slider->setTracking(true);
    return slider;
}

ZzBorderBeam *makeBeamSample(QWidget *parent, const QString &title, const QString &description)
{
    auto *beam = new ZzBorderBeam(parent);
    beam->setCornerRadius(14.0);
    beam->setMinimumSize(240, 150);

    auto *layout = new QVBoxLayout(beam);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(8);
    layout->addStretch();

    auto *titleLabel = new QLabel(title, beam);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPixelSize(15);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto *descriptionLabel = new QLabel(description, beam);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);
    layout->addStretch();
    return beam;
}

} // namespace

void ZzExampleShowcasePagePrivate::buildBorderBeam(QVBoxLayout *mainLayout, QWidget *content)
{
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);
    auto *description = new QLabel(zzBeamText("沿圆角边框真实周长匀速移动的渐变光束。控件继承 "
                                              "QFrame，可直接通过 Qt 布局承载任意内容。"),
        content);
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    auto *examplesCard = makeCard(content);
    auto *examplesLayout = new QVBoxLayout(examplesCard);
    examplesLayout->setContentsMargins(16, 16, 16, 16);
    examplesLayout->setSpacing(12);
    examplesLayout->addWidget(makeSectionTitle(zzBeamText("常用组合"), examplesCard));

    auto *samplesLayout = new QHBoxLayout;
    samplesLayout->setSpacing(16);

    auto *adaptiveBeam = makeBeamSample(examplesCard, zzBeamText("主题自适应"),
        zzBeamText("颜色留空时自动使用当前浅色或深色主题的默认配色。"));
    adaptiveBeam->setBeamLength(72.0);
    adaptiveBeam->setAnimationDuration(5200);
    samplesLayout->addWidget(adaptiveBeam, 1);

    auto *doubleBeam = makeBeamSample(
        examplesCard, zzBeamText("双色双光束"), zzBeamText("多个光束由一个控件统一绘制并沿路径均匀分布。"));
    doubleBeam->setStartColor(QColor(QStringLiteral("#FFAA40")));
    doubleBeam->setEndColor(QColor(QStringLiteral("#9C40FF")));
    doubleBeam->setBeamLength(92.0);
    doubleBeam->setBeamWidth(2.5);
    doubleBeam->setBeamCount(2);
    doubleBeam->setAnimationDuration(7000);
    samplesLayout->addWidget(doubleBeam, 1);

    auto *reverseBeam
        = makeBeamSample(examplesCard, zzBeamText("反向运动"), zzBeamText("切换方向不会改变当前光束位置。"));
    reverseBeam->setStartColor(QColor(QStringLiteral("#22D3EE")));
    reverseBeam->setEndColor(QColor(QStringLiteral("#3B82F6")));
    reverseBeam->setDirection(ZzBorderBeam::CounterClockwise);
    reverseBeam->setBeamLength(54.0);
    reverseBeam->setAnimationDuration(3600);
    samplesLayout->addWidget(reverseBeam, 1);

    examplesLayout->addLayout(samplesLayout);

    auto *beamButton = new ZzBorderBeamButton(zzBeamText("Border Beam Button"), examplesCard);
    beamButton->setStartColor(QColor(QStringLiteral("#22D3EE")));
    beamButton->setEndColor(QColor(QStringLiteral("#8B5CF6")));
    beamButton->setBeamLength(70.0);
    beamButton->setAnimationDuration(3600);
    beamButton->setMinimumWidth(220);
    examplesLayout->addWidget(beamButton, 0, Qt::AlignHCenter);
    mainLayout->addWidget(examplesCard);

    auto *propertiesCard = makeCard(content);
    auto *propertiesLayout = new QVBoxLayout(propertiesCard);
    propertiesLayout->setContentsMargins(16, 16, 16, 16);
    propertiesLayout->setSpacing(12);
    propertiesLayout->addWidget(makeSectionTitle(zzBeamText("实时属性"), propertiesCard));

    auto *editorLayout = new QHBoxLayout;
    editorLayout->setSpacing(24);

    auto *preview = new ZzBorderBeam(propertiesCard);
    preview->setObjectName(QStringLiteral("zzBorderBeamPreview"));
    preview->setMinimumSize(400, 250);
    preview->setCornerRadius(16.0);
    preview->setBeamLength(90.0);
    preview->setBeamWidth(2.5);
    preview->setAnimationDuration(5000);

    auto *previewContent = new QVBoxLayout(preview);
    previewContent->setContentsMargins(28, 28, 28, 28);
    previewContent->addStretch();
    auto *previewTitle = new QLabel(zzBeamText("Qt 原生容器"), preview);
    QFont previewTitleFont = previewTitle->font();
    previewTitleFont.setBold(true);
    previewTitleFont.setPixelSize(20);
    previewTitle->setFont(previewTitleFont);
    previewContent->addWidget(previewTitle, 0, Qt::AlignHCenter);
    auto *previewText = new QLabel(zzBeamText("内容仍由普通布局和子控件组成"), preview);
    previewText->setAlignment(Qt::AlignCenter);
    previewContent->addWidget(previewText);
    auto *actionButton = new QPushButton(zzBeamText("示例按钮"), preview);
    previewContent->addWidget(actionButton, 0, Qt::AlignHCenter);
    previewContent->addStretch();
    editorLayout->addWidget(preview, 1, Qt::AlignTop);

    auto *editor = new QWidget(propertiesCard);
    auto *form = new QFormLayout(editor);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    auto *lengthSlider = makeSlider(editor, 10, 300, qRound(preview->beamLength()));
    lengthSlider->setObjectName(QStringLiteral("zzBeam_lengthSlider"));
    auto *widthSlider = makeSlider(editor, 1, 16, qRound(preview->beamWidth() * 2.0));
    widthSlider->setObjectName(QStringLiteral("zzBeam_widthSlider"));
    widthSlider->setProperty("scale", 2);
    widthSlider->setProperty("precision", 1);
    auto *radiusSlider = makeSlider(editor, 0, 48, qRound(preview->cornerRadius()));
    radiusSlider->setObjectName(QStringLiteral("zzBeam_radiusSlider"));
    auto *durationSlider = makeSlider(editor, 500, 12000, preview->animationDuration());
    durationSlider->setObjectName(QStringLiteral("zzBeam_durationSlider"));
    durationSlider->setSingleStep(100);
    durationSlider->setPageStep(500);
    auto *progressSlider = makeSlider(editor, 0, 100, qRound(preview->initialProgress() * 100.0));
    progressSlider->setObjectName(QStringLiteral("zzBeam_progressSlider"));
    progressSlider->setProperty("scale", 100);
    progressSlider->setProperty("precision", 2);

    auto *countSpinBox = new QSpinBox(editor);
    countSpinBox->setObjectName(QStringLiteral("zzBeam_countSpinBox"));
    countSpinBox->setRange(1, 8);
    countSpinBox->setValue(preview->beamCount());

    auto *directionCombo = new QComboBox(editor);
    directionCombo->setObjectName(QStringLiteral("zzBeam_directionCombo"));
    directionCombo->addItem(zzBeamText("顺时针"), ZzBorderBeam::Clockwise);
    directionCombo->addItem(zzBeamText("逆时针"), ZzBorderBeam::CounterClockwise);

    auto *themeCombo = new QComboBox(editor);
    themeCombo->setObjectName(QStringLiteral("zzBeam_themeCombo"));
    themeCombo->addItem(zzBeamText("跟随应用"), ZzBorderBeam::AutoTheme);
    themeCombo->addItem(zzBeamText("Light"), ZzBorderBeam::LightTheme);
    themeCombo->addItem(zzBeamText("Dark"), ZzBorderBeam::DarkTheme);

    auto *startColorButton = new ZzBeamColorButton(editor);
    startColorButton->setSelectedColor(preview->activeTheme().startColor);
    auto *endColorButton = new ZzBeamColorButton(editor);
    endColorButton->setSelectedColor(preview->activeTheme().endColor);
    auto *backgroundColorButton = new ZzBeamColorButton(editor);
    backgroundColorButton->setSelectedColor(preview->activeTheme().backgroundColor);
    auto *borderColorButton = new ZzBeamColorButton(editor);
    borderColorButton->setSelectedColor(preview->activeTheme().borderColor);

    auto *animationCheck = new QCheckBox(zzBeamText("播放动画"), editor);
    animationCheck->setObjectName(QStringLiteral("zzBeam_animationCheck"));
    animationCheck->setChecked(preview->isAnimationEnabled());

    form->addRow(zzBeamText("光束长度"), lengthSlider);
    form->addRow(zzBeamText("光束线宽"), widthSlider);
    form->addRow(zzBeamText("圆角半径"), radiusSlider);
    form->addRow(zzBeamText("动画周期"), durationSlider);
    form->addRow(zzBeamText("初始位置"), progressSlider);
    form->addRow(zzBeamText("光束数量"), countSpinBox);
    form->addRow(zzBeamText("运动方向"), directionCombo);
    form->addRow(zzBeamText("主题模式"), themeCombo);
    for (auto *button : { startColorButton, endColorButton, backgroundColorButton, borderColorButton })
        button->setAccessibleName(zzBeamText("选择颜色"));
    startColorButton->themeColor = [preview] {
        return preview->startColor().isValid() ? preview->startColor() : preview->activeTheme().startColor;
    };
    endColorButton->themeColor = [preview] {
        return preview->endColor().isValid() ? preview->endColor() : preview->activeTheme().endColor;
    };
    backgroundColorButton->themeColor = [preview] {
        return preview->backgroundColor().isValid() ? preview->backgroundColor()
                                                    : preview->activeTheme().backgroundColor;
    };
    borderColorButton->themeColor = [preview] {
        return preview->borderColor().isValid() ? preview->borderColor() : preview->activeTheme().borderColor;
    };
    form->addRow(zzBeamText("起始颜色"), startColorButton);
    form->addRow(zzBeamText("结束颜色"), endColorButton);
    form->addRow(zzBeamText("背景颜色"), backgroundColorButton);
    form->addRow(zzBeamText("边框颜色"), borderColorButton);
    form->addRow(animationCheck);

    auto *resetButton = new QPushButton(zzBeamText("恢复默认属性"), editor);
    resetButton->setObjectName(QStringLiteral("zzBorderBeamReset"));
    form->addRow(resetButton);
    editorLayout->addWidget(editor, 1);
    propertiesLayout->addLayout(editorLayout);
    mainLayout->addWidget(propertiesCard);
    mainLayout->addStretch();

    QObject::connect(lengthSlider, &QSlider::valueChanged, preview,
        [preview](int value) { preview->setBeamLength(value); });
    QObject::connect(widthSlider, &QSlider::valueChanged, preview,
        [preview](int value) { preview->setBeamWidth(value / 2.0); });
    QObject::connect(radiusSlider, &QSlider::valueChanged, preview,
        [preview](int value) { preview->setCornerRadius(value); });
    QObject::connect(durationSlider, &QSlider::valueChanged, preview, &ZzBorderBeam::setAnimationDuration);
    QObject::connect(progressSlider, &QSlider::valueChanged, preview,
        [preview](int value) { preview->setInitialProgress(value / 100.0); });
    QObject::connect(
        countSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), preview, &ZzBorderBeam::setBeamCount);
    QObject::connect(directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), preview,
        [preview, directionCombo](int index) {
            preview->setDirection(
                static_cast<ZzBorderBeam::Direction>(directionCombo->itemData(index).toInt()));
        });
    QObject::connect(
        themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), preview, [=](int index) {
            preview->setThemeMode(static_cast<ZzBorderBeam::ThemeMode>(themeCombo->itemData(index).toInt()));
            const ZzBorderBeam::ThemeConfig theme = preview->activeTheme();
            if (!preview->startColor().isValid()) {
                const QSignalBlocker blocker(startColorButton);
                startColorButton->setSelectedColor(theme.startColor);
            }
            if (!preview->endColor().isValid()) {
                const QSignalBlocker blocker(endColorButton);
                endColorButton->setSelectedColor(theme.endColor);
            }
            if (!preview->backgroundColor().isValid()) {
                const QSignalBlocker blocker(backgroundColorButton);
                backgroundColorButton->setSelectedColor(theme.backgroundColor);
            }
            if (!preview->borderColor().isValid()) {
                const QSignalBlocker blocker(borderColorButton);
                borderColorButton->setSelectedColor(theme.borderColor);
            }
        });
    QObject::connect(
        startColorButton, &ZzBeamColorButton::selectedColorChanged, preview, &ZzBorderBeam::setStartColor);
    QObject::connect(
        endColorButton, &ZzBeamColorButton::selectedColorChanged, preview, &ZzBorderBeam::setEndColor);
    QObject::connect(backgroundColorButton, &ZzBeamColorButton::selectedColorChanged, preview,
        &ZzBorderBeam::setBackgroundColor);
    QObject::connect(
        borderColorButton, &ZzBeamColorButton::selectedColorChanged, preview, &ZzBorderBeam::setBorderColor);
    QObject::connect(animationCheck, &QCheckBox::toggled, preview, &ZzBorderBeam::setAnimationEnabled);

    QObject::connect(resetButton, &QPushButton::clicked, preview, [=] {
        lengthSlider->setValue(60);
        widthSlider->setValue(4);
        radiusSlider->setValue(8);
        durationSlider->setValue(6000);
        progressSlider->setValue(0);
        countSpinBox->setValue(1);
        directionCombo->setCurrentIndex(directionCombo->findData(ZzBorderBeam::Clockwise));
        themeCombo->setCurrentIndex(themeCombo->findData(ZzBorderBeam::AutoTheme));
        preview->setStartColor(QColor());
        preview->setEndColor(QColor());
        preview->setBackgroundColor(QColor());
        preview->setBorderColor(QColor());
        const ZzBorderBeam::ThemeConfig theme = preview->activeTheme();
        const QSignalBlocker startBlocker(startColorButton);
        const QSignalBlocker endBlocker(endColorButton);
        const QSignalBlocker backgroundBlocker(backgroundColorButton);
        const QSignalBlocker borderBlocker(borderColorButton);
        startColorButton->setSelectedColor(theme.startColor);
        endColorButton->setSelectedColor(theme.endColor);
        backgroundColorButton->setSelectedColor(theme.backgroundColor);
        borderColorButton->setSelectedColor(theme.borderColor);
        animationCheck->setChecked(true);
        preview->restartAnimation();
    });
}

} // namespace ZzExample

#include "ZzExampleBorderBeamPage.moc"
