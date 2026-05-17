#include "BoidsManagerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Misc/App.h"
#include "Kismet/KismetMathLibrary.h"

ABoidsManagerActor::ABoidsManagerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	mBounds = FBox(EForceInit::ForceInitToZero);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABoidsManagerActor::BeginPlay()
{
	Super::BeginPlay();

	// Dependencies Initialization --------------------------------------------
	Gpu::FactoryDesc factoryDesc;
	factoryDesc.PreferredBackend = Gpu::Backend::OpenCL;
	factoryDesc.bAllowFallback = false;

	mpGPUCore = Gpu::Factory::Create(factoryDesc);
	mpGPUDevice = mpGPUCore->GetDevice(0);
	mpGPUContext = mpGPUCore->CreateContext(mpGPUDevice);

	mpGPUQueue = mpGPUContext->CreateQueue();
	// ------------------------------------------------------------------------

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

			mBoidsComp->SetStaticMesh(mpMesh);
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

				boid->SetStaticMesh(mpMesh);

				boid->SetWorldLocation(randPos);
				mBoids.Add(boid);
			}
		}

		if (mUseInstancing)
		{
			mBoidsComp->AddInstances(mInstances, false);
		}

		// ------------------------------------------------

		FString programSource = mpProgramAsset->GetSourceCodeForBackend(mpGPUContext->GetBackend());
		const std::string programStr(TCHAR_TO_UTF8(*programSource));
		mpGPUProgram = mpGPUContext->CreateProgramFromSource(programStr);

		const std::string kernelStr(TCHAR_TO_UTF8(*mKernelName));
		mpGPUKernel = mpGPUProgram->CreateKernel(kernelStr);

		Gpu::BufferDescription bufferDesc;
		bufferDesc.InitialData = mPositions.GetData();
		bufferDesc.SizeBytes = mNumberOfBoids * sizeof(FVector4f);
		bufferDesc.SyncMode = Gpu::BufferSyncMode::ZeroCopy;
		mpPositionsBuffer = mpGPUContext->CreateBuffer(bufferDesc);

		bufferDesc.InitialData = mVelocities.GetData();
		mpVelocitiesBuffer = mpGPUContext->CreateBuffer(bufferDesc);

		bufferDesc.InitialData = mVelocities.GetData();
		mpAccelerationBuffer = mpGPUContext->CreateBuffer(bufferDesc);

		mpGPUKernel->SetValueArg(0, mNumberOfBoids);
		mpGPUKernel->SetValueArg(2, FVector4f(mBounds.Min.X, mBounds.Min.Y, mBounds.Min.Z, 0));
		mpGPUKernel->SetValueArg(3, FVector4f(mBounds.Max.X, mBounds.Max.Y, mBounds.Max.Z, 0));

		mpGPUKernel->SetBufferArg(6, *mpPositionsBuffer);
		mpGPUKernel->SetBufferArg(7, *mpVelocitiesBuffer);
		mpGPUKernel->SetBufferArg(8, *mpAccelerationBuffer);
	}
}

void ABoidsManagerActor::BeginDestroy()
{
	mpPositionsBuffer = nullptr;
	mpVelocitiesBuffer = nullptr;
	mpAccelerationBuffer = nullptr;

	mpGPUKernel = nullptr;
	mpGPUProgram = nullptr;

	mpGPUContext = nullptr;
	mpGPUDevice = nullptr;

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

	if (frame % 2 == 0)
	{
		if (mpGPUKernel)
		{
			mpGPUKernel->SetValueArg(1, DeltaTime);

			FVector4f influencesRadii(mSeperationRadius, mAlignmentRadius, mCohesionRadius, 0);
			FVector4f forces(mMaxSpeed, mMaxForce, mBounceFactor, 0);

			mpGPUKernel->SetValueArg(4, influencesRadii);
			mpGPUKernel->SetValueArg(5, forces);

			size_t globalWork[1] = { mNumberOfBoids };
			Gpu::DispatchDescription dispatchDesc;
			dispatchDesc.Dim = 1;
			dispatchDesc.Global[0] = mNumberOfBoids;
			mpGPUQueue->Dispatch(*mpGPUKernel, dispatchDesc);

			const size_t dataSize = mNumberOfBoids * sizeof(FVector4f);
			mpPositionsBuffer->DownloadAsync(*mpGPUQueue, mPositions.GetData(), dataSize);
			mpVelocitiesBuffer->DownloadAsync(*mpGPUQueue, mVelocities.GetData(), dataSize);
			mpFinalGPUEvent = mpAccelerationBuffer->DownloadAsync(*mpGPUQueue, mAccelerations.GetData(), dataSize);
		}
	}
	else
	{
		if (mpFinalGPUEvent)
		{
			mpFinalGPUEvent->Wait();

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

			mpFinalGPUEvent = nullptr;
		}
	}

	frame = (frame + 1) % 2;
}

#if WITH_EDITOR

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

#endif