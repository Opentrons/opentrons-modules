/**
 * @file putchar.c
 * @brief Implements a dummy _putchar function to work with the
 * mpaland printf library.
 * 
 * @details The definition in this file is a weak definition and
 * can be replaced with an implementation in another target that
 * uses this library.
 * 
 */

#include <printf.h>

__attribute__((weak)) void _putchar(char character) {
    return;
}
