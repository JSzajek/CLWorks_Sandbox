#include "SimRopeActor.h"

#include "ProceduralMeshComponent.h"


#if WITH_EDITOR

void UEditorRopePoint::PostEditChangeProperty(struct FPropertyChangedEvent& _event)
{
	AActor* actor = GetAttachParentActor();
	if (ASimRopeActor* ropeActor = Cast<ASimRopeActor>(actor))
	{
		ropeActor->UpdatePoint(mIndex, GetComponentLocation());
	}
}

#endif

ASimRopeActor::ASimRopeActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	mpStartPoint = CreateDefaultSubobject<UEditorRopePoint>(TEXT("Start"));
	mpStartPoint->SetMobility(EComponentMobility::Type::Static);
	mpStartPoint->SetRelativeLocation(FVector::ZeroVector);
	mpStartPoint->SetupAttachment(GetRootComponent());

	mpEndPoint = CreateDefaultSubobject<UEditorRopePoint>(TEXT("End"));
	mpEndPoint->SetMobility(EComponentMobility::Type::Static);
	mpEndPoint->SetRelativeLocation(FVector(100, 0, 0));
	mpEndPoint->SetupAttachment(GetRootComponent());

	mpRopeMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Rope Mesh"));
	mpRopeMeshComponent->SetupAttachment(GetRootComponent());
	mpRopeMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mpRopeMeshComponent->bUseAsyncCooking = true;
}

void ASimRopeActor::BeginPlay()
{
	Super::BeginPlay();

	// OpenCL Dependencies Initialization -------------------------------------
	mpDevice = OpenCL::MakeDevice();
	mpContext = MakeContext(mpDevice);

	mpQueue = std::make_unique<OpenCL::CommandQueue>(mpContext, mpDevice);
	mpForcesProgram = std::make_unique<OpenCL::Program>(mpContext, mpDevice);
	mpConstraintsProgram = std::make_unique<OpenCL::Program>(mpContext, mpDevice);
	mpEnforceProgram = std::make_unique<OpenCL::Program>(mpContext, mpDevice);
	// ------------------------------------------------------------------------

	// Forces Program ---------------------------------------------------------
	const std::string forceProgramString(TCHAR_TO_UTF8(*mpForcesProgramAsset->SourceCode));
	if (!mpForcesProgram->ReadFromString(forceProgramString))
		return;

	mpForcesKernel = std::make_unique<OpenCL::Kernel>(*mpForcesProgram, TCHAR_TO_UTF8(*mForcesKernelName));
	// ------------------------------------------------------------------------

	// Constraints Program ----------------------------------------------------
	const std::string constraintsProgramString(TCHAR_TO_UTF8(*mpConstraintsProgramAsset->SourceCode));
	if (!mpConstraintsProgram->ReadFromString(constraintsProgramString))
		return;

	mpConstraintsKernel = std::make_unique<OpenCL::Kernel>(*mpConstraintsProgram, TCHAR_TO_UTF8(*mConstraintsKernelName));
	// ------------------------------------------------------------------------

	// Enforce Program --------------------------------------------------------
	const std::string enforceProgramString(TCHAR_TO_UTF8(*mpEnforceProgramAsset->SourceCode));
	if (!mpEnforceProgram->ReadFromString(enforceProgramString))
		return;

	mpEnforceKernel = std::make_unique<OpenCL::Kernel>(*mpEnforceProgram, TCHAR_TO_UTF8(*mEnforceKernelName));
	// ------------------------------------------------------------------------

	mRopeResetLength = 0;

	float distance_cm = FVector::Distance(mpStartPoint->GetComponentLocation(), mpEndPoint->GetComponentLocation());
	uint32_t numSegments = FMath::Max(FMath::FloorToInt(distance_cm / mPointSampleRate_CM), 1);
	uint32_t numPoints = numSegments + 1;

	FVector Delta = mpEndPoint->GetComponentLocation() - mpStartPoint->GetComponentLocation();

	mRopeParticles.SetNumZeroed(numPoints);
	mReadbackRopeParticles.SetNumZeroed(numPoints);
	mRopeConstraints.SetNumZeroed(numPoints - 1);

	for (uint32_t i = 0; i < numPoints; ++i)
	{
		const FVector compLocation = mpStartPoint->GetComponentLocation();

		float t = (float)i / (numPoints - 1);
		FVector Pos = compLocation + Delta * t;

		FRopeParticle& particle = mRopeParticles[i];

		particle.mPosition = FVector4f(FVector3f(Pos), 0.0f);
		particle.mPreviousPosition = particle.mPosition;
		particle.mInverseMass = 1.0f;

		if (i == 0 && mpStartPoint->mIsPinned)
			particle.mInverseMass = 0.0f;

		if (i == numPoints - 1 && mpEndPoint->mIsPinned)
			particle.mInverseMass = 0.0f;

		if (i < numPoints - 1)
		{
			FVector nextPos = compLocation + Delta * ((float)(i + 1) / (numPoints - 1));

			FRopeConstraint& constraint = mRopeConstraints[i];
			constraint.mPoint1 = i;
			constraint.mPoint2 = i + 1;
			constraint.mRestLength = FVector::Dist(Pos, nextPos);

			mRopeResetLength += constraint.mRestLength;
		}
	}

	mpStartPoint->mIndex = 0;
	mpEndPoint->mIndex = numPoints - 1;

	mpParticlesBuffer = std::make_unique<OpenCL::Buffer>(mpDevice, 
														 mpContext, 
														 mRopeParticles.GetData(), 
														 mRopeParticles.Num() * sizeof(FRopeParticle), 
														 OpenCL::AccessType::READ_WRITE, 
														 OpenCL::MemoryStrategy::ZERO_COPY);

	mpConstraintsBuffer = std::make_unique<OpenCL::Buffer>(mpDevice, 
														   mpContext, 
														   mRopeConstraints.GetData(), 
														   mRopeConstraints.Num() * sizeof(FRopeConstraint), 
														   OpenCL::AccessType::READ_WRITE, 
														   OpenCL::MemoryStrategy::ZERO_COPY);

	mpForcesKernel->SetArgument<OpenCL::Buffer>(0, *mpParticlesBuffer);

	mpConstraintsKernel->SetArgument<OpenCL::Buffer>(0, *mpParticlesBuffer);
	mpConstraintsKernel->SetArgument<OpenCL::Buffer>(1, *mpConstraintsBuffer);

	mpEnforceKernel->SetArgument<OpenCL::Buffer>(0, *mpParticlesBuffer);
	mpEnforceKernel->SetArgument<OpenCL::Buffer>(1, *mpConstraintsBuffer);
	mpEnforceKernel->SetArgument(2, mRopeConstraints.Num());
	mpEnforceKernel->SetArgument(3, mRopeResetLength);

	// Start Readback Loop
	BeginReadback(GetWorld()->GetDeltaSeconds());

	mSimulating = true;
}

