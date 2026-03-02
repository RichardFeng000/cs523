// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoidAutoSpawnSubsystem.generated.h"

class UUserWidget;
class ABoidAgent;
class ACameraActor;
class APlayerController;

UCLASS()
class CS523A1_API UBoidAutoSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	// 尝试显示全局开始菜单（直到 PlayerController 就绪）。
	void TryShowStartMenu();

	// 点击“开始”后的处理。
	UFUNCTION()
	void HandleStartClicked();

	// 非自定义 PlayerController 下的兜底：启动鼠标第三人称环绕更新。
	void StartFallbackFishCameraOrbit(APlayerController* PC, ABoidAgent* FishTarget, ACameraActor* FishCamera);

	// 停止兜底相机环绕更新。
	void StopFallbackFishCameraOrbit();

	// 逐帧更新兜底鱼相机，让鼠标移动能改变视角。
	void TickFallbackFishCameraOrbit();

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartMenuWidgetInstance;

	FTimerHandle StartMenuRetryTimerHandle;
	FTimerHandle FallbackCameraTickTimerHandle;
	bool bStartMenuFinished = false;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> FallbackOrbitPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<ABoidAgent> FallbackOrbitFishTarget;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> FallbackOrbitCameraActor;
};
