// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2024 1BitSquared <info@1bitsquared.com>
// SPDX-FileContributor: Written by Rachel Mant <git@dragonmux.network>

#ifndef UNICODE_H
#define UNICODE_H

#include <stdint.h>
#include <stddef.h>

typedef uint16_t char16_t;

char *utf8FromUtf16(const char16_t *utf16String, size_t utf16Length);

#endif /*UNICODE_H*/
