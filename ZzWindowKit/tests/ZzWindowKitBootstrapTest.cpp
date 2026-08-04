#include <cstdlib>

#include <QtCore/QCoreApplication>
#include <QtCore/Qt>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

/**
 * @brief 验证 WindowKit 初始化入口的调用时序和幂等语义。
 */
class ZzWindowKitBootstrapTest final
{
public:
    /**
     * @brief 在应用对象创建前后执行初始化契约测试。
     * @param argc 进程参数数量。
     * @param argv 进程参数数组。
     * @return 契约成立时返回 EXIT_SUCCESS，否则返回不同失败码。
     */
    [[nodiscard]] static int run(int argc, char *argv[])
    {
        const auto first = ZzWindowKit::ZzWindowKitBootstrap::prepare();
        if (!first || !QCoreApplication::testAttribute(
                Qt::AA_DontCreateNativeWidgetSiblings)) {
            return 1;
        }

        const auto second = ZzWindowKit::ZzWindowKitBootstrap::prepare();
        if (!second) {
            return 2;
        }

        QCoreApplication application(argc, argv);
        const auto late = ZzWindowKit::ZzWindowKitBootstrap::prepare();
        return late ? 3 : EXIT_SUCCESS;
    }
};

int main(int argc, char *argv[])
{
    return ZzWindowKitBootstrapTest::run(argc, argv);
}
