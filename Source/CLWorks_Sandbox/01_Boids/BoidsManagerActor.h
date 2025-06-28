#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

#include "CLWorksLib.h"

#include "BoidsManagerActor.generated.h"

UCLASS()
class ABoidsManagerActor : public AActor
{
	GENERATED_BODY()
public:
	ABoidsManagerActor();

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

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	TObjectPtr<UCLProgramAsset> ProgramAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	FString KernelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids")
	UStaticMesh* mMesh;

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

	OpenCL::DevicePtr mpDevice = nullptr;
	OpenCL::ContextPtr mpContext = nullptr;

	std::unique_ptr<OpenCL::Program> mpProgram = nullptr;
	std::unique_ptr<OpenCL::Kernel> mpKernel = nullptr;
	std::shared_ptr<OpenCL::CommandQueue> mpQueue = nullptr;


	std::unique_ptr<OpenCL::Buffer> mpPositionsBuffer = nullptr;
	std::unique_ptr<OpenCL::Buffer> mpVelocitiesBuffer = nullptr;
	std::unique_ptr<OpenCL::Buffer> mpAccelerationBuffer = nullptr;

	FBox mBounds;
};