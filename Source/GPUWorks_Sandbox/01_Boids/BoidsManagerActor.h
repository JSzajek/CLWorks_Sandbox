#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

#include "GPUWorksLib.h"

#include "BoidsManagerActor.generated.h"

UCLASS()
class ABoidsManagerActor : public AActor
{
	GENERATED_BODY()
public:
	ABoidsManagerActor();
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
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	TObjectPtr<UGPUProgramAsset> mpProgramAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	FString mKernelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	TObjectPtr<UStaticMesh> mpMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	int32 mNumberOfBoids = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	FVector mMinimumPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	FVector mMaximumPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	float mSeperationRadius = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	float mAlignmentRadius = 95;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	float mCohesionRadius = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	float mMaxSpeed = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	float mMaxForce = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	float mBounceFactor = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	bool mUseInstancing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids|Debug")
	bool mDrawBounds = false;

	UPROPERTY(VisibleAnywhere, Category = "Boids|Debug")
	TArray<FVector4f> mPositions;

	UPROPERTY(VisibleAnywhere, Category = "Boids|Debug")
	TArray<FVector4f> mVelocities;

	UPROPERTY(VisibleAnywhere, Category = "Boids|Debug")
	TArray<FVector4f> mAccelerations;
private:
	UPROPERTY()
	TArray<class UStaticMeshComponent*> mBoids;

	UPROPERTY()
	class UHierarchicalInstancedStaticMeshComponent* mBoidsComp;

	TArray<FTransform> mInstances;

	FBox mBounds;

	std::shared_ptr<Gpu::ICore> mpGPUCore = nullptr;
	std::shared_ptr<Gpu::IDevice> mpGPUDevice = nullptr;
	std::shared_ptr<Gpu::IContext> mpGPUContext = nullptr;

	std::shared_ptr<Gpu::IProgram> mpGPUProgram = nullptr;
	std::shared_ptr<Gpu::IKernel> mpGPUKernel = nullptr;
	std::shared_ptr<Gpu::IQueue> mpGPUQueue= nullptr;

	std::shared_ptr<Gpu::IBuffer> mpPositionsBuffer = nullptr;
	std::shared_ptr<Gpu::IBuffer> mpVelocitiesBuffer = nullptr;
	std::shared_ptr<Gpu::IBuffer> mpAccelerationBuffer = nullptr;

	std::shared_ptr<Gpu::IEvent> mpFinalGPUEvent = nullptr;

	uint8_t frame = 0;
};