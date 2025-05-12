#include "DemoActor.h"

ADemoActor::ADemoActor()
{
	mWriteColor = FLinearColor::Red;
}

void ADemoActor::CreateTexture2D()
{
	OpenCL::Context context(mDevice);
	OpenCL::Program program(mDevice, context);

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
		mpTexture2D = cltexture.CreateUTexture2D(queue);
	}
	else
	{
		cltexture.UploadToUTexture2D(mpTexture2D, queue);
	}
}
