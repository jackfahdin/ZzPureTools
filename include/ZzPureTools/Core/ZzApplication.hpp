#ifndef ZZPURETOOLS_CORE_ZZAPPLICATION_HPP
#define ZZPURETOOLS_CORE_ZZAPPLICATION_HPP

#include <QPointer>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"

class QApplication;
class ZzTheme;

class ZZ_PURE_TOOLS_EXPORT ZzApplication
{
public:
    [[nodiscard]] static ZzApplication& instance();

    void initialize(QApplication& application);
    [[nodiscard]] ZzTheme* theme() const;
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    ZzApplication() = default;

    QPointer<QApplication> m_application;
    ZzTheme* m_theme = nullptr;
};

#endif // ZZPURETOOLS_CORE_ZZAPPLICATION_HPP
