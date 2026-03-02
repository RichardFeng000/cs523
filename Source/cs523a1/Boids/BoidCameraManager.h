// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoidCameraManager.generated.h"

class ACameraActor;
class AActor;

UCLASS(BlueprintType)
class CS523A1_API ABoidCameraManager : public AActor
{
	GENERATED_BODY()

public:
	// 构造函数：作为“相机管理器”Actor，仅负责鱼群顶部相机。
	ABoidCameraManager();

	// 获取或创建顶部相机（Outliner 中名称为 BoidTopCamera）。
	UFUNCTION(BlueprintCallable, Category = "Boid|Camera")
	ACameraActor* GetOrCreateTopViewCameraActor();

	// 获取“相机1”默认鱼视角目标（一条 BoidAgent）。
	UFUNCTION(BlueprintCallable, Category = "Boid|Camera")
	AActor* GetPrimaryFishViewTarget() const;

	// 获取或创建“相机1”使用的鱼第三人称相机（Outliner 中名称为 BoidFishCamera）。
	UFUNCTION(BlueprintCallable, Category = "Boid|Camera")
	ACameraActor* GetOrCreateFishThirdPersonCameraActor();

	// 手动把顶部相机复位到“鱼群中心 + 默认偏移”。
	UFUNCTION(BlueprintCallable, Category = "Boid|Camera")
	void RefreshTopCameraTransform();

	// 按当前鱼目标刷新第三人称鱼相机位置/朝向。
	UFUNCTION(BlueprintCallable, Category = "Boid|Camera")
	void RefreshFishThirdPersonCameraTransform(AActor* FishTarget, float DeltaTime = 0.0f);

protected:
	virtual void BeginPlay() override;

	// 顶部相机查找标签。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera")
	FName TopViewCameraTag = TEXT("BoidTopCamera");

	// 顶部相机默认偏移（相对鱼群中心）。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera")
	FVector TopViewCameraDefaultOffset = FVector(0.0f, 0.0f, 2500.0f);

	// 场景中没有相机时是否自动创建。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera")
	bool bAutoSpawnTopViewCamera = true;

	// 顶部相机默认 FOV。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera", meta = (ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float TopViewCameraFOV = 90.0f;

	// 鱼第三人称相机查找标签。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera")
	FName FishThirdPersonCameraTag = TEXT("BoidFishCamera");

	// 鱼第三人称相机默认 FOV。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera", meta = (ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float FishThirdPersonCameraFOV = 95.0f;

	// 鱼第三人称相机相对鱼体偏移：X<0 表示在鱼后方。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera")
	FVector FishThirdPersonOffset = FVector(-650.0f, 0.0f, 240.0f);

	// 看向鱼体前方的预瞄距离。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FishThirdPersonLookAhead = 180.0f;

	// 第三人称鱼相机插值速度（0=瞬移）。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FishThirdPersonInterpSpeed = 8.0f;

	// 是否让“相机1”优先使用鱼作为默认视角目标。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera")
	bool bUseFishAsDefaultCamera1 = true;

	// 作为默认视角的鱼索引（超范围时自动取模）。
	UPROPERTY(EditAnywhere, Category = "Boid|Camera", meta = (ClampMin = "0", UIMin = "0"))
	int32 PrimaryFishIndex = 0;

	// 缓存顶部相机引用。
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> TopViewCameraActor;

	// 缓存鱼第三人称相机引用。
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> FishThirdPersonCameraActor;

private:
	// 解析“鱼群中心”位置（优先 BoidManager，其次本 Actor 位置）。
	FVector ResolveFocusCenter() const;
};
