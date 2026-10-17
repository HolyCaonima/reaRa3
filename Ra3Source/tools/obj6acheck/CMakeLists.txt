ra3_add_executable(ra3_obj6acheck
    SOURCES
        main.cpp
    DEPS
        Ra3::gamelogic)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_obj6acheck PRIVATE /arch:SSE2 /fp:precise)
endif()
