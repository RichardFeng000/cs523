// Copyright Epic Games, Inc. All Rights Reserved.


#include "cs523a1PlayerController.h"
#include "Boids/BoidAgent.h"
#include "Boids/BoidCameraManager.h"
#include "Boids/BoidManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "UI/BoidStartMenuWidget.h"
#include "cs523a1CameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "cs523a1.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	// 左上角状态提示（复用固定 Key，避免刷屏叠很多行）。
	void ShowCameraStateHint(const FString& Message)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xC523A101), 1.6f, FColor::Cyan, Message);
		}
	}
}

Acs523a1PlayerController::Acs523a1PlayerController()
{
	// 使用项目自定义 CameraManager。
	PlayerCameraManagerClass = Acs523a1CameraManager::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void Acs523a1PlayerController::EnterCamera1FishThirdPerson()
{
	SwitchToDefaultCamera();
}

void Acs523a1PlayerController::BeginPlay()
{
	Super::BeginPlay();

	DefaultViewTargetActor = GetViewTarget();
	if (!DefaultViewTargetActor && IsValid(GetPawn()))
	{
		DefaultViewTargetActor = GetPawn();
	}
	
	// 仅本地控制器创建触控 UI。
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// 创建移动端触控界面。
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// 添加到本地玩家屏幕。
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logcs523a1, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	// 开场显示“开始”界面，点击后再进入相机1。
	if (IsLocalPlayerController() && bShowStartMenuOnBeginPlay)
	{
		ShowStartMenu();
	}
}

void Acs523a1PlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFishThirdPersonView(DeltaSeconds);
	UpdateCamera2LeaderMouseControl(DeltaSeconds);
}

void Acs523a1PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!bUsingBoidTopCamera && IsValid(InPawn))
	{
		// 非顶部视角状态下，实时更新默认视角目标。
		DefaultViewTargetActor = InPawn;
	}

	if (IsValid(InPawn))
	{
		// 鱼第三人称或顶部相机状态下，都隐藏机器人本体，避免同屏看到“原角色”。
		InPawn->SetActorHiddenInGame(bUsingFishThirdPersonCamera || bUsingBoidTopCamera);
	}
}

void Acs523a1PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 仅本地控制器加载输入映射。
	if (IsLocalPlayerController())
	{
		// 添加输入映射上下文。
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// 若非触控模式，再加载 PC 专用映射。
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	if (InputComponent)
	{
		// 1/2 键切换相机，主键盘与小键盘都支持。
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &Acs523a1PlayerController::SwitchToDefaultCamera);
		InputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &Acs523a1PlayerController::SwitchToDefaultCamera);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &Acs523a1PlayerController::SwitchToBoidTopCamera);
		InputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &Acs523a1PlayerController::SwitchToBoidTopCamera);
		InputComponent->BindKey(EKeys::J, IE_Pressed, this, &Acs523a1PlayerController::ToggleCamera2LeaderMouseControl);
	}
	
}

