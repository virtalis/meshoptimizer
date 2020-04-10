#include <string>

#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include <wasm_simd128.h>

#include <emscripten.h>


double timestamp()
{
	return emscripten_get_now() * 1e-3;
}

#ifdef BITMASK
#define SIMD_TARGET __attribute__((target("unimplemented-simd128")))

// int wasm_v8x16_bitmask(v128_t a)
SIMD_TARGET
static inline int wasm_v8x16_bitmask(v128_t a)
{
	return __builtin_wasm_bitmask_i8x16((__i8x16)a);
}
#else
#define SIMD_TARGET
#endif

SIMD_TARGET
static int wasmMoveMask(v128_t mask)
{
#ifndef BITMASK
	v128_t mask_0 = wasm_v32x4_shuffle(mask, mask, 0, 2, 1, 3);

	// TODO: when Chrome supports v128.const we can try doing vectorized and?
	uint64_t mask_1a = wasm_i64x2_extract_lane(mask_0, 0) & 0x0804020108040201ull;
	uint64_t mask_1b = wasm_i64x2_extract_lane(mask_0, 1) & 0x8040201080402010ull;

	uint64_t mask_2 = mask_1a | mask_1b;
	uint64_t mask_4 = mask_2 | (mask_2 >> 16);
	uint64_t mask_8 = mask_4 | (mask_4 >> 8);

	return uint8_t(mask_8) | (uint8_t(mask_8 >> 32) << 8);
#else
	return wasm_v8x16_bitmask(mask);
#endif
}

class LiteralMatcher16
{
public:
	LiteralMatcher16(const char* string)
	{
		size_t length = strlen(string);

		size_t firstPos = getLeastFrequentLetter(string, length);
		size_t dataOffset = firstPos < 16 ? 0 : firstPos - 16;

		for (size_t i = 0; i < 16; ++i)
		{
			firstLetter[i] = string[firstPos];

			patternData[i] = (dataOffset + i < length) ? string[dataOffset + i] : 0;
			patternMask[i] = (dataOffset + i < length) ? 0 : 0xff;
		}

		firstLetterPos = firstPos;
		firstLetterOffset = firstPos - dataOffset;

		pattern = string;
	}

	SIMD_TARGET
	size_t match(const char* data, size_t size)
	{
		v128_t firstLetter = wasm_v128_load(this->firstLetter);
		v128_t patternData = wasm_v128_load(this->patternData);
		v128_t patternMask = wasm_v128_load(this->patternMask);

		size_t offset = firstLetterPos;

		while (offset + 32 <= size)
		{
			v128_t value = wasm_v128_load(data + offset);
			unsigned int mask = wasmMoveMask(wasm_i8x16_eq(value, firstLetter));

			// advance offset regardless of match results to reduce number of live values
			offset += 16;

			while (mask != 0)
			{
				unsigned int pos = __builtin_ctz(mask);
				size_t dataOffset = offset - 16 + pos - firstLetterOffset;

				mask &= ~(1 << pos);

				// check if we have a match
				v128_t patternMatch = wasm_v128_load(data + dataOffset);
				v128_t matchMask = wasm_v128_or(patternMask, wasm_i8x16_eq(patternMatch, patternData));

				if (wasm_i8x16_all_true(matchMask))
				{
					size_t matchOffset = dataOffset + firstLetterOffset - firstLetterPos;

					// final check for full pattern
					if (matchOffset + pattern.size() < size && memcmp(data + matchOffset, pattern.c_str(), pattern.size()) == 0)
					{
						return matchOffset;
					}
				}
			}
		}

		return findMatch(pattern.c_str(), pattern.size(), data, size, offset - firstLetterPos);
	}

private:
	unsigned char firstLetter[16];
	unsigned char patternData[16];
	unsigned char patternMask[16];
	size_t firstLetterPos;
	size_t firstLetterOffset;

	std::string pattern;

