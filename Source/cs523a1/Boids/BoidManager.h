// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoidManager.generated.h"

class ABoidAgent;

UENUM(BlueprintType)
enum class EBoidSwarmState : uint8
{
	Cohesive UMETA(DisplayName = "Cohesive"),
	SplitFlow UMETA(DisplayName = "Split Flow")
};

UENUM(BlueprintType)
enum class EBoidBaitballVersion : uint8
{
	Version1_Original UMETA(DisplayName = "Version1 Original"),
	Version2_Enhanced UMETA(DisplayName = "Version2 Enhanced")
};

UCLASS(BlueprintType)
class CS523A1_API ABoidManager : public AActor
{
	GENERATED_BODY()

public:
	// 构造函数：开启 Tick，用于每帧更新鱼群。
	ABoidManager();

	virtual void Tick(float DeltaTime) override;

	// 获取鱼群模拟边界半径（盒体半尺寸）。
	UFUNCTION(BlueprintPure, Category = "Boid|Setup")
	FVector GetSimulationBoundsExtent() const
	{
		return SimulationBoundsExtent;
	}

	// 获取鱼群中心位置（通常就是管理器位置）。
	UFUNCTION(BlueprintPure, Category = "Boid|Setup")
	FVector GetSimulationCenter() const
	{
		return GetActorLocation();
	}

	// 销毁并重生全部鱼体（用于你在 Details 调参后快速重建）。
	UFUNCTION(BlueprintCallable, Category = "Boid")
	void RespawnBoids();

	// 把锚点立即同步到玩家视角（仅当锚点模式启用时生效）。
	UFUNCTION(BlueprintCallable, Category = "Boid")
	void SnapAnchorToPlayer();

