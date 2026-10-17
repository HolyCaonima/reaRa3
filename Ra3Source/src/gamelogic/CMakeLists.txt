ra3_add_module(gamelogic
    SOURCES
        include/ra3/gamelogic/AIState7A.h
        include/ra3/gamelogic/AIStateMachine7C.h
        include/ra3/gamelogic/AITNGuardViews.h
        include/ra3/gamelogic/AIStateViews.h
        include/ra3/gamelogic/Behavior.h
        include/ra3/gamelogic/GameLogicCore4D.h
        include/ra3/gamelogic/ObjectCore6A.h
        include/ra3/gamelogic/ObjectUpdate69.h
        include/ra3/gamelogic/ProtectedCaveLogic.h
        include/ra3/gamelogic/BehaviorModules68.h
        include/ra3/gamelogic/ObjectModules74.h
        include/ra3/gamelogic/ScriptsCluster.h
        include/ra3/gamelogic/SkirmishAIBaseBuild.h
        include/ra3/gamelogic/UpdateModules.h
        include/ra3/gamelogic/ModuleIdentity72.h
        include/ra3/gamelogic/WeaponLogic.h
        include/ra3/gamelogic/ScriptNamedQueries.h
        include/ra3/gamelogic/SkirmishAI87.h
        include/ra3/gamelogic/AIState79.h
        include/ra3/gamelogic/SkirmishAI8B.h
        src/AIState7A.cpp
        include/ra3/gamelogic/AIState7B.h
        include/ra3/gamelogic/AIState79B.h
        include/ra3/gamelogic/AIState79C.h
        src/AIStateMachine7C.cpp
        src/AITNGuardViews.cpp
        src/AIStateViews.cpp
        src/Behavior.cpp
        src/GameLogicCore4D.cpp
        src/ObjectCore6A.cpp
        src/ObjectUpdate69.cpp
        src/ProtectedCaveLogic.cpp
        src/BehaviorModules68.cpp
        src/ObjectModules74.cpp
        src/ScriptsCluster.cpp
        src/SkirmishAIBaseBuild.cpp
        src/UpdateModules.cpp
        src/ModuleIdentity72.cpp
        src/WeaponLogic.cpp
        src/ScriptNamedQueries.cpp
        src/SkirmishAI87.cpp
        src/AIState79.cpp
        src/SkirmishAI8B.cpp
        include/ra3/gamelogic/ObjectUpdates78.h
        src/ObjectUpdates78.cpp
        src/AIState7B.cpp
        src/AIState79B.cpp
        src/AIState79C.cpp
    PUBLIC_DEPS
        Ra3::sim
        Ra3::common)

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
