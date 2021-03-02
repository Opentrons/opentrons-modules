
volatile bool
    loopy =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    false;

auto main() -> int {
    while (true) {
        loopy = !loopy;
    }
    return 0;
}
