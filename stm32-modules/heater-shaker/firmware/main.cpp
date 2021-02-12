
volatile bool loopy = false;

int main(int argc, char *argv[]) {

    while(true) {
        loopy = !loopy;
    }
    return 0;
}
