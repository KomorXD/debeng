#include "eng/test.hpp"
#include <cstdio>

int main(int argc, char **argv) {
    const char *user = get_user();
    printf("Invoking user: %s\r\n", user);

    return 0;
}
