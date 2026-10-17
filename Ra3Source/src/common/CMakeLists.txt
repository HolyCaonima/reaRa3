ra3_add_module(common
    SOURCES
        include/ra3/common/ByteString.h
        include/ra3/common/CommonSubsystem.h
        include/ra3/common/Crc32.h
        include/ra3/common/GameString.h
        include/ra3/common/Player.h
        include/ra3/common/StateMachine.h
        include/ra3/common/ThingTemplate.h
        include/ra3/common/Thing78.h
        src/ByteString.cpp
        src/CommonSubsystem.cpp
        src/Crc32.cpp
        src/GameString.cpp
        src/Player.cpp
        src/StateMachine.cpp
        src/ThingTemplate.cpp
        src/Thing78.cpp)

if(MSVC)
    set_source_files_properties(src/Thing78.cpp PROPERTIES COMPILE_FLAGS "/arch:SSE2 /fp:precise")
endif()
