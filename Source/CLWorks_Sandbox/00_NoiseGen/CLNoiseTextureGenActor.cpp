#include "CLNoiseTextureGenActor.h"

#include "Kismet/GameplayStatics.h"

ACLNoiseTextureGenActor::ACLNoiseTextureGenActor()
{
	PrimaryActorTick.bCanEverTick = true;

	
}

void ACLNoiseTextureGenActor::BeginPlay()
{
	Super::BeginPlay();

	// OpenCL Dependencies Initialization -------------------------------------
	mpDevice = OpenCL::MakeDevice();
	mpContext = MakeContext(mpDevice);

	mpQueue = std::make_unique<OpenCL::CommandQueue>(mpContext, mpDevice);
	mpProgram = std::make_unique<OpenCL::Program>(mpContext, mpDevice);
	// ------------------------------------------------------------------------

	if (mTextureSize > 0)
	{
		const std::string programString(TCHAR_TO_UTF8(*mpProgramAsset->SourceCode));
		if (!mpProgram->ReadFromString(programString))
			return;

		const std::string name(TCHAR_TO_UTF8(*mKernelName));
		mpKernel = std::make_unique<OpenCL::Kernel>(*mpProgram, name);

		mpImage = std::make_unique<OpenCL::Image>(mpContext,
												  mpDevice,
												  mTextureSize, 
												  mTextureSize, 
												  1,		
												  OpenCL::Image::Format::RGBA8, 
												  OpenCL::Image::Type::Texture2D);

		mpKernel->SetArgument(0, mpImage->Get());
	}
}

void ACLNoiseTextureGenActor::BeginDestroy()
{
	mpImage = nullptr;

	mpKernel = nullptr;
	mpProgram = nullptr;

	mpContext = nullptr;
	mpDevice = nullptr;

	Super::BeginDestroy();
}

void ACLNoiseTextureGenActor::TickActor(float DeltaTime, 
										ELevelTick TickType, 
										FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if (mpImage)
	{
		float realTime_s = static_cast<float>(UGameplayStatics::GetRealTimeSeconds(GetWorld()));
		mpKernel->SetArgument(1, realTime_s);

		size_t global_work_size[2] = { mTextureSize, mTextureSize };
		mpQueue->EnqueueRange(*mpKernel, 2, global_work_size);

		// Create a texture in UE
		if (!mpTexture2D)
		{
			mpTexture2D = mpImage->CreateUTexture2D(*mpQueue, false, false);

			if (mpVisualMesh)
			{
				if (UMaterialInterface* material = mpVisualMesh->GetMaterial(0))
				{
					mpDynamicMaterial = UMaterialInstanceDynamic::Create(material, this);

					if (mpTexture2D)
						mpDynamicMaterial->SetTextureParameterValue("Noise", mpTexture2D);

					mpVisualMesh->SetMaterial(0, mpDynamicMaterial);
				}
			}
		}
		else
		{
			mpImage->UploadToUTexture2D(mpTexture2D, *mpQueue, false);
		}
	}
}