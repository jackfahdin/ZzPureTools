#pragma once

#include <QtGui/QStandardItemModel>

namespace ZzExample {

/** @brief 提供只读的本地目录演示数据，不连接文件系统或网络。 */
class ZzExampleFileModel final : public QStandardItemModel
{
public:
    /** @brief 构造两列目录模型；对象由注入方持有。 */
    explicit ZzExampleFileModel(QObject *parent = nullptr);
};

} // namespace ZzExample
