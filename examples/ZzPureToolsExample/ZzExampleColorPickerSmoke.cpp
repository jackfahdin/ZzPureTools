#include "ZzExampleColorPickerSmoke.h"
#include <QtCore/QMetaObject>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorPickerButton.h>
#include <ZzFluentUI/ZzColorPickerDialog.h>

namespace ZzExample {
bool zzColorPickerPageReady(const QWidget &window)
{
    using namespace ZzFluentUI;
    auto *page = window.findChild<QWidget *>(QStringLiteral("colorPickerContent"));
    if (!page && window.objectName() == QStringLiteral("colorPickerContent"))
        page = const_cast<QWidget *>(&window);
    if (!page) return false;
    auto *picker = page->findChild<ZzColorPicker *>(QStringLiteral("colorPickerInline"));
    auto *hex = page->findChild<QLineEdit *>(QStringLiteral("colorPickerColorInput"));
    auto *shape = page->findChild<QComboBox *>(QStringLiteral("colorPickerShape"));
    auto *representation = page->findChild<QComboBox *>(QStringLiteral("colorPickerRepresentation"));
    auto *palette = page->findChild<QComboBox *>(QStringLiteral("colorPickerPalette"));
    auto *button = page->findChild<ZzColorPickerButton *>(QStringLiteral("colorPickerButton"));
    auto *open = page->findChild<QPushButton *>(QStringLiteral("colorPickerOpenDialog"));
    auto *dialog = page->findChild<ZzColorPickerDialog *>(QStringLiteral("colorPickerDialog"));
    auto *selected = page->findChild<QLabel *>(QStringLiteral("colorPickerDialogValue"));
    auto *reset = page->findChild<QPushButton *>(QStringLiteral("colorPickerReset"));
    if (!picker || !hex || !shape || !representation || !palette || !button || !open
        || !dialog || !selected || !reset) return false;
    if (qEnvironmentVariableIntValue("ZZ_PURETOOLS_EXAMPLE_EXPECT_ENGLISH") == 1) {
        const auto *tabs = picker->findChild<QWidget *>(QStringLiteral("zzColorPickerTabs"));
        if (open->text() != QStringLiteral("Open color dialog")
            || button->accessibleName() != QStringLiteral("Choose color")
            || dialog->title() != QStringLiteral("Edit color") || !tabs
            || tabs->accessibleName() != QStringLiteral("Color editing mode")) return false;
        const auto buttons = dialog->findChildren<QPushButton *>();
        bool ok = false;
        bool cancel = false;
        for (const auto *action : buttons) {
            ok = ok || action->text() == QStringLiteral("OK");
            cancel = cancel || action->text() == QStringLiteral("Cancel");
        }
        if (!ok || !cancel) return false;
    }
    hex->setText(QStringLiteral("#804080C0"));
    QMetaObject::invokeMethod(hex, "editingFinished", Qt::DirectConnection);
    if (picker->currentColor() != QColor("#804080C0")) return false;
    shape->setCurrentIndex(1);
    representation->setCurrentIndex(1);
    palette->setCurrentIndex(1);
    if (picker->colorSpectrumShape() != ZzColorPicker::Ring
        || picker->colorRepresentation() != ZzColorPicker::Hsva
        || picker->paletteColorCount() != 6) return false;
    for (const auto *name : {"colorPickerAlpha", "colorPickerSpectrumVisible", "colorPickerPaletteVisible",
             "colorPickerPreviewVisible", "colorPickerAlphaVisible", "colorPickerSliderVisible", "colorPickerInputsVisible"}) {
        auto *box = page->findChild<QCheckBox *>(QString::fromLatin1(name));
        if (!box) return false;
        box->click();
    }
    if (picker->isAlphaEnabled() || picker->isColorSpectrumVisible() || picker->isColorPaletteVisible()
        || picker->isColorPreviewVisible() || picker->isAlphaSliderVisible()
        || picker->isColorSliderVisible() || picker->isColorChannelTextInputVisible()) return false;
    auto *disabled = page->findChild<QCheckBox *>(QStringLiteral("colorPickerDisabled"));
    auto *rtl = page->findChild<QCheckBox *>(QStringLiteral("colorPickerRtl"));
    if (!disabled || !rtl) return false;
    disabled->click();
    rtl->click();
    if (picker->isEnabled() || picker->layoutDirection() != Qt::RightToLeft) return false;
    button->click();
    if (!button->colorPicker()->window()->isVisible()) return false;
    auto *buttonHex = button->colorPicker()->findChild<QLineEdit *>(QStringLiteral("zzHexColorEditor"));
    if (!buttonHex) return false;
    buttonHex->setText(QStringLiteral("#FF10893E"));
    QMetaObject::invokeMethod(buttonHex, "editingFinished", Qt::DirectConnection);
    button->colorPicker()->commitPendingEdits();
    if (button->selectedColor() != QColor("#10893E")) return false;
    button->click();
    if (button->colorPicker()->window()->isVisible()) return false;
    open->click();
    if (!dialog->isVisible()) return false;
    auto *dialogHex = dialog->colorPicker()->findChild<QLineEdit *>(QStringLiteral("zzHexColorEditor"));
    if (!dialogHex) return false;
    dialogHex->setText(QStringLiteral("#FF944E9B"));
    QMetaObject::invokeMethod(dialogHex, "editingFinished", Qt::DirectConnection);
    dialog->reject();
    if (dialog->currentColor() != QColor("#0078D4") || selected->text() != QStringLiteral("#FF0078D4"))
        return false;
    open->click();
    dialogHex->setText(QStringLiteral("#C0FFB900"));
    dialog->accept();
    if (dialog->currentColor() != QColor("#C0FFB900") || selected->text() != QStringLiteral("#C0FFB900"))
        return false;
    reset->click();
    return picker->currentColor() == QColor("#944E9B") && picker->isAlphaEnabled()
        && picker->isColorSpectrumVisible() && picker->isColorPaletteVisible()
        && picker->isColorPreviewVisible() && picker->isAlphaSliderVisible()
        && picker->isColorSliderVisible() && picker->isColorChannelTextInputVisible()
        && picker->colorSpectrumShape() == ZzColorPicker::Box
        && picker->colorRepresentation() == ZzColorPicker::Rgba && picker->paletteColorCount() == 48
        && picker->isEnabled() && picker->layoutDirection() == Qt::LeftToRight
        && button->selectedColor() == QColor("#FFB900") && dialog->currentColor() == QColor("#0078D4");
}
} // namespace ZzExample
