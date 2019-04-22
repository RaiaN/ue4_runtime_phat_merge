#include "PhAtLibrary.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "PreviewScene.h"
#include "Engine/World.h"
#include "Animation/SkeletalMeshActor.h"


// PRAGMA_DISABLE_OPTIMIZATION

int32 CreateNewSKBody(UPhysicsAsset* PhysAsset, const FName& InBodyName)
{
    check(PhysAsset);

    int32 BodyIndex = PhysAsset->FindBodyIndex(InBodyName);
    if (BodyIndex != INDEX_NONE)
    {
        return BodyIndex; // if we already have one for this name - just return that.
    }

    USkeletalBodySetup* NewBodySetup = NewObject<USkeletalBodySetup>(PhysAsset, NAME_None, RF_Transactional);
    // make default to be use complex as simple 
    NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    // newly created bodies default to simulating
    NewBodySetup->PhysicsType = PhysType_Default;

    int32 BodySetupIndex = PhysAsset->SkeletalBodySetups.Add(NewBodySetup);
    NewBodySetup->BoneName = InBodyName;

    PhysAsset->UpdateBodySetupIndexMap();
    PhysAsset->UpdateBoundsBodiesArray();

    for (int i = 0; i < PhysAsset->SkeletalBodySetups.Num(); ++i)
    {
        PhysAsset->DisableCollision(i, BodySetupIndex);
    }
    // Return index of new body.
    return BodySetupIndex;
}