	static size_t findMatch(const char* x, size_t m, const char* y, size_t n, size_t start)
	{
		for (size_t j = start; j + m <= n; ++j)
		{
			size_t i = 0;
			while (i < m && x[i] == y[i + j]) ++i;

			if (i == m) return j;
		}

		return n;
	}

	static size_t getLeastFrequentLetter(const char* string, size_t length)
	{
		static const int kFrequencyTable[256] =
		{
			0, 1, 0, 0, 0, 0, 0, 0, 0, 10602590, 15871966, 0, 115, 15871967, 0, 0, 
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 
			137776326, 380531, 2050160, 1390430, 55652, 156711, 622798, 377541, 6347309, 6349017, 15625360, 795950, 19415821, 7560692, 4462299, 7677639, 
			28564723, 4287685, 3463240, 3246751, 2524164, 2032662, 2395948, 1767308, 2015273, 1511489, 3656040, 5020957, 952959, 3703051, 1709838, 58757, 
			33589, 4925784, 2109034, 4461995, 3250571, 5025264, 3924677, 1936150, 1023430, 3832789, 193034, 437171, 2790600, 2535084, 2734912, 2410167, 
			2863507, 180258, 3341306, 4837122, 4450499, 1455744, 1216353, 703917, 784740, 538485, 229684, 898206, 1013666, 897273, 16601, 7723320, 
			4063, 19048491, 4008087, 10148475, 9126676, 33071235, 7304196, 4732808, 5355028, 17956702, 429890, 1506676, 11310158, 8442069, 17783591, 16421409, 
			7587849, 310591, 18113867, 15601492, 26260892, 8333809, 2934125, 2055891, 16325891, 3402971, 743219, 1688043, 180102, 1687646, 39011, 0, 
			195, 316, 835, 1262, 32, 60, 69, 39, 75, 79, 97, 112, 98, 114, 64, 92, 
			119, 87, 475, 193, 184, 109, 521, 5121, 9, 20, 24, 11, 23, 39, 48, 19, 
			39, 44, 57, 59, 81, 455, 51, 30, 31, 1126, 86, 54, 39, 93, 39, 29, 
			110, 38, 46, 104, 69, 193, 85, 8353, 132, 68, 99, 64, 87, 138, 143, 97, 
			16, 11, 334, 13773, 126, 117, 14, 9, 22, 70, 44, 10, 66, 24, 12, 27, 
			1254, 50889, 79, 77, 62, 140, 14, 11, 9, 4, 7, 11, 21, 8, 0, 9, 
			11, 11, 84, 627, 43, 113, 31, 58, 81, 69, 36, 4, 9, 27, 9, 18, 
			61, 7, 5, 12, 6, 8, 7, 6, 8, 9, 20, 4, 18, 5, 4, 2, 
		};

		size_t result = 0;

		for (size_t i = 1; i < length; ++i)
			if (kFrequencyTable[static_cast<unsigned char>(string[i])] < kFrequencyTable[static_cast<unsigned char>(string[result])])
				result = i;
			
		return result;
	}
};

int main()
{
	int N = 100*1024*1024;

	std::string corpus;
	corpus.reserve(N + 1024);

	LiteralMatcher16 matcher("bbbb!cccc");

	for (int gap = 1; gap < 100; gap *= 2)
	{
		corpus.clear();

		while (corpus.size() < N)
		{
			for (int i = 0; i < gap; ++i)
				corpus += 'a';

			corpus += '!';
		}

		for (int i = 0; i < 5; ++i)
		{
			double t0 = timestamp();

			matcher.match(corpus.data(), corpus.size());

			double t1 = timestamp();

			double GB = 1024 * 1024 * 1024;

			printf("gap %d: %.3f ms (%.3f GB/sec)\n",
				gap,
				(t1 - t0) * 1000, double(corpus.size()) / GB / (t1 - t0));
		}
	}
}
