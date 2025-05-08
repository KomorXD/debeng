#include <cstdlib>
#include "test.hpp"

const char *get_user() {
    char *user = getenv("USER");
    return user;
}