void Acs523a1PlayerController::ShowStartMenu()
{
	if (IsValid(StartMenuWidgetInstance))
	{
		return;
	}

	TSubclassOf<UUserWidget> WidgetClass = StartMenuWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UBoidStartMenuWidget::StaticClass();
	}

	StartMenuWidgetInstance = CreateWidget<UUserWidget>(this, WidgetClass);
	if (!IsValid(StartMenuWidgetInstance))
	{
		return;
	}

	if (UBoidStartMenuWidget* StartWidget = Cast<UBoidStartMenuWidget>(StartMenuWidgetInstance))
	{
		StartWidget->OnStartClicked.RemoveDynamic(this, &Acs523a1PlayerController::HandleStartMenuStartClicked);
		StartWidget->OnStartClicked.AddDynamic(this, &Acs523a1PlayerController::HandleStartMenuStartClicked);
	}

	StartMenuWidgetInstance->AddToViewport(1000);

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void Acs523a1PlayerController::HandleStartMenuStartClicked()
{
	if (IsValid(StartMenuWidgetInstance))
	{
		StartMenuWidgetInstance->RemoveFromParent();
		StartMenuWidgetInstance = nullptr;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;

	// 点击开始后直接进入相机1（鱼第三人称）。
	SwitchToDefaultCamera();
}

void Acs523a1PlayerController::SwitchToDefaultCamera()
{
	SetCamera2LeaderMouseControl(false);

	// “相机1”改为鱼第三人称：优先走 BoidCameraManager。
	ABoidCameraManager* CameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(this, ABoidCameraManager::StaticClass()));
	ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass()));
	if (IsValid(CameraManager))
	{
		PrimaryFishViewTarget = Cast<ABoidAgent>(CameraManager->GetPrimaryFishViewTarget());
		FishThirdPersonCameraActor = CameraManager->GetOrCreateFishThirdPersonCameraActor();
	}

	if (IsValid(BoidManager))
	{
		BoidManager->SetExternallyControlledBoid(PrimaryFishViewTarget.Get());
		// Camera1: keep controlled fish, but do NOT let it drive the whole swarm.
		BoidManager->SetExternalLeaderInfluenceEnabled(false);
	}

	if (IsValid(CameraManager) && IsValid(PrimaryFishViewTarget) && IsValid(FishThirdPersonCameraActor))
	{
		CameraManager->RefreshFishThirdPersonCameraTransform(PrimaryFishViewTarget, 0.0f);
			ShowCameraStateHint(TEXT("Switching to Camera 1 (Fish Third-Person View)"));
		bUsingBoidTopCamera = false;
		bUsingFishThirdPersonCamera = true;
		if (bCamera1UseMouseOrbit)
		{
			// 进入相机1时，把控制器朝向先对齐到鱼前进方向，避免一进来镜头反向。
			FVector FishForward = PrimaryFishViewTarget->GetActorUpVector().GetSafeNormal();
			if (!PrimaryFishViewTarget->GetVelocity().IsNearlyZero())
			{
				FishForward = PrimaryFishViewTarget->GetVelocity().GetSafeNormal();
			}
			FRotator StartControlRot = FishForward.Rotation();
			StartControlRot.Pitch = FMath::ClampAngle(StartControlRot.Pitch, Camera1MinPitch, Camera1MaxPitch);
			StartControlRot.Roll = 0.0f;
			SetControlRotation(StartControlRot);
		}
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->SetActorHiddenInGame(true);
		}

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		SetViewTargetWithBlend(FishThirdPersonCameraActor, CameraSwitchBlendTime);
		return;
	}

	// 没有鱼时兜底回玩家 Pawn。
	AActor* RestoreTarget = IsValid(GetPawn()) ? Cast<AActor>(GetPawn()) : DefaultViewTargetActor.Get();
	if (IsValid(RestoreTarget))
	{
		ShowCameraStateHint(TEXT("Switching to Camera 1 (Default Character View / Fallback)"));
		bUsingBoidTopCamera = false;
		bUsingFishThirdPersonCamera = false;
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->SetActorHiddenInGame(false);
		}
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		SetViewTargetWithBlend(RestoreTarget, CameraSwitchBlendTime);
	}
}

void Acs523a1PlayerController::SwitchToBoidTopCamera()
{
	if (ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass())))
	{
		// 切到顶部相机时，释放外部控制鱼，恢复群体自治。
		BoidManager->SetExternallyControlledBoid(nullptr);
		BoidManager->SetExternalLeaderInfluenceEnabled(false);
		BoidManager->SetForceBaitballAtWorldOrigin(false);
	}
	bUsingFishThirdPersonCamera = false;
	if (APawn* ControlledPawn = GetPawn())
	{
		// 相机2也隐藏机器人，仅保留鱼群视角内容。
		ControlledPawn->SetActorHiddenInGame(true);
	}

	EnsureBoidTopCamera();
	if (!IsValid(BoidTopCameraActor))
	{
		ShowCameraStateHint(TEXT("Camera 2 switch failed: Top camera not found"));
		return;
	}

	if (!bUsingBoidTopCamera)
	{
		// 首次切到顶部相机时缓存默认目标，便于按 1 返回。
		DefaultViewTargetActor = IsValid(GetPawn())
			? Cast<AActor>(GetPawn())
			: GetViewTarget();
	}

	ShowCameraStateHint(TEXT("Switching to Camera 2 (Boid Top View)"));
	bUsingBoidTopCamera = true;
	SetViewTargetWithBlend(BoidTopCameraActor, CameraSwitchBlendTime);
}

