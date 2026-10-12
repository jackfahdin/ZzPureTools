// Diagnostic only, Qt 6.11.1; do not install into the SDK or framework.
// Adapted from Qt's qtimerinfo_unix.cpp, Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2016 Intel Corporation.
// SPDX-License-Identifier: LGPL-3.0-only
#include <QtCore/private/qtimerinfo_unix_p.h>
#include <cstdio>
#include <cstdlib>

std::optional<QTimerInfoList::Duration> QTimerInfoList::timerWait()
{
    static const bool precise = std::getenv("ZZ_DIAGNOSTIC_PRECISE_WAIT") != nullptr;
    static const bool announced = [] {
        std::fprintf(stderr, "diagnostic timerWait active: %s\n", precise ? "precise" : "rounded");
        return true;
    }();
    (void)announced;
    const auto now = updateCurrentTime();
    const auto it = std::find_if(timers.cbegin(), timers.cend(),
                                [](const QTimerInfo *timer) { return !timer->activateRef; });
    if (it == timers.cend()) return std::nullopt;
    const Duration remaining = std::max(Duration::zero(), (*it)->timeout - now);
    return precise ? remaining : std::chrono::ceil<std::chrono::milliseconds>(remaining);
}
