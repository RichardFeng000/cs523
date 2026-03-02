// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boids/BoidAutoSpawnSubsystem.h"

#include "Boids/BoidAgent.h"
#include "Boids/BoidCameraManager.h"
#include "Boids/BoidManager.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/BoidStartMenuWidget.h"
#include "cs523a1PlayerController.h"
#include "cs523a1.h"

void UBoidAutoSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	StopFallbackFishCameraOrbit();

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(&InWorld, 0))
	{
		PlayerController->GetPlayerViewPoint(SpawnLocation, SpawnRotation);
		SpawnLocation += SpawnRotation.Vector() * 900.0f + FVector(0.0f, 0.0f, 80.0f);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 只保留一个 BoidManager，避免出现“两套鱼群同时运行”。
	TArray<ABoidManager*> BoidManagers;
	for (TActorIterator<ABoidManager> It(&InWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			BoidManagers.Add(*It);
		}
	}
	UE_LOG(Logcs523a1, Display, TEXT("[Boids] BoidManager count before dedupe: %d"), BoidManagers.Num());
	if (BoidManagers.Num() == 0)
	{
		InWorld.SpawnActor<ABoidManager>(ABoidManager::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	}
	else if (BoidManagers.Num() > 1)
	{
		for (int32 Index = 1; Index < BoidManagers.Num(); ++Index)
		{
			if (IsValid(BoidManagers[Index]))
			{
				BoidManagers[Index]->Destroy();
			}
		}
		UE_LOG(Logcs523a1, Warning, TEXT("[Boids] Found %d BoidManagers, kept 1 and destroyed duplicates."), BoidManagers.Num());
	}

	// 同理，顶部相机管理器也只保留一个。
	TArray<ABoidCameraManager*> CameraManagers;
	for (TActorIterator<ABoidCameraManager> It(&InWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			CameraManagers.Add(*It);
		}
	}
	UE_LOG(Logcs523a1, Display, TEXT("[Boids] BoidCameraManager count before dedupe: %d"), CameraManagers.Num());
	if (CameraManagers.Num() == 0)
	{
		InWorld.SpawnActor<ABoidCameraManager>(ABoidCameraManager::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	}
	else if (CameraManagers.Num() > 1)
	{
		for (int32 Index = 1; Index < CameraManagers.Num(); ++Index)
		{
			if (IsValid(CameraManagers[Index]))
			{
				CameraManagers[Index]->Destroy();
			}
		}
		UE_LOG(Logcs523a1, Warning, TEXT("[Boids] Found %d BoidCameraManagers, kept 1 and destroyed duplicates."), CameraManagers.Num());
	}

	// 全局开始菜单：不依赖具体 GameMode/PlayerController 蓝图配置。
	if (!bStartMenuFinished)
	{
		UE_LOG(Logcs523a1, Display, TEXT("[StartMenu] Schedule retry timer"));
		InWorld.GetTimerManager().SetTimer(
			StartMenuRetryTimerHandle,
			this,
			&UBoidAutoSpawnSubsystem::TryShowStartMenu,
			0.25f,
			true);
		TryShowStartMenu();
	}
}

void UBoidAutoSpawnSubsystem::TryShowStartMenu()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bStartMenuFinished)
	{
		World->GetTimerManager().ClearTimer(StartMenuRetryTimerHandle);
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	if (!IsValid(StartMenuWidgetInstance))
	{
		StartMenuWidgetInstance = CreateWidget<UUserWidget>(PC, UBoidStartMenuWidget::StaticClass());
		if (!IsValid(StartMenuWidgetInstance))
		{
			return;
		}

		if (UBoidStartMenuWidget* StartWidget = Cast<UBoidStartMenuWidget>(StartMenuWidgetInstance))
		{
			StartWidget->OnStartClicked.RemoveDynamic(this, &UBoidAutoSpawnSubsystem::HandleStartClicked);
			StartWidget->OnStartClicked.AddDynamic(this, &UBoidAutoSpawnSubsystem::HandleStartClicked);
		}

		StartMenuWidgetInstance->AddToViewport(2000);
		UE_LOG(Logcs523a1, Display, TEXT("[StartMenu] Shown successfully (first add)"));
	}
	else if (!StartMenuWidgetInstance->IsInViewport())
	{
		// 某些情况下 UI 可能被外部逻辑移除，这里做一次自动补挂载。
		StartMenuWidgetInstance->AddToViewport(2000);
		UE_LOG(Logcs523a1, Warning, TEXT("[StartMenu] Widget was not in viewport, re-added"));
	}

	StartMenuWidgetInstance->SetVisibility(ESlateVisibility::Visible);
	StartMenuWidgetInstance->SetIsEnabled(true);

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(0xC523A102),
				1.0f,
				FColor::Yellow,
				TEXT("Start menu is ready: click Start Game."));
		}
	}

