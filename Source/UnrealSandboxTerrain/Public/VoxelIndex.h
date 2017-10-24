#pragma once

struct TVoxelIndex {
	int X = 0;
	int Y = 0;
	int Z = 0;

	TVoxelIndex(int XIndex, int YIndex, int ZIndex) : X(XIndex), Y(YIndex), Z(ZIndex) { }

	bool operator==(const TVoxelIndex &other) const {
		return (X == other.X && Y == other.Y && Z == other.Z);
	}
};

namespace std {

	template <>
	struct hash<TVoxelIndex> {
		std::size_t operator()(const TVoxelIndex& k) const {
			using std::hash;

			// Compute individual hash values for first,
			// second and third and combine them using XOR
			// and bit shifting:

			return ((hash<int>()(k.X) ^ (hash<int>()(k.Y) << 1)) >> 1) ^ (hash<int>()(k.Z) << 1);
		}
	};
}