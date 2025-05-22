#include "DemoActor.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/VolumeTexture.h"

ADemoActor::ADemoActor()
{
	mWriteColor = FLinearColor::Red;
}

void ADemoActor::ClearTexture()
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

void ADemoActor::CreateTexture2D()
{
	OpenCL::Context context(mDevice);
	OpenCL::Program program(context, mDevice);

	OpenCL::Image cltexture(context,
							mDevice,
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
	OpenCL::CommandQueue queue(context, mDevice);

	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	OpenCL::Buffer colorBuffer(context, writeColor, 4 * sizeof(float), OpenCL::AccessType::READ_ONLY);

	kernel.SetArgument(0, cltexture.Get());
	kernel.SetArgument<cl_mem>(1, colorBuffer);

	size_t global_work_size[2] = { mTextureWidth, mTextureHeight };
	queue.EnqueueRange(kernel, 2, global_work_size);

	// Create a texture in UE
	if (!mpTexture2D)
	{
		mpTexture2D = cltexture.CreateUTexture2D(queue, mGenerateMips);
	}
	else
	{
		cltexture.UploadToUTexture2D(mpTexture2D, queue, mGenerateMips);
	}
}

void ADemoActor::CreateTexture2DArray()
{
	OpenCL::Context context(mDevice);
	OpenCL::Program program(context, mDevice);

	OpenCL::Image cltexture(context,
							mDevice,
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
	OpenCL::CommandQueue queue(context, mDevice);

	float writeColor[4] = { mWriteColor.R, mWriteColor.G, mWriteColor.B, mWriteColor.A };
	OpenCL::Buffer colorBuffer(context, writeColor, 4 * sizeof(float), OpenCL::AccessType::READ_ONLY);

	kernel.SetArgument(0, cltexture.Get());
	kernel.SetArgument<cl_mem>(1, colorBuffer);
	kernel.SetArgument(2, static_cast<float>(mTextureLayers));

	size_t global_work_size[3] = { mTextureWidth, mTextureHeight, mTextureLayers };
	queue.EnqueueRange(kernel, 3, global_work_size);

	// Create a texture in UE
	if (!mpTexture2DArray)
	{
		mpTexture2DArray = cltexture.CreateUTexture2DArray(queue, mGenerateMips);
	}
	else
	{
		cltexture.UploadToUTexture2DArray(mpTexture2DArray, queue, mGenerateMips);
	}
}
