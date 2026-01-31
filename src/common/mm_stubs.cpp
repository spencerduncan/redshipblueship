/**
 * MM C++ Stubs for Single Executable Build
 *
 * This file provides stub implementations for C++ enhancement layer
 * functions that were excluded in single-exe mode.
 */

#include <string>

/* GetActorCategoryName - returns name for actor category */
std::string GetActorCategoryName(unsigned char category) {
    (void)category;
    return "Unknown";
}
