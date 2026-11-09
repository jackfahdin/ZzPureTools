#pragma once
#include <QtCore/QObject>

/** @brief 验证枚举元对象宏不消耗后续方法注释。 */
class ZzGoodEnumWidget : public QObject
{
    Q_OBJECT
public:
    /** @brief 有文档的枚举类型。 */
    enum class ZzMode { None, Fade };
    Q_ENUM(ZzMode)
    /** @brief 有文档的公开方法。 */
    void activate();
};
