#pragma once

#include <QtGui/QKeySequence>

namespace ZzFluentUI {

class ZzKeyBinder;

/** @brief 持有快捷键录制事务的回滚快照和派生状态。 */
class ZzKeyBinderPrivate final
{
public:
    /**
     * @brief 连接 Qt 编辑完成信号并初始化本地化说明。
     * @param q 非空、非拥有的公开快捷键录制器。
     */
    explicit ZzKeyBinderPrivate(ZzKeyBinder *q);

    /** @brief 幂等进入录制事务并保存当前键序列。 */
    void beginRecording();

    /** @brief 接受 Qt 当前键序列并结束录制事务。 */
    void acceptRecording();

    /** @brief 恢复开始前键序列并结束录制事务。 */
    void cancelRecording();

    /** @brief 刷新本地化 tooltip 和无障碍操作说明。 */
    void refreshAccessibleText();

    ZzKeyBinder *const q_ptr;
    QKeySequence sequenceBeforeRecording;
    bool recording = false;
};

} // namespace ZzFluentUI
