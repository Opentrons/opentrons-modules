
volatile bool loopy = false;

int main(void) {

    while(true) {
        loopy = !loopy;
    }
    return 0;
}
