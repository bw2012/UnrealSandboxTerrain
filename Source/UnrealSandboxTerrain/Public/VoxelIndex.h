#pragma once

struct TVoxelIndex {
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;

	TVoxelIndex(int32 XIndex, int32 YIndex, int32 ZIndex) : X(XIndex), Y(YIndex), Z(ZIndex) { }

	bool operator==(const TVoxelIndex &other) const {
		return (X == other.X && Y == other.Y && Z == other.Z);
	}

	friend TVoxelIndex operator+(const TVoxelIndex& lhs, const TVoxelIndex& rhs) {
		return TVoxelIndex(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z);
	}
};

namespace std {

	template <>
	struct hash<TVoxelIndex> {
		std::size_t operator()(const TVoxelIndex& k) const {
			using std::hash;
			return ((hash<int>()(k.X) ^ (hash<int>()(k.Y) << 1)) >> 1) ^ (hash<int>()(k.Z) << 1);
		}
	};
}

struct TVoxelIndex4 {
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;
	int32 W = 0;

	TVoxelIndex4(int32 A) : X(A), Y(A), Z(A), W(A) { }

	TVoxelIndex4(int32 XIndex, int32 YIndex, int32 ZIndex, int32 WIndex) : X(XIndex), Y(YIndex), Z(ZIndex), W(WIndex) { }

	bool operator==(const TVoxelIndex4 &other) const {
		return (X == other.X && Y == other.Y && Z == other.Z && W == other.W);
	}

	friend TVoxelIndex4 operator+(const TVoxelIndex4& lhs, const TVoxelIndex4& rhs) {
		return TVoxelIndex4(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z, lhs.W + rhs.W);
	}

	friend TVoxelIndex4 operator-(const TVoxelIndex4& lhs, const TVoxelIndex4& rhs) {
		return TVoxelIndex4(lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z, lhs.W - rhs.W);
	}
};