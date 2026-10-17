ra3_add_module(gameclient
    SOURCES
        include/ra3/gameclient/ParticleModules.h
        include/ra3/gameclient/DisplayExtent.h
        src/ParticleModules.cpp
        src/DisplayExtent.cpp
    PUBLIC_DEPS
        Ra3::math
        Ra3::sim)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_gameclient PRIVATE /arch:SSE2 /fp:precise)
endif()
