#pragma once

struct TVoxelIndex {
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;

	TVoxelIndex() {}

	TVoxelIndex(int32 XIndex, int32 YIndex, int32 ZIndex) : X(XIndex), Y(YIndex), Z(ZIndex) { }

	bool operator==(const TVoxelIndex &other) const {
		return (X == other.X && Y == other.Y && Z == other.Z);
	}

	friend TVoxelIndex operator+(const TVoxelIndex& lhs, const TVoxelIndex& rhs) {
		return TVoxelIndex(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z);
	}

	friend TVoxelIndex operator-(const TVoxelIndex& lhs, const TVoxelIndex& rhs) {
		return TVoxelIndex(lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z);
	}

	friend TVoxelIndex operator*(const TVoxelIndex& lhs, const int rhs) {
		return TVoxelIndex(lhs.X * rhs, lhs.Y * rhs, lhs.Z * rhs);
	}

	friend TVoxelIndex operator/(const TVoxelIndex& lhs, const int rhs) {
		return TVoxelIndex(lhs.X / rhs, lhs.Y / rhs, lhs.Z / rhs);
	}

	void operator = (const TVoxelIndex& A) {
		X = A.X; 
		Y = A.Y; 
		Z = A.Z;
	}

	friend uint32 GetTypeHash(const TVoxelIndex& Index) {
		return ((std::hash<int>()(Index.X) ^ (std::hash<int>()(Index.Y) << 1)) >> 1) ^ (std::hash<int>()(Index.Z) << 1);
	}
};

namespace std {

	template <>
	struct hash<TVoxelIndex> {
		std::size_t operator()(const TVoxelIndex& Index) const {
			return ((std::hash<int>()(Index.X) ^ (std::hash<int>()(Index.Y) << 1)) >> 1) ^ (std::hash<int>()(Index.Z) << 1);
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