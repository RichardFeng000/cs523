// Copyright Epic Games, Inc. All Rights Reserved.

#include "cs523a1Character.h"
#include "Boids/BoidCameraManager.h"
#include "Boids/BoidManager.h"
#include "UI/BoidStartMenuWidget.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "cs523a1PlayerController.h"
#include "cs523a1.h"

namespace
{
	// Character 兜底路径下也显示同样的左上角提示。
	void ShowCameraStateHint(const FString& Message)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xC523A102), 1.6f, FColor::Cyan, Message);
		}
	}
}

Acs523a1Character::Acs523a1Character()
{
	// 初始化碰撞体尺寸。
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// 创建第一人称网格（仅本地玩家可见）。
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// 创建第一人称相机组件。
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// 配置角色网格显示规则。
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// 配置移动参数。
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void Acs523a1Character::BeginPlay()
{
	Super::BeginPlay();

	// 兜底：如果当前不是自定义 PlayerController，也要显示开始菜单。
	if (bShowStartMenuFallback)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!Cast<Acs523a1PlayerController>(PC))
		{
			ShowStartMenuFallback();
		}
	}
}

void Acs523a1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 默认视角目标是当前角色本体。
	DefaultViewTargetActor = this;

	// 绑定 Enhanced Input 动作。
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 跳跃输入。
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &Acs523a1Character::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &Acs523a1Character::DoJumpEnd);

		// 移动输入。
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &Acs523a1Character::MoveInput);

		// 视角输入。
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Acs523a1Character::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &Acs523a1Character::LookInput);
	}
	else
	{
		UE_LOG(Logcs523a1, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	if (PlayerInputComponent)
	{
		// 若未使用自定义 PlayerController，才启用角色内兜底的 1/2 键绑定，避免按键冲突。
		if (!Cast<Acs523a1PlayerController>(GetController()))
		{
			PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &Acs523a1Character::SwitchToDefaultCameraView);
			PlayerInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &Acs523a1Character::SwitchToDefaultCameraView);
			PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &Acs523a1Character::SwitchToBoidTopCameraView);
			PlayerInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &Acs523a1Character::SwitchToBoidTopCameraView);

			// 兜底菜单二次触发点：此时通常已经有有效控制器，不会错过开场菜单。
			if (bShowStartMenuFallback && !IsValid(StartMenuWidgetInstanceFallback))
			{
				ShowStartMenuFallback();
			}
		}
	}
}

void Acs523a1Character::ShowStartMenuFallback()
{
	if (IsValid(StartMenuWidgetInstanceFallback))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!IsValid(PC))
	{
		return;
	}

	TSubclassOf<UUserWidget> WidgetClass = StartMenuWidgetClassFallback;
	if (!WidgetClass)
	{
		WidgetClass = UBoidStartMenuWidget::StaticClass();
	}

	StartMenuWidgetInstanceFallback = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!IsValid(StartMenuWidgetInstanceFallback))
	{
		return;
	}

	if (UBoidStartMenuWidget* StartWidget = Cast<UBoidStartMenuWidget>(StartMenuWidgetInstanceFallback))
	{
		StartWidget->OnStartClicked.RemoveDynamic(this, &Acs523a1Character::HandleStartMenuFallbackClicked);
		StartWidget->OnStartClicked.AddDynamic(this, &Acs523a1Character::HandleStartMenuFallbackClicked);
	}

	StartMenuWidgetInstanceFallback->AddToViewport(1000);

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}

void Acs523a1Character::HandleStartMenuFallbackClicked()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (IsValid(StartMenuWidgetInstanceFallback))
	{
		StartMenuWidgetInstanceFallback->RemoveFromParent();
		StartMenuWidgetInstanceFallback = nullptr;
	}

	if (IsValid(PC))
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}

	// 优先触发自定义控制器的“相机1=鱼第三人称”；否则走角色兜底相机1。
	if (Acs523a1PlayerController* BoidPC = Cast<Acs523a1PlayerController>(PC))
	{
		BoidPC->EnterCamera1FishThirdPerson();
	}
	else
	{
		SwitchToDefaultCameraView();
	}
}


