#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PhAtLibrary.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * 
 */
UCLASS()
class PHATMERGE_API UPhATLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    static UPhysicsAsset* MergePhysicsAssets(TArray<UPhysicsAsset*> PhysicsAssets, USkeletalMesh* SkeletalMesh);
};
