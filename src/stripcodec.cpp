#include "meshoptimizer.h"

#include <string.h>

#ifndef TRACE
#define TRACE 0
#endif

#if TRACE
#include <stdio.h>
#endif

static void encodeVByte(unsigned char*& data, unsigned int v)
{
	// encode 32-bit value in up to 5 7-bit groups
	do
	{
		*data++ = (v & 127) | (v > 127 ? 128 : 0);
		v >>= 7;
	} while (v);
}

static void encode4(unsigned char* data, size_t offset, unsigned int v)
{
	assert(v < 16);

	if (offset & 1)
		data[offset >> 1] |= v << 4;
	else
		data[offset >> 1] = v;
}

static void encodeDelta(unsigned char*& data, size_t& offset, unsigned int v)
{
	if (0)
	{
		return encodeVByte(data, v);
	}

	if (v < 15)
	{
		encode4(data, offset, v);
		offset++;
	}
	else
	{
		encode4(data, offset, 15);
		offset++;

		do
		{
			encode4(data, offset, (v & 7) | (v > 7 ? 8 : 0));
			offset++;

			v >>= 3;
		}
		while (v);
	}
}

size_t meshopt_encodeStripBuffer(unsigned char* buffer, size_t buffer_size, const unsigned int* indices, size_t index_count, size_t triangle_count)
{
	(void)buffer;
	(void)buffer_size;

	// 2bit * triangle_count
	size_t triangle_bytes = (triangle_count + 3) / 4;

	memset(buffer, 0, triangle_bytes);

	unsigned char* triangles = buffer;
	unsigned char* extra = buffer + triangle_bytes;

	unsigned int next = 0;

	size_t start = 0;

	size_t triangle_offset = 0; // note: the offset is in 2-bit units
	size_t extra_offset = 0; // note: the offset is in 4-bit units

	for (size_t i = 0; i < index_count; ++i)
	{
	#if TRACE > 1
		printf("%d\n", indices[i]);
	#endif

		if (indices[i] == ~0u)
		{
			start = i + 1;
		}
		else if (i - start >= 2)
		{
			unsigned int a = indices[i - 2], b = indices[i - 1], c = indices[i];

			if (i - start == 2)
			{
				int ae = next - a;
				if (a == next) next++;
				int be = next - b;
				if (b == next) next++;
				int ce = next - c;
				if (c == next) next++;

				triangles[triangle_offset / 4] |= 3 << ((triangle_offset % 4) * 2);
				triangle_offset++;

				encodeDelta(extra, extra_offset, ae);
				encodeDelta(extra, extra_offset, be);
				encodeDelta(extra, extra_offset, ce);

			#if TRACE > 1
				printf("# restart %d %d %d\n", ae, be, ce);
			#endif
			}
			else
			{
				unsigned int z = indices[i - 3];

				if (c == a)
				{
					// degenerate triangle, next triangle is treated as swapped
				}
				else if (b == z)
				{
					if (c == next)
					{
						triangles[triangle_offset / 4] |= 1 << ((triangle_offset % 4) * 2);
						triangle_offset++;

					#if TRACE > 1
						printf("# swap-next\n");
					#endif
					}
					else
					{
						int ce = next - c;

						triangles[triangle_offset / 4] |= 2 << ((triangle_offset % 4) * 2);
						triangle_offset++;

						encodeDelta(extra, extra_offset, (ce << 1) | 1);

					#if TRACE > 1
						printf("# swap-other %d\n", ce);
					#endif
					}

					if (c == next) next++;
				}
				else
				{
					if (c == next)
					{
						triangles[triangle_offset / 4] |= 0 << ((triangle_offset % 4) * 2);
						triangle_offset++;

					#if TRACE > 1
						printf("# cont-next\n");
					#endif
					}
					else
					{
						int ce = next - c;

						triangles[triangle_offset / 4] |= 2 << ((triangle_offset % 4) * 2);
						triangle_offset++;

						encodeDelta(extra, extra_offset, (ce << 1) | 0);

					#if TRACE > 1
						printf("# cont-other %d\n", ce);
					#endif
					}

					if (c == next) next++;
				}
			}
		}
	}

	assert(triangle_offset == triangle_count);

	return (extra_offset + 1) / 2 + (extra - buffer);
}

size_t meshopt_encodeStripBufferBound(size_t index_count, size_t vertex_count)
{
	(void)vertex_count;

	return index_count * 100;
}