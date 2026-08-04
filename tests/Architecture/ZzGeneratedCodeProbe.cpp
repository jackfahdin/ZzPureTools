#include "ZzGeneratedCodeProbe.h"

#include <QtCore/QFile>

int main()
{
    [[maybe_unused]] ZzGeneratedCodeProbe probe;
    QFile resource(QStringLiteral(":/probe/message.txt"));
    if (!resource.open(QIODevice::ReadOnly)) {
        return 1;
    }

    return resource.readAll().trimmed() == "generated-code-probe" ? 0 : 2;
}
