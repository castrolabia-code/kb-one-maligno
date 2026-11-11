// Runner de pruebas: "KBOneTests <Categoria>" corre una categoría,
// sin argumentos corre todas. Devuelve 1 si alguna falla (ctest lo detecta).
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    if (argc > 1)
        runner.runTestsInCategory (argv[1]);
    else
        runner.runAllTests();

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    if (runner.getNumResults() == 0)
    {
        std::cerr << "No hay pruebas en la categoria pedida\n";
        return 1;
    }

    std::cout << "\nResultado: " << passes << " OK, " << failures << " fallas\n";
    return failures == 0 ? 0 : 1;
}
