#ifndef ZZPURETOOLS_CORE_ZZAPPLICATION_HPP
#define ZZPURETOOLS_CORE_ZZAPPLICATION_HPP

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"

#include <QPointer>

class QApplication;
class ZzTheme;

/**
 * @brief ZzPureTools 应用入口类。
 *
 * 负责初始化 ZzPureTools 运行环境，并持有全局主题对象。
 * 使用单例模式，整个应用程序生命周期内只有一个实例。
 */
class ZZ_PURE_TOOLS_EXPORT ZzApplication
{
public:
    /**
     * @brief 获取 ZzApplication 单例。
     * @return ZzApplication 实例引用。
     */
    [[nodiscard]] static ZzApplication& instance();

    /**
     * @brief 初始化 ZzPureTools。
     * @param application 当前 QApplication 实例。
     *
     * 初始化主题系统，并将主题应用到传入的 QApplication 上。
     */
    void initialize(QApplication& application);

    /**
     * @brief 获取当前主题对象。
     * @return 主题对象指针；未初始化时返回 nullptr。
     */
    [[nodiscard]] ZzTheme* theme() const;

    /**
     * @brief 判断 ZzPureTools 是否已完成初始化。
     * @return 已初始化返回 true，否则返回 false。
     */
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    ZzApplication() = default;

    QPointer<QApplication> m_application;  ///< 关联的 QApplication 实例
    ZzTheme* m_theme = nullptr;            ///< 全局主题对象
};

#endif  // ZZPURETOOLS_CORE_ZZAPPLICATION_HPP
