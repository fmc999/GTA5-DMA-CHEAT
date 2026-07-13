#pragma once

#include <string>

class ModeSelector {
public:
    enum Mode {
        NORMAL = 1,
        FUSION = 2,
        FULLSCREEN = 3
    };

    static Mode SelectMode();
    static void RunMode(Mode mode);
};