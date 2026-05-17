#include "GPUNoiseTextureGenActor.h"

#include "Kismet/GameplayStatics.h"

AGPUNoiseTextureGenActor::AGPUNoiseTextureGenActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGPUNoiseTextureGenActor::BeginPlay()
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

	if (mTextureSize > 0)
	{
		mpGPUImageObj = NewObject<UGPUImageObject>();
		mpGPUImageObj->CreateImage2D(mpGPUContextObj,
									 mTextureSize,
									 mTextureSize,
									 EGpuPixelFormat::RGBA8);

		mpGPUProgramObj->SetImageArg(0, mpGPUImageObj);
	}
}

void AGPUNoiseTextureGenActor::BeginDestroy()
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

void AGPUNoiseTextureGenActor::TickActor(float DeltaTime, 
										ELevelTick TickType, 
										FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if (mpGPUProgramObj && mpGPUImageObj)
	{
		float realTime_s = static_cast<float>(UGameplayStatics::GetRealTimeSeconds(GetWorld()));
		mpGPUProgramObj->SetFloatArg(1, realTime_s);

		Gpu::DispatchDescription dispatchDesc;
		dispatchDesc.Dim = 2;
		dispatchDesc.Global[0] = mTextureSize;
		dispatchDesc.Global[1] = mTextureSize;
		std::shared_ptr<Gpu::IEvent> event = mpGPUContextObj->GetDefaultQueue()->Dispatch(*mpGPUProgramObj->GetKernel(), dispatchDesc);
		event->Wait();

		// Create a texture in UE
		if (!mpTexture2D)
		{
			mpTexture2D = mpGPUImageObj->CreateTexture2D(mpGPUContextObj,
														 false,
														 false);

			if (mpVisualMesh)
			{
				if (UMaterialInterface* material = mpVisualMesh->GetMaterial(0))
				{
					mpDynamicMaterial = UMaterialInstanceDynamic::Create(material, this);

					if (mpTexture2D)
					{
						mpDynamicMaterial->SetTextureParameterValue("Noise", mpTexture2D);
					}

					mpVisualMesh->SetMaterial(0, mpDynamicMaterial);
				}
			}
		}
		else
		{
			mpGPUImageObj->UpdateTexture2D(mpGPUContextObj, mpTexture2D);
		}
	}
}