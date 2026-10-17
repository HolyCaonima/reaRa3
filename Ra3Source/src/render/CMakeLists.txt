ra3_add_module(render
    SOURCES
        include/ra3/render/W3DAnimation.h
        include/ra3/render/W3DDrawable.h
        include/ra3/render/W3DMaterial.h
        include/ra3/render/W3DRainManager.h
        include/ra3/render/W3DScriptedModelDraw.h
        include/ra3/render/W3DTerrainCluster56.h
        include/ra3/render/W3DView.h
        include/ra3/render/W3DLayer56.h
        include/ra3/render/RenderCore99.h
        include/ra3/render/RenderScript9B.h
        include/ra3/render/RenderScript9B2.h
        src/W3DAnimation.cpp
        src/W3DDrawable.cpp
        src/W3DMaterial.cpp
        src/W3DRainManager.cpp
        src/W3DScriptedModelDraw.cpp
        src/W3DTerrainCluster56.cpp
        src/W3DView.cpp
        src/W3DLayer56.cpp
        src/RenderCore99.cpp
        src/RenderScript9B.cpp
        src/RenderScript9B2.cpp
    PUBLIC_DEPS
        Ra3::math)

set_source_files_properties(src/W3DLayer56.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2 /fp:precise")
set_source_files_properties(src/RenderScript9B2.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2 /fp:precise")
