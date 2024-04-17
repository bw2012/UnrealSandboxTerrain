// dd2012 10.04.2024

#pragma once

class UNREALSANDBOXTERRAIN_API TChunkFloatMatrix {

private:

    float* FloatArray;
    int Size;

    float Max;
    float Min;

public:

    TChunkFloatMatrix(int Size);

    ~TChunkFloatMatrix();

    float const* const GetArrayPtr() { return FloatArray; };

    void SetVal(const int X, const int Y, float Val);

    float GetVal(const int X, const int Y) const;

    float GetMax() const;

    float GetMin() const;
};

class UNREALSANDBOXTERRAIN_API TChunkData {

private:

    TChunkFloatMatrix* Height = nullptr;

public:

    TChunkData(int Size);

    ~TChunkData();

    float const* const GetHeightLevelArrayPtr() const;

    void SetHeightLevel(const int X, const int Y, float HeightLevel);

    float GetHeightLevel(const int X, const int Y) const;

    float GetMaxHeightLevel() const;

    float GetMinHeightLevel() const;
};