void UBoidAutoSpawnSubsystem::HandleStartClicked()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!IsValid(PC))
	{
		return;
	}

	bStartMenuFinished = true;
	World->GetTimerManager().ClearTimer(StartMenuRetryTimerHandle);

	if (IsValid(StartMenuWidgetInstance))
	{
		StartMenuWidgetInstance->RemoveFromParent();
		StartMenuWidgetInstance = nullptr;
	}

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;

	UE_LOG(Logcs523a1, Display, TEXT("[StartMenu] Clicked start, enter Camera1"));

	// 优先调用自定义控制器的相机1逻辑。
	if (Acs523a1PlayerController* BoidPC = Cast<Acs523a1PlayerController>(PC))
	{
		StopFallbackFishCameraOrbit();
		BoidPC->EnterCamera1FishThirdPerson();
		return;
	}

	// 通用兜底：直接切到鱼第三人称相机。
	ABoidCameraManager* CameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(World, ABoidCameraManager::StaticClass()));
	ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(World, ABoidManager::StaticClass()));
	if (!IsValid(CameraManager))
	{
		return;
	}

	AActor* FishTarget = CameraManager->GetPrimaryFishViewTarget();
	ACameraActor* FishCamera = CameraManager->GetOrCreateFishThirdPersonCameraActor();
	if (!IsValid(FishTarget) || !IsValid(FishCamera))
	{
		return;
	}

	if (IsValid(BoidManager))
	{
		BoidManager->SetExternallyControlledBoid(Cast<ABoidAgent>(FishTarget));
	}

	CameraManager->RefreshFishThirdPersonCameraTransform(FishTarget, 0.0f);
	PC->SetViewTargetWithBlend(FishCamera, 0.25f);
	if (APawn* ControlledPawn = PC->GetPawn())
	{
		ControlledPawn->SetActorHiddenInGame(true);
	}
	StartFallbackFishCameraOrbit(PC, Cast<ABoidAgent>(FishTarget), FishCamera);
}

void UBoidAutoSpawnSubsystem::StartFallbackFishCameraOrbit(APlayerController* PC, ABoidAgent* FishTarget, ACameraActor* FishCamera)
{
	if (!IsValid(PC) || !IsValid(FishTarget) || !IsValid(FishCamera))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FallbackOrbitPlayerController = PC;
	FallbackOrbitFishTarget = FishTarget;
	FallbackOrbitCameraActor = FishCamera;

	// 标准第三人称输入状态：隐藏鼠标并进入纯游戏输入。
	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;
	PC->SetIgnoreLookInput(false);
	PC->SetIgnoreMoveInput(false);

	// 将朝向先对齐到鱼当前前进方向，避免镜头初始方向突变。
	FVector FishForward = FishTarget->GetActorUpVector().GetSafeNormal();
	if (!FishTarget->GetBoidVelocity().IsNearlyZero())
	{
		FishForward = FishTarget->GetBoidVelocity().GetSafeNormal();
	}
	FRotator StartControlRot = FishForward.Rotation();
	StartControlRot.Pitch = FMath::ClampAngle(StartControlRot.Pitch, -65.0f, 25.0f);
	StartControlRot.Roll = 0.0f;
	PC->SetControlRotation(StartControlRot);

	World->GetTimerManager().SetTimer(
		FallbackCameraTickTimerHandle,
		this,
		&UBoidAutoSpawnSubsystem::TickFallbackFishCameraOrbit,
		0.0f,
		true);
}

void UBoidAutoSpawnSubsystem::StopFallbackFishCameraOrbit()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(FallbackCameraTickTimerHandle);
	}
	FallbackOrbitPlayerController = nullptr;
	FallbackOrbitFishTarget = nullptr;
	FallbackOrbitCameraActor = nullptr;
}

void UBoidAutoSpawnSubsystem::TickFallbackFishCameraOrbit()
{
	if (!IsValid(FallbackOrbitPlayerController) || !IsValid(FallbackOrbitFishTarget) || !IsValid(FallbackOrbitCameraActor))
	{
		StopFallbackFishCameraOrbit();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		StopFallbackFishCameraOrbit();
		return;
	}

	// 鼠标输入驱动控制器旋转，镜头按控制器旋转做第三人称环绕。
	FRotator ControlRot = FallbackOrbitPlayerController->GetControlRotation();
	ControlRot.Pitch = FMath::ClampAngle(ControlRot.Pitch, -65.0f, 25.0f);
	ControlRot.Roll = 0.0f;
	FallbackOrbitPlayerController->SetControlRotation(ControlRot);

	const FVector FishLocation = FallbackOrbitFishTarget->GetActorLocation();
	FVector FishForward = FallbackOrbitFishTarget->GetActorUpVector().GetSafeNormal();
	if (!FallbackOrbitFishTarget->GetBoidVelocity().IsNearlyZero())
	{
		FishForward = FallbackOrbitFishTarget->GetBoidVelocity().GetSafeNormal();
	}

	const FVector OrbitForward = ControlRot.Vector().GetSafeNormal();
	const FVector DesiredCameraLocation =
		FishLocation
		- OrbitForward * 720.0f
		+ FVector(0.0f, 0.0f, 220.0f);
	const FVector LookAtTarget = FishLocation + FishForward * 220.0f + FVector(0.0f, 0.0f, 70.0f);
	const FRotator DesiredCameraRotation = (LookAtTarget - DesiredCameraLocation).Rotation();

	const float DeltaSeconds = World->GetDeltaSeconds();
	const float InterpSpeed = 10.0f;
	const FVector SmoothedLocation = FMath::VInterpTo(
		FallbackOrbitCameraActor->GetActorLocation(),
		DesiredCameraLocation,
		DeltaSeconds,
		InterpSpeed);
	const FRotator SmoothedRotation = FMath::RInterpTo(
		FallbackOrbitCameraActor->GetActorRotation(),
		DesiredCameraRotation,
		DeltaSeconds,
		InterpSpeed);
	FallbackOrbitCameraActor->SetActorLocationAndRotation(
		SmoothedLocation,
		SmoothedRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (FallbackOrbitPlayerController->GetViewTarget() != FallbackOrbitCameraActor.Get())
	{
		FallbackOrbitPlayerController->SetViewTargetWithBlend(FallbackOrbitCameraActor.Get(), 0.1f);
	}
}
