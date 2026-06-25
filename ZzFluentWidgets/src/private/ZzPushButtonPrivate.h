#ifndef ZZPUSHBUTTONPRIVATE_H
#define ZZPUSHBUTTONPRIVATE_H

#include "ZzDesignTokens.h"

class ZzPushButton;

class ZzPushButtonPrivate
{
public:
    explicit ZzPushButtonPrivate(ZzPushButton* q);

    ZzPushButton* q_ptr = nullptr;
    ZzPushButton::ZzButtonStyle buttonStyle = ZzPushButton::ZzButtonStyle::Standard;
    bool hovered = false;
    bool pressed = false;
};

#endif // ZZPUSHBUTTONPRIVATE_H
