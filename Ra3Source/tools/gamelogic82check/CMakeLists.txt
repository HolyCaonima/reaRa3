ra3_add_executable(ra3_gamelogic82check
    SOURCES
        main.cpp
    DEPS
        Ra3::gamelogic)

ra3_add_executable(ra3_gamelogic82check_mutant
    SOURCES
        main.cpp
        ../../src/gamelogic/src/ScriptNamedQueries.cpp
    DEPS
        Ra3::core)

target_include_directories(ra3_gamelogic82check_mutant PRIVATE
    ../../src/gamelogic/include)

target_compile_definitions(ra3_gamelogic82check_mutant PRIVATE
    RA3_GAMELOGIC82_ORACLE_MUTANT=1)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_gamelogic82check_mutant PRIVATE /arch:SSE2 /fp:precise)
endif()
