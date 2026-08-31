#include <iostream>

extern "C" long run_one_generation_timing(int ai_depth, int population_size);

int main()
{
    const int AI_DEPTH = 7;
    const int POP = 48;
    long ms = run_one_generation_timing(AI_DEPTH, POP);
    std::cout << "one generation timing: " << ms << " ms (POP=" << POP << ")\n";
    return 0;
}
