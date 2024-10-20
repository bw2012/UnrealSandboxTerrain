

## Генерация воксльного ландшафта

За генерацию воксельного ладншафта отвечает класс `UTerrainGeneratorComponent`.
Для изменения параметров генерации или добавления новых структур необходимо создать 2 класса:

1. создать свой класс контроллера (родитель `ASandboxTerrainController`) 
см. пример `ATerrainController` в файле `TerrainController.h`

2. создать свой класс генератора (родитель `UTerrainGeneratorComponent`)
см. пример `UMainTerrainGeneratorComponent` в файле `MainTerrainGeneratorComponent.h`

3. переопределить метод `ATerrainController::NewTerrainGenerator()` (см. `TerrainController.cpp`) 
так чтобы он возвращал ссылку на новый генератор созданный в пункте 2

`UTerrainGeneratorComponent* ATerrainController::NewTerrainGenerator() {
	UMainTerrainGeneratorComponent* Generator = NewObject<UMainTerrainGeneratorComponent>(this, TEXT("TerrainGenerator"));
	return Generator;
}`

Изменение генерации происходит посредством переопредеоения виртуальных методов класса UTerrainGeneratorComponent
см. пример `MainTerrainGeneratorComponent.cpp`


#### DensityFunctionExt

`virtual float DensityFunctionExt(float InDensity, const TFunctionIn& In) const override;`

Управляет генерацией плотности в данной конкретной точке. М.б. для генерации пустот, тоннелей или огранияения генерации мира по размеру
6


#### FoliageExt

`virtual FSandboxFoliage FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage & FoliageType, const TVoxelIndex & ZoneIndex, const FVector & WorldPos) override;`

Управляет генерацией растительности


#### OnBatchGenerationFinished

`virtual void OnBatchGenerationFinished() override;`

Вызывается после окончании генерации пачки зон (не используется в данный момент)


#### IsForcedComplexZone

`virtual bool IsForcedComplexZone(const TVoxelIndex& ZoneIndex) override;`

Переопределяет тип генерации зоны на Complex.
Д.б. применено вместе с DensityFunctionExt


#### PrepareMetaData

`virtual void PrepareMetaData() override;`

Не доделано


#### PostGenerateNewInstanceObjects

`virtual void PostGenerateNewInstanceObjects(const TVoxelIndex& ZoneIndex, const TZoneGenerationType ZoneType, const TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) const override;`

Вызвается после генерации зоны в фазе когда можно расставлять по зоне меши.
Используется для раскидывания камней, бревен, других объектов


#### MaterialFuncionExt

`virtual TMaterialId MaterialFuncionExt(const TGenerateVdTempItm* GenItm, const TMaterialId MatId, const FVector& WorldPos, const TVoxelIndex VoxelIndex) const override;`

Переопределяет материал в данной точке. Д.б. использовано вместе с IsForcedComplexZone


#### ExtVdGenerationData

`virtual void ExtVdGenerationData(TGenerateVdTempItm& VdGenerationData) override;`

Вызывается перед началом генерации зоны. Используется для переопределения параметров генерации. Добавления объектов, пещер и т.п.


#### GenerateRegion

`virtual void GenerateRegion(TTerrainRegion& Region);`

Не доделано.


#### NewChunkData

`virtual TChunkDataPtr NewChunkData() override;`

Используется чтобы заменить тип данных ChunkData. Добавить в него поля и т.д.


#### GenerateChunkDataExt

`virtual void GenerateChunkDataExt(TChunkDataPtr ChunkData, const TVoxelIndex& Index, int X, int Y, const FVector& WorldPos) const override;`

Вызывается перед началом генерации ChunkData


#### GroundLevelFunction

`virtual float GroundLevelFunction(const TVoxelIndex& ZoneIndex, const FVector& WorldPos) const;`

Переопределяет параметры генерации карты высот ландшафта. Использует шумы перлина. Здесь можно менять параметры и получать плоский или более холмистый ландшафт

