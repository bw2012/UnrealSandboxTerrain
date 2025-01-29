// Copyright blackw 2025-2030

#include "FlatTerrainGeneratorComponent.h"


UFlatTerrainGeneratorComponent::UFlatTerrainGeneratorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

float UFlatTerrainGeneratorComponent::GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const {
    return 0.f;
}