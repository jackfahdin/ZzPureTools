#pragma once

#include <QtCore/QObject>

class ZzVersionApiTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reportsProjectVersion();
};
