ra3_add_module(core
    SOURCES
        include/ra3/core/ByteOrder.h
        include/ra3/core/FileUtil.h
        include/ra3/core/GameConfig.h
        include/ra3/core/IniFile.h
        include/ra3/core/LuaVmPorts.h
        include/ra3/core/Result.h
        include/ra3/core/SkuDef.h
        include/ra3/core/StringUtil.h
        include/ra3/core/TextFile4D.h
        src/FileUtil.cpp
        src/GameConfig.cpp
        src/IniFile.cpp
        src/LuaVmPorts.cpp
        src/SkuDef.cpp
        src/TextFile4D.cpp)

target_compile_definitions(ra3_core PRIVATE RA3_DEFAULT_CONFIG_PATH="${RA3_DEFAULT_CONFIG}")
