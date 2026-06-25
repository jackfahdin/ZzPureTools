#ifndef ZZAPPLICATION_H
#define ZZAPPLICATION_H

#include <QPointer>

#include "ZzFluentCoreGlobal.h"

class QApplication;
class ZzTheme;

class ZZ_CORE_EXPORT ZzApplication
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

#endif // ZZAPPLICATION_H
