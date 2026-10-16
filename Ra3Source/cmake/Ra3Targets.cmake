# Target conventions shared by every Ra3Source module, tool and test.

function(ra3_apply_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /MP)
        target_compile_definitions(${target} PRIVATE
            NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ra3_add_module(<name> SOURCES <files...> [PUBLIC_DEPS <targets...>] [PRIVATE_DEPS <targets...>])
#
# Creates the static library ra3_<name> (alias Ra3::<name>). Public headers live
# in <module dir>/include/ra3/<name>/ and are included as <ra3/<name>/Header.h>.
function(ra3_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    set(target ra3_${name})
    add_library(${target} STATIC ${ARG_SOURCES})
    add_library(Ra3::${name} ALIAS ${target})
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(${target} PUBLIC ${ARG_PUBLIC_DEPS} PRIVATE ${ARG_PRIVATE_DEPS})
    target_compile_features(${target} PUBLIC cxx_std_17)
    ra3_apply_compile_options(${target})
endfunction()

# ra3_add_executable(<name> SOURCES <files...> [DEPS <targets...>])
function(ra3_add_executable name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_DEPS})
    ra3_apply_compile_options(${name})
endfunction()