void ASimRopeActor::BeginDestroy()
{
	mSimulating = false;

	mpParticlesBuffer = nullptr;
	mpConstraintsBuffer = nullptr;

	mpConstraintsKernel = nullptr;
	mpConstraintsProgram = nullptr;

	mpForcesKernel = nullptr;
	mpForcesProgram = nullptr;

	mpContext = nullptr;
	mpDevice = nullptr;

	Super::BeginDestroy();
}

void ASimRopeActor::TickActor(float DeltaTime, 
							  ELevelTick TickType, 
							  FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if (mSimulating)
	{
		if (bSwapReady.load(std::memory_order_acquire))
		{
			{
				const std::scoped_lock lock(BufferSwapLock);
				Swap(mRopeParticles, mReadbackRopeParticles);
			}
			bSwapReady.store(false, std::memory_order_release);

			UpdateRopeMesh();

			BeginReadback(DeltaTime);
		}

	}

#if WITH_EDITOR
	if (!mSimulating && mDrawLine)
	{
		UWorld* world = GetWorld();
		DrawDebugBox(world, mpStartPoint->GetComponentLocation(), FVector(10), FColor::Green);
		DrawDebugLine(world, mpStartPoint->GetComponentLocation(), mpEndPoint->GetComponentLocation(), FColor::Cyan);
		DrawDebugBox(world, mpEndPoint->GetComponentLocation(), FVector(10), FColor::Green);
	}
#endif
}

void ASimRopeActor::UpdatePoint(int32 index, const FVector& position)
{
	if (mRopeParticles.IsValidIndex(index))
	{
		mRopeParticles[index].mPosition = FVector4f(FVector3f(position), 0.0f);

		mpParticlesBuffer->Upload(*mpQueue, mRopeParticles.GetData(), mRopeParticles.Num() * sizeof(FRopeParticle));
	}
}

