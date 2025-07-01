#include "TerrainTileSpawnerActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "Kismet/GameplayStatics.h"

ATerrainTileSpawnerActor::ATerrainTileSpawnerActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATerrainTileSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	// OpenCL Dependencies Initialization -------------------------------------
	mpDevice = OpenCL::MakeDevice();
	mpContext = MakeContext(mpDevice);

	mpQueue = std::make_unique<OpenCL::CommandQueue>(mpContext, mpDevice);
	mpProgram = std::make_unique<OpenCL::Program>(mpContext, mpDevice);
	// ------------------------------------------------------------------------


	const std::string programString(TCHAR_TO_UTF8(*mpProgramAsset->SourceCode));
	if (!mpProgram->ReadFromString(programString))
		return;

	const std::string name(TCHAR_TO_UTF8(*mKernelName));
	mpKernel = std::make_unique<OpenCL::Kernel>(*mpProgram, name);


	mLastPlayerLocation = FVector::ZeroVector;

	UpdateTiles();
}

void ATerrainTileSpawnerActor::BeginDestroy()
{
	mpKernel = nullptr;
	mpProgram = nullptr;

	mpContext = nullptr;
	mpDevice = nullptr;
	
	Super::BeginDestroy();
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
				OpenCL::Image img = OpenCL::Image(mpContext,
												  mpDevice,
												  mapSize, 
												  mapSize,
												  1,
												  OpenCL::Image::Format::R8, 
												  OpenCL::Image::Type::Texture2D);

				mpKernel->SetArgument(0, img.Get());
				mpKernel->SetArgument(1, FVector2f(Coord.X, Coord.Y));
				mpKernel->SetArgument(2, FVector2f(mMapBuffer, mMapBuffer)); // Buffer size

				size_t global_work_size[2] = { mapSize, mapSize };
				mpQueue->EnqueueRange(*mpKernel, 2, global_work_size);

				UTexture2D* texture = img.CreateUTexture2D(*mpQueue, false, false);

				if (NewTile)
				{
					if (UMaterialInterface* material = NewTile->GetMaterial(0))
					{
						UMaterialInstanceDynamic* mat = UMaterialInstanceDynamic::Create(material, this);

						mat->SetScalarParameterValue("MaxWorldPositionOffsetDisplacement", mMaxDisplacement_CM);

						mat->SetScalarParameterValue("Elevation_Scale", mMaxDisplacement_CM);
						mat->SetScalarParameterValue("Buffer_Size", mMapBuffer);
						mat->SetScalarParameterValue("Map_Size", mElevationMapSize + mElevationMapSize);

						if (texture)
							mat->SetTextureParameterValue("Elevation Map", texture);

						NewTile->SetMaterial(0, mat);
					}
				}

				mTileCoordToTextures.Add(Coord, texture);
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