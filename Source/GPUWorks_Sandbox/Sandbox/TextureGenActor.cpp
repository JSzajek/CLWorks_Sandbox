#include "TextureGenActor.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/VolumeTexture.h"

ATextureGenActor::ATextureGenActor()
{
	mWriteColor = FLinearColor::Red;
}

void ATextureGenActor::PostLoad()
{
	Super::PostLoad();

	mpGPUContextObject = NewObject<UGPUContextObject>();
	if (mpGPUContextObject)
	{
		mpGPUContextObject->Initialize(EGPUBackend::OpenCL);
		mpGPUContextObject->CreateDefaultQueue();
	}
}

void ATextureGenActor::ClearTexture()
{
	if (mpTexture2D)
	{
		mpTexture2D->ConditionalBeginDestroy();
		mpTexture2D = nullptr;
	}

	if (mpTexture2DArray)
	{
		mpTexture2DArray->ConditionalBeginDestroy();
		mpTexture2DArray = nullptr;
	}
}

void ATextureGenActor::CreateTexture2D()
{
	UGPUImageObject* image = NewObject<UGPUImageObject>();
	image->CreateImage2D(mpGPUContextObject,
						 mTextureWidth,
						 mTextureHeight,
						 EGpuPixelFormat::RGBA8);

	UGPUBufferObject* buffer = NewObject<UGPUBufferObject>();
	buffer->Initialize(mpGPUContextObject, 4 * sizeof(float), false, true);
	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	buffer->UploadFloatArray(mpGPUContextObject, TArray<float>(writeColor, 4));

	const FString programString("__kernel void write_color_img(read_write image2d_t output, __global const float* write_color)\n" 
								"{ const int2 coord = (int2)(get_global_id(0), get_global_id(1)); \n"
								"  const float4 color = (float4)(write_color[0], write_color[1], write_color[2], write_color[3]); \n"
								"  write_imagef(output, coord, color); }");

	UGPUProgramObject* program = NewObject<UGPUProgramObject>();
	program->BuildFromSource(mpGPUContextObject, programString);

	program->SetKernel("write_color_img");

	program->SetImageArg(0, image);
	program->SetBufferArg(1, buffer);

	Gpu::DispatchDescription dispatchDesc;
	dispatchDesc.Dim = 2;
	dispatchDesc.Global[0] = mTextureWidth;
	dispatchDesc.Global[1] = mTextureHeight;
	std::shared_ptr<Gpu::IEvent> event = mpGPUContextObject->GetDefaultQueue()->Dispatch(*program->GetKernel(), dispatchDesc);
	if (!event)
		return;

	event->Wait();

	// Create a texture in UE
	if (!mpTexture2D)
	{
		mpTexture2D = image->CreateTexture2D(mpGPUContextObject, mIsSRGB, mGenerateMips);
	}
	else
	{
		image->UpdateTexture2D(mpGPUContextObject, mpTexture2D);
	}
}

void ATextureGenActor::CreateTexture2DArray()
{
	UGPUImageObject* image = NewObject<UGPUImageObject>();
	image->CreateImage2DArray(mpGPUContextObject,
						 mTextureWidth,
						 mTextureHeight,
						 mTextureLayers,
						 EGpuPixelFormat::RGBA8);

	UGPUBufferObject* buffer = NewObject<UGPUBufferObject>();
	buffer->Initialize(mpGPUContextObject, 4 * sizeof(float), false, true);
	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	buffer->UploadFloatArray(mpGPUContextObject, TArray<float>(writeColor, 4));

	const FString programString("__kernel void write_color_img(read_write image2d_array_t output, __global const float* write_color, float num_layers)\n" 
								"{ const int4 coord = (int4)(get_global_id(0), get_global_id(1), get_global_id(2), 0); \n"
								"  float layer_mod = 1.0f - ((float)get_global_id(2) / num_layers); \n"
								"  const float4 color = (float4)(write_color[0] * layer_mod, write_color[1] * layer_mod, write_color[2] * layer_mod, write_color[3]); \n"
								"  write_imagef(output, coord, color); }");

	UGPUProgramObject* program = NewObject<UGPUProgramObject>();
	program->BuildFromSource(mpGPUContextObject, programString);

	program->SetKernel("write_color_img");

	program->SetImageArg(0, image);
	program->SetBufferArg(1, buffer);
	program->SetFloatArg(2, static_cast<float>(mTextureLayers));

	Gpu::DispatchDescription dispatchDesc;
	dispatchDesc.Dim = 3;
	dispatchDesc.Global[0] = mTextureWidth;
	dispatchDesc.Global[1] = mTextureHeight;
	dispatchDesc.Global[2] = mTextureLayers;
	std::shared_ptr<Gpu::IEvent> event = mpGPUContextObject->GetDefaultQueue()->Dispatch(*program->GetKernel(), dispatchDesc);
	if (!event)
		return;

	event->Wait();

	// Create a texture in UE
	if (!mpTexture2DArray)
	{
		mpTexture2DArray = image->CreateTexture2DArray(mpGPUContextObject, mIsSRGB, mGenerateMips);
	}
	else
	{
		image->UpdateTexture2DArray(mpGPUContextObject, mpTexture2DArray);
	}
}

void ATextureGenActor::CopyTexture2DToRenderTarget2D()
{
	if (!mpTargetRenderTarget2D)
		return;

	UGPUImageObject* image = NewObject<UGPUImageObject>();
	image->CreateImage2D(mpGPUContextObject,
						 mTextureWidth,
						 mTextureHeight,
						 EGpuPixelFormat::RGBA8);

	UGPUBufferObject* buffer = NewObject<UGPUBufferObject>();
	buffer->Initialize(mpGPUContextObject, 4 * sizeof(float), false, true);
	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	buffer->UploadFloatArray(mpGPUContextObject, TArray<float>(writeColor, 4));

	const FString programString("__kernel void write_color_img(read_write image2d_t output, __global const float* write_color)\n" 
								"{ const int2 coord = (int2)(get_global_id(0), get_global_id(1)); \n"
								"  const float4 color = (float4)(write_color[0], write_color[1], write_color[2], write_color[3]); \n"
								"  write_imagef(output, coord, color); }");

	UGPUProgramObject* program = NewObject<UGPUProgramObject>();
	program->BuildFromSource(mpGPUContextObject, programString);

	program->SetKernel("write_color_img");

	program->SetImageArg(0, image);
	program->SetBufferArg(1, buffer);

	Gpu::DispatchDescription dispatchDesc;
	dispatchDesc.Dim = 2;
	dispatchDesc.Global[0] = mTextureWidth;
	dispatchDesc.Global[1] = mTextureHeight;
	std::shared_ptr<Gpu::IEvent> event = mpGPUContextObject->GetDefaultQueue()->Dispatch(*program->GetKernel(), dispatchDesc);
	if (!event)
		return;

	event->Wait();

	// Upload to render target
	image->WriteToRenderTarget2D(mpGPUContextObject, mpTargetRenderTarget2D);
}