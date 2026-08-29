#include <random>
#include <mutex>

// single thread_local PRNG shared across code; declared extern in individual.h
thread_local std::mt19937 engine(std::random_device{}());
