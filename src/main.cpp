#include "vkEngine.h"

int main(int argc, char** argv) {
    initialise(argc, argv);
    run();
    cleanup();

    return 0;
}
