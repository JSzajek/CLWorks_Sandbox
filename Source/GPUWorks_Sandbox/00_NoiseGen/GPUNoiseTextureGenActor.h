#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GPUWorksLib.h"

#include <memory>

#include "GPUNoiseTextureGenActor.generated.h"

UCLASS()
class AGPUNoiseTextureGenActor : public AActor
{
	GENERATED_BODY()
public:
	AGPUNoiseTextureGenActor();

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	/**
	 * Dispatches the once-per frame Tick() function for this actor
	 * @param	DeltaTime			The time slice of this tick
	 * @param	TickType			The type of tick that is happening
	 * @param	ThisTickFunction	The tick function that is firing, useful for getting the completion handle
	 */
	virtual void TickActor(float DeltaTime, 
						   enum ELevelTick TickType, 
						   FActorTickFunction& ThisTickFunction) override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CLNoise")
	TObjectPtr<UGPUProgramAsset> mpProgramAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CLNoise")
	FString mKernelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CLNoise")
	int32 mTextureSize = 256;

	UPROPERTY(VisibleAnywhere, Transient, Category = "CLNoise|Debug")
	TObjectPtr<UTexture2D> mpTexture2D = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> mpVisualMesh = nullptr;
private:
	TObjectPtr<UMaterialInstanceDynamic> mpDynamicMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UGPUContextObject> mpGPUContextObj = nullptr;

	UPROPERTY()
	TObjectPtr<UGPUProgramObject> mpGPUProgramObj = nullptr;

	UPROPERTY()
	TObjectPtr<UGPUImageObject> mpGPUImageObj = nullptr;
};