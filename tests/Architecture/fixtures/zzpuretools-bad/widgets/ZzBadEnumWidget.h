#pragma once
#include <QtCore/QObject>

/** @brief 验证枚举元对象宏之后仍检查公开方法注释。 */
class ZzBadEnumWidget : public QObject
{
    Q_OBJECT
public:
    /** @brief 有文档的枚举类型。 */
    enum class ZzMode { None, Fade };
    Q_ENUM(ZzMode)
    void undocumented();
};