UPhysicsAsset* UPhATLibrary::MergePhysicsAssets(TArray<UPhysicsAsset*> PhysicsAssets, USkeletalMesh* SkeletalMesh)
{
    if (PhysicsAssets.Num() < 2)
    {
		UE_LOG(LogTemp, Warning, TEXT("Please pass at least 2 Physics assets"));
        return nullptr;
    }

    UPhysicsAsset* MergedPhysicsAsset = NewObject<UPhysicsAsset>(SkeletalMesh->GetOutermost(), TEXT("MergedPhysicsAsset"), RF_Public | RF_Standalone | RF_Transient);

    MergedPhysicsAsset->SetPreviewMesh(SkeletalMesh);

    // Add SK setups
    {
        for (UPhysicsAsset* PhysicsAsset : PhysicsAssets)
        {
            for (USkeletalBodySetup* SKBSetup : PhysicsAsset->SkeletalBodySetups)
            {
                const int32 BodySetupIndex = CreateNewSKBody(MergedPhysicsAsset, SKBSetup->BoneName);

                MergedPhysicsAsset->SkeletalBodySetups[BodySetupIndex]->AggGeom = SKBSetup->AggGeom;
            }
        }
    }

    const TArray<FTransform> LocalPose = SkeletalMesh->RefSkeleton.GetRefBonePose();
    
    TArray<FName> AllBoneNames;
    MergedPhysicsAsset->BodySetupIndexMap.GenerateKeyArray(AllBoneNames);

    // create constraints
    for (int32 Index = 0; Index < AllBoneNames.Num(); ++Index)
    {
        const FName BoneName = AllBoneNames[Index];

        const int32 ConstraintIndex = MergedPhysicsAsset->FindConstraintIndex(BoneName);
        if (ConstraintIndex != INDEX_NONE)
        {
            continue;
        }
        
        const int32 BodyIndex = MergedPhysicsAsset->BodySetupIndexMap[BoneName];
        if (BodyIndex == INDEX_NONE)
        {
            continue;
        }

        const int32 BoneIndex = SkeletalMesh->RefSkeleton.FindRawBoneIndex(BoneName);

        int32 ParentBoneIndex = BoneIndex;
        int32 ParentBodyIndex = INDEX_NONE;
        FName ParentBoneName;

        FTransform RelTM = FTransform::Identity;
        do
        {
            // Transform of child from parent is just child ref-pose entry.
            RelTM = RelTM * LocalPose[ParentBoneIndex];

            //Travel up the hierarchy to find a parent which has a valid body
            ParentBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(ParentBoneIndex);
            if (ParentBoneIndex != INDEX_NONE)
            {
                ParentBoneName = SkeletalMesh->RefSkeleton.GetBoneName(ParentBoneIndex);
                ParentBodyIndex = MergedPhysicsAsset->FindBodyIndex(ParentBoneName);
            }
            else
            {
                //no more parents so just stop
                break;
            }

        } 
        while (ParentBodyIndex == INDEX_NONE);

        if (ParentBodyIndex != INDEX_NONE)
        {
            // add constraint between BoneName and ParentBoneName

            // constraintClass must be a subclass of UPhysicsConstraintTemplate

            UPhysicsConstraintTemplate* NewConstraintSetup = NewObject<UPhysicsConstraintTemplate>(MergedPhysicsAsset, NAME_None, RF_Transient);
            const int32 NewConstraintSetupIndex = MergedPhysicsAsset->ConstraintSetup.Add(NewConstraintSetup);

            NewConstraintSetup->DefaultInstance.JointName = BoneName;

            UPhysicsConstraintTemplate* CS = MergedPhysicsAsset->ConstraintSetup[NewConstraintSetupIndex];

            // set angular constraint mode
            CS->DefaultInstance.SetAngularSwing1Motion(EAngularConstraintMotion::ACM_Limited);
            CS->DefaultInstance.SetAngularSwing2Motion(EAngularConstraintMotion::ACM_Limited);
            CS->DefaultInstance.SetAngularTwistMotion(EAngularConstraintMotion::ACM_Limited);

            // Place joint at origin of child
            CS->DefaultInstance.ConstraintBone1 = BoneName;
            CS->DefaultInstance.Pos1 = FVector::ZeroVector;
            CS->DefaultInstance.PriAxis1 = FVector(1, 0, 0);
            CS->DefaultInstance.SecAxis1 = FVector(0, 1, 0);

            CS->DefaultInstance.ConstraintBone2 = ParentBoneName;
            CS->DefaultInstance.Pos2 = RelTM.GetLocation();
            CS->DefaultInstance.PriAxis2 = RelTM.GetUnitAxis(EAxis::X);
            CS->DefaultInstance.SecAxis2 = RelTM.GetUnitAxis(EAxis::Y);

            // CS->DefaultProfile = CS->DefaultInstance;

            MergedPhysicsAsset->DisableCollision(BodyIndex, ParentBodyIndex);
        }
    }


    // update CollisionDisableTable
    {
        FPreviewScene TmpScene;
        UWorld* TmpWorld = TmpScene.GetWorld();
        ASkeletalMeshActor* SkeletalMeshActor = TmpWorld->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform::Identity);
        SkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh);
        USkeletalMeshComponent* SKC = SkeletalMeshActor->GetSkeletalMeshComponent();
        SKC->SetPhysicsAsset(MergedPhysicsAsset);
        SkeletalMeshActor->RegisterAllComponents();

        const TArray<FBodyInstance*> Bodies = SKC->Bodies;
        const int32 NumBodies = Bodies.Num();
        for (int32 BodyIdx = 0; BodyIdx < NumBodies; ++BodyIdx)
        {
            FBodyInstance* BodyInstance = Bodies[BodyIdx];
            if (BodyInstance && BodyInstance->BodySetup.IsValid())
            {
                FTransform BodyTM = BodyInstance->GetUnrealWorldTransform();

                for (int32 OtherBodyIdx = BodyIdx + 1; OtherBodyIdx < NumBodies; ++OtherBodyIdx)
                {
                    FBodyInstance* OtherBodyInstance = Bodies[OtherBodyIdx];
                    if (OtherBodyInstance && OtherBodyInstance->BodySetup.IsValid())
                    {
                        if (BodyInstance->OverlapTestForBody(BodyTM.GetLocation(), BodyTM.GetRotation(), OtherBodyInstance))
                        {
                            MergedPhysicsAsset->DisableCollision(BodyIdx, OtherBodyIdx);
                        }
                    }
                }
            }
        }
    }

    return MergedPhysicsAsset;
}

// PRAGMA_ENABLE_OPTIMIZATION