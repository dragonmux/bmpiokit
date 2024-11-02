// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2024 1BitSquared <info@1bitsquared.com>
// SPDX-FileContributor: Written by Rachel Mant <git@dragonmux.network>

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include "unicode.h"

static inline uint16_t safeIndex(const char16_t *const string, const size_t index, const size_t length)
{
	if (index >= length)
		return UINT16_MAX;
	return (uint16_t)string[index];
}

static size_t countUnits(const char16_t *const string, const size_t length)
{
	size_t count = 0U;
	// Loop through all the code units in the string
	for (size_t offset = 0; offset < length; ++offset)
	{
		// Grab the next code unit
		const uint16_t uintA = safeIndex(string, offset, length);
		// Check if it's a valid high surrogate (start of a surrogate pair)
		if ((uintA & 0xfe00U) == 0xd800U)
		{
			// If we got one, get the next code unit
			const uint16_t uintB = safeIndex(string, ++offset, length);
			// Validate that it's a valid low surrogate, and if not bail
			if ((uintB & 0xfe00U) != 0xdc00U)
				return 0U;
			// The character needs 3 additional bytes for a total of 4
			count += 3U;
		}
		// Check if it's a low surrogate (unpaired) and if so, bair
		else if ((uintA & 0xfe00U) == 0xdc00U)
			return 0U;
		else
		{
			// It's a normal character, calculate how many bytes we need to encode it, starting with whether it needs more than 1 (total 2)
			if (uintA > 0x007fU)
				++count;
			// Now check if it actually needs more than 2 bytes (total 3)
			if (uintA > 0x07ffU)
				++count;
		}
		// Whatever happened, we need at least one more byte to fully represent the value
		++count;
	}
	return count;
}

char *utf8FromUtf16(const char16_t *const utf16String, const size_t utf16Length)
{
	// Figure out how long the UTF-8 string equivilent of the UTF-16 string is exactly
	const size_t utf8Length = countUnits(utf16String, utf16Length);
	if (!utf8Length)
		return NULL;
	// Try to allocate storage for the new string
	char *const result = malloc(utf8Length);
	if (!result)
		return NULL;
	// Loop through all the code units on the input string
	for (size_t inputOffset = 0U, outputOffset = 0U; inputOffset < utf16Length; ++inputOffset, ++outputOffset)
	{
		// Extract a code unit
		const uint16_t uintA = safeIndex(utf16String, inputOffset, utf16Length);
		// Handle if it's a surrogate pair (already validated whole in countUnits())
		if ((uintA & 0xfe00U) == 0xd800U)
		{
			// Recover the upper 10 (11) bits from the first surrogate in the pair
			const uint16_t upper = (uintA & 0x03ffU) + 0x0040U;
			// Recover the lower 10 bits from the second surrogate of the pair
			const uint16_t lower = safeIndex(utf16String, ++inputOffset, utf8Length) & 0x3ffU;

			// Now encode the entire thing as a 4 byte sequence
			result[outputOffset + 0] = (char)(0xf0U | ((uint8_t)(upper >> 8U) & 0x07U));
			result[outputOffset + 1] = (char)(0x80U | ((uint8_t)(upper >> 2U) & 0x3fU));
			result[outputOffset + 2] = (char)(0x80U | ((uint8_t)(upper << 4U) & 0x30U) | ((uint8_t)(lower >> 6U) & 0x0fU));
			result[outputOffset + 3] = (char)(0x80U | (uint8_t)(lower & 0x3fU));
			outputOffset += 3U;
		}
		else
		{
			// It's a simple character, re-encode as appropriate. If it's able to fit in one byte, then do that
			if (uintA <= 0x007fU)
				result[outputOffset] = (char)uintA;
			// Otherwise, if it can fit in 2 bytes, then encode it accordingly
			else if (uintA <= 0x07ffU)
			{
				result[outputOffset + 0U] = (char)(0xc0U | ((uint8_t)(uintA >> 6U) & 0x1fU));
				result[outputOffset + 1U] = (char)(0x80U | (uint8_t)(uintA & 0x3fU));
				++outputOffset;
			}
			// Otherwise it has to be a 3 byte sequence
			else
			{
				result[outputOffset + 0U] = (char)(0xe0U | ((uint8_t)(uintA >> 12U) & 0x0fU));
				result[outputOffset + 1U] = (char)(0x80U | ((uint8_t)(uintA >> 6U) & 0x3fU));
				result[outputOffset + 2U] = (char)(0x80U | (uint8_t)(uintA & 0x3fU));
				outputOffset += 2U;
			}
		}
	}
	return result;
}
