ra3_add_executable(ra3_unit_tests
    SOURCES
        support/BigBuilder.h
        support/TempDir.h
        support/TestHarness.h
        support/TestMain.cpp
        asset/ManifestTests.cpp
        asset/NameHashTests.cpp
        asset/TypedAssetTests.cpp
        common/ByteStringTests.cpp
        core/GameConfigTests.cpp
        core/IniFileTests.cpp
        core/SkuDefTests.cpp
        hook/EmittedDecoder.h
        hook/HookTests.cpp
        io/ArchiveSetTests.cpp
        io/BigArchiveTests.cpp
        io/PathCompareTests.cpp
        io/RefPackTests.cpp
        text/CsfTests.cpp
    DEPS
        Ra3::asset
        Ra3::common
        Ra3::core
        Ra3::hook
        Ra3::io
        Ra3::text)
target_include_directories(ra3_unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME unit COMMAND ra3_unit_tests)

ra3_add_executable(ra3_game_data_tests
    SOURCES
        support/TestHarness.h
        support/TestMain.cpp
        data/AssetGameDataTests.cpp
        data/CsfGameDataTests.cpp
        data/GameDataTests.cpp
    DEPS
        Ra3::asset
        Ra3::core
        Ra3::io
        Ra3::text)
target_include_directories(ra3_game_data_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME game_data COMMAND ra3_game_data_tests)
set_tests_properties(game_data PROPERTIES SKIP_RETURN_CODE 77)

ra3_add_executable(ra3_hook_dummy
    SOURCES
        hook/DummyTarget.cpp
        hook/EmittedDecoder.h)
target_include_directories(ra3_hook_dummy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

ra3_add_executable(ra3_hook_integration
    SOURCES
        hook/HookIntegration.cpp)
add_test(
    NAME hook_integration
    COMMAND ra3_hook_integration
        $<TARGET_FILE:ra3_hook_dummy>
        $<TARGET_FILE:ra3inject>
        $<TARGET_FILE:ra3hook>)
set_tests_properties(hook_integration PROPERTIES TIMEOUT 60)
