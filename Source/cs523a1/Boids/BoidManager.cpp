// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boids/BoidManager.h"

#include "Boids/BoidAgent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "cs523a1.h"

namespace
{
	const TCHAR* ToSwarmStateText(EBoidSwarmState State)
	{
		return State == EBoidSwarmState::SplitFlow ? TEXT("SplitFlow") : TEXT("Cohesive");
	}
}

ABoidManager::ABoidManager()
{
	// 需要每帧更新群体行为。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
}

void ABoidManager::BeginPlay()
{
	Super::BeginPlay();

	// 单例保护：统一按 GetActorOfClass 返回的主实例保留，其他实例自毁。
	if (ABoidManager* PrimaryManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass())))
	{
		if (PrimaryManager != this)
		{
			UE_LOG(Logcs523a1, Warning, TEXT("[Boids] Duplicate BoidManager detected (%s). Destroy self: %s"),
				*GetNameSafe(PrimaryManager), *GetName());
			Destroy();
			return;
		}
	}

	// 首帧先重置状态，再生成鱼群。
	LastAppliedFishVisualScale = FVector::ZeroVector;
	CurrentSwarmState = bStartInSplitState ? EBoidSwarmState::SplitFlow : EBoidSwarmState::Cohesive;
	SwarmStateElapsed = 0.0f;
	SplitModeAlpha = (CurrentSwarmState == EBoidSwarmState::SplitFlow) ? 1.0f : 0.0f;
	CurrentRandomSubSchoolCount = 1;
	BoidSubSchoolIds.Reset();
	// 默认关闭运行时调试点/速度线，避免误看成“第二队鱼”。
	bDrawDebugBoidPoints = false;
	bDrawDebugVelocity = false;
	UpdateAnchorLocation(true);
	SpawnBoids();

	TArray<AActor*> AllBoidAgents;
	UGameplayStatics::GetAllActorsOfClass(this, ABoidAgent::StaticClass(), AllBoidAgents);
	UE_LOG(Logcs523a1, Display, TEXT("[Boids] BeginPlay manager=%s managed=%d worldAgents=%d"),
		*GetName(), Boids.Num(), AllBoidAgents.Num());
}

void ABoidManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyBoids();
	Super::EndPlay(EndPlayReason);
}

void ABoidManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 可选：让群体中心跟随玩家。
	UpdateAnchorLocation(false);
	UpdateSwarmStateCycle(DeltaTime);

	// 统一鱼体缩放有变更时，实时同步到现有所有鱼。
	if (!FishVisualScale.Equals(LastAppliedFishVisualScale, 0.001f))
	{
		ApplyFishVisualScaleToAllBoids();
	}

	// 绘制活动边界。
	if (bDrawDebugBounds)
	{
		DrawDebugBox(
			GetWorld(),
			GetActorLocation(),
			SimulationBoundsExtent,
			FColor(40, 180, 255),
			false,
			DebugDrawDuration,
			0,
			1.5f);
	}

	// 执行一帧 Boids 模拟。
	SimulateBoids(DeltaTime);

	// 屏幕实时信息。
	if (bShowOnScreenStats && GEngine)
	{
		const FString DebugText = FString::Printf(
			TEXT("Boids:%d | Bounds:(%.0f, %.0f, %.0f) | Anchor:%s | Mode:%s (%.0f%%) | Groups:%d | Baitball:%s | Ver:%s"),
			Boids.Num(),
			GetActorLocation().X,
			GetActorLocation().Y,
			GetActorLocation().Z,
			bAnchorToPlayer ? TEXT("On") : TEXT("Off"),
			ToSwarmStateText(CurrentSwarmState),
			SplitModeAlpha * 100.0f,
			bEnableRandomSubSchools ? CurrentRandomSubSchoolCount : 1,
			bForceBaitballAtWorldOrigin ? TEXT("On") : TEXT("Off"),
			(BaitballVersion == EBoidBaitballVersion::Version2_Enhanced) ? TEXT("V2") : TEXT("V1"));
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.0f, FColor(40, 200, 255), DebugText);
	}
}

void ABoidManager::RespawnBoids()
{
	// 调参后重建鱼群。
	DestroyBoids();
	UpdateAnchorLocation(true);
	SpawnBoids();
}

void ABoidManager::SnapAnchorToPlayer()
{
	UpdateAnchorLocation(true);
}

void ABoidManager::SetForceBaitballAtWorldOrigin(bool bEnable)
{
	bForceBaitballAtWorldOrigin = bEnable;
}

ABoidAgent* ABoidManager::GetBoidByIndex(int32 Index) const
{
	return Boids.IsValidIndex(Index) ? Boids[Index].Get() : nullptr;
}

void ABoidManager::SetExternallyControlledBoid(ABoidAgent* InBoid)
{
	if (IsValid(ExternallyControlledBoid) && ExternallyControlledBoid != InBoid)
	{
		ExternallyControlledBoid->SetCamera1Highlighted(false);
	}

	ExternallyControlledBoid = InBoid;
	if (IsValid(ExternallyControlledBoid))
	{
		ExternallyControlledBoid->SetCamera1Highlighted(true);
	}
}

void ABoidManager::SetExternalLeaderInfluenceEnabled(bool bEnable)
{
	bExternalLeaderInfluenceEnabled = bEnable;
}

void ABoidManager::SpawnBoids()
{
	if (!GetWorld())
	{
		return;
	}

	TSubclassOf<ABoidAgent> SpawnClass = AgentClass;
	if (!SpawnClass)
	{
		SpawnClass = ABoidAgent::StaticClass();
	}
	Boids.Reserve(BoidCount);

	const FVector SpawnExtent = SimulationBoundsExtent * FMath::Clamp(SpawnFillPercent, 0.1f, 1.0f);
	const FVector Center = GetActorLocation();

	// 在边界盒内随机生成初始位置与速度。
	for (int32 Index = 0; Index < BoidCount; ++Index)
	{
		const FVector RandomOffset = FVector(
			FMath::FRandRange(-SpawnExtent.X, SpawnExtent.X),
			FMath::FRandRange(-SpawnExtent.Y, SpawnExtent.Y),
			FMath::FRandRange(-SpawnExtent.Z, SpawnExtent.Z));

		const FVector SpawnLocation = Center + RandomOffset;
		const FVector InitialDirection = UKismetMathLibrary::RandomUnitVector();
		const float InitialSpeed = FMath::FRandRange(MinSpeed, MaxSpeed);
		const FVector InitialVelocity = InitialDirection * InitialSpeed;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ABoidAgent* Boid = GetWorld()->SpawnActor<ABoidAgent>(
			SpawnClass,
			SpawnLocation,
			InitialDirection.Rotation(),
			SpawnParams);

		if (!Boid)
		{
			continue;
		}

		Boid->SetVelocity(InitialVelocity);
		// 生成时应用当前统一缩放。
		Boid->SetVisualScale(FishVisualScale);
		Boids.Add(Boid);
	}

	if (bEnableRandomSubSchools)
	{
		const int32 MinCount = FMath::Max(1, MinRandomSubSchoolCount);
		const int32 MaxCount = FMath::Max(MinCount, MaxRandomSubSchoolCount);
		CurrentRandomSubSchoolCount = FMath::RandRange(MinCount, MaxCount);
	}
	else
	{
		CurrentRandomSubSchoolCount = 1;
	}
	EnsureSubSchoolAssignments();
	LastAppliedFishVisualScale = FishVisualScale;
}