	// Force all non-controlled boids to gather into a baitball around a world-space point.
	UFUNCTION(BlueprintCallable, Category = "Boid|Baitball")
	void SetForceBaitballAtWorldOrigin(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Boid|Baitball")
	bool IsForceBaitballAtWorldOrigin() const
	{
		return bForceBaitballAtWorldOrigin;
	}

	// 返回当前鱼数量。
	UFUNCTION(BlueprintPure, Category = "Boid")
	int32 GetBoidNum() const
	{
		return Boids.Num();
	}

	// 通过索引获取鱼（越界返回 nullptr）。
	UFUNCTION(BlueprintPure, Category = "Boid")
	ABoidAgent* GetBoidByIndex(int32 Index) const;

	// 设置外部控制鱼（例如玩家视角鱼）。被设置的鱼不会参与 Boids 自动推进。
	UFUNCTION(BlueprintCallable, Category = "Boid")
	void SetExternallyControlledBoid(ABoidAgent* InBoid);

	// Enable/disable whether externally controlled leader should influence the rest of swarm.
	UFUNCTION(BlueprintCallable, Category = "Boid|Leader")
	void SetExternalLeaderInfluenceEnabled(bool bEnable);

	// 获取当前外部控制鱼。
	UFUNCTION(BlueprintPure, Category = "Boid")
	ABoidAgent* GetExternallyControlledBoid() const
	{
		return ExternallyControlledBoid.Get();
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 鱼体类型（不设置时默认使用 ABoidAgent）。
	UPROPERTY(EditAnywhere, Category = "Boid|Setup")
	TSubclassOf<ABoidAgent> AgentClass;

	// 鱼数量。
	UPROPERTY(EditAnywhere, Category = "Boid|Setup", meta = (ClampMin = "1", UIMin = "1"))
	int32 BoidCount = 120;

	// 模拟边界（盒体半尺寸）：X/Y/Z 越大活动空间越大。
	UPROPERTY(EditAnywhere, Category = "Boid|Setup")
	FVector SimulationBoundsExtent = FVector(2000.0f, 2000.0f, 1400.0f);

	// 初始生成时占用边界的比例。
	UPROPERTY(EditAnywhere, Category = "Boid|Setup", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float SpawnFillPercent = 0.88f;

	// 全体鱼统一可视缩放（在 Outliner 选 BoidManager 调一次全体生效）。
	UPROPERTY(EditAnywhere, Category = "Boid|Visual", meta = (ClampMin = "0.01", UIMin = "0.01"))
	FVector FishVisualScale = FVector(0.42f, 0.42f, 1.15f);

	// 速度下限。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinSpeed = 220.0f;

	// 速度上限。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxSpeed = 420.0f;

	// 单帧转向力上限。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxSteeringForce = 1150.0f;

	// 朝向插值速度（越大转头越快）。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RotationInterpSpeed = 8.0f;

	// 是否让鱼头方向部分跟随“局部群体平均方向”，减少单体抖动。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion|Orientation")
	bool bUseGroupFacingDirection = true;

	// 鱼头方向中，群体平均方向占比（0=完全自身速度，1=完全群体方向）。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion|Orientation", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GroupFacingBlend = 0.65f;

	// 计算群体朝向时使用的邻域半径倍率（乘以 NeighborRadius）。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion|Orientation", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float GroupFacingNeighborRadiusScale = 1.0f;

	// 低于该角度变化时忽略，防止微小噪声导致鱼头抖动。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion|Orientation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingDeadZoneDegrees = 2.5f;

	// 鱼头最大转向速度（度/秒）。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion|Orientation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingTurnSpeedDegPerSec = 280.0f;

	// 低于该速度时保持当前朝向，避免低速漂移时乱转头。
	UPROPERTY(EditAnywhere, Category = "Boid|Motion|Orientation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingMinSpeed = 35.0f;

	// 邻居感知半径（用于对齐/聚合）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float NeighborRadius = 500.0f;

	// 分离半径（保留给规则调参，当前主要由 DesiredFishSpacing 控制间距）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SeparationRadius = 220.0f;

	// 目标鱼间距（核心参数：越大越分散）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float DesiredFishSpacing = 220.0f;

	// 三条 Boids 规则权重：对齐 / 聚合 / 分离。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules")
	float AlignmentWeight = 0.95f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules")
	float CohesionWeight = 0.16f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules")
	float SeparationWeight = 3.2f;

	// 轻微拉回群体中心，减少“自动分成两团长期不合并”。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CenterAttractionWeight = 0.12f;

	// 领头鱼引领：当存在外部控制鱼时，为其他鱼注入更强的全局跟随/对齐力。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader")
	bool bEnableLeaderInfluence = true;

	// 领头鱼影响半径（越大影响范围越广）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float LeaderInfluenceRadius = 2800.0f;

	// 朝领头鱼聚拢权重。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LeaderFollowWeight = 2.2f;

	// 与领头鱼速度方向对齐权重。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LeaderAlignmentWeight = 1.35f;

	// 领头鱼位置预测时间（秒），>0 会更像“追随未来位置”。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LeaderPredictionTime = 0.30f;

	// 领头鱼影响距离衰减指数（越大越强调近处跟随）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float LeaderFalloffExponent = 1.25f;

	// 即便离得较远，也保留的最小影响比例。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Leader", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LeaderMinInfluenceAlpha = 0.12f;

	// Camera1 special mode: force a baitball around world target.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball")
	bool bForceBaitballAtWorldOrigin = false;

	// Baitball behavior version switch: V1 = early/original, V2 = enhanced/layered.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball")
	EBoidBaitballVersion BaitballVersion = EBoidBaitballVersion::Version1_Original;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball")
	FVector BaitballWorldTarget = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "10.0", UIMin = "10.0"))
	float BaitballDesiredRadius = 280.0f;

	// 空心核心半径：小于该半径会被强力向外推，保持“中间是空的”。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballInnerVoidRadius = 140.0f;

	// 壳层容差带宽：在目标半径附近此范围内，径向拉回更柔和。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballShellBandWidth = 30.0f;

	// 壳层回拉强度：越大越容易形成“薄壳”。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballShellTightness = 2.1f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballAttractionWeight = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballSwirlWeight = 0.95f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballInnerRepulsionWeight = 1.1f;

	// Baitball 涡旋速度比例（相对 MaxSpeed）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballSwirlSpeedScale = 0.70f;

	// 离球心很远时的吸引力额外增强倍数。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BaitballOuterAttractionBoost = 1.5f;

	// 触发“远距离增强吸引”的距离倍率（相对 BaitballDesiredRadius）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BaitballOuterBoostRadiusMultiplier = 2.0f;

	// 是否把 baitball 约束在世界固定高度（球心 Z）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball")
	bool bBaitballLockToTargetZ = false;

	// Auto-fit baitball radius based on boid count, so the shell has enough surface area.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population")
	bool bAutoFitBaitballRadiusToPopulation = false;

	// Effective spacing used for population-based radius estimation (multiplies DesiredFishSpacing).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float BaitballPopulationSpacingScale = 0.82f;

	// Surface packing efficiency (0..1). Lower means more conservative (larger radius).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float BaitballPopulationPackingEfficiency = 0.78f;

	// Safety multiplier on computed radius from population.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BaitballPopulationRadiusSafetyScale = 1.08f;

	// Auto-thin shell by deriving inner void from desired shell layer count.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population")
	bool bAutoSetInnerVoidForShellLayers = false;

	// Target shell thickness in "fish layers" (1.0 ~ one fish thick).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population", meta = (ClampMin = "0.2", UIMin = "0.2"))
	float BaitballTargetShellLayers = 1.15f;

	// Hard minimum shell thickness used by auto inner-void calculation.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Population", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BaitballMinShellThickness = 60.0f;

	// Prefer layered shell structure (multiple concentric bands) instead of one thin shell.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered")
	bool bPreferLayeredBaitballShell = false;

	// Radial spacing between shell layers (multiplies effective fish spacing).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float BaitballLayerSpacingScale = 0.95f;

	// Maximum number of layers allowed away from base desired radius.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaitballMaxLayerOffset = 4;

	// Additional radial attraction multiplier when layered mode is enabled.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballLayeredRadialAttractionBoost = 1.25f;

	// Scale inner void radius in layered mode (smaller value gives thicker stacked shell volume).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BaitballLayeredInnerVoidScale = 0.72f;

	// Use toroidal (ring-like) layer metric instead of purely spherical layers.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered")
	bool bUseToroidalLayerMetric = true;

	// Major ring radius scale in toroidal layered mode (relative to DesiredRadius).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.2", UIMin = "0.2"))
	float BaitballLayerMajorRadiusScale = 0.82f;

	// Base tube radius scale in toroidal layered mode (relative to DesiredRadius).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float BaitballLayerTubeRadiusScale = 0.34f;

	// Attraction back to major ring (keeps swarm in torus instead of collapsing to sphere).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballLayerMajorRadiusWeight = 1.25f;

	// Extra flow alignment along torus direction to produce natural stacked stream-lines.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballLayerFlowAlignWeight = 1.05f;

	// Compress layer spacing as boids move to outer layers (reduces large inter-layer gaps).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.2", ClampMax = "1.0", UIMin = "0.2", UIMax = "1.0"))
	float BaitballOuterLayerSpacingScale = 0.72f;

	// Pull outer layers back toward the main torus to prevent detaching.
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballOuterLayerTetherWeight = 0.95f;

	// Shape the outer tether falloff by layer index (higher = tether mostly on far outer layers).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float BaitballOuterLayerTetherExponent = 1.45f;

	// Top opening control (to mimic vent/hole near bubble side in real baitballs).
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BaitballTopOpeningStrength = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Layered", meta = (ClampMin = "0.5", UIMin = "0.5"))
	float BaitballTopOpeningExponent = 1.6f;

	// 锁高度时的垂直回拉权重。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballVerticalLockWeight = 1.2f;

	// baitball 模式下基础 Boids 规则缩放（越小越不会塌成偏团）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BaitballBaseRulesScale = 0.35f;

	// 壳面邻居扩散半径（用于在球面上“摊开”鱼群）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "10.0", UIMin = "10.0"))
	float BaitballSurfaceNeighborRadius = 360.0f;

	// 壳面扩散权重（越大越容易形成完整球壳覆盖）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballSurfaceSpreadWeight = 2.4f;

	// 随机切向推进权重（打散单一旋涡，填补缺失扇区）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballRandomTangentWeight = 0.55f;

	// 径向速度阻尼（抑制穿过球壳，帮助停留在壳面）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballRadialVelocityDamping = 1.1f;

	// 是否启用更自然的 baitball 扰动（形变/湍动/威胁压迫）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural")
	bool bEnableNaturalBaitballVariation = true;

	// 全体低频“呼吸”幅度（相对半径比例）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballBreathingAmplitude = 0.10f;

	// 全体低频“呼吸”速度。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballBreathingSpeed = 0.26f;

	// 局部半径噪声幅度（相对半径比例）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballRadiusNoiseAmplitude = 0.14f;

	// 局部半径噪声空间频率。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.00001", UIMin = "0.00001"))
	float BaitballRadiusNoiseSpaceScale = 0.0022f;

	// 局部半径噪声时间速度。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballRadiusNoiseTimeScale = 0.55f;

	// 壳面随机切向湍动权重（打破过度规则旋转）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballTurbulenceWeight = 0.85f;

	// baitball 目标中心摆动幅度（世界单位）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballCenterWobbleAmplitude = 140.0f;

	// baitball 目标中心摆动速度。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballCenterWobbleSpeed = 0.18f;

	// 伪“捕食者”压迫权重：靠近威胁轨迹一侧会更紧、更乱。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Natural", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballPredatorPressureWeight = 1.2f;

	// 环形偏置：把 baitball 从球壳推向“甜甜圈/环形团”。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Toroidal")
	bool bEnableBaitballToroidalBias = true;

	// 向“赤道平面”回拉权重（提高后更扁、更像环）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Toroidal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballEquatorAttractionWeight = 1.35f;

	// 中轴空洞半径（围绕 Up 轴保持空心通道）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Toroidal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballAxisVoidRadius = 220.0f;

	// 轴心排斥权重（越大中间越空）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Toroidal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballAxisRepulsionWeight = 1.8f;

	// 围绕轴心环向游动权重（形成图中那种环向流线）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|Baitball|Toroidal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BaitballAxisCirculationWeight = 1.1f;

	// 启用“流场分群”：鱼会按空间噪声场出现临时分队，再在规则作用下重新汇合。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|SubSchool")
	bool bEnableSubSchoolFlow = true;

	// 分群流场权重（越大越容易分成 2~4 团）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|SubSchool", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SubSchoolFlowWeight = 0.55f;

	// 流场空间频率（越大每个子群更小、更碎）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|SubSchool", meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float SubSchoolFlowSpaceScale = 0.0012f;

	// 流场时间速度（越大分群形态变化越快）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|SubSchool", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SubSchoolFlowTimeScale = 0.20f;

	// 自动在“聚集”和“分群”两种状态之间循环切换。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateCycle")
	bool bEnableStateCycling = false;

	// 聚集状态持续时长（秒）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateCycle", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float CohesiveStateDuration = 12.0f;

	// 分群状态持续时长（秒）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateCycle", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SplitStateDuration = 8.0f;

	// 两种状态之间的平滑过渡速度。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateCycle", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float StateBlendInterpSpeed = 1.2f;

	// 开局是否先进入分群状态。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateCycle")
	bool bStartInSplitState = false;

	// 随机状态切换：不按固定周期轮流，而是按概率随机分合。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateRandom")
	bool bEnableRandomStateTransitions = true;

	// 每秒进入 SplitFlow 的概率（当前为 Cohesive 时生效）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateRandom", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EnterSplitChancePerSecond = 0.18f;

	// 每秒回到 Cohesive 的概率（当前为 SplitFlow 时生效）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateRandom", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExitSplitChancePerSecond = 0.12f;

	// 每次状态切换后的最短保持时间（秒），避免频繁抖动。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|StateRandom", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RandomStateMinHoldTime = 2.0f;

	// 启用随机多子群：每条鱼会被分配到一个随机子群，并追随各自的动态锚点。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool")
	bool bEnableRandomSubSchools = true;

	// 子群数量范围。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "1", UIMin = "1"))
	int32 MinRandomSubSchoolCount = 2;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRandomSubSchoolCount = 5;

	// 每秒改变“子群数量”的概率。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SubSchoolCountChangeChancePerSecond = 0.08f;

	// 每秒重分配“单条鱼所属子群”的概率。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoidSubSchoolReassignChancePerSecond = 0.10f;

	// 子群锚点牵引强度（越大群体越容易拉开）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RandomSubSchoolAnchorWeight = 0.62f;

	// 子群锚点活动范围（相对边界百分比）。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.05", UIMax = "1.0"))
	float RandomSubSchoolSpread = 0.72f;

	// 子群锚点运动速度。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules|MultiSchool", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float RandomSubSchoolAnchorTimeScale = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Boid|Rules")
	float BoundsWeight = 1.15f;

	// 靠近边界开始回推的缓冲宽度。
	UPROPERTY(EditAnywhere, Category = "Boid|Rules", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BoundsBuffer = 350.0f;

	// 可选目标点追踪。
	UPROPERTY(EditAnywhere, Category = "Boid|Target")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Boid|Target")
	float TargetWeight = 0.0f;

	// 锚点模式：让鱼群中心跟随玩家视角（当前默认关闭）。
	UPROPERTY(EditAnywhere, Category = "Boid|Anchor")
	bool bAnchorToPlayer = false;

	UPROPERTY(EditAnywhere, Category = "Boid|Anchor")
	FVector PlayerAnchorOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Boid|Anchor")
	bool bRotateAnchorOffsetWithPlayer = false;

	UPROPERTY(EditAnywhere, Category = "Boid|Anchor", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AnchorRecenterDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Boid|Anchor")
	bool bMoveBoidsWithAnchor = false;

	// 调试绘制与屏幕统计。
	UPROPERTY(EditAnywhere, Category = "Boid|Debug")
	bool bDrawDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Boid|Debug")
	bool bDrawDebugBoidPoints = false;

	UPROPERTY(EditAnywhere, Category = "Boid|Debug", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float DebugBoidPointRadius = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Boid|Debug")
	bool bDrawDebugVelocity = false;

	UPROPERTY(EditAnywhere, Category = "Boid|Debug", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float DebugVelocityScale = 0.14f;

	UPROPERTY(EditAnywhere, Category = "Boid|Debug")
	bool bShowOnScreenStats = true;

	UPROPERTY(EditAnywhere, Category = "Boid|Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebugDrawDuration = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABoidAgent>> Boids;

	// 外部控制鱼（例如由 PlayerController 用 WASD 驱动的那一条）。
	UPROPERTY(Transient)
	TObjectPtr<ABoidAgent> ExternallyControlledBoid = nullptr;

	// 记录上一次应用的统一鱼体缩放，用于检测是否需要刷新全体。
	UPROPERTY(Transient)
	FVector LastAppliedFishVisualScale = FVector::ZeroVector;

	// 当前群体状态（聚集 or 分群）。
	UPROPERTY(Transient)
	EBoidSwarmState CurrentSwarmState = EBoidSwarmState::Cohesive;

	// 当前状态已运行时间（秒）。
	UPROPERTY(Transient)
	float SwarmStateElapsed = 0.0f;

	// 分群混合系数（0=完全聚集，1=完全分群）。
	UPROPERTY(Transient)
	float SplitModeAlpha = 0.0f;

	// 当前随机子群数。
	UPROPERTY(Transient)
	int32 CurrentRandomSubSchoolCount = 1;

	// 每条鱼当前所属子群（按 Boids 数组索引一一对应）。
	UPROPERTY(Transient)
	TArray<uint8> BoidSubSchoolIds;

	// 外部控制鱼缓存（当前帧领头鱼位置/速度）。
	UPROPERTY(Transient)
	bool bHasExternalLeaderData = false;

	UPROPERTY(Transient)
	FVector ExternalLeaderLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ExternalLeaderVelocity = FVector::ZeroVector;

	// Runtime gate: external leader exists but affects others only when explicitly enabled (e.g. Camera2 J mode).
	UPROPERTY(Transient)
	bool bExternalLeaderInfluenceEnabled = false;

private:
	struct FBoidSnapshot
	{
		// 只缓存本帧位置与速度，避免迭代过程中互相污染数据。
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
	};

	// 生命周期与模拟流程。
	void SpawnBoids();
	void DestroyBoids();
	void SimulateBoids(float DeltaTime);
	void UpdateSwarmStateCycle(float DeltaTime);
	void EnsureSubSchoolAssignments();
	void MaybeRefreshRandomSubSchools(float DeltaTime);
	void UpdateAnchorLocation(bool bForceUpdate);
	void ApplyFishVisualScaleToAllBoids();

	// 力计算工具函数。
	FVector ComputeBoidForce(int32 BoidIndex, const TArray<FBoidSnapshot>& Snapshots) const;
	FVector ComputeRandomSubSchoolAnchor(int32 SchoolId, float TimeSeconds) const;
	FVector ComputeSteerTowards(const FVector& CurrentVelocity, const FVector& DesiredVelocity) const;
	FVector ComputeNeighborAverageVelocityDirection(int32 BoidIndex, const TArray<FBoidSnapshot>& Snapshots) const;
	FVector ComputeBoundsForce(const FVector& Location) const;
	FVector ClampToBounds(const FVector& Location) const;
};
