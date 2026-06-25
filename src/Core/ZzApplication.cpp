#include "ZzPureTools/Core/ZzApplication.hpp"

#include <QApplication>

#include "ZzPureTools/Core/ZzTheme.hpp"

ZzApplication& ZzApplication::instance()
{
    static ZzApplication application;
    return application;
}

void ZzApplication::initialize(QApplication& application)
{
    if (m_theme != nullptr) {
        return;
    }

    m_application = &application;
    m_theme = new ZzTheme(&application);
}

ZzTheme* ZzApplication::theme() const
{
    return m_theme;
}

bool ZzApplication::isInitialized() const noexcept
{
    return m_theme != nullptr;
}
