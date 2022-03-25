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

#include <stdio.h>

void _putchar(char character) {
    (void)fputc(character, stdout);
}