void Acs523a1PlayerController::ToggleCamera2LeaderMouseControl()
{
	if (bUsingFishThirdPersonCamera)
	{
		if (!bEnableCamera1BaitballHotkey)
		{
			ShowCameraStateHint(TEXT("Camera 1 J baitball hotkey is disabled"));
			return;
		}

		if (ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass())))
		{
			const bool bEnableBaitball = !BoidManager->IsForceBaitballAtWorldOrigin();
			BoidManager->SetForceBaitballAtWorldOrigin(bEnableBaitball);
			ShowCameraStateHint(
				bEnableBaitball
					? TEXT("Camera 1 baitball at world origin: ON")
					: TEXT("Camera 1 baitball at world origin: OFF"));
		}
		return;
	}

	if (!bUsingBoidTopCamera)
	{
		ShowCameraStateHint(TEXT("J works in Camera 1/2 only"));
		return;
	}
	if (!bEnableCamera2LeaderHotkey)
	{
		ShowCameraStateHint(TEXT("Camera 2 J leader hotkey is disabled"));
		return;
	}

	SetCamera2LeaderMouseControl(!bCamera2LeaderMouseControlEnabled);
	ShowCameraStateHint(
		bCamera2LeaderMouseControlEnabled
			? TEXT("Camera 2 leader mouse control: ON")
			: TEXT("Camera 2 leader mouse control: OFF"));
}

