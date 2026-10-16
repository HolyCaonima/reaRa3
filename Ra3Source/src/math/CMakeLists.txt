ra3_add_module(math
    SOURCES
        include/ra3/math/Math.h
        src/Math.cpp)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_options(ra3_math PRIVATE /arch:SSE2 /fp:precise)
endif()