void ABoidManager::DestroyBoids()
{
	if (IsValid(ExternallyControlledBoid))
	{
		ExternallyControlledBoid->SetCamera1Highlighted(false);
	}
	ExternallyControlledBoid = nullptr;

	for (const TObjectPtr<ABoidAgent>& Boid : Boids)
	{
		if (IsValid(Boid))
		{
			Boid->Destroy();
		}
	}

	Boids.Reset();
	BoidSubSchoolIds.Reset();
	CurrentRandomSubSchoolCount = 1;
}

void ABoidManager::ApplyFishVisualScaleToAllBoids()
{
	// 把 BoidManager 的统一缩放推送到每条鱼实例。
	for (const TObjectPtr<ABoidAgent>& Boid : Boids)
	{
		if (!IsValid(Boid))
		{
			continue;
		}

		Boid->SetVisualScale(FishVisualScale);
	}

	LastAppliedFishVisualScale = FishVisualScale;
}

void ABoidManager::SimulateBoids(float DeltaTime)
{
	if (DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 清理失效实例。
	Boids.RemoveAll([](const TObjectPtr<ABoidAgent>& Boid)
	{
		return !IsValid(Boid);
	});
	if (ExternallyControlledBoid && !IsValid(ExternallyControlledBoid))
	{
		ExternallyControlledBoid = nullptr;
	}

	if (Boids.Num() == 0)
	{
		bHasExternalLeaderData = false;
		return;
	}
	EnsureSubSchoolAssignments();
	MaybeRefreshRandomSubSchools(DeltaTime);

	UWorld* World = GetWorld();

	// 先拍快照，再统一计算，避免同一帧读写顺序影响结果。
	TArray<FBoidSnapshot> Snapshots;
	Snapshots.Reserve(Boids.Num());
	for (const TObjectPtr<ABoidAgent>& Boid : Boids)
	{
		FBoidSnapshot Snapshot;
		Snapshot.Location = Boid->GetActorLocation();
		Snapshot.Velocity = Boid->GetBoidVelocity();
		Snapshots.Add(Snapshot);
	}

	bHasExternalLeaderData = false;
	if (IsValid(ExternallyControlledBoid))
	{
		const int32 LeaderIndex = Boids.IndexOfByPredicate([this](const TObjectPtr<ABoidAgent>& Candidate)
		{
			return Candidate.Get() == ExternallyControlledBoid.Get();
		});

		if (Snapshots.IsValidIndex(LeaderIndex))
		{
			bHasExternalLeaderData = true;
			ExternalLeaderLocation = Snapshots[LeaderIndex].Location;
			ExternalLeaderVelocity = Snapshots[LeaderIndex].Velocity;
		}
	}

	// 逐条鱼计算合力 -> 新速度 -> 新位置 -> 朝向。
	for (int32 Index = 0; Index < Boids.Num(); ++Index)
	{
		ABoidAgent* Boid = Boids[Index];
		if (!IsValid(Boid))
		{
			continue;
		}

		// 外部控制鱼交给玩家逻辑推进，这里跳过自动模拟。
		if (Boid == ExternallyControlledBoid.Get())
		{
			continue;
		}

		const FBoidSnapshot& Snapshot = Snapshots[Index];

		const FVector TotalForce = ComputeBoidForce(Index, Snapshots);
		FVector NewVelocity = Snapshot.Velocity + TotalForce * DeltaTime;

		const float Speed = NewVelocity.Size();
		if (Speed < MinSpeed)
		{
			const FVector Direction = Speed > KINDA_SMALL_NUMBER
				? NewVelocity / Speed
				: UKismetMathLibrary::RandomUnitVector();
			NewVelocity = Direction * MinSpeed;
		}
		else if (Speed > MaxSpeed)
		{
			NewVelocity = NewVelocity / Speed * MaxSpeed;
		}

		const FVector NewLocation = ClampToBounds(Snapshot.Location + NewVelocity * DeltaTime);
		FVector DesiredFacingVelocity = NewVelocity;
		if (bUseGroupFacingDirection && GroupFacingBlend > 0.0f)
		{
			const FVector GroupFacingDirection = ComputeNeighborAverageVelocityDirection(Index, Snapshots);
			if (!GroupFacingDirection.IsNearlyZero())
			{
				const FVector SelfFacingDirection = NewVelocity.IsNearlyZero()
					? Snapshot.Velocity.GetSafeNormal()
					: NewVelocity.GetSafeNormal();
				FVector FacingDirection = GroupFacingDirection;
				if (!SelfFacingDirection.IsNearlyZero())
				{
					const float BlendAlpha = FMath::Clamp(GroupFacingBlend, 0.0f, 1.0f);
					FacingDirection = FMath::Lerp(SelfFacingDirection, GroupFacingDirection, BlendAlpha).GetSafeNormal();
				}

				if (!FacingDirection.IsNearlyZero())
				{
					DesiredFacingVelocity = FacingDirection * FMath::Max(NewVelocity.Size(), MinSpeed);
				}
			}
		}

		Boid->SetSimulationState(
			NewLocation,
			NewVelocity,
			DesiredFacingVelocity,
			DeltaTime,
			RotationInterpSpeed,
			FacingDeadZoneDegrees,
			FacingTurnSpeedDegPerSec,
			FacingMinSpeed);

		// 调试：画当前鱼位置。
		if (World && bDrawDebugBoidPoints)
		{
			DrawDebugSphere(
				World,
				NewLocation,
				DebugBoidPointRadius,
				8,
				FColor::Yellow,
				false,
				DebugDrawDuration,
				0,
				1.6f);
		}

		// 调试：画速度向量。
		if (World && bDrawDebugVelocity)
		{
			DrawDebugLine(
				World,
				NewLocation,
				NewLocation + NewVelocity * DebugVelocityScale,
				FColor::Green,
				false,
				DebugDrawDuration,
				0,
				1.3f);
		}
	}
}

void ABoidManager::UpdateSwarmStateCycle(float DeltaTime)
{
	if (DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (bEnableStateCycling)
	{
		SwarmStateElapsed += DeltaTime;
		const float CurrentDuration = (CurrentSwarmState == EBoidSwarmState::SplitFlow)
			? FMath::Max(SplitStateDuration, 0.1f)
			: FMath::Max(CohesiveStateDuration, 0.1f);

		if (SwarmStateElapsed >= CurrentDuration)
		{
			SwarmStateElapsed = 0.0f;
			CurrentSwarmState = (CurrentSwarmState == EBoidSwarmState::SplitFlow)
				? EBoidSwarmState::Cohesive
				: EBoidSwarmState::SplitFlow;
		}
	}
	else if (bEnableRandomStateTransitions)
	{
		SwarmStateElapsed += DeltaTime;
		const float HoldTime = FMath::Max(RandomStateMinHoldTime, 0.0f);
		if (SwarmStateElapsed >= HoldTime)
		{
			const float ChancePerSecond = (CurrentSwarmState == EBoidSwarmState::SplitFlow)
				? FMath::Max(ExitSplitChancePerSecond, 0.0f)
				: FMath::Max(EnterSplitChancePerSecond, 0.0f);
			const float ToggleChance = FMath::Clamp(ChancePerSecond * DeltaTime, 0.0f, 1.0f);

			if (FMath::FRand() < ToggleChance)
			{
				CurrentSwarmState = (CurrentSwarmState == EBoidSwarmState::SplitFlow)
					? EBoidSwarmState::Cohesive
					: EBoidSwarmState::SplitFlow;
				SwarmStateElapsed = 0.0f;
			}
		}
	}

	const float TargetSplitAlpha = (CurrentSwarmState == EBoidSwarmState::SplitFlow) ? 1.0f : 0.0f;
	SplitModeAlpha = FMath::FInterpTo(
		SplitModeAlpha,
		TargetSplitAlpha,
		DeltaTime,
		FMath::Max(StateBlendInterpSpeed, 0.1f));
}

void ABoidManager::EnsureSubSchoolAssignments()
{
	if (!bEnableRandomSubSchools || Boids.Num() == 0)
	{
		CurrentRandomSubSchoolCount = 1;
		BoidSubSchoolIds.Reset();
		return;
	}

	const int32 MinCount = FMath::Max(1, MinRandomSubSchoolCount);
	const int32 MaxCount = FMath::Max(MinCount, MaxRandomSubSchoolCount);
	if (CurrentRandomSubSchoolCount < MinCount || CurrentRandomSubSchoolCount > MaxCount)
	{
		CurrentRandomSubSchoolCount = FMath::Clamp(CurrentRandomSubSchoolCount, MinCount, MaxCount);
	}

	if (BoidSubSchoolIds.Num() != Boids.Num())
	{
		BoidSubSchoolIds.SetNum(Boids.Num());
		for (int32 Index = 0; Index < BoidSubSchoolIds.Num(); ++Index)
		{
			BoidSubSchoolIds[Index] = static_cast<uint8>(FMath::RandRange(0, CurrentRandomSubSchoolCount - 1));
		}
	}
}

void ABoidManager::MaybeRefreshRandomSubSchools(float DeltaTime)
{
	if (!bEnableRandomSubSchools || Boids.Num() == 0 || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 MinCount = FMath::Max(1, MinRandomSubSchoolCount);
	const int32 MaxCount = FMath::Max(MinCount, MaxRandomSubSchoolCount);

	const float CountChangeChance = FMath::Clamp(SubSchoolCountChangeChancePerSecond * DeltaTime, 0.0f, 1.0f);
	if (FMath::FRand() < CountChangeChance)
	{
		const int32 NewCount = FMath::RandRange(MinCount, MaxCount);
		if (NewCount != CurrentRandomSubSchoolCount)
		{
			CurrentRandomSubSchoolCount = NewCount;
			for (int32 Index = 0; Index < BoidSubSchoolIds.Num(); ++Index)
			{
				BoidSubSchoolIds[Index] = static_cast<uint8>(FMath::RandRange(0, CurrentRandomSubSchoolCount - 1));
			}
		}
	}

	if (CurrentRandomSubSchoolCount <= 1)
	{
		return;
	}

	const float ReassignChance = FMath::Clamp(BoidSubSchoolReassignChancePerSecond * DeltaTime, 0.0f, 1.0f);
	if (ReassignChance <= 0.0f)
	{
		return;
	}

	for (int32 Index = 0; Index < BoidSubSchoolIds.Num(); ++Index)
	{
		if (FMath::FRand() < ReassignChance)
		{
			BoidSubSchoolIds[Index] = static_cast<uint8>(FMath::RandRange(0, CurrentRandomSubSchoolCount - 1));
		}
	}
}

void ABoidManager::UpdateAnchorLocation(bool bForceUpdate)
{
	if (!bAnchorToPlayer || !GetWorld())
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PlayerController))
	{
		return;
	}

	// 根据玩家视角更新锚点中心。
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector EffectiveAnchorOffset = bRotateAnchorOffsetWithPlayer
		? ViewRotation.RotateVector(PlayerAnchorOffset)
		: PlayerAnchorOffset;
	const FVector DesiredLocation = ViewLocation + EffectiveAnchorOffset;
	const FVector Delta = DesiredLocation - GetActorLocation();
	if (Delta.IsNearlyZero())
	{
		return;
	}

	if (!bForceUpdate && AnchorRecenterDistance > 0.0f && Delta.SizeSquared() < FMath::Square(AnchorRecenterDistance))
	{
		return;
	}

	SetActorLocation(DesiredLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (!bMoveBoidsWithAnchor)
	{
		return;
	}

	// 若开启“随锚点一起平移”，则把全体鱼整体平移同样的 Delta。
	for (const TObjectPtr<ABoidAgent>& Boid : Boids)
	{
		if (!IsValid(Boid))
		{
			continue;
		}

		const FVector NewBoidLocation = Boid->GetActorLocation() + Delta;
		Boid->SetActorLocation(NewBoidLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FVector ABoidManager::ComputeBoidForce(int32 BoidIndex, const TArray<FBoidSnapshot>& Snapshots) const
{
	// 读取当前鱼状态。
	const bool bIsControlledLeader = Boids.IsValidIndex(BoidIndex) && (Boids[BoidIndex].Get() == ExternallyControlledBoid.Get());
	const bool bUsingBaitballMode = bForceBaitballAtWorldOrigin && !bIsControlledLeader;
	const FVector SelfLocation = Snapshots[BoidIndex].Location;
	const FVector SelfVelocity = Snapshots[BoidIndex].Velocity;
	const float TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float SplitAlpha = FMath::Clamp(SplitModeAlpha, 0.0f, 1.0f);
	const float BaseRulesScale = bUsingBaitballMode ? FMath::Clamp(BaitballBaseRulesScale, 0.0f, 1.0f) : 1.0f;
	const float NeighborRadiusSq = NeighborRadius * NeighborRadius;
	const float EffectiveSeparationRadius = FMath::Max(DesiredFishSpacing, 1.0f);
	const float SeparationRadiusSq = EffectiveSeparationRadius * EffectiveSeparationRadius;

	FVector AlignmentSum = FVector::ZeroVector;
	FVector CohesionCenterSum = FVector::ZeroVector;
	FVector SeparationSum = FVector::ZeroVector;
	FVector SeparationCenterSum = FVector::ZeroVector;
	FVector ClosestNeighborDelta = FVector::ZeroVector;

	int32 NeighborCount = 0;
	int32 SeparationCount = 0;
	float NeighborDistanceSum = 0.0f;
	float ClosestNeighborDistanceSq = TNumericLimits<float>::Max();

	// 统计邻域信息：对齐、聚合、分离。
	for (int32 OtherIndex = 0; OtherIndex < Snapshots.Num(); ++OtherIndex)
	{
		if (OtherIndex == BoidIndex)
		{
			continue;
		}

		const FVector Delta = Snapshots[OtherIndex].Location - SelfLocation;
		const float DistanceSq = Delta.SizeSquared();
		if (DistanceSq <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (DistanceSq <= NeighborRadiusSq)
		{
			AlignmentSum += Snapshots[OtherIndex].Velocity;
			CohesionCenterSum += Snapshots[OtherIndex].Location;
			NeighborDistanceSum += FMath::Sqrt(DistanceSq);
			++NeighborCount;
		}

		if (DistanceSq <= SeparationRadiusSq)
		{
			const float Distance = FMath::Sqrt(DistanceSq);
			const float PushStrength = 1.0f - FMath::Clamp(Distance / EffectiveSeparationRadius, 0.0f, 1.0f);
			SeparationSum -= (Delta / Distance) * PushStrength;
			SeparationCenterSum += Snapshots[OtherIndex].Location;

			if (DistanceSq < ClosestNeighborDistanceSq)
			{
				ClosestNeighborDistanceSq = DistanceSq;
				ClosestNeighborDelta = Delta;
			}
			++SeparationCount;
		}
	}

	FVector ResultForce = FVector::ZeroVector;

	// 对齐 + 聚合（并依据密度动态衰减聚合强度，减少抱团）。
	if (NeighborCount > 0)
	{
		const FVector AlignmentDesired = AlignmentSum / static_cast<float>(NeighborCount);
		const FVector CohesionCenter = CohesionCenterSum / static_cast<float>(NeighborCount);
		const FVector CohesionDesired = (CohesionCenter - SelfLocation).GetSafeNormal() * MaxSpeed;
		const float AverageNeighborDistance = NeighborDistanceSum / static_cast<float>(NeighborCount);
		const float DensityAlpha = 1.0f - FMath::Clamp(
			(AverageNeighborDistance - EffectiveSeparationRadius) / FMath::Max(NeighborRadius - EffectiveSeparationRadius, 1.0f),
			0.0f,
			1.0f);
			const float DenseCohesionScale = 1.0f - DensityAlpha;
			const float DynamicCohesionWeight = CohesionWeight
				* FMath::Lerp(1.0f, 0.1f, DenseCohesionScale)
				* FMath::Lerp(1.35f, 0.65f, SplitAlpha)
				* BaseRulesScale;
			const float DynamicAlignmentWeight = AlignmentWeight
				* FMath::Lerp(1.0f, 0.65f, DenseCohesionScale)
				* FMath::Lerp(1.05f, 0.90f, SplitAlpha)
				* BaseRulesScale;

		ResultForce += ComputeSteerTowards(SelfVelocity, AlignmentDesired.GetSafeNormal() * MaxSpeed) * DynamicAlignmentWeight;
		ResultForce += ComputeSteerTowards(SelfVelocity, CohesionDesired) * DynamicCohesionWeight;
	}

	// 分离（核心间距控制来自 DesiredFishSpacing）。
	if (SeparationCount > 0)
	{
		const FVector SeparationCenter = SeparationCenterSum / static_cast<float>(SeparationCount);
		const FVector AwayFromCloseCenter = (SelfLocation - SeparationCenter).GetSafeNormal();
		const FVector AwayFromClosest = (ClosestNeighborDistanceSq < TNumericLimits<float>::Max())
			? (-ClosestNeighborDelta).GetSafeNormal()
			: FVector::ZeroVector;
		const FVector SeparationBlend = SeparationSum + AwayFromCloseCenter * 1.4f + AwayFromClosest * 1.8f;
			const float ProximityAlpha = (ClosestNeighborDistanceSq < TNumericLimits<float>::Max())
				? 1.0f - FMath::Clamp(FMath::Sqrt(ClosestNeighborDistanceSq) / EffectiveSeparationRadius, 0.0f, 1.0f)
				: 0.0f;
			const FVector SeparationDesired = SeparationBlend.GetSafeNormal() * FMath::Lerp(MaxSpeed * 0.55f, MaxSpeed, ProximityAlpha);
			const float DynamicSeparationWeight = SeparationWeight * FMath::Lerp(0.95f, 1.25f, SplitAlpha) * BaseRulesScale;
			ResultForce += ComputeSteerTowards(SelfVelocity, SeparationDesired) * DynamicSeparationWeight;
		}

	// 分群流场：空间中的低频噪声会驱动鱼群形成子群，并随时间重组。
	if (bEnableSubSchoolFlow && SubSchoolFlowWeight > 0.0f && !bUsingBaitballMode)
	{
		const float T = TimeSeconds * SubSchoolFlowTimeScale;
		const FVector P = SelfLocation * SubSchoolFlowSpaceScale;
		const FVector FlowVector(
			FMath::PerlinNoise3D(P + FVector(T, 0.0f, 0.0f)),
			FMath::PerlinNoise3D(P + FVector(31.3f, T, 0.0f)),
			FMath::PerlinNoise3D(P + FVector(0.0f, 57.7f, T)));

		if (!FlowVector.IsNearlyZero())
		{
			const FVector FlowDesired = FlowVector.GetSafeNormal() * MaxSpeed;
			const float DynamicFlowWeight = SubSchoolFlowWeight * SplitAlpha;
			ResultForce += ComputeSteerTowards(SelfVelocity, FlowDesired) * DynamicFlowWeight;
		}
	}

	// 随机多子群：每条鱼朝其所属子群的动态锚点靠拢，形成多团并持续随机重组。
	if (bEnableRandomSubSchools
		&& RandomSubSchoolAnchorWeight > 0.0f
		&& CurrentRandomSubSchoolCount > 1
		&& BoidSubSchoolIds.IsValidIndex(BoidIndex)
		&& !bUsingBaitballMode)
	{
		const int32 SchoolId = static_cast<int32>(BoidSubSchoolIds[BoidIndex]) % CurrentRandomSubSchoolCount;
		const FVector SchoolAnchor = ComputeRandomSubSchoolAnchor(SchoolId, TimeSeconds);
		const FVector ToSchoolAnchor = SchoolAnchor - SelfLocation;
		if (!ToSchoolAnchor.IsNearlyZero())
		{
			const FVector SchoolDesired = ToSchoolAnchor.GetSafeNormal() * MaxSpeed;
			const float DynamicSchoolWeight = RandomSubSchoolAnchorWeight * FMath::Lerp(0.28f, 1.0f, SplitAlpha);
			ResultForce += ComputeSteerTowards(SelfVelocity, SchoolDesired) * DynamicSchoolWeight;
		}
	}

	// 领头鱼引领：让其他鱼更明显跟随“外部控制鱼”。
	if (bEnableLeaderInfluence
		&& bHasExternalLeaderData
		&& bExternalLeaderInfluenceEnabled
		&& !bIsControlledLeader
		&& !bForceBaitballAtWorldOrigin
		&& LeaderInfluenceRadius > 0.0f)
	{
		const FVector PredictedLeaderLocation =
			ExternalLeaderLocation + ExternalLeaderVelocity * FMath::Max(LeaderPredictionTime, 0.0f);
		const FVector ToLeader = PredictedLeaderLocation - SelfLocation;
		const float LeaderDistSq = ToLeader.SizeSquared();
		if (!ToLeader.IsNearlyZero())
		{
			const float LeaderDist = FMath::Sqrt(FMath::Max(LeaderDistSq, 0.0f));
			const float ProximityLinear = 1.0f - FMath::Clamp(LeaderDist / LeaderInfluenceRadius, 0.0f, 1.0f);
			const float ProximityFalloff = FMath::Pow(ProximityLinear, FMath::Max(LeaderFalloffExponent, 0.1f));
			const float InfluenceAlpha = FMath::Max(
				FMath::Clamp(LeaderMinInfluenceAlpha, 0.0f, 1.0f),
				ProximityFalloff);

			const FVector LeaderFollowDesired = ToLeader.GetSafeNormal() * MaxSpeed;
			const float DynamicLeaderFollowWeight = LeaderFollowWeight
				* FMath::Lerp(0.8f, 1.2f, SplitAlpha)
				* FMath::Lerp(0.45f, 1.0f, InfluenceAlpha);
			ResultForce += ComputeSteerTowards(SelfVelocity, LeaderFollowDesired) * DynamicLeaderFollowWeight;

			if (!ExternalLeaderVelocity.IsNearlyZero())
			{
				const FVector LeaderAlignDesired = ExternalLeaderVelocity.GetSafeNormal() * MaxSpeed;
				const float DynamicLeaderAlignWeight = LeaderAlignmentWeight
					* FMath::Lerp(0.65f, 1.0f, SplitAlpha)
					* InfluenceAlpha;
				ResultForce += ComputeSteerTowards(SelfVelocity, LeaderAlignDesired) * DynamicLeaderAlignWeight;
			}
		}
	}

	// Force baitball mode around fixed world position (0,0,0 by default).
	if (bUsingBaitballMode)
	{
		const bool bUseBaitballV2 = (BaitballVersion == EBoidBaitballVersion::Version2_Enhanced);
		FVector DynamicBaitballTarget = BaitballWorldTarget;
		if (bEnableNaturalBaitballVariation && BaitballCenterWobbleAmplitude > 0.0f)
		{
			const float WobbleT = TimeSeconds * FMath::Max(BaitballCenterWobbleSpeed, 0.0f);
			const FVector WobbleOffset(
				FMath::Sin(WobbleT * 1.17f + 0.9f),
				FMath::Sin(WobbleT * 0.83f + 2.2f),
				FMath::Sin(WobbleT * 1.31f + 1.5f) * 0.45f);
			DynamicBaitballTarget += WobbleOffset * BaitballCenterWobbleAmplitude;
		}

		const FVector BaitballTargetForPlanar = bBaitballLockToTargetZ
			? FVector(DynamicBaitballTarget.X, DynamicBaitballTarget.Y, SelfLocation.Z)
			: DynamicBaitballTarget;
		const FVector ToBaitball = BaitballTargetForPlanar - SelfLocation;
		const float DistToBaitball = ToBaitball.Size();
		float BaseDesiredRadius = FMath::Max(BaitballDesiredRadius, 10.0f);
		const int32 ActiveBaitballCount = FMath::Max(
			Boids.Num() - ((bHasExternalLeaderData && IsValid(ExternallyControlledBoid)) ? 1 : 0),
			1);
		const float EffectivePopulationSpacing =
			FMath::Max(DesiredFishSpacing * FMath::Max(BaitballPopulationSpacingScale, 0.1f), 1.0f);
		if (bUseBaitballV2 && bAutoFitBaitballRadiusToPopulation && ActiveBaitballCount > 0)
		{
			// Approximate one-layer shell capacity on sphere surface:
			// N ~= 4*pi*R^2 / AreaPerFish  =>  R ~= sqrt(N*AreaPerFish/(4*pi))
			const float Packing = FMath::Clamp(BaitballPopulationPackingEfficiency, 0.1f, 1.0f);
			const float AreaPerFish = (EffectivePopulationSpacing * EffectivePopulationSpacing) / Packing;
			const float RequiredRadius = FMath::Sqrt((static_cast<float>(ActiveBaitballCount) * AreaPerFish) / (4.0f * PI));
			BaseDesiredRadius = FMath::Max(
				BaseDesiredRadius,
				RequiredRadius * FMath::Max(BaitballPopulationRadiusSafetyScale, 1.0f));
		}
			float DesiredRadius = BaseDesiredRadius;
			if (bEnableNaturalBaitballVariation)
			{
			const float BreathingT = TimeSeconds * FMath::Max(BaitballBreathingSpeed, 0.0f);
			const float BreathingAlpha = FMath::Sin(BreathingT * 2.0f * PI);
			const float BreathingScale = 1.0f + BreathingAlpha * FMath::Max(BaitballBreathingAmplitude, 0.0f);

			const float NoiseT = TimeSeconds * FMath::Max(BaitballRadiusNoiseTimeScale, 0.0f);
			const FVector NoiseP = SelfLocation * FMath::Max(BaitballRadiusNoiseSpaceScale, 0.00001f)
				+ FVector(NoiseT, NoiseT * 0.77f, NoiseT * 1.19f);
				const float RadiusNoise = FMath::PerlinNoise3D(NoiseP);
				const float NoiseScale = 1.0f + RadiusNoise * FMath::Max(BaitballRadiusNoiseAmplitude, 0.0f);
				DesiredRadius = BaseDesiredRadius * FMath::Clamp(BreathingScale * NoiseScale, 0.55f, 1.55f);
			}
		float InnerVoidRadius = FMath::Clamp(BaitballInnerVoidRadius, 0.0f, DesiredRadius * 0.98f);
		if (bUseBaitballV2 && bPreferLayeredBaitballShell)
			{
				InnerVoidRadius = FMath::Clamp(
					InnerVoidRadius * FMath::Clamp(BaitballLayeredInnerVoidScale, 0.0f, 1.0f),
					0.0f,
					DesiredRadius * 0.98f);
			}
		else if (bUseBaitballV2 && bAutoSetInnerVoidForShellLayers)
			{
				const float TargetShellThickness = FMath::Max(
					FMath::Max(BaitballMinShellThickness, 1.0f),
					EffectivePopulationSpacing * FMath::Max(BaitballTargetShellLayers, 0.2f));
				const float AutoInnerVoid = FMath::Clamp(DesiredRadius - TargetShellThickness, 0.0f, DesiredRadius * 0.98f);
				// Use the larger inner void to keep shell thinner and reduce multi-layer stacking.
				InnerVoidRadius = FMath::Max(InnerVoidRadius, AutoInnerVoid);
			}
		const float ShellBand = FMath::Max(BaitballShellBandWidth, 0.0f);

		if (DistToBaitball > KINDA_SMALL_NUMBER)
		{
			const FVector CenterDir = ToBaitball / DistToBaitball;
			const FVector AxisUp = FVector::UpVector;
			const FVector RelToCenter = SelfLocation - DynamicBaitballTarget;
			const float AxisSignedHeight = FVector::DotProduct(RelToCenter, AxisUp);
			FVector AxisPlanar = RelToCenter - AxisUp * AxisSignedHeight;
			float AxisPlanarDist = AxisPlanar.Size();
			if (AxisPlanarDist <= KINDA_SMALL_NUMBER)
			{
				AxisPlanar = FVector::RightVector;
				AxisPlanarDist = 1.0f;
			}
			const FVector AxisPlanarDir = AxisPlanar / AxisPlanarDist;

			if (bEnableBaitballToroidalBias)
			{
				// 1) 赤道回拉：把鱼压回中心水平层，形成“扁环”而不是球。
				if (BaitballEquatorAttractionWeight > 0.0f && FMath::Abs(AxisSignedHeight) > KINDA_SMALL_NUMBER)
				{
					const FVector ToEquatorDir = (AxisSignedHeight > 0.0f) ? -AxisUp : AxisUp;
					const float EquatorAlpha = FMath::Clamp(FMath::Abs(AxisSignedHeight) / FMath::Max(DesiredRadius, 1.0f), 0.0f, 1.0f);
					const FVector EquatorDesired = ToEquatorDir * (MaxSpeed * 0.7f);
					ResultForce += ComputeSteerTowards(SelfVelocity, EquatorDesired)
						* BaitballEquatorAttractionWeight
						* EquatorAlpha;
				}

				// 2) 轴心排斥：保持中轴空洞。
				if (BaitballAxisRepulsionWeight > 0.0f && BaitballAxisVoidRadius > 0.0f && AxisPlanarDist < BaitballAxisVoidRadius)
				{
					const float AxisVoidAlpha = 1.0f - FMath::Clamp(AxisPlanarDist / BaitballAxisVoidRadius, 0.0f, 1.0f);
					const FVector AxisPushDesired = AxisPlanarDir * MaxSpeed;
					ResultForce += ComputeSteerTowards(SelfVelocity, AxisPushDesired)
						* BaitballAxisRepulsionWeight
						* FMath::Lerp(0.8f, 2.0f, AxisVoidAlpha);
				}

				// 3) 围轴环向流动：更接近参考图的旋转纹理。
				if (BaitballAxisCirculationWeight > 0.0f)
				{
					FVector AxisCirculationDir = FVector::CrossProduct(AxisUp, AxisPlanarDir);
					if (!AxisCirculationDir.IsNearlyZero())
					{
						const FVector AxisCirculationDesired = AxisCirculationDir.GetSafeNormal() * (MaxSpeed * 0.78f);
						ResultForce += ComputeSteerTowards(SelfVelocity, AxisCirculationDesired) * BaitballAxisCirculationWeight;
					}
				}
			}

				float TopOpeningSuppression = 1.0f;
				if (bUseBaitballV2 && bPreferLayeredBaitballShell)
				{
					const float TopCapAlpha = FMath::Pow(
						FMath::Clamp(AxisSignedHeight / FMath::Max(DesiredRadius, 1.0f), 0.0f, 1.0f),
						FMath::Max(BaitballTopOpeningExponent, 0.5f));
					TopOpeningSuppression = 1.0f - FMath::Clamp(BaitballTopOpeningStrength, 0.0f, 1.0f) * TopCapAlpha;
				}

				float ShellError = 0.0f;
				float ShellReferenceRadius = DesiredRadius;
				FVector RadialDir = CenterDir;

				if (bUseBaitballV2 && bPreferLayeredBaitballShell && bUseToroidalLayerMetric)
				{
					const float LayerSpacing = FMath::Max(
						EffectivePopulationSpacing * FMath::Max(BaitballLayerSpacingScale, 0.1f),
						1.0f);
					const int32 MaxLayerOffset = FMath::Max(BaitballMaxLayerOffset, 1);
					const float MajorRadius = FMath::Clamp(
						DesiredRadius * FMath::Max(BaitballLayerMajorRadiusScale, 0.2f),
						LayerSpacing * 2.0f,
						DesiredRadius * 2.5f);
					const float BaseTubeRadius = FMath::Clamp(
						DesiredRadius * FMath::Max(BaitballLayerTubeRadiusScale, 0.1f),
						LayerSpacing * 0.5f,
						DesiredRadius * 1.2f);

					const FVector TubeCenter = DynamicBaitballTarget + AxisPlanarDir * MajorRadius;
					const FVector ToTubeCenter = TubeCenter - SelfLocation;
					const float TubeDist = ToTubeCenter.Size();
					const float TubeOffset = TubeDist - BaseTubeRadius;
					const float LayerIndex = FMath::Clamp(
						FMath::RoundToFloat(TubeOffset / LayerSpacing),
						-static_cast<float>(MaxLayerOffset),
						static_cast<float>(MaxLayerOffset));
					const float LayerIndexAlpha = FMath::Clamp(
						FMath::Abs(LayerIndex) / static_cast<float>(MaxLayerOffset),
						0.0f,
						1.0f);
					const float LayerSpacingForIndex = LayerSpacing * FMath::Lerp(
						1.0f,
						FMath::Clamp(BaitballOuterLayerSpacingScale, 0.2f, 1.0f),
						LayerIndexAlpha);
					const float TargetTubeRadius = FMath::Clamp(
						BaseTubeRadius + LayerIndex * LayerSpacingForIndex,
						LayerSpacing * 0.5f,
						BaseTubeRadius + LayerSpacingForIndex * static_cast<float>(MaxLayerOffset));

					ShellError = TubeDist - TargetTubeRadius;
					ShellReferenceRadius = TargetTubeRadius;
					if (TubeDist > KINDA_SMALL_NUMBER)
					{
						RadialDir = (ShellError > 0.0f)
							? ToTubeCenter / TubeDist
							: -ToTubeCenter / TubeDist;
					}

					// Keep boids attached to the torus major ring.
					if (BaitballLayerMajorRadiusWeight > 0.0f)
					{
						const float MajorError = AxisPlanarDist - MajorRadius;
						const FVector MajorDir = (MajorError > 0.0f) ? -AxisPlanarDir : AxisPlanarDir;
						const float MajorAlpha = FMath::Clamp(
							FMath::Abs(MajorError) / FMath::Max(MajorRadius, 1.0f),
							0.0f,
							1.0f);
						const FVector MajorDesired = MajorDir * MaxSpeed;
						ResultForce += ComputeSteerTowards(SelfVelocity, MajorDesired)
							* BaitballLayerMajorRadiusWeight
							* FMath::Lerp(0.35f, 1.0f, MajorAlpha)
							* TopOpeningSuppression;
					}

					// Prevent outer bands from drifting away from the main school.
					if (LayerIndex > 0.0f && BaitballOuterLayerTetherWeight > 0.0f && TubeDist > KINDA_SMALL_NUMBER)
					{
						const float OuterAlpha = FMath::Pow(
							FMath::Clamp(LayerIndex / static_cast<float>(MaxLayerOffset), 0.0f, 1.0f),
							FMath::Max(BaitballOuterLayerTetherExponent, 0.1f));
						const FVector OuterInwardDesired = (ToTubeCenter / TubeDist) * MaxSpeed;
						ResultForce += ComputeSteerTowards(SelfVelocity, OuterInwardDesired)
							* BaitballOuterLayerTetherWeight
							* OuterAlpha
							* TopOpeningSuppression;
					}

					// Align motion along torus flow direction for layered stream-like stacking.
					if (BaitballLayerFlowAlignWeight > 0.0f)
					{
						FVector ToroidalFlowDir = FVector::CrossProduct(AxisUp, AxisPlanarDir);
						if (!ToroidalFlowDir.IsNearlyZero())
						{
							const FVector ToroidalFlowDesired = ToroidalFlowDir.GetSafeNormal() * (MaxSpeed * 0.82f);
							ResultForce += ComputeSteerTowards(SelfVelocity, ToroidalFlowDesired)
								* BaitballLayerFlowAlignWeight;
						}
					}
				}
				else
				{
					float TargetShellRadius = DesiredRadius;
					if (bUseBaitballV2 && bPreferLayeredBaitballShell)
					{
						const float LayerSpacing = FMath::Max(
							EffectivePopulationSpacing * FMath::Max(BaitballLayerSpacingScale, 0.1f),
							1.0f);
						const int32 MaxLayerOffset = FMath::Max(BaitballMaxLayerOffset, 1);
						const float RadialOffset = DistToBaitball - DesiredRadius;
						const float LayerIndex = FMath::Clamp(
							FMath::RoundToFloat(RadialOffset / LayerSpacing),
							-static_cast<float>(MaxLayerOffset),
							static_cast<float>(MaxLayerOffset));
						TargetShellRadius = DesiredRadius + LayerIndex * LayerSpacing;
						TargetShellRadius = FMath::Clamp(
							TargetShellRadius,
							InnerVoidRadius + LayerSpacing * 0.5f,
							DesiredRadius + LayerSpacing * static_cast<float>(MaxLayerOffset));
					}

					ShellError = DistToBaitball - TargetShellRadius;
					ShellReferenceRadius = TargetShellRadius;
					RadialDir = (ShellError > 0.0f) ? CenterDir : -CenterDir;
				}

				const float AbsShellError = FMath::Abs(ShellError);
				if (AbsShellError > KINDA_SMALL_NUMBER)
				{
					const float ErrorAlpha = FMath::Clamp(AbsShellError / FMath::Max(ShellReferenceRadius, 1.0f), 0.0f, 1.0f);
					const float BandAlpha = (ShellBand > 0.0f) ? FMath::Clamp(AbsShellError / ShellBand, 0.0f, 1.0f) : 1.0f;
					const float OuterBoostDistance = DesiredRadius * FMath::Max(BaitballOuterBoostRadiusMultiplier, 1.0f);
					const float OuterBoost = (ShellError > 0.0f && DistToBaitball > OuterBoostDistance)
						? FMath::Max(BaitballOuterAttractionBoost, 1.0f)
						: 1.0f;
					const float RadialWeight = BaitballAttractionWeight
						* BaitballShellTightness
						* FMath::Lerp(0.25f, 1.0f, ErrorAlpha)
						* BandAlpha
						* OuterBoost
						* ((bUseBaitballV2 && bPreferLayeredBaitballShell) ? FMath::Max(BaitballLayeredRadialAttractionBoost, 0.0f) : 1.0f)
						* TopOpeningSuppression;
					const FVector RadialDesired = RadialDir * MaxSpeed;
					ResultForce += ComputeSteerTowards(SelfVelocity, RadialDesired) * RadialWeight;
				}

			if (DistToBaitball < InnerVoidRadius && InnerVoidRadius > KINDA_SMALL_NUMBER)
			{
				const float VoidAlpha = 1.0f - FMath::Clamp(DistToBaitball / InnerVoidRadius, 0.0f, 1.0f);
				const FVector PushDesired = (-CenterDir) * MaxSpeed;
				ResultForce += ComputeSteerTowards(SelfVelocity, PushDesired)
					* BaitballInnerRepulsionWeight
					* FMath::Lerp(0.8f, 2.0f, VoidAlpha);
			}

			FVector SwirlAxis = FVector::UpVector;
			if (bEnableNaturalBaitballVariation)
			{
				const float AxisT = TimeSeconds * 0.31f;
				SwirlAxis = FVector(
					FMath::Sin(AxisT * 1.11f + 0.7f),
					FMath::Sin(AxisT * 0.87f + 2.4f),
					FMath::Cos(AxisT * 1.03f + 1.2f)).GetSafeNormal();
				if (SwirlAxis.IsNearlyZero())
				{
					SwirlAxis = FVector::UpVector;
				}
			}

			FVector SwirlDir = FVector::CrossProduct(CenterDir, SwirlAxis);
			if (SwirlDir.IsNearlyZero())
			{
				SwirlDir = FVector::CrossProduct(CenterDir, FVector::RightVector);
			}
			if (!SwirlDir.IsNearlyZero())
			{
				const FVector SwirlDesired = SwirlDir.GetSafeNormal() * (MaxSpeed * FMath::Max(BaitballSwirlSpeedScale, 0.0f));
				ResultForce += ComputeSteerTowards(SelfVelocity, SwirlDesired) * BaitballSwirlWeight;
			}

			if (bEnableNaturalBaitballVariation && BaitballTurbulenceWeight > 0.0f)
			{
				const float TurbT = TimeSeconds * 0.63f;
				const FVector TurbP = SelfLocation * 0.0031f + FVector(TurbT, TurbT * 1.3f, TurbT * 0.8f);
				FVector TurbVec(
					FMath::PerlinNoise3D(TurbP + FVector(13.1f, 0.0f, 0.0f)),
					FMath::PerlinNoise3D(TurbP + FVector(0.0f, 29.7f, 0.0f)),
					FMath::PerlinNoise3D(TurbP + FVector(0.0f, 0.0f, 47.9f)));
				TurbVec -= FVector::DotProduct(TurbVec, CenterDir) * CenterDir;
				if (!TurbVec.IsNearlyZero())
				{
					const float ShellProximity = 1.0f - FMath::Clamp(AbsShellError / FMath::Max(DesiredRadius, 1.0f), 0.0f, 1.0f);
					const FVector TurbDesired = TurbVec.GetSafeNormal() * (MaxSpeed * 0.6f);
					ResultForce += ComputeSteerTowards(SelfVelocity, TurbDesired)
						* BaitballTurbulenceWeight
						* FMath::Lerp(0.35f, 1.0f, ShellProximity);
				}
			}

			// 壳面邻居扩散：把局部密集鱼沿球面摊开，提升球壳覆盖度。
			if (BaitballSurfaceSpreadWeight > 0.0f && BaitballSurfaceNeighborRadius > 0.0f)
			{
				const float SpreadRadiusSq = FMath::Square(BaitballSurfaceNeighborRadius);
				FVector TangentialSpreadSum = FVector::ZeroVector;
				for (int32 OtherIndex = 0; OtherIndex < Snapshots.Num(); ++OtherIndex)
				{
					if (OtherIndex == BoidIndex)
					{
						continue;
					}

					const FVector ToOther = Snapshots[OtherIndex].Location - SelfLocation;
					const float DistSq = ToOther.SizeSquared();
					if (DistSq <= KINDA_SMALL_NUMBER || DistSq > SpreadRadiusSq)
					{
						continue;
					}

					const float Dist = FMath::Sqrt(DistSq);
					const float Falloff = 1.0f - FMath::Clamp(Dist / FMath::Max(BaitballSurfaceNeighborRadius, 1.0f), 0.0f, 1.0f);
					FVector AwayFromOther = (-ToOther / Dist);
					AwayFromOther -= FVector::DotProduct(AwayFromOther, CenterDir) * CenterDir;
					if (!AwayFromOther.IsNearlyZero())
					{
						TangentialSpreadSum += AwayFromOther.GetSafeNormal() * Falloff;
					}
				}

				if (!TangentialSpreadSum.IsNearlyZero())
				{
					const FVector SpreadDesired = TangentialSpreadSum.GetSafeNormal() * MaxSpeed;
					ResultForce += ComputeSteerTowards(SelfVelocity, SpreadDesired) * BaitballSurfaceSpreadWeight;
				}
			}

			// 每条鱼的稳定随机切向方向，打散单一旋涡（避免只形成 2/3 壳）。
			if (BaitballRandomTangentWeight > 0.0f)
			{
				const float Id = static_cast<float>(BoidIndex);
				FVector RandomSeed(
					FMath::Sin(Id * 12.9898f + 3.17f),
					FMath::Sin(Id * 78.2330f + 1.93f),
					FMath::Sin(Id * 37.7190f + 2.41f));
				if (!RandomSeed.IsNearlyZero())
				{
					RandomSeed = RandomSeed.GetSafeNormal();
					FVector RandomTangent = RandomSeed - FVector::DotProduct(RandomSeed, CenterDir) * CenterDir;
					if (!RandomTangent.IsNearlyZero())
					{
						const FVector RandomTangentialDesired = RandomTangent.GetSafeNormal() * (MaxSpeed * 0.65f);
						ResultForce += ComputeSteerTowards(SelfVelocity, RandomTangentialDesired) * BaitballRandomTangentWeight;
					}
				}
			}

			// 径向速度阻尼：减少穿壳，帮助停留在球壳表面。
			if (BaitballRadialVelocityDamping > 0.0f)
			{
				const float RadialSpeed = FVector::DotProduct(SelfVelocity, CenterDir);
				const FVector TangentialVelocity = SelfVelocity - CenterDir * RadialSpeed;
				if (!TangentialVelocity.IsNearlyZero())
				{
					const FVector TangentialDesired = TangentialVelocity.GetSafeNormal() * (MaxSpeed * 0.75f);
					ResultForce += ComputeSteerTowards(SelfVelocity, TangentialDesired) * BaitballRadialVelocityDamping;
				}
			}

			// 伪捕食者压迫：制造局部压缩/逃逸，避免“教科书式完美壳”。
			if (bEnableNaturalBaitballVariation && BaitballPredatorPressureWeight > 0.0f)
			{
				const float PredatorT = TimeSeconds * 0.42f;
				const FVector PredatorDir = FVector(
					FMath::Sin(PredatorT * 0.73f + 1.3f),
					FMath::Cos(PredatorT * 1.07f + 2.1f),
					FMath::Sin(PredatorT * 0.51f + 0.4f) * 0.25f).GetSafeNormal();
				const FVector PredatorPos = DynamicBaitballTarget + PredatorDir * (BaseDesiredRadius * 1.55f);
				const FVector AwayFromPredator = SelfLocation - PredatorPos;
				const float PredatorDistance = AwayFromPredator.Size();
				const float PredatorInfluenceRadius = BaseDesiredRadius * 1.7f;
				if (PredatorDistance < PredatorInfluenceRadius && !AwayFromPredator.IsNearlyZero())
				{
					const float PressureAlpha = 1.0f - FMath::Clamp(PredatorDistance / PredatorInfluenceRadius, 0.0f, 1.0f);
					const FVector PredatorEscapeDesired = AwayFromPredator.GetSafeNormal() * MaxSpeed;
					ResultForce += ComputeSteerTowards(SelfVelocity, PredatorEscapeDesired)
						* BaitballPredatorPressureWeight
						* PressureAlpha;
				}
			}
		}

		if (bBaitballLockToTargetZ)
		{
			const float ZDelta = DynamicBaitballTarget.Z - SelfLocation.Z;
			if (FMath::Abs(ZDelta) > KINDA_SMALL_NUMBER)
			{
				const FVector VerticalDesired = FVector::UpVector * FMath::Sign(ZDelta) * MaxSpeed * 0.5f;
				const float VerticalAlpha = FMath::Clamp(FMath::Abs(ZDelta) / FMath::Max(DesiredRadius, 1.0f), 0.0f, 1.0f);
				ResultForce += ComputeSteerTowards(SelfVelocity, VerticalDesired)
					* BaitballVerticalLockWeight
					* VerticalAlpha;
			}
		}
	}

	// 可选目标追踪。
	if (!(bUsingBaitballMode) && TargetActor && TargetWeight != 0.0f)
	{
		const FVector ToTarget = TargetActor->GetActorLocation() - SelfLocation;
		const FVector TargetDesired = ToTarget.GetSafeNormal() * MaxSpeed;
		ResultForce += ComputeSteerTowards(SelfVelocity, TargetDesired) * TargetWeight;
	}

	// 轻微回拉到群体中心，降低鱼群分裂概率。
	if (CenterAttractionWeight > 0.0f && !(bForceBaitballAtWorldOrigin && !bIsControlledLeader))
	{
		const FVector ToCenter = GetActorLocation() - SelfLocation;
		if (!ToCenter.IsNearlyZero())
		{
			const FVector CenterDesired = ToCenter.GetSafeNormal() * MaxSpeed;
			const float DynamicCenterWeight = CenterAttractionWeight * FMath::Lerp(1.45f, 0.55f, SplitAlpha);
			ResultForce += ComputeSteerTowards(SelfVelocity, CenterDesired) * DynamicCenterWeight;
		}
	}

	// 边界约束。
	ResultForce += ComputeBoundsForce(SelfLocation) * BoundsWeight;
	return ResultForce.GetClampedToMaxSize(MaxSteeringForce);
}

FVector ABoidManager::ComputeRandomSubSchoolAnchor(int32 SchoolId, float TimeSeconds) const
{
	const FVector SpreadExtent = SimulationBoundsExtent * FMath::Clamp(RandomSubSchoolSpread, 0.05f, 1.0f);
	const float T = TimeSeconds * FMath::Max(RandomSubSchoolAnchorTimeScale, 0.01f);
	const float Seed = static_cast<float>(SchoolId) * 17.317f + 3.91f;
	const FVector UnitOffset(
		FMath::Sin(T * 0.91f + Seed),
		FMath::Sin(T * 1.07f + Seed * 1.73f),
		FMath::Sin(T * 0.83f + Seed * 2.11f));

	return GetActorLocation() + FVector(
		UnitOffset.X * SpreadExtent.X,
		UnitOffset.Y * SpreadExtent.Y,
		UnitOffset.Z * SpreadExtent.Z * 0.75f);
}

FVector ABoidManager::ComputeSteerTowards(const FVector& CurrentVelocity, const FVector& DesiredVelocity) const
{
	const FVector Steering = DesiredVelocity - CurrentVelocity;
	return Steering.GetClampedToMaxSize(MaxSteeringForce);
}

FVector ABoidManager::ComputeNeighborAverageVelocityDirection(int32 BoidIndex, const TArray<FBoidSnapshot>& Snapshots) const
{
	if (!Snapshots.IsValidIndex(BoidIndex) || Snapshots.Num() <= 1)
	{
		return FVector::ZeroVector;
	}

	const float RadiusScale = FMath::Max(GroupFacingNeighborRadiusScale, 0.1f);
	const float EffectiveRadius = FMath::Max(NeighborRadius * RadiusScale, 1.0f);
	const float EffectiveRadiusSq = FMath::Square(EffectiveRadius);
	const FVector SelfLocation = Snapshots[BoidIndex].Location;

	FVector WeightedVelocitySum = FVector::ZeroVector;
	float TotalWeight = 0.0f;

	for (int32 OtherIndex = 0; OtherIndex < Snapshots.Num(); ++OtherIndex)
	{
		if (OtherIndex == BoidIndex)
		{
			continue;
		}

		const FVector Delta = Snapshots[OtherIndex].Location - SelfLocation;
		const float DistanceSq = Delta.SizeSquared();
		if (DistanceSq <= KINDA_SMALL_NUMBER || DistanceSq > EffectiveRadiusSq)
		{
			continue;
		}

		const FVector OtherVelocityDir = Snapshots[OtherIndex].Velocity.GetSafeNormal();
		if (OtherVelocityDir.IsNearlyZero())
		{
			continue;
		}

		const float Distance = FMath::Sqrt(DistanceSq);
		const float Weight = 1.0f - FMath::Clamp(Distance / EffectiveRadius, 0.0f, 1.0f);
		WeightedVelocitySum += OtherVelocityDir * Weight;
		TotalWeight += Weight;
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	return (WeightedVelocitySum / TotalWeight).GetSafeNormal();
}

FVector ABoidManager::ComputeBoundsForce(const FVector& Location) const
{
	// 在边界附近施加回推力，越贴边推力越大。
	const FVector Local = Location - GetActorLocation();
	FVector Push = FVector::ZeroVector;
	const float SafeBoundsBuffer = FMath::Max(BoundsBuffer, 1.0f);

	auto ComputeAxisPush = [this, SafeBoundsBuffer](float Value, float Extent) -> float
	{
		const float DistToPositive = Extent - Value;
		const float DistToNegative = Value + Extent;
		float AxisPush = 0.0f;

		if (DistToPositive < SafeBoundsBuffer)
		{
			AxisPush -= 1.0f - FMath::Clamp(DistToPositive / SafeBoundsBuffer, 0.0f, 1.0f);
		}
		if (DistToNegative < SafeBoundsBuffer)
		{
			AxisPush += 1.0f - FMath::Clamp(DistToNegative / SafeBoundsBuffer, 0.0f, 1.0f);
		}

		if (Value > Extent)
		{
			AxisPush -= 1.0f + (Value - Extent) / FMath::Max(Extent, 1.0f);
		}
		else if (Value < -Extent)
		{
			AxisPush += 1.0f + (-Extent - Value) / FMath::Max(Extent, 1.0f);
		}

		return AxisPush;
	};

	Push.X = ComputeAxisPush(Local.X, SimulationBoundsExtent.X);
	Push.Y = ComputeAxisPush(Local.Y, SimulationBoundsExtent.Y);
	Push.Z = ComputeAxisPush(Local.Z, SimulationBoundsExtent.Z);

	return Push.GetSafeNormal() * MaxSteeringForce;
}

FVector ABoidManager::ClampToBounds(const FVector& Location) const
{
	// 最终位置硬钳制，保证不会逃出模拟盒。
	const FVector Center = GetActorLocation();
	const FVector Local = Location - Center;

	return Center + FVector(
		FMath::Clamp(Local.X, -SimulationBoundsExtent.X, SimulationBoundsExtent.X),
		FMath::Clamp(Local.Y, -SimulationBoundsExtent.Y, SimulationBoundsExtent.Y),
		FMath::Clamp(Local.Z, -SimulationBoundsExtent.Z, SimulationBoundsExtent.Z));
}
