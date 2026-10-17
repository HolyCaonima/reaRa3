ra3_add_module(gamelogic
    SOURCES
        include/ra3/gamelogic/AITNGuardViews.h
        include/ra3/gamelogic/Behavior.h
        src/AITNGuardViews.cpp
        src/Behavior.cpp
        include/ra3/gamelogic/WeaponLogic.h
        src/WeaponLogic.cpp)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_gamelogic PRIVATE /arch:SSE2 /fp:precise)
endif()
