// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boids/BoidCameraManager.h"

#include "Boids/BoidAgent.h"
#include "Boids/BoidManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ABoidCameraManager::ABoidCameraManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABoidCameraManager::BeginPlay()
{
	Super::BeginPlay();

	// 单例保护：统一按 GetActorOfClass 返回的主实例保留，其他实例自毁。
	if (ABoidCameraManager* PrimaryCameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(this, ABoidCameraManager::StaticClass())))
	{
		if (PrimaryCameraManager != this)
		{
			Destroy();
			return;
		}
	}

	GetOrCreateTopViewCameraActor();
}

FVector ABoidCameraManager::ResolveFocusCenter() const
{
	if (const ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass())))
	{
		return BoidManager->GetSimulationCenter();
	}

	return GetActorLocation();
}

AActor* ABoidCameraManager::GetPrimaryFishViewTarget() const
{
	if (!bUseFishAsDefaultCamera1)
	{
		return nullptr;
	}

	const ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass()));
	if (!IsValid(BoidManager))
	{
		return nullptr;
	}

	const int32 BoidNum = BoidManager->GetBoidNum();
	if (BoidNum <= 0)
	{
		return nullptr;
	}

	const int32 SafeIndex = FMath::Abs(PrimaryFishIndex) % BoidNum;
	return BoidManager->GetBoidByIndex(SafeIndex);
}

ACameraActor* ABoidCameraManager::GetOrCreateTopViewCameraActor()
{
	if (IsValid(TopViewCameraActor))
	{
		return TopViewCameraActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Candidate = *It;
		if (IsValid(Candidate) && Candidate->ActorHasTag(TopViewCameraTag))
		{
			TopViewCameraActor = Candidate;
			if (TopViewCameraActor->GetCameraComponent())
			{
				TopViewCameraActor->GetCameraComponent()->SetFieldOfView(TopViewCameraFOV);
			}
			return TopViewCameraActor;
		}
	}

	if (!bAutoSpawnTopViewCamera)
	{
		return nullptr;
	}

	const FVector CameraLocation = ResolveFocusCenter() + TopViewCameraDefaultOffset;
	const FRotator CameraRotation = (ResolveFocusCenter() - CameraLocation).Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TopViewCameraActor = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		CameraLocation,
		CameraRotation,
		SpawnParams);

	if (!IsValid(TopViewCameraActor))
	{
		return nullptr;
	}

	TopViewCameraActor->Tags.AddUnique(TopViewCameraTag);
	if (TopViewCameraActor->GetCameraComponent())
	{
		TopViewCameraActor->GetCameraComponent()->SetFieldOfView(TopViewCameraFOV);
	}
#if WITH_EDITOR
	TopViewCameraActor->SetActorLabel(TEXT("BoidTopCamera"));
#endif
	return TopViewCameraActor;
}

ACameraActor* ABoidCameraManager::GetOrCreateFishThirdPersonCameraActor()
{
	if (IsValid(FishThirdPersonCameraActor))
	{
		return FishThirdPersonCameraActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Candidate = *It;
		if (IsValid(Candidate) && Candidate->ActorHasTag(FishThirdPersonCameraTag))
		{
			FishThirdPersonCameraActor = Candidate;
			if (FishThirdPersonCameraActor->GetCameraComponent())
			{
				FishThirdPersonCameraActor->GetCameraComponent()->SetFieldOfView(FishThirdPersonCameraFOV);
			}
			return FishThirdPersonCameraActor;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FishThirdPersonCameraActor = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams);

	if (!IsValid(FishThirdPersonCameraActor))
	{
		return nullptr;
	}

	FishThirdPersonCameraActor->Tags.AddUnique(FishThirdPersonCameraTag);
	if (FishThirdPersonCameraActor->GetCameraComponent())
	{
		FishThirdPersonCameraActor->GetCameraComponent()->SetFieldOfView(FishThirdPersonCameraFOV);
	}
#if WITH_EDITOR
	FishThirdPersonCameraActor->SetActorLabel(TEXT("BoidFishCamera"));
#endif
	return FishThirdPersonCameraActor;
}

void ABoidCameraManager::RefreshTopCameraTransform()
{
	ACameraActor* Camera = GetOrCreateTopViewCameraActor();
	if (!IsValid(Camera))
	{
		return;
	}

	const FVector FocusCenter = ResolveFocusCenter();
	const FVector CameraLocation = FocusCenter + TopViewCameraDefaultOffset;
	const FRotator CameraRotation = (FocusCenter - CameraLocation).Rotation();
	Camera->SetActorLocationAndRotation(CameraLocation, CameraRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ABoidCameraManager::RefreshFishThirdPersonCameraTransform(AActor* FishTarget, float DeltaTime)
{
	ACameraActor* Camera = GetOrCreateFishThirdPersonCameraActor();
	if (!IsValid(Camera) || !IsValid(FishTarget))
	{
		return;
	}

	// 相机放在鱼体“运动方向后上方”，并朝向鱼前方一点，形成稳定第三人称尾随视角。
	const FVector FishLocation = FishTarget->GetActorLocation();
	FVector FishForward = FishTarget->GetActorUpVector().GetSafeNormal();
	if (const ABoidAgent* FishAgent = Cast<ABoidAgent>(FishTarget))
	{
		const FVector FishVelocity = FishAgent->GetBoidVelocity();
		if (!FishVelocity.IsNearlyZero())
		{
			FishForward = FishVelocity.GetSafeNormal();
		}
	}
	const FVector FishRight = FishTarget->GetActorRightVector().GetSafeNormal();
	const FVector FishUp = FVector::UpVector;

	const FVector DesiredCameraLocation =
		FishLocation +
		FishForward * FishThirdPersonOffset.X +
		FishRight * FishThirdPersonOffset.Y +
		FishUp * FishThirdPersonOffset.Z;
	const FVector LookAtTarget = FishLocation + FishForward * FishThirdPersonLookAhead;
	const FRotator DesiredCameraRotation = (LookAtTarget - DesiredCameraLocation).Rotation();

	if (FishThirdPersonInterpSpeed > 0.0f && DeltaTime > 0.0f)
	{
		const FVector SmoothedLocation = FMath::VInterpTo(Camera->GetActorLocation(), DesiredCameraLocation, DeltaTime, FishThirdPersonInterpSpeed);
		const FRotator SmoothedRotation = FMath::RInterpTo(Camera->GetActorRotation(), DesiredCameraRotation, DeltaTime, FishThirdPersonInterpSpeed);
		Camera->SetActorLocationAndRotation(SmoothedLocation, SmoothedRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		Camera->SetActorLocationAndRotation(DesiredCameraLocation, DesiredCameraRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (Camera->GetCameraComponent())
	{
		Camera->GetCameraComponent()->SetFieldOfView(FishThirdPersonCameraFOV);
	}
}
