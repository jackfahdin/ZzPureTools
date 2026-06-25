#include "MainWindow.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <ZzPureTools/ZzPureTools.hpp>

#include "backend/ZzDemoViewModel.hpp"

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , m_viewModel(new ZzDemoViewModel(this))
{
    setWindowTitle(QStringLiteral("ZzPureTools Example"));
    resize(720, 420);
    buildUi();
    bindViewModel();
    bindTheme();
}

void MainWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(16);

    auto* titleLabel = new QLabel(QStringLiteral("ZzPureTools Widgets Demo"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_statusLabel = new QLabel(this);
    m_selectionLabel = new QLabel(this);

    m_accentButton = new ZzPushButton(QStringLiteral("Accent Button"), this);
    m_accentButton->setButtonStyle(ZzPushButton::ZzButtonStyle::Accent);

    m_standardButton = new ZzPushButton(QStringLiteral("Standard Button"), this);

    m_comboBox = new ZzComboBox(this);
    m_comboBox->addItems(m_viewModel->comboItems());

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_accentButton);
    buttonRow->addWidget(m_standardButton);
    buttonRow->addStretch();

    auto* themeRow = new QHBoxLayout();
    auto* lightButton = new ZzPushButton(QStringLiteral("Light"), this);
    auto* darkButton = new ZzPushButton(QStringLiteral("Dark"), this);
    auto* systemButton = new ZzPushButton(QStringLiteral("System"), this);
    themeRow->addWidget(new QLabel(QStringLiteral("Theme:"), this));
    themeRow->addWidget(lightButton);
    themeRow->addWidget(darkButton);
    themeRow->addWidget(systemButton);
    themeRow->addStretch();

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(m_statusLabel);
    rootLayout->addWidget(m_selectionLabel);
    rootLayout->addLayout(buttonRow);
    rootLayout->addWidget(new QLabel(QStringLiteral("ZzComboBox"), this));
    rootLayout->addWidget(m_comboBox);
    rootLayout->addLayout(themeRow);
    rootLayout->addStretch();

    connect(lightButton, &ZzPushButton::clicked, this, [this]() {
        m_viewModel->onThemeModeRequested(ZzThemeMode::Light);
    });
    connect(darkButton, &ZzPushButton::clicked, this, [this]() {
        m_viewModel->onThemeModeRequested(ZzThemeMode::Dark);
    });
    connect(systemButton, &ZzPushButton::clicked, this, [this]() {
        m_viewModel->onThemeModeRequested(ZzThemeMode::System);
    });
}

void MainWindow::bindViewModel()
{
    connect(m_accentButton, &ZzPushButton::clicked, m_viewModel, &ZzDemoViewModel::onAccentButtonClicked);
    connect(m_standardButton, &ZzPushButton::clicked, m_viewModel, &ZzDemoViewModel::onStandardButtonClicked);
    connect(
        m_comboBox,
        QOverload<int>::of(&ZzComboBox::currentIndexChanged),
        m_viewModel,
        &ZzDemoViewModel::onComboIndexChanged);

    connect(m_viewModel, &ZzDemoViewModel::statusMessageChanged, this, [this](const QString& message) {
        m_statusLabel->setText(QStringLiteral("Status: %1").arg(message));
    });
    connect(m_viewModel, &ZzDemoViewModel::selectedItemChanged, this, [this](const QString& item) {
        m_selectionLabel->setText(QStringLiteral("Selection: %1").arg(item));
    });
    connect(m_viewModel, &ZzDemoViewModel::themeModeApplyRequested, this, [](ZzThemeMode mode) {
        if (ZzTheme* theme = ZzApplication::instance().theme()) {
            theme->setMode(mode);
        }
    });

    m_statusLabel->setText(QStringLiteral("Status: %1").arg(m_viewModel->statusMessage()));
    m_selectionLabel->setText(QStringLiteral("Selection: %1").arg(m_viewModel->selectedItem()));
}

void MainWindow::bindTheme()
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        return;
    }

    auto applyTheme = [this, theme]() {
        ZzPalette::applyTo(this, *theme);
    };
    applyTheme();
    connect(theme, &ZzTheme::colorsChanged, this, applyTheme);
}
