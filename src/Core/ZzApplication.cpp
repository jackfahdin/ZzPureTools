#include "ZzPureTools/Core/ZzApplication.hpp"

#include "ZzPureTools/Core/ZzTheme.hpp"

#include <QApplication>

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
