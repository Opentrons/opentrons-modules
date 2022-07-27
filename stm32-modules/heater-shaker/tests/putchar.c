/**
 * @file putchar.c
 * @brief Implements a real putchar implementation for testing purposes
 * 
 */

#include <printf.h>

#include <stdio.h>

void _putchar(char character) {
    (void)fputc(character, stdout);
}
