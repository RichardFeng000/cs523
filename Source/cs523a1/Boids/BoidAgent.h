// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoidAgent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS(BlueprintType)
class CS523A1_API ABoidAgent : public AActor
{
	GENERATED_BODY()

public:
	// 构造函数：初始化可见网格与默认显示参数。
	ABoidAgent();

	// 编辑器或运行时重建时触发：把可视参数重新应用到网格。
	virtual void OnConstruction(const FTransform& Transform) override;

	// 读取当前速度（用于 BoidManager 计算下一帧）。
	const FVector& GetBoidVelocity() const
	{
		return Velocity;
	}

	// 只更新速度，不移动位置。
	void SetVelocity(const FVector& NewVelocity);
	// 设置该鱼体的可视缩放（由管理器统一调用）。
	void SetVisualScale(const FVector& NewScale);
	// 同步更新位置、速度、朝向。
	void SetSimulationState(
		const FVector& NewLocation,
		const FVector& NewVelocity,
		const FVector& DesiredFacingVelocity,
		float DeltaTime,
		float RotationInterpSpeed,
		float FacingDeadZoneDegrees,
		float FacingTurnSpeedDegPerSec,
		float FacingMinSpeed);

	// 标记该鱼是否为“相机1控制鱼”，用于高亮显示（红色）。
	void SetCamera1Highlighted(bool bHighlighted);

protected:
	// 把 VisualScale 等可视参数安全应用到网格组件。
	void ApplyVisualSettings();

	// 鱼体可见网格（使用基础 Cone）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boid")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	// 单条鱼的可视缩放（可在 Outliner/Details 中按实例调整）。
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boid|Visual", meta = (ClampMin = "0.01", UIMin = "0.01"))
	FVector VisualScale = FVector(0.42f, 0.42f, 1.15f);

	// 相机1控制鱼的额外放大倍率（即使材质不支持染色，也能一眼看出来）。
	UPROPERTY(EditDefaultsOnly, Category = "Boid|Visual", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float Camera1HighlightScaleMultiplier = 1.25f;

	// 当前速度向量（由 BoidManager 每帧写入）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boid")
	FVector Velocity = FVector::ZeroVector;

	// 原始材质（用于取消高亮时恢复）。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

	// 运行时高亮材质实例。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighlightMaterialInstance = nullptr;

	// 当前高亮状态缓存，避免重复切换材质。
	UPROPERTY(Transient)
	bool bCamera1Highlighted = false;

	// 缓存平滑朝向，减少速度微抖导致的鱼头乱晃。
	UPROPERTY(Transient)
	FVector SmoothedFacingDirection = FVector::ZeroVector;
};
