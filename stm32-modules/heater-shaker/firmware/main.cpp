
volatile bool loopy = false;

auto main() -> int {
    while (true) {
        loopy = !loopy;
    }
    return 0;
}
