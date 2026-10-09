#include "ZzColorPickerDialogPrivate.h"
#include <QtWidgets/QLabel>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorPickerDialog.h>
#include <ZzFluentUI/ZzPushButton.h>

namespace ZzFluentUI {
ZzColorPickerDialogPrivate::ZzColorPickerDialogPrivate(ZzColorPickerDialog *q)
    : q_ptr(q), theme(q), picker(new ZzColorPicker(q)), titleLabel(new QLabel(q)),
      footer(new QWidget(q)), acceptButton(new ZzPushButton(footer)), cancelButton(new ZzPushButton(footer))
{
    q->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    auto *root = new QVBoxLayout(q);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *body = new QVBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    titleLabel->setTextFormat(Qt::PlainText);
    titleLabel->setWordWrap(true);
    QFont font = titleLabel->font();
    font.setPixelSize(20);
    font.setWeight(QFont::DemiBold);
    titleLabel->setFont(font);
    auto *heading = new QVBoxLayout;
    heading->setContentsMargins(16, 16, 16, 0);
    heading->addWidget(titleLabel);
    body->addLayout(heading);
    picker->setAppearance(ZzColorPicker::Fluent);
    picker->setAlphaEnabled(true);
    picker->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // The Fluent picker includes its own 12 px content padding. Compose that
    // public widget with 4 px outside padding to align with the 16 px heading.
    auto *pickerBody = new QVBoxLayout;
    pickerBody->setContentsMargins(4, 4, 4, 4);
    pickerBody->addWidget(picker);
    body->addLayout(pickerBody, 1);
    root->addLayout(body, 1);
    auto *buttons = new QHBoxLayout(footer);
    buttons->setContentsMargins(16, 16, 16, 16);
    buttons->setSpacing(8);
    acceptButton->setAppearance(ZzButtonAppearance::Accent);
    acceptButton->setDefault(true);
    for (auto *button : {acceptButton, cancelButton}) {
        button->setMinimumWidth(120);
        button->setFixedHeight(40);
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        buttons->addWidget(button, 1);
    }
    root->addWidget(footer);
    QObject::connect(q, &QWidget::windowTitleChanged, titleLabel, &QLabel::setText);
    QObject::connect(acceptButton, &QPushButton::clicked, q, &QDialog::accept);
    QObject::connect(cancelButton, &QPushButton::clicked, q, &QDialog::reject);
    initialColor = picker->currentColor();
    lastPreview = initialColor;
    QObject::connect(picker, &ZzColorPicker::currentColorChanged, q,
            [this](const QColor &color) {
                if (suppressPreview) return;
                const QColor snapshot = color;
                lastPreview = snapshot;
                Q_EMIT q_ptr->currentColorChanged(snapshot);
            });
    refreshText();
}
void ZzColorPickerDialogPrivate::refreshText()
{
    acceptButton->setText(ZzColorPickerDialog::tr("确定"));
    cancelButton->setText(ZzColorPickerDialog::tr("取消"));
    if (defaultTitle) q_ptr->setWindowTitle(ZzColorPickerDialog::tr("编辑颜色"));
}
} // namespace ZzFluentUI
