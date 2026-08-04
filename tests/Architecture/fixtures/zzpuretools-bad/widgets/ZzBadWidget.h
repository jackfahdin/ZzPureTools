#pragma once

#include <Business/Repository.h>
#include <QtWidgets/private/qwidget_p.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzWindowKit/ZzWindowAgent.h>

namespace ZzPureTools {

/** @brief Bad presentation fixture. */
class ZzBadWidget final
{
public:
    /** @brief Reads a repository through a QWK-backed widget. */
    void refresh();
};

} // namespace ZzPureTools
