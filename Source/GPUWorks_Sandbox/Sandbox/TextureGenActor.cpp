#include "TextureGenActor.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/VolumeTexture.h"

ATextureGenActor::ATextureGenActor()
{
	mWriteColor = FLinearColor::Red;
}

void ATextureGenActor::BeginPlay()
{
	Super::BeginPlay();

	mpGPUContextObject = NewObject<UGPUContextObject>();
	if (mpGPUContextObject)
	{
		mpGPUContextObject->Initialize(EGPUBackend::OpenCL);
		mpGPUContextObject->CreateDefaultQueue();
	}
}

void ATextureGenActor::BeginDestroy()
{
	Super::BeginDestroy();
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


#if 0
	OpenCL::ContextPtr context = MakeContext(mpDevice);
	OpenCL::Program program(context, mpDevice);

	OpenCL::Image cltexture(context,
							mpDevice,
							mTextureWidth, 
							mTextureHeight, 
							1,		
							OpenCL::Image::Format::RGBA8, 
							OpenCL::Image::Type::Texture2D);

	program.ReadFromString("__kernel void write_color_img(read_write image2d_t output, __global const float* write_color)\n" 
						   "{ const int2 coord = (int2)(get_global_id(0), get_global_id(1)); \n"
						   "  const float4 color = (float4)(write_color[0], write_color[1], write_color[2], write_color[3]); \n"
						   "  write_imagef(output, coord, color); }");

	OpenCL::Kernel kernel(program, "write_color_img");
	OpenCL::CommandQueue queue(context, mpDevice);

	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	OpenCL::Buffer colorBuffer(mpDevice, context, writeColor, 4 * sizeof(float), OpenCL::AccessType::WRITE_ONLY, OpenCL::MemoryStrategy::COPY_ONCE);

	kernel.SetArgument(0, cltexture.Get());
	kernel.SetArgument<OpenCL::Buffer>(1, colorBuffer);

	size_t global_work_size[2] = { mTextureWidth, mTextureHeight };
	queue.EnqueueRange(kernel, 2, global_work_size);

	// Create a texture in UE
	if (!mpTexture2D)
	{
		mpTexture2D = cltexture.CreateUTexture2D(queue, mIsSRGB, mGenerateMips, mIsAsyncGen);
	}
	else
	{
		cltexture.UploadToUTexture2D(mpTexture2D, queue, mGenerateMips, mIsAsyncGen);
	}
#endif
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

#if 0
	OpenCL::ContextPtr context = MakeContext(mpDevice);
	OpenCL::Program program(context, mpDevice);

	OpenCL::Image cltexture(context,
							mpDevice,
							mTextureWidth, 
							mTextureHeight, 
							mTextureLayers,		
							OpenCL::Image::Format::RGBA8, 
							OpenCL::Image::Type::Texture2DArray);

	program.ReadFromString("__kernel void write_color_img(read_write image2d_array_t output, __global const float* write_color, float num_layers)\n" 
						   "{ const int4 coord = (int4)(get_global_id(0), get_global_id(1), get_global_id(2), 0); \n"
						   "  float layer_mod = 1.0f - ((float)get_global_id(2) / num_layers); \n"
						   "  const float4 color = (float4)(write_color[0] * layer_mod, write_color[1] * layer_mod, write_color[2] * layer_mod, write_color[3]); \n"
						   "  write_imagef(output, coord, color); }");

	OpenCL::Kernel kernel(program, "write_color_img");
	OpenCL::CommandQueue queue(context, mpDevice);

	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	OpenCL::Buffer colorBuffer(mpDevice, context, writeColor, 4 * sizeof(float), OpenCL::AccessType::WRITE_ONLY, OpenCL::MemoryStrategy::COPY_ONCE);

	kernel.SetArgument(0, cltexture.Get());
	kernel.SetArgument<OpenCL::Buffer>(1, colorBuffer);
	kernel.SetArgument(2, static_cast<float>(mTextureLayers));

	size_t global_work_size[3] = { mTextureWidth, mTextureHeight, mTextureLayers };
	queue.EnqueueRange(kernel, 3, global_work_size);

	// Create a texture in UE
	if (!mpTexture2DArray)
	{
		mpTexture2DArray = cltexture.CreateUTexture2DArray(queue, mIsSRGB, mGenerateMips);
	}
	else
	{
		cltexture.UploadToUTexture2DArray(mpTexture2DArray, queue, mGenerateMips);
	}
#endif
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
	event->Wait();

	// Upload to render target
	image->WriteToRenderTarget2D(mpGPUContextObject, mpTargetRenderTarget2D);

#if 0
	if (!mpTargetRenderTarget2D)
		return;

	OpenCL::ContextPtr context = MakeContext(mpDevice);
	OpenCL::Program program(context, mpDevice);

	OpenCL::Image cltexture(context,
							mpDevice,
							mTextureWidth, 
							mTextureHeight, 
							1,		
							OpenCL::Image::Format::RGBA8, 
							OpenCL::Image::Type::Texture2D);

	program.ReadFromString("__kernel void write_color_img(read_write image2d_t output, __global const float* write_color)\n" 
						   "{ const int2 coord = (int2)(get_global_id(0), get_global_id(1)); \n"
						   "  const float4 color = (float4)(write_color[0], write_color[1], write_color[2], write_color[3]); \n"
						   "  write_imagef(output, coord, color); }");

	OpenCL::Kernel kernel(program, "write_color_img");
	OpenCL::CommandQueue queue(context, mpDevice);

	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	OpenCL::Buffer colorBuffer(mpDevice, context, writeColor, 4 * sizeof(float), OpenCL::AccessType::WRITE_ONLY, OpenCL::MemoryStrategy::COPY_ONCE);

	kernel.SetArgument(0, cltexture.Get());
	kernel.SetArgument<OpenCL::Buffer>(1, colorBuffer);

	size_t global_work_size[2] = { mTextureWidth, mTextureHeight };
	queue.EnqueueRange(kernel, 2, global_work_size);

	cltexture.UploadToUTextureRenderTarget2D(mpTargetRenderTarget2D, queue, mGenerateMips);
#endif
}