ra3_add_module(gameclient
    SOURCES
        include/ra3/gameclient/DisplayExtent.h
        include/ra3/gameclient/GameClient92.h
        include/ra3/gameclient/ParticleModules.h
        src/DisplayExtent.cpp
        src/GameClient92.cpp
        src/ParticleModules.cpp
    PUBLIC_DEPS
        Ra3::math
        Ra3::sim)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_gameclient PRIVATE /arch:SSE2 /fp:precise)
endif()
