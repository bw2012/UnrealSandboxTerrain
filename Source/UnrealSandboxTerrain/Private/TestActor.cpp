
#include "TestActor.h"
#include "VoxelData.h"
#include "VoxelIndex.h"
#include <vector>

ATestActor::ATestActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

class TOctreeNode {

public:

    int Parent;

    TVoxelIndex VoxelIndex;
    int Level;

    unsigned long CaseCode;

    bool HasChilds = false;
};

class TOctreeTraversal;

class TVoxelOctreeData {

private:

    int MaxDepth;
    int Num;
    int Total;

    std::vector<TOctreeNode> Nodes;
    TDensityVal* DensityData;

    friend class TOctreeTraversal;
};

class TOctreeTraversal { 

public:
    int MaxDepth;
    int Num;
    int Total;

    std::vector<TOctreeNode> Nodes;
    TDensityVal* DensityData;

private:

    TVoxelOctreeData* VoxelOctree;

    int* Processed;

    int ElementIndex = 0;
    int Count = 0;

    int TargetLevel = 0;

    void PerformVoxel(const TVoxelIndex& VoxelIndex, const int Level) {
        const int LOD = MaxDepth - Level - 1;
        const int S = 1 << LOD;

        if (/*CheckVoxel(Parent, S, LOD, VoxelData)*/ true ) {
            TVoxelIndex TmpVi[8];
            int8 TmpDensity[8];

            vd::tools::makeIndexes(TmpVi, VoxelIndex, S);

            for (auto I = 0; I < 8; I++) {
                int Idx = vd::tools::clcLinearIndex(Num, TmpVi[I].X, TmpVi[I].Y, TmpVi[I].Z);
                if (Processed[Idx] == 0x0) {
                    TDensityVal Density = DensityHandler(TmpVi[I], Idx, Level, LOD) * 255;
                    DensityData[Idx] = Density;
                    TmpDensity[I] = Density;
                    Count++;
                    Processed[Idx] = 0xff;
                } else {
                    TmpDensity[I] = DensityData[Idx];
                }
            }

            unsigned long CaseCode = vd::tools::caseCode(TmpDensity);

            TOctreeNode Node;
            Node.Level = Level;
            Node.VoxelIndex = VoxelIndex;
            Node.CaseCode = CaseCode;
            Nodes.push_back(Node);


            if (LOD != 0 && (CaseCode != 0x0 && CaseCode != 0xff)) {
                TOctreeNode& ParentNode = Nodes.at(Nodes.size() - 1);
                ParentNode.HasChilds = true;

                const int ChildLOD = LOD - 1;
                const int ChildLevel = Level + 1;
                const int ChildS = 1 << ChildLOD;
                TVoxelIndex Child[8];
                vd::tools::makeIndexes(Child, VoxelIndex, ChildS);
                for (auto I = 0; I < 8; I++) {
                    PerformVoxel(Child[I], ChildLevel);
                }
            }


        }
    };

public:

    std::function<float(const TVoxelIndex&, const int, const int, const int)> DensityHandler = nullptr;
    std::function<bool(const TVoxelIndex&, int, int, const TVoxelData*)> CheckVoxel = nullptr;

    TOctreeTraversal(int MD) : MaxDepth(MD) {
        Num = (1 << (MaxDepth - 1)) + 1;
        Total = Num * Num * Num;

        DensityData = new TDensityVal[Total];
        Processed = new int[Total];
        for (int I = 0; I < Total; I++) {
            Processed[I] = 0x0;
            DensityData[I] = 0x0;
        }

        DensityHandler = [](const TVoxelIndex& V, const int Idx, const int Level, const int LOD) { return 0.f; };
        CheckVoxel = [](const TVoxelIndex& V, int S, int LOD, const TVoxelData* VoxelData) { return true;  };
    };

    ~TOctreeTraversal() {
        delete Processed;
    };

    void Start(int TargetLevel_ = 0) {
        //PerformVoxel(TVoxelIndex(0, 0, 0), MaxDepth - 1);

        if (TargetLevel_ == 0) {
            TargetLevel = MaxDepth + 1;
        } else {
            TargetLevel = TargetLevel_;
        }


        PerformVoxel(TVoxelIndex(0, 0, 0), 0);
    };

    int GetCount() {
        return Count;
    }

    int GetTotal() {
        return Total;
    }
};




void ATestActor::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("ATestActor"));

    TVoxelData* VoxelData = new TVoxelData(33, 1000);
    VoxelData->setOrigin(GetActorLocation());

    TOctreeTraversal TestOctree(LOD_ARRAY_SIZE);

    TestOctree.DensityHandler = [=](const TVoxelIndex& V, const int Idx, const int Level, const int LOD) {
        //if (Level < 3) {
            const FVector& LocalPos = VoxelData->voxelIndexToVector(V.X, V.Y, V.Z);
            const FVector& WorldPos = LocalPos + VoxelData->getOrigin();

            if (WorldPos.Z < 200) {
                //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugPoint(GetWorld(), WorldPos, 3.f, FColor(255, 255, 255, 0), true); });
                return 1.f;
            }


        //}

        return 0.f;
    };

    TestOctree.Start();

    for (const TOctreeNode& Node : TestOctree.Nodes) {

        if (!Node.HasChilds) {
            //UE_LOG(LogTemp, Warning, TEXT("Node: %d - %d %d %d"), Node.Level, Node.VoxelIndex.X, Node.VoxelIndex.Y, Node.VoxelIndex.Z);
        }

        if (Node.CaseCode != 0x0 && Node.CaseCode != 0xff) {

            const int LOD = TestOctree.MaxDepth - Node.Level - 1;
            const int S = 1 << LOD;

            TVoxelIndex TmpVi[8];
            vd::tools::makeIndexes(TmpVi, Node.VoxelIndex, S);

            for (auto I = 0; I < 8; I++) {
                TVoxelIndex V = TmpVi[I];
                const FVector& LocalPos = VoxelData->voxelIndexToVector(V.X, V.Y, V.Z);
                const FVector& WorldPos = LocalPos + VoxelData->getOrigin();


                if (!Node.HasChilds) {
                    AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugPoint(GetWorld(), WorldPos, 3.f, FColor(255, 255, 255, 0), true); });
                }


            }

        }

    }
}
