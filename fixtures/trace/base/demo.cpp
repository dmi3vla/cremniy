#include <cstdint>

static volatile int state = 0;

extern "C" __attribute__((noinline)) int shared_helper(int input) {
    return input * 3 + 7;
}

extern "C" __attribute__((noinline)) int calculate_alpha(int input) {
    int doubled = input * 2;
    return doubled + 5;
}

extern "C" __attribute__((noinline)) int calculate_clone(int input) {
    int doubled = input * 2;
    return doubled + 5;
}

extern "C" __attribute__((noinline)) int calculate_beta(int value) {
    int scaled = value * 2;
    return scaled + 5;
}

extern "C" __attribute__((noinline)) int calculate_with_state(int input) {
    int doubled = input * 2;
    state = doubled;
    return doubled + 5;
}

extern "C" __attribute__((noinline)) int scenario_one(int input) {
    return calculate_alpha(input) + shared_helper(input);
}

extern "C" __attribute__((noinline)) int scenario_two(int input) {
    return calculate_beta(input);
}

static int debug_only(int input) { return input + 17; }

extern "C" __attribute__((noinline)) int removed_probe(int input) {
    return debug_only(input);
}

int main(int argc, char**) {
    return scenario_one(argc) + scenario_two(argc) + calculate_with_state(argc)
           + removed_probe(argc);
}
