set(RA3_GAMELOGIC_SOURCES
    src/AIStateViews.cpp)

ra3_add_module(gamelogic
    SOURCES ${RA3_GAMELOGIC_SOURCES})

if(RA3_BUILD_TOOLS)
    add_library(ra3_gamelogic_mutant STATIC ${RA3_GAMELOGIC_SOURCES})
    target_include_directories(ra3_gamelogic_mutant
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_features(ra3_gamelogic_mutant PUBLIC cxx_std_17)
    target_compile_definitions(ra3_gamelogic_mutant
        PRIVATE RA3_AISTATE_ORACLE_MUTANT=1)
    ra3_apply_compile_options(ra3_gamelogic_mutant)
    add_library(Ra3::gamelogic_mutant ALIAS ra3_gamelogic_mutant)
endif()
