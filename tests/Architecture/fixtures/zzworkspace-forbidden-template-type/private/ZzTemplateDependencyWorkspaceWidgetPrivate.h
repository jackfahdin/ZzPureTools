#pragma once

#include <QtCore/QList>

class SftpSession final
{
};

class ZzTemplateDependencyWorkspaceWidgetPrivate final
{
private:
    QList<SftpSession> sessions_;
};