void Acs523a1PlayerController::SetCamera2LeaderMouseControl(bool bEnable)
{
	bCamera2LeaderMouseControlEnabled = bEnable;

	ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass()));
	if (!IsValid(BoidManager))
	{
		Camera2LeaderFish = nullptr;
		return;
	}

	if (!bEnable)
	{
		BoidManager->SetExternallyControlledBoid(nullptr);
		BoidManager->SetExternalLeaderInfluenceEnabled(false);
		Camera2LeaderFish = nullptr;

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		return;
	}

	BoidManager->SetForceBaitballAtWorldOrigin(false);

	if (!IsValid(Camera2LeaderFish))
	{
		if (Camera2PreferredLeaderBoidIndex >= 0 && BoidManager->GetBoidNum() > 0)
		{
			const int32 SafeIndex = Camera2PreferredLeaderBoidIndex % BoidManager->GetBoidNum();
			Camera2LeaderFish = BoidManager->GetBoidByIndex(SafeIndex);
		}

		if (ABoidCameraManager* CameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(this, ABoidCameraManager::StaticClass())))
		{
			if (!IsValid(Camera2LeaderFish))
			{
				Camera2LeaderFish = Cast<ABoidAgent>(CameraManager->GetPrimaryFishViewTarget());
			}
		}
		if (!IsValid(Camera2LeaderFish))
		{
			Camera2LeaderFish = BoidManager->GetBoidByIndex(0);
		}
	}

	BoidManager->SetExternallyControlledBoid(Camera2LeaderFish.Get());
	// Camera2 J mode: externally controlled fish should lead the swarm.
	BoidManager->SetExternalLeaderInfluenceEnabled(true);

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void Acs523a1PlayerController::UpdateCamera2LeaderMouseControl(float DeltaSeconds)
{
	if (!bUsingBoidTopCamera || !bCamera2LeaderMouseControlEnabled || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass()));
	if (!IsValid(BoidManager) || !IsValid(Camera2LeaderFish))
	{
		return;
	}

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return;
	}

	const FVector CurrentLocation = Camera2LeaderFish->GetActorLocation();
	const FVector RayEnd = WorldOrigin + WorldDirection * FMath::Max(Camera2MouseRayLength, 1000.0f);
	const FPlane MovePlane(CurrentLocation, FVector::UpVector);
	const FVector MouseWorldOnPlane = FMath::LinePlaneIntersection(WorldOrigin, RayEnd, MovePlane);

	FVector DesiredLocation = MouseWorldOnPlane;
	if (bCamera2LeaderClampToSimulationBounds)
	{
		const FVector BoundsCenter = BoidManager->GetSimulationCenter();
		const FVector BoundsExtent = BoidManager->GetSimulationBoundsExtent();
		DesiredLocation.X = FMath::Clamp(DesiredLocation.X, BoundsCenter.X - BoundsExtent.X, BoundsCenter.X + BoundsExtent.X);
		DesiredLocation.Y = FMath::Clamp(DesiredLocation.Y, BoundsCenter.Y - BoundsExtent.Y, BoundsCenter.Y + BoundsExtent.Y);
		DesiredLocation.Z = FMath::Clamp(DesiredLocation.Z, BoundsCenter.Z - BoundsExtent.Z, BoundsCenter.Z + BoundsExtent.Z);
	}

	const FVector ToTarget = DesiredLocation - CurrentLocation;
	const float ToTargetDistance = ToTarget.Size();
	if (ToTargetDistance <= FMath::Max(Camera2LeaderArrivalRadius, 0.0f))
	{
		Camera2LeaderFish->SetVelocity(FVector::ZeroVector);
		return;
	}

	const FVector MoveDirection = ToTarget / ToTargetDistance;
	const float MoveStep = FMath::Min(ToTargetDistance, FMath::Max(Camera2LeaderMaxSpeed, 1.0f) * DeltaSeconds);
	const FVector RawNextLocation = CurrentLocation + MoveDirection * MoveStep;
	const FVector SmoothedLocation = (Camera2LeaderMoveInterpSpeed > 0.0f)
		? FMath::VInterpTo(
			CurrentLocation,
			RawNextLocation,
			DeltaSeconds,
			Camera2LeaderMoveInterpSpeed)
		: RawNextLocation;

	Camera2LeaderFish->SetActorLocation(SmoothedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Camera2LeaderFish->SetVelocity((SmoothedLocation - CurrentLocation) / DeltaSeconds);
	Camera2LeaderFish->SetActorRotation(
		FQuat::FindBetweenNormals(FVector(0.0f, 0.0f, 1.0f), MoveDirection),
		ETeleportType::TeleportPhysics);

	BoidManager->SetExternallyControlledBoid(Camera2LeaderFish.Get());
	BoidManager->SetExternalLeaderInfluenceEnabled(true);
}

