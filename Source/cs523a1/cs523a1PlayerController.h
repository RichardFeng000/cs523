// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "cs523a1PlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class AActor;
class APawn;
class ACameraActor;
class ABoidAgent;
class UBoidStartMenuWidget;

/**
 * 第一人称玩家控制器：
 * 1) 管理输入映射上下文；
 * 2) 管理触控 UI；
 * 3) 负责 1/2 键相机切换（默认视角/鱼群顶部视角）。
 */
UCLASS(config="Game")
class CS523A1_API Acs523a1PlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** 构造函数 */
	Acs523a1PlayerController();

	/** 外部调用：进入“相机1（鱼第三人称）” */
	UFUNCTION(BlueprintCallable, Category = "Camera|Boids")
	void EnterCamera1FishThirdPerson();

protected:

	/** 默认输入映射上下文（所有平台都可用） */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** 非移动端附加输入映射上下文 */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** 触控操作 UI 类（移动端） */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** 触控 UI 实例缓存 */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** 开场菜单 UI 类（默认使用纯 C++ 的 UBoidStartMenuWidget）。 */
	UPROPERTY(EditAnywhere, Category = "UI|Start")
	TSubclassOf<UUserWidget> StartMenuWidgetClass;

	/** 开场菜单实例缓存。 */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartMenuWidgetInstance;

	/** 是否在 BeginPlay 自动弹出开场菜单。 */
	UPROPERTY(EditAnywhere, Category = "UI|Start")
	bool bShowStartMenuOnBeginPlay = false;

	/** 强制启用触控 UI（即使不在移动平台） */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** 游戏开始初始化 */
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 输入初始化 */
	virtual void SetupInputComponent() override;

	/** 占有 Pawn 时刷新默认视角目标 */
	virtual void OnPossess(APawn* InPawn) override;

	/** 是否使用触控 UI */
	bool ShouldUseTouchControls() const;

	/** 显示开场“开始”界面。 */
	void ShowStartMenu();

	/** 点击开始后进入游戏，并直接切到相机1。 */
	UFUNCTION()
	void HandleStartMenuStartClicked();

	/** 切回默认玩家相机（按键 1） */
	void SwitchToDefaultCamera();

	/** 切到鱼群顶部相机（按键 2） */
	void SwitchToBoidTopCamera();

	/** 查找/创建顶部相机并更新引用 */
	void EnsureBoidTopCamera();

	/** 按 1 使用鱼第三人称视角时，更新鱼体与相机。 */
	void UpdateFishThirdPersonView(float DeltaSeconds);

	/** Toggle camera2 leader-fish mouse control (key J). */
	void ToggleCamera2LeaderMouseControl();

	/** Update leader fish movement while camera2 mouse control is enabled. */
	void UpdateCamera2LeaderMouseControl(float DeltaSeconds);

	/** Enable/disable camera2 leader-fish mouse control. */
	void SetCamera2LeaderMouseControl(bool bEnable);

	/** 当前是否处于顶部相机视角 */
	UPROPERTY(Transient)
	bool bUsingBoidTopCamera = false;

	/** 当前是否处于“相机1：鱼第三人称”视角。 */
	UPROPERTY(Transient)
	bool bUsingFishThirdPersonCamera = false;

	/** 默认视角缓存（用于按 1 恢复） */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DefaultViewTargetActor;

	/** 顶部相机引用 */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BoidTopCameraActor;

	/** 鱼第三人称相机引用（按 1 使用）。 */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> FishThirdPersonCameraActor;

	/** 当前“相机1”选中的鱼目标。 */
	UPROPERTY(Transient)
	TObjectPtr<ABoidAgent> PrimaryFishViewTarget;

	/** Camera2 leader fish controlled by mouse when J mode is enabled. */
	UPROPERTY(Transient)
	TObjectPtr<ABoidAgent> Camera2LeaderFish;

	/** 相机切换混合时间 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraSwitchBlendTime = 0.25f;

	/** 顶部相机 FOV */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float BoidTopCameraFOV = 90.0f;

	/** 鱼位置跟随玩家 Pawn 时的局部偏移（让鱼不贴地）。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	FVector FishPawnFollowOffset = FVector(0.0f, 0.0f, 90.0f);

	/** 相机1是否使用鼠标可控的第三人称环绕。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	bool bCamera1UseMouseOrbit = true;

	/** 相机1第三人称距离。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float Camera1OrbitDistance = 720.0f;

	/** 相机1第三人称高度。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	float Camera1OrbitHeight = 220.0f;

	/** 相机1俯仰下限。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	float Camera1MinPitch = -65.0f;

	/** 相机1俯仰上限。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	float Camera1MaxPitch = 25.0f;

	/** 相机1环绕平滑速度（0 表示不平滑）。 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Camera1OrbitInterpSpeed = 10.0f;

	/** 顶部相机高度额外补偿（预留参数） */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoidTopCameraHeightPadding = 250.0f;

	/** 按键 2 时优先使用场景中手动放置的相机 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	bool bPreferPlacedBoidTopCamera = true;

	/** 手动放置顶部相机的 Tag 名称 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	FName PlacedBoidTopCameraTag = TEXT("BoidTopCamera");

	/** Whether camera2 currently uses mouse-driven leader fish control. */
	UPROPERTY(Transient)
	bool bCamera2LeaderMouseControlEnabled = false;

	/** Max speed for the camera2 leader fish mouse control. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float Camera2LeaderMaxSpeed = 1800.0f;

	/** Interpolation speed when leader fish moves toward mouse target. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Camera2LeaderMoveInterpSpeed = 0.0f;

	/** Enable J hotkey for camera1 baitball mode. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	bool bEnableCamera1BaitballHotkey = true;

	/** Enable J hotkey for camera2 leader-fish mouse control. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	bool bEnableCamera2LeaderHotkey = true;

	/** Mouse ray length used for camera2 leader world projection. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "1000.0", UIMin = "1000.0"))
	float Camera2MouseRayLength = 100000.0f;

	/** Stop radius to avoid jitter around target mouse point. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Camera2LeaderArrivalRadius = 10.0f;

	/** Clamp camera2 leader movement to boid simulation bounds. */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	bool bCamera2LeaderClampToSimulationBounds = true;

	/** Preferred leader index for camera2 mouse mode (-1 means auto pick). */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "-1", UIMin = "-1"))
	int32 Camera2PreferredLeaderBoidIndex = -1;
};
