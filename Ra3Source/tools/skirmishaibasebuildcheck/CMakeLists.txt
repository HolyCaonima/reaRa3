ra3_add_executable(ra3_skirmishaibasebuildcheck
    SOURCES
        main.cpp
    DEPS
        Ra3::gamelogic)

ra3_add_executable(ra3_skirmishaibasebuildcheck_mutant
    SOURCES
        main.cpp
        ../../src/gamelogic/src/SkirmishAIBaseBuild.cpp)
target_include_directories(ra3_skirmishaibasebuildcheck_mutant
    PRIVATE "${CMAKE_SOURCE_DIR}/src/gamelogic/include")
target_compile_definitions(ra3_skirmishaibasebuildcheck_mutant
    PRIVATE RA3_SKIRMISHAIBASEBUILD_ORACLE_MUTANT=1)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_skirmishaibasebuildcheck PRIVATE /arch:SSE2 /fp:precise)
    target_compile_options(ra3_skirmishaibasebuildcheck_mutant PRIVATE /arch:SSE2 /fp:precise)
endif()
