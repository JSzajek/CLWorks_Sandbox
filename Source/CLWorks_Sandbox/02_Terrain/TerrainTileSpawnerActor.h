#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

#include "CLWorksLib.h"

#include "TerrainTileSpawnerActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class ATerrainTileSpawnerActor : public AActor
{
	GENERATED_BODY()
public:
	ATerrainTileSpawnerActor();
public:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	/**
	 * Dispatches the once-per frame Tick() function for this actor
	 * @param	DeltaTime			The time slice of this tick
	 * @param	TickType			The type of tick that is happening
	 * @param	ThisTickFunction	The tick function that is firing, useful for getting the completion handle
	 */
	virtual void TickActor(float DeltaTime, 
						   enum ELevelTick TickType, 
						   FActorTickFunction& ThisTickFunction) override;
private:
	void UpdateTiles();

	FIntPoint WorldToTileCoord(const FVector& WorldLocation) const;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TObjectPtr<UCLProgramAsset> mpProgramAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	FString mKernelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TObjectPtr<UStaticMesh> mpTerrainMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Terrain")
    float mUpdateDistanceThreshold = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
    float mMaxDisplacement_CM = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
    int32 mGridRadius = 3;

	UPROPERTY(EditAnywhere, Category = "Terrain")
    int32 mElevationMapSize = 256;

	UPROPERTY(EditAnywhere, Category = "Terrain")
    int32 mMapBuffer = 1;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    float mTileSize = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
    float mTileOverlap = 1.0f;
private:
	OpenCL::DevicePtr mpDevice = nullptr;
	OpenCL::ContextPtr mpContext = nullptr;

	std::unique_ptr<OpenCL::Program> mpProgram = nullptr;
	std::unique_ptr<OpenCL::Kernel> mpKernel = nullptr;
	std::shared_ptr<OpenCL::CommandQueue> mpQueue = nullptr;

	FVector mLastPlayerLocation;

	UPROPERTY(VisibleAnywhere, Category = "Terrain|Debug")
	TMap<FIntPoint, TObjectPtr<UStaticMeshComponent>> mActiveTiles;

	UPROPERTY(VisibleAnywhere, Category = "Terrain|Debug")
	TMap<FIntPoint, TObjectPtr<UTexture>> mTileCoordToTextures;
};