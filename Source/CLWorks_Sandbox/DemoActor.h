#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CLWorksLib.h"

#include "DemoActor.generated.h"

UCLASS()
class ADemoActor : public AActor
{
	GENERATED_BODY()
public:
	ADemoActor();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "CL")
	void ClearTexture();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "CL")
	void CreateTexture2D();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "CL")
	void CreateTexture2DArray();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "CL")
	void CopyTexture2DToRenderTarget2D();
public:
	UPROPERTY(VisibleAnywhere, Transient, Category = "CL")
	TObjectPtr<UTexture2D> mpTexture2D = nullptr;

	UPROPERTY(VisibleAnywhere, Transient, Category = "CL")
	TObjectPtr<UTexture2DArray> mpTexture2DArray = nullptr;

	UPROPERTY(EditAnywhere, Category = "CL")
	TObjectPtr<UTextureRenderTarget2D> mpTargetRenderTarget2D = nullptr;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	FLinearColor mWriteColor;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	uint32 mTextureWidth = 0;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	uint32 mTextureHeight = 0;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	uint32 mTextureLayers = 1;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	bool mGenerateMips = false;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	bool mIsSRGB = true;

	UPROPERTY(EditAnywhere, Category = "CL|Texture")
	bool mIsAsyncGen = false;
private:
	OpenCL::DevicePtr mpDevice;
};