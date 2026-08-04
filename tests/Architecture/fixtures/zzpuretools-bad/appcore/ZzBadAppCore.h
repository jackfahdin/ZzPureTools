#pragma once

#include <QtGui/QGuiApplication>

namespace Sample::Bad {

/** @brief Bad AppCore fixture. */
class ZzBadAppCore final
{
public:
    /** @brief Returns a GUI object. */
    [[nodiscard]] QGuiApplication *application() const;
};

} // namespace Sample::Bad
