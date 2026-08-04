#pragma once

#include <QtCore/QObject>

/**
 * @brief 触发 AUTOMOC 并为生成代码编译边界提供稳定探针。
 *
 * 该测试对象只在创建它的测试线程使用，不向外转移所有权。
 */
class ZzGeneratedCodeProbe final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
};
