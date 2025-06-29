#include "BoidsManagerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Misc/App.h"
#include "Kismet/KismetMathLibrary.h"

ABoidsManagerActor::ABoidsManagerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	mpDevice = OpenCL::MakeDevice();
	mpContext = MakeContext(mpDevice);

	mpQueue = std::make_unique<OpenCL::CommandQueue>(mpContext, mpDevice);

	mpProgram = std::make_unique<OpenCL::Program>(mpContext, mpDevice);

	mBounds = FBox(EForceInit::ForceInitToZero);
}

void ABoidsManagerActor::BeginPlay()
{
	Super::BeginPlay();


	if (mNumberOfBoids > 0)
	{
		// Bounds Initialization --------------------------
		mBounds = FBox(EForceInit::ForceInitToZero);
		mBounds += mMinimumPoint;
		mBounds += mMaximumPoint;
		// ------------------------------------------------

		// Positions & Velocities -------------------------
		mPositions.SetNumZeroed(mNumberOfBoids);
		mVelocities.SetNumZeroed(mNumberOfBoids);
		mAccelerations.SetNumZeroed(mNumberOfBoids);
		// ------------------------------------------------

		// Visualization Initialization -------------------
		if (mUseInstancing)
		{
			mBoidsComp = NewObject<UHierarchicalInstancedStaticMeshComponent>(GetRootComponent(),
																		  UHierarchicalInstancedStaticMeshComponent::StaticClass(),
																		  NAME_None,
																		  RF_Transient | RF_NonPIEDuplicateTransient | RF_TextExportTransient);
			mBoidsComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			mBoidsComp->RegisterComponent();
			AddInstanceComponent(mBoidsComp);

			mBoidsComp->SetStaticMesh(mMesh);
			mBoidsComp->bAutoRebuildTreeOnInstanceChanges = false;

			mInstances.Reserve(mNumberOfBoids);
		}

		for (int32_t i = 0; i < mNumberOfBoids; ++i)
		{
			FVector randPos = FMath::RandPointInBox(mBounds);

			mPositions[i] = FVector4f(randPos.X, randPos.Y, randPos.Z, 0);
			mVelocities[i] = FVector4f(FMath::RandRange(0.f, mMaxForce),
									   FMath::RandRange(0.f, mMaxForce),
									   FMath::RandRange(0.f, mMaxForce),
									   0);

			if (mUseInstancing)
			{
				mInstances.Add(FTransform(randPos));
			}
			else
			{
				UStaticMeshComponent* boid = NewObject<UStaticMeshComponent>(GetRootComponent(),
																			 UStaticMeshComponent::StaticClass(),
																			 NAME_None,
																			 RF_Transient | RF_NonPIEDuplicateTransient | RF_TextExportTransient);
			
				boid->RegisterComponent();
				AddInstanceComponent(boid);

				boid->SetStaticMesh(mMesh);

				boid->SetWorldLocation(randPos);
				mBoids.Add(boid);
			}
		}

		if (mUseInstancing)
		{
			mBoidsComp->AddInstances(mInstances, false);
		}

		// ------------------------------------------------

		const std::string programString(TCHAR_TO_UTF8(*ProgramAsset->SourceCode));
		if (!mpProgram->ReadFromString(programString))
			return;

		const std::string name(TCHAR_TO_UTF8(*KernelName));
		mpKernel = std::make_unique<OpenCL::Kernel>(*mpProgram, name);

		mpPositionsBuffer = std::make_unique<OpenCL::Buffer>(mpDevice, mpContext, mPositions.GetData(), mNumberOfBoids * sizeof(FVector4f), OpenCL::AccessType::READ_WRITE, OpenCL::MemoryStrategy::STREAM);
		mpVelocitiesBuffer = std::make_unique<OpenCL::Buffer>(mpDevice, mpContext, mVelocities.GetData(), mNumberOfBoids * sizeof(FVector4f), OpenCL::AccessType::READ_WRITE, OpenCL::MemoryStrategy::STREAM);
		mpAccelerationBuffer = std::make_unique<OpenCL::Buffer>(mpDevice, mpContext, mAccelerations.GetData(), mNumberOfBoids * sizeof(FVector4f), OpenCL::AccessType::READ_WRITE, OpenCL::MemoryStrategy::STREAM);

		mpKernel->SetArgument(0, mNumberOfBoids);
		mpKernel->SetArgument(2, FVector4f(mBounds.Min.X, mBounds.Min.Y, mBounds.Min.Z, 0));
		mpKernel->SetArgument(3, FVector4f(mBounds.Max.X, mBounds.Max.Y, mBounds.Max.Z, 0));

		mpKernel->SetArgument<OpenCL::Buffer>(6, *mpPositionsBuffer);
		mpKernel->SetArgument<OpenCL::Buffer>(7, *mpVelocitiesBuffer);
		mpKernel->SetArgument<OpenCL::Buffer>(8, *mpAccelerationBuffer);
	}
	else
	{
		mpKernel = nullptr;
	}
}

void ABoidsManagerActor::BeginDestroy()
{
	Super::BeginDestroy();
}

void ABoidsManagerActor::TickActor(float DeltaTime, 
								   ELevelTick TickType, 
								   FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if (mDrawBounds)
	{
		UWorld* world = GetWorld();
		DrawDebugBox(world, mBounds.GetCenter(), mBounds.GetExtent(), FColor::Cyan, false);
	}

	if (mpKernel)
	{
		mpKernel->SetArgument<float>(1, DeltaTime);

		FVector4f influencesRadii(mSeperationRadius, mAlignmentRadius, mCohesionRadius, 0);
		FVector4f forces(mMaxSpeed, mMaxForce, mBounceFactor, 0);

		mpKernel->SetArgument(4, influencesRadii);
		mpKernel->SetArgument(5, forces);

		size_t globalWork[1] = { mNumberOfBoids };
		mpQueue->EnqueueRange(*mpKernel, 1, globalWork, nullptr);

		mpPositionsBuffer->Fetch(*mpQueue, mPositions.GetData(), mNumberOfBoids * sizeof(FVector4f));
		mpVelocitiesBuffer->Fetch(*mpQueue, mVelocities.GetData(), mNumberOfBoids * sizeof(FVector4f));
		mpAccelerationBuffer->Fetch(*mpQueue, mAccelerations.GetData(), mNumberOfBoids * sizeof(FVector4f));

		for (int32_t i = 0; i < mNumberOfBoids; ++i)
		{
			FVector forward = FVector(mVelocities[i]).GetSafeNormal();

			if (mUseInstancing)
			{
				mInstances[i] = FTransform(forward.Rotation(), FVector(mPositions[i]));
			}
			else
			{
				mBoids[i]->SetWorldLocationAndRotation(FVector(mPositions[i]), forward.Rotation());
			}
		}

		if (mUseInstancing)
		{
			mBoidsComp->BatchUpdateInstancesTransforms(0, mInstances, true, true, true);
			mBoidsComp->MarkRenderStateDirty();
		}
	}
}

void ABoidsManagerActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName propName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (propName == GET_MEMBER_NAME_CHECKED(ABoidsManagerActor, mMinimumPoint))
	{
		mBounds = FBox(EForceInit::ForceInitToZero);
		mBounds += mMinimumPoint;
		mBounds += mMaximumPoint;
	}
	else if (propName == GET_MEMBER_NAME_CHECKED(ABoidsManagerActor, mMinimumPoint))
	{
		mBounds = FBox(EForceInit::ForceInitToZero);
		mBounds += mMinimumPoint;
		mBounds += mMaximumPoint;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}