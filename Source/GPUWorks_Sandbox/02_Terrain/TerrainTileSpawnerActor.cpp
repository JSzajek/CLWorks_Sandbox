#include "TerrainTileSpawnerActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "Kismet/GameplayStatics.h"

ATerrainTileSpawnerActor::ATerrainTileSpawnerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATerrainTileSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	mpGPUContextObj = NewObject<UGPUContextObject>();
	if (mpGPUContextObj)
	{
		mpGPUContextObj->Initialize(EGPUBackend::OpenCL);
		mpGPUContextObj->CreateDefaultQueue();
	}

	mpGPUProgramObj = NewObject<UGPUProgramObject>();
	mpGPUProgramObj->BuildFromAsset(mpGPUContextObj, mpProgramAsset);

	if (!mpGPUProgramObj->HasKernel(mKernelName))
	{
		mpGPUProgramObj->ConditionalBeginDestroy();
		mpGPUProgramObj = nullptr;
		return;
	}

	mpGPUProgramObj->SetKernel(mKernelName);

	mLastPlayerLocation = FVector::ZeroVector;

	UpdateTiles();
}

void ATerrainTileSpawnerActor::BeginDestroy()
{
	Super::BeginDestroy();

	if (mpGPUContextObj)
	{
		mpGPUContextObj->ConditionalBeginDestroy();
		mpGPUContextObj = nullptr;
	}

	if (mpGPUProgramObj)
	{
		mpGPUProgramObj->ConditionalBeginDestroy();
		mpGPUProgramObj = nullptr;
	}

	if (mpGPUImageObj)
	{
		mpGPUImageObj->ConditionalBeginDestroy();
		mpGPUImageObj = nullptr;
	}
}

void ATerrainTileSpawnerActor::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	AActor* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) 
		return;

	FVector CurrentLocation = Player->GetActorLocation();
	if (FVector::Dist2D(CurrentLocation, mLastPlayerLocation) > mUpdateDistanceThreshold)
	{
		mLastPlayerLocation = CurrentLocation;

		UpdateTiles();
	}
}

void ATerrainTileSpawnerActor::UpdateTiles()
{
	AActor* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player || !mpTerrainMesh) 
		return;

	FVector PlayerLocation = Player->GetActorLocation();

	FIntPoint Center = WorldToTileCoord(Player->GetActorLocation());
	TSet<FIntPoint> DesiredCoords;

	for (int dx = -mGridRadius; dx <= mGridRadius; ++dx)
	{
		for (int dy = -mGridRadius; dy <= mGridRadius; ++dy)
		{
			FIntPoint Coord = Center + FIntPoint(dx, dy);
			DesiredCoords.Add(Coord);

			if (!mActiveTiles.Contains(Coord))
			{
				// Create Mesh Component ----------------------------------------------------------
				FString Name = FString::Printf(TEXT("Tile_%d_%d"), Coord.X, Coord.Y);
				UStaticMeshComponent* NewTile = NewObject<UStaticMeshComponent>(this, *Name);

				NewTile->RegisterComponent();
				NewTile->SetStaticMesh(mpTerrainMesh);
				NewTile->SetWorldLocation(FVector(Coord.X * (mTileSize - mTileOverlap),
												  Coord.Y * (mTileSize - mTileOverlap),
												  0));

				NewTile->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
				

				// Create Elevation Map -----------------------------------------------------------
				uint32_t mapSize = mElevationMapSize + (mMapBuffer * 2);

				FTileData data;

				data.mpGPUImage = NewObject<UGPUImageObject>();
				data.mpGPUImage->CreateImage2D(mpGPUContextObj,
											 mapSize,
											 mapSize,
											 EGpuPixelFormat::R8);



				mpGPUProgramObj->SetImageArg(0, data.mpGPUImage);
				mpGPUProgramObj->SetVector2fArg(1, FVector2f(Coord.X, Coord.Y));
				mpGPUProgramObj->SetVector2fArg(2, FVector2f(mMapBuffer, mMapBuffer)); // Buffer size

				Gpu::DispatchDescription dispatchDesc;
				dispatchDesc.Dim = 2;
				dispatchDesc.Global[0] = mapSize;
				dispatchDesc.Global[1] = mapSize;
				mpGPUContextObj->GetDefaultQueue()->Dispatch(*mpGPUProgramObj->GetKernel(), dispatchDesc);

				data.mpTexture = data.mpGPUImage->CreateTexture2D(mpGPUContextObj, true);
				if (NewTile)
				{
					if (UMaterialInterface* material = NewTile->GetMaterial(0))
					{
						UMaterialInstanceDynamic* mat = UMaterialInstanceDynamic::Create(material, this);

						mat->SetScalarParameterValue("MaxWorldPositionOffsetDisplacement", mMaxDisplacement_CM);

						mat->SetScalarParameterValue("Elevation_Scale", mMaxDisplacement_CM);
						mat->SetScalarParameterValue("Buffer_Size", mMapBuffer);
						mat->SetScalarParameterValue("Map_Size", mElevationMapSize + mElevationMapSize);

						if (data.mpTexture)
						{
							mat->SetTextureParameterValue("Elevation Map", data.mpTexture);
						}

						NewTile->SetMaterial(0, mat);
					}
				}

				mTileCoordToTextures.Add(Coord, data);
				mActiveTiles.Add(Coord, NewTile);
				// --------------------------------------------------------------------------------
			}
		}
	}

	// Remove tiles not in the desired set
	for (auto It = mActiveTiles.CreateIterator(); It; ++It)
	{
		if (!DesiredCoords.Contains(It->Key))
		{
			mTileCoordToTextures.Remove(It->Key);
			if (It->Value)
			{
				It->Value->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}
}

FIntPoint ATerrainTileSpawnerActor::WorldToTileCoord(const FVector& WorldLocation) const
{
	return FIntPoint(FMath::FloorToInt(WorldLocation.X / mTileSize),
					 FMath::FloorToInt(WorldLocation.Y / mTileSize));
}