void ASimRopeActor::BeginReadback(double DeltaTime)
{
	const FVector4f gravityVector(FVector3f(mGravityForce_MPerS * 100), 0.0f);

	if (mpForcesKernel)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASimRopeActor::ProcessForces);

		mpForcesKernel->SetArgument<FVector4f>(1, gravityVector);
		mpForcesKernel->SetArgument<float>(2, mDampning);
		mpForcesKernel->SetArgument<float>(3, DeltaTime);

		size_t globalWork[1] = { mRopeParticles.Num() };
		mpQueue->EnqueueRange(*mpForcesKernel, 1, globalWork);
	}

	if (mpConstraintsKernel)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASimRopeActor::ProcessConstraints);

		mpConstraintsKernel->SetArgument<float>(2, mStiffness);

		size_t globalWork[1] = { mRopeConstraints.Num() };
		for (int32_t i = 0; i < mNumSolverIterations; ++i)
		{
			mpQueue->EnqueueRange(*mpConstraintsKernel, 1, globalWork);
		}
	}

	if (mpEnforceKernel)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASimRopeActor::Enforce);

		size_t globalWork[1] = { 1 };
		mpQueue->EnqueueRange(*mpEnforceKernel, 1, globalWork);
	}

	if (mpParticlesBuffer)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASimRopeActor::FetchParticles);

		mpParticlesBuffer->FetchAsync(mpQueue, [this]()
		{
			OnReadbackComplete();

		}, mReadbackRopeParticles.GetData(), mReadbackRopeParticles.NumBytes());
	}
}

void ASimRopeActor::OnReadbackComplete()
{
	bSwapReady.store(true, std::memory_order_release);;
}

void ASimRopeActor::UpdateRopeMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASimRopeActor::UpdateRopeMesh);

	if (mRopeParticles.Num() < 2 || mRopeSides < 3) 
		return;


	const FTransform toWorldTransform = mpRopeMeshComponent->GetComponentToWorld();

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    int32 NumSegments = mRopeParticles.Num();

    Vertices.Reserve(NumSegments * mRopeSides);
    Triangles.Reserve((NumSegments - 1) * mRopeSides * 6);

    FVector PrevUp = FVector::UpVector;

    for (int32 i = 0; i < NumSegments; ++i)
    {
        FVector Center = FVector(mRopeParticles[i].mPosition);
		Center = toWorldTransform.InverseTransformPosition(Center);

        // Compute direction to next particle
        FVector Forward;
        if (i < NumSegments - 1)
            Forward = (FVector(mRopeParticles[i + 1].mPosition) - Center).GetSafeNormal();
        else
            Forward = (Center - FVector(mRopeParticles[i - 1].mPosition)).GetSafeNormal();

        FVector Right = FVector::CrossProduct(Forward, PrevUp).GetSafeNormal();
        if (Right.IsNearlyZero()) Right = FVector::RightVector;

        FVector Up = FVector::CrossProduct(Right, Forward).GetSafeNormal();
        PrevUp = Up; // carry to next loop

        // Generate ring vertices
        for (int32 j = 0; j < mRopeSides; ++j)
        {
            float Angle_Rad = 2.f * PI * j / mRopeSides;
            FVector Radial = (FMath::Cos(Angle_Rad) * Up) + (FMath::Sin(Angle_Rad) * Right);
            FVector Pos = Center + Radial * mRopeRadius_CM;

            Vertices.Add(Pos);
            Normals.Add(Radial);
            UVs.Add(FVector2D((float)j / mRopeSides, (float)i / (NumSegments - 1)));
            Tangents.Add(FProcMeshTangent(Right, false));
        }
    }

    // Generate triangle strips between each ring
    for (int32 i = 0; i < NumSegments - 1; ++i)
    {
        int32 RingStart = i * mRopeSides;
        int32 NextRingStart = (i + 1) * mRopeSides;

        for (int32 j = 0; j < mRopeSides; ++j)
        {
            int32 Curr = RingStart + j;
            int32 Next = RingStart + (j + 1) % mRopeSides;
            int32 CurrNext = NextRingStart + j;
            int32 NextNext = NextRingStart + (j + 1) % mRopeSides;

            // Two triangles per quad
            Triangles.Add(Next);
            Triangles.Add(CurrNext);
            Triangles.Add(Curr);

            Triangles.Add(NextNext);
            Triangles.Add(CurrNext);
            Triangles.Add(Next);
        }
    }

	mpRopeMeshComponent->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, {}, Tangents, true);
    mpRopeMeshComponent->SetMaterial(0, mpRopeMaterial ? mpRopeMaterial : mpRopeMeshComponent->GetMaterial(0)); // Keep material
}