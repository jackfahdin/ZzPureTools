#ifndef ZZFLUENT_CORE_ZZAPPLICATION_HPP
#define ZZFLUENT_CORE_ZZAPPLICATION_HPP

#include <QPointer>

#include "ZzFluent/Core/ZzFluentGlobal.hpp"

class QApplication;
class ZzTheme;

class ZZ_FLUENT_EXPORT ZzApplication
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

#endif // ZZFLUENT_CORE_ZZAPPLICATION_HPP
