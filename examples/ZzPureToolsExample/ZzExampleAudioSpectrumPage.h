#pragma once
class QWidget;
namespace ZzExample {
/** @brief 创建不依赖音频设备的实时频谱演示页。 */
class ZzExampleAudioSpectrumPage final
{
public:
    static QWidget *create(QWidget *parent);
};
}
