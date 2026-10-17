ra3_add_module(sim
    SOURCES
        include/ra3/sim/Random.h
        include/ra3/sim/TerrainLogic.h
        src/Random.cpp
        src/TerrainLogic.cpp
    PRIVATE_DEPS
        Ra3::common)
