#pragma once

#include <QVector>
class QAudioBuffer;

namespace ZzExample {

/** @brief 提取最多八个声道的独立峰值 dBFS；静音为 -160，空/无效帧返回空数组。 */
class ZzExampleAudioLevels final
{
public:
    /** @brief 提取最多八个声道的独立峰值 dBFS；静音为 -160，空/无效帧返回空数组。 */
    [[nodiscard]] static QVector<qreal> peaks(const QAudioBuffer &buffer);
};

} // namespace ZzExample
