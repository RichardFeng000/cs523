// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "cs523a1Character.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UUserWidget;
class ACameraActor;
class AActor;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * 基础第一人称角色：
 * 1) 处理移动/视角/跳跃；
 * 2) 绑定 1/2 键作为相机切换兜底（即使未使用自定义 PlayerController 也可切）。
 */
UCLASS(abstract)
class Acs523a1Character : public ACharacter
{
	GENERATED_BODY()

	/** 第一人称网格（仅自己可见） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** 第一人称相机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** 跳跃输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** 移动输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** 视角输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** 鼠标视角输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	Acs523a1Character();

protected:
	/** 角色开局初始化（兜底开始菜单） */
	virtual void BeginPlay() override;

	/** 接收移动输入 */
	void MoveInput(const FInputActionValue& Value);

	/** 接收视角输入 */
	void LookInput(const FInputActionValue& Value);

	/** 处理转向（Yaw/Pitch） */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** 处理移动（Right/Forward） */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** 处理跳跃开始 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** 处理跳跃结束 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** 绑定输入动作 */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** 切回默认视角（按键 1，兜底） */
	void SwitchToDefaultCameraView();

	/** 切到鱼群顶部相机（按键 2，兜底） */
	void SwitchToBoidTopCameraView();

	/** 查找/创建/更新顶部相机引用 */
	void EnsureBoidTopCameraForView();

	/** 显示角色兜底开始菜单（当未使用自定义 PlayerController 时）。 */
	void ShowStartMenuFallback();

	/** 点击开始后的处理。 */
	UFUNCTION()
	void HandleStartMenuFallbackClicked();

protected:
	/** 默认视角目标缓存 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DefaultViewTargetActor;

	/** 顶部相机引用 */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BoidTopCameraActor;

	/** 相机切换混合时间 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoidCameraBlendTime = 0.25f;

	/** 顶部相机 FOV */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float BoidCameraFOV = 90.0f;

	/** 顶部相机高度补偿（预留参数） */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoidCameraHeightPadding = 250.0f;

	/** 是否优先使用场景里已有相机作为鱼群相机 */
	UPROPERTY(EditAnywhere, Category = "Camera|Boids")
	bool bUsePlacedCameraActorForBoids = true;

	/** 角色兜底开始菜单类（默认使用 UBoidStartMenuWidget）。 */
	UPROPERTY(EditAnywhere, Category = "UI|Start")
	TSubclassOf<UUserWidget> StartMenuWidgetClassFallback;

	/** 角色兜底开始菜单实例。 */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartMenuWidgetInstanceFallback;

	/** 未使用自定义 PlayerController 时是否显示兜底开始菜单。 */
	UPROPERTY(EditAnywhere, Category = "UI|Start")
	bool bShowStartMenuFallback = false;
	

public:

	/** 返回第一人称网格 **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** 返回第一人称相机组件 **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};
