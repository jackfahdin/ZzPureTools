#include "MainWindow.hpp"

#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <ZzPureTools/ZzPureTools.hpp>

#include "backend/ZzDemoViewModel.hpp"

MainWindow::MainWindow(QWidget* parent) : QWidget(parent), m_viewModel(new ZzDemoViewModel(this))
{
    setWindowTitle(QStringLiteral("ZzPureTools Example"));
    resize(800, 520);
    buildUi();
    bindViewModel();
    bindTheme();
}

void MainWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(32, 32, 32, 32);
    rootLayout->setSpacing(24);

    // 标题
    auto* titleLabel = new QLabel(QStringLiteral("ZzPureTools"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(QStringLiteral("Fluent UI Widgets for Qt 6"), this);
    QFont subtitleFont = subtitleLabel->font();
    subtitleFont.setPointSize(11);
    subtitleLabel->setFont(subtitleFont);

    // 状态栏
    auto* statusGroup = createSection(QStringLiteral("Status"));
    auto* statusLayout = new QGridLayout(statusGroup);
    m_statusLabel = new QLabel(QStringLiteral("Ready"), this);
    m_selectionLabel = new QLabel(QStringLiteral("Selection: Fluent UI"), this);
    m_themeLabel = new QLabel(QStringLiteral("Theme: System"), this);
    m_clickCountLabel = new QLabel(QStringLiteral("Clicks: 0"), this);
    statusLayout->addWidget(m_statusLabel, 0, 0);
    statusLayout->addWidget(m_selectionLabel, 0, 1);
    statusLayout->addWidget(m_themeLabel, 1, 0);
    statusLayout->addWidget(m_clickCountLabel, 1, 1);

    // 按钮展示
    auto* buttonGroup = createSection(QStringLiteral("Buttons"));
    auto* buttonLayout = new QHBoxLayout(buttonGroup);
    buttonLayout->setSpacing(16);
    m_accentButton = new ZzPushButton(QStringLiteral("Accent Button"), this);
    m_accentButton->setButtonStyle(ZzPushButton::ZzButtonStyle::Accent);
    m_accentButton->setMinimumWidth(140);
    m_standardButton = new ZzPushButton(QStringLiteral("Standard Button"), this);
    m_standardButton->setMinimumWidth(140);
    buttonLayout->addWidget(m_accentButton);
    buttonLayout->addWidget(m_standardButton);
    buttonLayout->addStretch();

    // 下拉框展示
    auto* comboGroup = createSection(QStringLiteral("Selection"));
    auto* comboLayout = new QHBoxLayout(comboGroup);
    comboLayout->setSpacing(16);
    m_comboBox = new ZzComboBox(this);
    m_comboBox->addItems(m_viewModel->comboItems());
    m_comboBox->setMinimumWidth(240);
    comboLayout->addWidget(new QLabel(QStringLiteral("ZzComboBox:"), this));
    comboLayout->addWidget(m_comboBox);
    comboLayout->addStretch();

    // 主题切换
    auto* themeGroup = createSection(QStringLiteral("Theme"));
    auto* themeLayout = new QHBoxLayout(themeGroup);
    themeLayout->setSpacing(16);
    auto* lightButton = new ZzPushButton(QStringLiteral("Light"), this);
    auto* darkButton = new ZzPushButton(QStringLiteral("Dark"), this);
    auto* systemButton = new ZzPushButton(QStringLiteral("System"), this);
    lightButton->setMinimumWidth(100);
    darkButton->setMinimumWidth(100);
    systemButton->setMinimumWidth(100);
    themeLayout->addWidget(lightButton);
    themeLayout->addWidget(darkButton);
    themeLayout->addWidget(systemButton);
    themeLayout->addStretch();

    connect(lightButton, &ZzPushButton::clicked, this,
            [this]() { m_viewModel->onThemeModeRequested(ZzThemeMode::Light); });
    connect(darkButton, &ZzPushButton::clicked, this,
            [this]() { m_viewModel->onThemeModeRequested(ZzThemeMode::Dark); });
    connect(systemButton, &ZzPushButton::clicked, this,
            [this]() { m_viewModel->onThemeModeRequested(ZzThemeMode::System); });

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(subtitleLabel);
    rootLayout->addWidget(statusGroup);
    rootLayout->addWidget(buttonGroup);
    rootLayout->addWidget(comboGroup);
    rootLayout->addWidget(themeGroup);
    rootLayout->addStretch();
}

void MainWindow::bindViewModel()
{
    connect(m_accentButton, &ZzPushButton::clicked, m_viewModel,
            &ZzDemoViewModel::onAccentButtonClicked);
    connect(m_standardButton, &ZzPushButton::clicked, m_viewModel,
            &ZzDemoViewModel::onStandardButtonClicked);
    connect(m_comboBox, QOverload<int>::of(&ZzComboBox::currentIndexChanged), m_viewModel,
            &ZzDemoViewModel::onComboIndexChanged);

    connect(m_viewModel, &ZzDemoViewModel::statusMessageChanged, this,
            [this](const QString& message) { m_statusLabel->setText(message); });
    connect(m_viewModel, &ZzDemoViewModel::selectedItemChanged, this, [this](const QString& item) {
        m_selectionLabel->setText(QStringLiteral("Selection: %1").arg(item));
    });
    connect(m_viewModel, &ZzDemoViewModel::clickCountChanged, this, [this](int count) {
        m_clickCountLabel->setText(QStringLiteral("Clicks: %1").arg(count));
    });
    connect(m_viewModel, &ZzDemoViewModel::themeModeApplyRequested, this, [](ZzThemeMode mode) {
        if (ZzTheme* theme = ZzApplication::instance().theme()) {
            theme->setMode(mode);
        }
    });

    m_statusLabel->setText(m_viewModel->statusMessage());
    m_selectionLabel->setText(QStringLiteral("Selection: %1").arg(m_viewModel->selectedItem()));
    m_clickCountLabel->setText(QStringLiteral("Clicks: %1").arg(m_viewModel->clickCount()));
}

void MainWindow::bindTheme()
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        return;
    }

    auto updateThemeLabel = [this, theme]() {
        const QString modeText = [theme]() {
            switch (theme->effectiveMode()) {
                case ZzThemeMode::Light:
                    return QStringLiteral("Light");
                case ZzThemeMode::Dark:
                    return QStringLiteral("Dark");
                case ZzThemeMode::System:
                default:
                    return QStringLiteral("System");
            }
        }();
        m_themeLabel->setText(QStringLiteral("Theme: %1").arg(modeText));
    };

    auto applyTheme = [this, theme, updateThemeLabel]() {
        ZzPalette::applyTo(this, *theme);
        updateThemeLabel();
    };

    applyTheme();
    connect(theme, &ZzTheme::colorsChanged, this, applyTheme);
}

QGroupBox* MainWindow::createSection(const QString& title)
{
    auto* group = new QGroupBox(title, this);
    group->setFlat(false);
    return group;
}
