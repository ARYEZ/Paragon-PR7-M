FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(catch2)

add_executable(ParagonPR7MTests
    TestMain.cpp
    ProcessorTests.cpp
    UiTests.cpp
    MeteringTests.cpp
    LimiterTests.cpp
    CompressorTests.cpp
    EditorSnapshot.cpp)

# Link the plugin's shared code (processor, editor, JUCE modules) so the
# tests exercise exactly the code that ships in the VST3. JUCE modules are
# linked privately into that target, so borrow its include paths and defines
# (as SYSTEM, so warnings inside JUCE headers don't flood the test build).
target_link_libraries(ParagonPR7MTests PRIVATE ParagonPR7M Catch2::Catch2)
target_include_directories(ParagonPR7MTests SYSTEM PRIVATE $<TARGET_PROPERTY:ParagonPR7M,INCLUDE_DIRECTORIES>)
target_compile_definitions(ParagonPR7MTests PRIVATE $<TARGET_PROPERTY:ParagonPR7M,COMPILE_DEFINITIONS>)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(ParagonPR7MTests)
