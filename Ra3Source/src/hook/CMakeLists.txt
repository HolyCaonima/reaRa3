ra3_add_module(hook
    SOURCES
        include/ra3/hook/Detour.h
        src/Detour.cpp
    PUBLIC_DEPS
        Ra3::core)

ra3_add_shared_library(ra3hook
    SOURCES
        dll/DllMain.cpp
    DEPS
        Ra3::hook
        Ra3::io)
