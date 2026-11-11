# Ejecutable de pruebas propio (juce::UnitTest) registrado en ctest por categoría.
juce_add_console_app(KBOneTests PRODUCT_NAME "KBOneTests")

target_sources(KBOneTests PRIVATE
    TestMain.cpp
    TestHelpers.h
    OscillatorTests.cpp
    FilterTests.cpp
    EnvelopeTests.cpp
    ProcessorTests.cpp
    StateTests.cpp
)

target_link_libraries(KBOneTests PRIVATE KBOneCore)

foreach(category Oscillator Filter Envelope Processor State)
    add_test(NAME ${category} COMMAND KBOneTests ${category})
    set_tests_properties(${category} PROPERTIES TIMEOUT 300)
endforeach()