void Acs523a1Character::MoveInput(const FInputActionValue& Value)
{
	// 读取二维移动输入。
	FVector2D MovementVector = Value.Get<FVector2D>();

	// 转发给统一移动处理。
	DoMove(MovementVector.X, MovementVector.Y);

}

void Acs523a1Character::LookInput(const FInputActionValue& Value)
{
	// 读取二维视角输入。
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// 转发给统一转向处理。
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void Acs523a1Character::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// 传递转向输入到控制器。
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void Acs523a1Character::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// 传递移动输入到角色移动组件。
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void Acs523a1Character::DoJumpStart()
{
	// 开始跳跃。
	Jump();
}

void Acs523a1Character::DoJumpEnd()
{
	// 结束跳跃。
	StopJumping();
}

void Acs523a1Character::SwitchToDefaultCameraView()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!IsValid(PlayerController))
	{
		return;
	}

	AActor* RestoreTarget = IsValid(this) ? Cast<AActor>(this) : DefaultViewTargetActor.Get();
	if (!IsValid(RestoreTarget))
	{
		return;
	}

	// 平滑切回默认角色视角。
	ShowCameraStateHint(TEXT("Switching to Camera 1 (Default Character View / Character Fallback)"));
	PlayerController->SetViewTargetWithBlend(RestoreTarget, BoidCameraBlendTime);
}

void Acs523a1Character::SwitchToBoidTopCameraView()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!IsValid(PlayerController))
	{
		return;
	}

	EnsureBoidTopCameraForView();
	if (!IsValid(BoidTopCameraActor))
	{
		ShowCameraStateHint(TEXT("Camera 2 switch failed: Top camera not found"));
		return;
	}

	// 平滑切到顶部相机。
	ShowCameraStateHint(TEXT("Switching to Camera 2 (Boid Top View / Character Fallback)"));
	PlayerController->SetViewTargetWithBlend(BoidTopCameraActor, BoidCameraBlendTime);
}

void Acs523a1Character::EnsureBoidTopCameraForView()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 优先复用独立 BoidCameraManager 维护的顶部相机。
	if (ABoidCameraManager* CameraManager = Cast<ABoidCameraManager>(UGameplayStatics::GetActorOfClass(this, ABoidCameraManager::StaticClass())))
	{
		if (ACameraActor* ManagerCamera = CameraManager->GetOrCreateTopViewCameraActor())
		{
			BoidTopCameraActor = ManagerCamera;
			if (BoidTopCameraActor->GetCameraComponent())
			{
				BoidTopCameraActor->GetCameraComponent()->SetFieldOfView(BoidCameraFOV);
			}
			return;
		}
	}

	if (bUsePlacedCameraActorForBoids)
	{
		// 其次使用场景里已有 CameraActor。
		TArray<AActor*> FoundCameras;
		UGameplayStatics::GetAllActorsOfClass(this, ACameraActor::StaticClass(), FoundCameras);
		for (AActor* Candidate : FoundCameras)
		{
			if (IsValid(Candidate))
			{
				BoidTopCameraActor = Cast<ACameraActor>(Candidate);
				break;
			}
		}

		if (IsValid(BoidTopCameraActor))
		{
			// 使用场景相机时仅设置 FOV，不覆盖位置与角度。
			if (BoidTopCameraActor->GetCameraComponent())
			{
				BoidTopCameraActor->GetCameraComponent()->SetFieldOfView(BoidCameraFOV);
			}
			return;
		}
	}

	FVector FocusCenter = FVector::ZeroVector;
	if (ABoidManager* BoidManager = Cast<ABoidManager>(UGameplayStatics::GetActorOfClass(this, ABoidManager::StaticClass())))
	{
		FocusCenter = BoidManager->GetSimulationCenter();
	}

	// 最后兜底：自动生成固定高度顶部相机。
	const float CameraHeight = 2500.0f;
	const FVector CameraLocation = FocusCenter + FVector(0.0f, 0.0f, CameraHeight);
	const FRotator CameraRotation = (FocusCenter - CameraLocation).Rotation();

	if (!IsValid(BoidTopCameraActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		BoidTopCameraActor = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(),
			CameraLocation,
			CameraRotation,
			SpawnParams);
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
		BoidTopCameraActor->GetCameraComponent()->SetFieldOfView(BoidCameraFOV);
	}
}
