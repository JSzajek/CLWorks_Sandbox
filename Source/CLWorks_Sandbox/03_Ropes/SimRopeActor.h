#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

#include "CLWorksLib.h"

#include "SimRopeActor.generated.h"


class UProceduralMeshComponent;


USTRUCT()
struct FRopeParticle
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere)
	FVector4f mPosition;

	UPROPERTY(VisibleAnywhere)
	FVector4f mPreviousPosition;

	UPROPERTY(VisibleAnywhere)
	float mInverseMass = 0;
};

USTRUCT()
struct FRopeConstraint
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere)
	int32 mPoint1;

	UPROPERTY(VisibleAnywhere)
	int32 mPoint2;

	UPROPERTY(VisibleAnywhere)
	float mRestLength;
};

UCLASS()
class UEditorRopePoint : public USceneComponent
{
	GENERATED_BODY()
public:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& _event) override;
public:
	UPROPERTY(EditAnywhere, Category = "Rope Point")
	bool mIsPinned = false;

	UPROPERTY()
	int32 mIndex = -1;
};


UCLASS()
class ASimRopeActor : public AActor
{
	friend UEditorRopePoint;

	GENERATED_BODY()
public:
	ASimRopeActor();
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
	/** If true, actor is ticked even if TickType==LEVELTICK_ViewportsOnly	 */
	virtual bool ShouldTickIfViewportsOnly() const { return true; }
#endif
private:
	void UpdatePoint(int32 index, const FVector& position);

	void UpdateRopeMesh();
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	TObjectPtr<UCLProgramAsset> mpForcesProgramAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FString mForcesKernelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	TObjectPtr<UCLProgramAsset> mpConstraintsProgramAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FString mConstraintsKernelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	TObjectPtr<UCLProgramAsset> mpEnforceProgramAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FString mEnforceKernelName;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Rope")
	UEditorRopePoint* mpStartPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Rope")
	UEditorRopePoint* mpEndPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	UMaterialInterface* mpRopeMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 mNumSolverIterations = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float mPointSampleRate_CM = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FVector mGravityForce_MPerS = FVector(0, 0, -9.81);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float mStiffness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float mDampning = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float mRopeRadius_CM = 10.0f;

	UPROPERTY(EditAnywhere)
	int32 mRopeSides = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug")
	bool mDrawLine = true;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Debug")
	float mRopeResetLength = 0;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Debug")
	TArray<FRopeParticle> mRopeParticles;

	UPROPERTY(VisibleAnywhere, Category = "Rope|Debug")
	TArray<FRopeConstraint> mRopeConstraints;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Rope|Debug")
	TObjectPtr<UProceduralMeshComponent> mpRopeMeshComponent = nullptr;
private:
	bool mSimulating = false;

	OpenCL::DevicePtr mpDevice = nullptr;
	OpenCL::ContextPtr mpContext = nullptr;

	std::unique_ptr<OpenCL::Program> mpForcesProgram = nullptr;
	std::unique_ptr<OpenCL::Kernel> mpForcesKernel = nullptr;

	std::unique_ptr<OpenCL::Program> mpConstraintsProgram = nullptr;
	std::unique_ptr<OpenCL::Kernel> mpConstraintsKernel = nullptr;

	std::unique_ptr<OpenCL::Program> mpEnforceProgram = nullptr;
	std::unique_ptr<OpenCL::Kernel> mpEnforceKernel = nullptr;

	std::shared_ptr<OpenCL::CommandQueue> mpQueue = nullptr;


	std::unique_ptr<OpenCL::Buffer> mpParticlesBuffer = nullptr;
	std::unique_ptr<OpenCL::Buffer> mpConstraintsBuffer = nullptr;
};