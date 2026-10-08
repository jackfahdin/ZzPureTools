# 时间轴

`ZzFluentUI::ZzTimeline` 继承 `QListView`，通过私有模型和委托展示 `ZzTimelineEvent`。
控件及事件均在 GUI 线程创建、修改；仅依赖 Qt 6.8+ 公共 API。

```cpp
#include <ZzFluentUI/ZzTimeline.h>

auto *timeline = new ZzFluentUI::ZzTimeline(parent);
timeline->setLayoutMode(ZzFluentUI::ZzTimeline::Alternating);
auto *event = timeline->addEvent(QDateTime::currentDateTime(),
    QStringLiteral("处理数据"), QStringLiteral("正在生成结果"),
    ZzFluentUI::ZzTimelineEvent::Current);
event->setIcon(ZzFluentUI::ZzSegoeIcon::Settings);
```

方向支持 `Qt::Vertical` 和 `Qt::Horizontal`。布局支持 `ContentOnRight`、`ContentOnLeft`、
`Alternating`、`AlternatingReverse`；水平模式下左／右对应上／下。
交错布局还允许每个事件通过 `placement` 指定 `Automatic`、`LeftSide` 或 `RightSide`。
`reverse` 只改变显示顺序；`events()` 始终返回插入顺序，`eventAt()` 使用显示索引。

事件包含时间戳、时间覆盖文本、标题、描述、状态、颜色、字体图标和内容位置。
`timeText` 非空时优先于 `timestamp`，否则按 `timestampFormat` 格式显示时间戳。
六种状态为 `Normal`、`Completed`、`Current`、`Pending`、`Warning` 和 `Error`。
自动颜色遵循参考项目：完成绿色、警告黄色、错误红色，当前／普通使用主题强调色。
无效 `QColor` 恢复自动配色。自定义图标使用项目内嵌的原始 Segoe Fluent Icons TTF，
支持 `QString` 字符或 `ZzSegoeIcon` 枚举；空字符恢复状态节点。

| 属性 | 默认值 | 有效范围 |
| --- | --- | --- |
| `timestampWidth` | 116px | 32～400px |
| `nodeSize` | 14px | 6～64px |
| `lineWidth` | 2px | 0.5～24px；忽略非有限值 |
| `itemSpacing` | 18px | 0～160px |
| `horizontalItemWidth` | 240px | 120～640px |
| `contentPadding` | 12px | 0～160px |
| `titleFontPixelSize` / `descriptionFontPixelSize` / `timestampFontPixelSize` | 0 | 0～96px；0 自动 |
| `animationDuration` | 1400ms | 200～10000ms |

时间和描述默认可见，时间格式默认为 `yyyy-MM-dd HH:mm`，默认纵向右侧内容。
`lineColor` 无效时使用 Base 与 Text 混合的主题中性色。
Current 节点共享一条持久呼吸动画；隐藏、禁用、关闭动画、减少动效或没有 Current 节点时停止。

`addEvent()` 接管事件，也支持从另一时间轴转移。`takeEvent()` 移出并解除父对象，
调用方接管返回对象；`removeEvent()` 和 `clearEvents()` 负责释放移除的对象。
若同步通知回调已经删除事件或重新接管它，`takeEvent()` 返回 `nullptr`，调用方无需释放。
`removeEvent()` 和 `clearEvents()` 使用延迟删除，回调转移到其他父对象的事件会保留。
`eventClicked` / `eventActivated` 返回实际事件指针，反序显示时不需要自行换算索引。

外部直接删除事件或修改父对象后，`events()` / `eventAt()` 立即不再返回该事件；
模型行移除通知会延迟到事件循环，避免在 Qt 对象操作栈中触发重入删除。
模型通知回调内的嵌套增删和反序设置在当前模型事务结束后生效。

示例入口为“自定义控件 → 时间轴(Timeline)”，紧随环形进度条。
页面提供订单流程、倒序更新记录、系统事件和水平时间轴，布局／样式／事件三个属性页，
支持事件增删、点击编辑、自动／手动颜色、禁用状态和恢复默认。示例使用固定时间便于比较。

## 验证记录

Linux GCC / Qt 6.11.1 下，核心行为测试、中文／英文示例集成、工作区烟测、公共头与架构检查通过。
浅色、深色、高对比三主题均保存了 100%、125%、150%、200% 缩放基线，四组截图回归通过。
使用实际 FluentUIStyle 控件，在同数据、配色、字体且关闭动画的条件下对比六组布局，
浅色和深色各 104 万像素均无差异。Windows／MSVC 尚未验证。
