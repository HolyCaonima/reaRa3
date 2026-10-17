ra3_add_module(gamelogic
    SOURCES
        include/ra3/gamelogic/AITNGuardViews.h
        include/ra3/gamelogic/AIStateViews.h
        include/ra3/gamelogic/Behavior.h
        include/ra3/gamelogic/ScriptsCluster.h
        include/ra3/gamelogic/WeaponLogic.h
        src/AITNGuardViews.cpp
        src/AIStateViews.cpp
        src/Behavior.cpp
        src/ScriptsCluster.cpp
        src/WeaponLogic.cpp)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_gamelogic PRIVATE /arch:SSE2 /fp:precise)
endif()

if(RA3_BUILD_TOOLS)
    add_library(ra3_gamelogic_mutant STATIC src/AIStateViews.cpp)
    target_include_directories(ra3_gamelogic_mutant
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_features(ra3_gamelogic_mutant PUBLIC cxx_std_17)
    target_compile_definitions(ra3_gamelogic_mutant
        PRIVATE RA3_AISTATE_ORACLE_MUTANT=1)
    ra3_apply_compile_options(ra3_gamelogic_mutant)
    add_library(Ra3::gamelogic_mutant ALIAS ra3_gamelogic_mutant)
endif()
