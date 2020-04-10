#include "../src/meshoptimizer.h"

#include <vector>

#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include <emscripten.h>

double timestamp()
{
	return emscripten_get_now() * 1e-3;
}

struct Vertex
{
	uint16_t data[16];
};

uint32_t murmur3(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;

	return h;
}

void benchCodecs(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
	std::vector<Vertex> vb(vertices.size());
	std::vector<unsigned int> ib(indices.size());

	std::vector<unsigned char> vc(meshopt_encodeVertexBufferBound(vertices.size(), sizeof(Vertex)));
	std::vector<unsigned char> ic(meshopt_encodeIndexBufferBound(indices.size(), vertices.size()));

	printf("source: vertex data %d bytes\n", int(vertices.size() * sizeof(Vertex)));

	for (int pass = 0; pass < 2; ++pass)
	{
		if (pass == 1)
			meshopt_optimizeVertexCacheStrip(&ib[0], &indices[0], indices.size(), vertices.size());
		else
			meshopt_optimizeVertexCache(&ib[0], &indices[0], indices.size(), vertices.size());

		meshopt_optimizeVertexFetch(&vb[0], &ib[0], indices.size(), &vertices[0], vertices.size(), sizeof(Vertex));

		vc.resize(vc.capacity());
		vc.resize(meshopt_encodeVertexBuffer(&vc[0], vc.size(), &vb[0], vertices.size(), sizeof(Vertex)));

		ic.resize(ic.capacity());
		ic.resize(meshopt_encodeIndexBuffer(&ic[0], ic.size(), &ib[0], indices.size()));

		printf("pass %d: vertex data %d bytes\n", pass, int(vc.size()));

		for (int attempt = 0; attempt < 10; ++attempt)
		{
			double t0 = timestamp();

			int rv = meshopt_decodeVertexBuffer(&vb[0], vertices.size(), sizeof(Vertex), &vc[0], vc.size());
			assert(rv == 0);
			(void)rv;

			double t1 = timestamp();

			double GB = 1024 * 1024 * 1024;

			printf("decode: vertex %.3f ms (%.3f GB/sec)\n",
				(t1 - t0) * 1000, double(vertices.size() * sizeof(Vertex)) / GB / (t1 - t0));
		}
	}
}

int main()
{
	const int N = 1000;

	std::vector<Vertex> vertices;
	vertices.reserve((N + 1) * (N + 1));

	for (int x = 0; x <= N; ++x)
	{
		for (int y = 0; y <= N; ++y)
		{
			Vertex v;

			for (int k = 0; k < 16; ++k)
			{
				uint32_t h = murmur3((x * (N + 1) + y) * 16 + k);

				// use random k-bit sequence for each word to test all encoding types
				// note: this doesn't stress the sentinel logic too much but it's all branchless so it's probably fine?
				v.data[k] = h & ((1 << k) - 1);
			}

			vertices.push_back(v);
		}
	}

	std::vector<unsigned int> indices;
	indices.reserve(N * N * 6);

	for (int x = 0; x < N; ++x)
	{
		for (int y = 0; y < N; ++y)
		{
			indices.push_back((x + 0) * N + (y + 0));
			indices.push_back((x + 1) * N + (y + 0));
			indices.push_back((x + 0) * N + (y + 1));

			indices.push_back((x + 0) * N + (y + 1));
			indices.push_back((x + 1) * N + (y + 0));
			indices.push_back((x + 1) * N + (y + 1));
		}
	}

	benchCodecs(vertices, indices);
}
