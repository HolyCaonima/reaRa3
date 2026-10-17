ra3_add_executable(ra3_aistatecheck
    SOURCES main.cpp
    DEPS Ra3::gamelogic)

ra3_add_executable(ra3_aistatecheck_mutant
    SOURCES main.cpp
    DEPS Ra3::gamelogic_mutant)
