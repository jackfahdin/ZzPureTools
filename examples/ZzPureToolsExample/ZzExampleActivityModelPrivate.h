#pragma once

#include <QtCore/QStringList>

namespace ZzExample {

/** @brief 保存活动模型容量和有序字符串记录。 */
class ZzExampleActivityModelPrivate final
{
public:
    /** @brief 保存已经收敛为正数的容量。 */
    explicit ZzExampleActivityModelPrivate(qsizetype maximumRows)
        : capacity(maximumRows)
    {
    }

    qsizetype capacity = 1;
    QStringList rows;
};

} // namespace ZzExample
