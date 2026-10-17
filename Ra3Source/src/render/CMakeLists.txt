ra3_add_module(render
    SOURCES
        include/ra3/render/W3DRainManager.h
        include/ra3/render/W3DView.h
        src/W3DRainManager.cpp
        src/W3DView.cpp
    PUBLIC_DEPS
        Ra3::math)
