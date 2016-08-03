#pragma once


namespace usand {

	class PerlinNoise {
	public:
		PerlinNoise();
		~PerlinNoise();

		// Generates a Perlin (smoothed) noise value between -1 and 1, at the given 3D position.
		float noise(float sample_x, float sample_y, float sample_z);

	private:
		int *p; // Permutation table
				// Gradient vectors
		float *Gx;
		float *Gy;
		float *Gz;
	};

}