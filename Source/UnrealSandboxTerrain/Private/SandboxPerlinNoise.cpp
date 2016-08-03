#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxPerlinNoise.h"


PerlinNoise::PerlinNoise() {
	srand(999666);

	p = new int[256];
	Gx = new float[256];
	Gy = new float[256];
	Gz = new float[256];

	for (int i = 0; i<256; ++i) {
		p[i] = i;

		Gx[i] = (float(rand()) / (RAND_MAX / 2)) - 1.0f;
		Gy[i] = (float(rand()) / (RAND_MAX / 2)) - 1.0f;
		Gz[i] = (float(rand()) / (RAND_MAX / 2)) - 1.0f;
	}

	int j = 0;
	int swp = 0;
	for (int i = 0; i<256; i++) {
		j = rand() & 255;

		swp = p[i];
		p[i] = p[j];
		p[j] = swp;
	}
}

PerlinNoise::~PerlinNoise()
{
	delete p;
	delete Gx;
	delete Gy;
	delete Gz;
}


float PerlinNoise::noise(float sample_x, float sample_y, float sample_z)
{
	// Unit cube vertex coordinates surrounding the sample point
	int x0 = int(floorf(sample_x));
	int x1 = x0 + 1;
	int y0 = int(floorf(sample_y));
	int y1 = y0 + 1;
	int z0 = int(floorf(sample_z));
	int z1 = z0 + 1;

	// Determine sample point position within unit cube
	float px0 = sample_x - float(x0);
	float px1 = px0 - 1.0f;
	float py0 = sample_y - float(y0);
	float py1 = py0 - 1.0f;
	float pz0 = sample_z - float(z0);
	float pz1 = pz0 - 1.0f;

	// Compute dot product between gradient and sample position vector
	int gIndex = p[(x0 + p[(y0 + p[z0 & 255]) & 255]) & 255];
	float d000 = Gx[gIndex] * px0 + Gy[gIndex] * py0 + Gz[gIndex] * pz0;
	gIndex = p[(x1 + p[(y0 + p[z0 & 255]) & 255]) & 255];
	float d001 = Gx[gIndex] * px1 + Gy[gIndex] * py0 + Gz[gIndex] * pz0;

	gIndex = p[(x0 + p[(y1 + p[z0 & 255]) & 255]) & 255];
	float d010 = Gx[gIndex] * px0 + Gy[gIndex] * py1 + Gz[gIndex] * pz0;
	gIndex = p[(x1 + p[(y1 + p[z0 & 255]) & 255]) & 255];
	float d011 = Gx[gIndex] * px1 + Gy[gIndex] * py1 + Gz[gIndex] * pz0;

	gIndex = p[(x0 + p[(y0 + p[z1 & 255]) & 255]) & 255];
	float d100 = Gx[gIndex] * px0 + Gy[gIndex] * py0 + Gz[gIndex] * pz1;
	gIndex = p[(x1 + p[(y0 + p[z1 & 255]) & 255]) & 255];
	float d101 = Gx[gIndex] * px1 + Gy[gIndex] * py0 + Gz[gIndex] * pz1;

	gIndex = p[(x0 + p[(y1 + p[z1 & 255]) & 255]) & 255];
	float d110 = Gx[gIndex] * px0 + Gy[gIndex] * py1 + Gz[gIndex] * pz1;
	gIndex = p[(x1 + p[(y1 + p[z1 & 255]) & 255]) & 255];
	float d111 = Gx[gIndex] * px1 + Gy[gIndex] * py1 + Gz[gIndex] * pz1;

	// Interpolate dot product values at sample point using polynomial interpolation 6x^5 - 15x^4 + 10x^3
	float wx = ((6 * px0 - 15)*px0 + 10)*px0*px0*px0;
	float wy = ((6 * py0 - 15)*py0 + 10)*py0*py0*py0;
	float wz = ((6 * pz0 - 15)*pz0 + 10)*pz0*pz0*pz0;

	float xa = d000 + wx*(d001 - d000);
	float xb = d010 + wx*(d011 - d010);
	float xc = d100 + wx*(d101 - d100);
	float xd = d110 + wx*(d111 - d110);
	float ya = xa + wy*(xb - xa);
	float yb = xc + wy*(xd - xc);
	float value = ya + wz*(yb - ya);

	return value;
}