void Acs523a1PlayerController::UpdateFishThirdPersonView(float DeltaSeconds)
{
	// 仅在“非顶部相机”状态维护鱼第三人称，避免覆盖按 2 视角。
	if (bUsingBoidTopCamera)
	{
		return;
	}
	if (!bUsingFishThirdPersonCamera)
	{
		return;
	}

	ABoidCameraManager* CameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(this, ABoidCameraManager::StaticClass()));
	ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass()));
	if (!IsValid(CameraManager) || !IsValid(BoidManager))
	{
		return;
	}

	if (!IsValid(PrimaryFishViewTarget))
	{
		PrimaryFishViewTarget = Cast<ABoidAgent>(CameraManager->GetPrimaryFishViewTarget());
	}
	if (!IsValid(FishThirdPersonCameraActor))
	{
		FishThirdPersonCameraActor = CameraManager->GetOrCreateFishThirdPersonCameraActor();
	}
	if (!IsValid(PrimaryFishViewTarget) || !IsValid(FishThirdPersonCameraActor))
	{
		return;
	}

	// 用玩家 Pawn 的移动去驱动该鱼，这样 WASD 仍然有效。
	APawn* ControlledPawn = GetPawn();
		if (IsValid(ControlledPawn))
		{
			ControlledPawn->SetActorHiddenInGame(true);

			// Camera1 下不再“绑定地面 Pawn 位置”，改为按当前视角直接驱动鱼前进。
			// 这样抬头前进会升高，低头前进会下降。
			const float MoveSpeed = FMath::Max(ControlledPawn->GetVelocity().Size(), 420.0f);
			const FRotator ViewRot = GetControlRotation();
			const FVector ViewForward = ViewRot.Vector().GetSafeNormal();
			const FVector ViewRight = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y).GetSafeNormal();

			FVector DesiredDir = FVector::ZeroVector;
			if (IsInputKeyDown(EKeys::W))
			{
				DesiredDir += ViewForward;
			}
			if (IsInputKeyDown(EKeys::S))
			{
				DesiredDir -= ViewForward;
			}
			if (IsInputKeyDown(EKeys::D))
			{
				DesiredDir += ViewRight;
			}
			if (IsInputKeyDown(EKeys::A))
			{
				DesiredDir -= ViewRight;
			}
			if (IsInputKeyDown(EKeys::SpaceBar))
			{
				DesiredDir += FVector::UpVector;
			}
			if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl) || IsInputKeyDown(EKeys::C))
			{
				DesiredDir += FVector::DownVector;
			}

			FVector FishVelocity = FVector::ZeroVector;
			if (!DesiredDir.IsNearlyZero())
			{
				const FVector FishMoveDir = DesiredDir.GetSafeNormal();
				const FVector DesiredFishLocation =
					PrimaryFishViewTarget->GetActorLocation() + FishMoveDir * MoveSpeed * DeltaSeconds;
				PrimaryFishViewTarget->SetActorLocation(DesiredFishLocation, false, nullptr, ETeleportType::TeleportPhysics);
				const FQuat FishTargetQuat = FQuat::FindBetweenNormals(FVector(0.0f, 0.0f, 1.0f), FishMoveDir);
				PrimaryFishViewTarget->SetActorRotation(FishTargetQuat, ETeleportType::TeleportPhysics);
				FishVelocity = FishMoveDir * MoveSpeed;
			}
			PrimaryFishViewTarget->SetVelocity(FishVelocity);
		}

	BoidManager->SetExternallyControlledBoid(PrimaryFishViewTarget.Get());
	BoidManager->SetExternalLeaderInfluenceEnabled(false);
	if (bCamera1UseMouseOrbit)
	{
		// 鼠标输入会驱动 ControlRotation，这里按它计算“鱼后方环绕”第三人称位姿。
		FRotator ControlRot = GetControlRotation();
		ControlRot.Pitch = FMath::ClampAngle(ControlRot.Pitch, Camera1MinPitch, Camera1MaxPitch);
		ControlRot.Roll = 0.0f;
		SetControlRotation(ControlRot);

		FVector FishForward = PrimaryFishViewTarget->GetActorUpVector().GetSafeNormal();
		if (!PrimaryFishViewTarget->GetVelocity().IsNearlyZero())
		{
			FishForward = PrimaryFishViewTarget->GetVelocity().GetSafeNormal();
		}

		const FVector OrbitForward = ControlRot.Vector().GetSafeNormal();
		const FVector FishLocation = PrimaryFishViewTarget->GetActorLocation();
		const FVector DesiredCameraLocation =
			FishLocation
			- OrbitForward * Camera1OrbitDistance
			+ FVector(0.0f, 0.0f, Camera1OrbitHeight);
		const FVector LookAtTarget = FishLocation + FishForward * 220.0f + FVector(0.0f, 0.0f, 70.0f);
		const FRotator DesiredCameraRotation = (LookAtTarget - DesiredCameraLocation).Rotation();

		if (Camera1OrbitInterpSpeed > 0.0f && DeltaSeconds > 0.0f)
		{
			const FVector SmoothedLocation = FMath::VInterpTo(
				FishThirdPersonCameraActor->GetActorLocation(),
				DesiredCameraLocation,
				DeltaSeconds,
				Camera1OrbitInterpSpeed);
			const FRotator SmoothedRotation = FMath::RInterpTo(
				FishThirdPersonCameraActor->GetActorRotation(),
				DesiredCameraRotation,
				DeltaSeconds,
				Camera1OrbitInterpSpeed);
			FishThirdPersonCameraActor->SetActorLocationAndRotation(
				SmoothedLocation,
				SmoothedRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		else
		{
			FishThirdPersonCameraActor->SetActorLocationAndRotation(
				DesiredCameraLocation,
				DesiredCameraRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
	}
	else
	{
		CameraManager->RefreshFishThirdPersonCameraTransform(PrimaryFishViewTarget, DeltaSeconds);
	}

	// 若当前还没切到鱼相机，则自动切过去。
	if (GetViewTarget() != FishThirdPersonCameraActor)
	{
		SetViewTargetWithBlend(FishThirdPersonCameraActor, CameraSwitchBlendTime);
	}
}

void Acs523a1PlayerController::EnsureBoidTopCamera()
{
	if (!GetWorld())
	{
		return;
	}

	// 优先走独立的 BoidCameraManager（相机与鱼群逻辑分离）。
	if (ABoidCameraManager* CameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(this, ABoidCameraManager::StaticClass())))
	{
		if (ACameraActor* ManagerCamera = CameraManager->GetOrCreateTopViewCameraActor())
		{
			BoidTopCameraActor = ManagerCamera;
			if (BoidTopCameraActor->GetCameraComponent())
			{
				BoidTopCameraActor->GetCameraComponent()->SetFieldOfView(BoidTopCameraFOV);
			}
			return;
		}
	}

	if (bPreferPlacedBoidTopCamera)
	{
		// 兜底：从场景里查找手动放置的 CameraActor。
		TArray<AActor*> FoundCameras;
		UGameplayStatics::GetAllActorsOfClass(this, ACameraActor::StaticClass(), FoundCameras);
		ACameraActor* FirstPlacedCamera = nullptr;
		for (AActor* Candidate : FoundCameras)
		{
			if (!IsValid(Candidate))
			{
				continue;
			}

			ACameraActor* CameraCandidate = Cast<ACameraActor>(Candidate);
			if (!FirstPlacedCamera)
			{
				FirstPlacedCamera = CameraCandidate;
			}

			if (Candidate->ActorHasTag(PlacedBoidTopCameraTag))
			{
				BoidTopCameraActor = CameraCandidate;
				break;
			}
		}

		// 未设置 Tag 时，退化为使用找到的第一个相机。
		if (!IsValid(BoidTopCameraActor))
		{
			BoidTopCameraActor = FirstPlacedCamera;
		}
	}

	const bool bUsingPlacedCamera = IsValid(BoidTopCameraActor) && BoidTopCameraActor->ActorHasTag(PlacedBoidTopCameraTag);
	const bool bUsingAnyPlacedCamera = IsValid(BoidTopCameraActor);
	if (bUsingPlacedCamera || bUsingAnyPlacedCamera)
	{
		// 使用场景相机时只校正 FOV，不覆盖其 Transform（便于手动调位姿）。
		if (BoidTopCameraActor->GetCameraComponent())
		{
			BoidTopCameraActor->GetCameraComponent()->SetFieldOfView(BoidTopCameraFOV);
		}
		return;
	}

	FVector FocusCenter = FVector::ZeroVector;

	if (ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass())))
	{
		FocusCenter = BoidManager->GetSimulationCenter();
	}

	const float CameraHeight = 2500.0f;
	const FVector CameraLocation = FocusCenter + FVector(0.0f, 0.0f, CameraHeight);
	const FRotator CameraRotation = (FocusCenter - CameraLocation).Rotation();

	// 最终兜底：运行时生成一个顶部相机。
	if (!IsValid(BoidTopCameraActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		BoidTopCameraActor = GetWorld()->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(),
			CameraLocation,
			CameraRotation,
			SpawnParams);
		if (IsValid(BoidTopCameraActor))
		{
			BoidTopCameraActor->Tags.AddUnique(PlacedBoidTopCameraTag);
		}
	}
	else
	{
		BoidTopCameraActor->SetActorLocationAndRotation(
			CameraLocation,
			CameraRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	if (IsValid(BoidTopCameraActor) && BoidTopCameraActor->GetCameraComponent())
	{
		BoidTopCameraActor->GetCameraComponent()->SetFieldOfView(BoidTopCameraFOV);
	}
}

bool Acs523a1PlayerController::ShouldUseTouchControls() const
{
	// 触控判断：平台触控或强制触控。
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
