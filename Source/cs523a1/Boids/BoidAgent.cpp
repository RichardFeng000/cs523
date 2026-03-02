// Copyright Epic Games, Inc. All Rights Reserved.

#include "Boids/BoidAgent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

ABoidAgent::ABoidAgent()
{
	PrimaryActorTick.bCanEverTick = false;

	// 构建可见网格并作为根节点。
	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	SetRootComponent(VisualMeshComponent);

	// 鱼体不参与碰撞与导航，只做视觉展示。
	VisualMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	VisualMeshComponent->SetGenerateOverlapEvents(false);
	VisualMeshComponent->SetCanEverAffectNavigation(false);
	VisualMeshComponent->SetCastShadow(false);
	VisualMeshComponent->SetHiddenInGame(false);
	VisualMeshComponent->SetVisibility(true, true);
	VisualMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);

	// 使用引擎自带圆锥作为鱼体网格。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		VisualMeshComponent->SetStaticMesh(ConeMesh.Object);
	}

	DefaultMaterial = VisualMeshComponent->GetMaterial(0);

	// 首次应用可视参数。
	ApplyVisualSettings();
}

void ABoidAgent::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 保障在编辑器中改参数后能即时生效。
	ApplyVisualSettings();
}

void ABoidAgent::ApplyVisualSettings()
{
	if (!VisualMeshComponent)
	{
		return;
	}

	const FVector SafeScale = FVector(
		FMath::Max(VisualScale.X, 0.01f),
		FMath::Max(VisualScale.Y, 0.01f),
		FMath::Max(VisualScale.Z, 0.01f));
	const float HighlightScale = bCamera1Highlighted ? Camera1HighlightScaleMultiplier : 1.0f;
	// 防止缩放为 0 导致不可见或异常。
	VisualMeshComponent->SetRelativeScale3D(SafeScale * HighlightScale);
}

void ABoidAgent::SetVelocity(const FVector& NewVelocity)
{
	Velocity = NewVelocity;
}

void ABoidAgent::SetVisualScale(const FVector& NewScale)
{
	VisualScale = NewScale;
	// 每次外部改缩放后立即刷新网格。
	ApplyVisualSettings();
}

void ABoidAgent::SetSimulationState(
	const FVector& NewLocation,
	const FVector& NewVelocity,
	const FVector& DesiredFacingVelocity,
	float DeltaTime,
	float RotationInterpSpeed,
	float FacingDeadZoneDegrees,
	float FacingTurnSpeedDegPerSec,
	float FacingMinSpeed)
{
	Velocity = NewVelocity;
	// 位置与旋转都用瞬移方式更新，避免物理插值干扰。
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector RawFacingVelocity = DesiredFacingVelocity.IsNearlyZero() ? Velocity : DesiredFacingVelocity;
	const float FacingSpeed = RawFacingVelocity.Size();
	if (FacingSpeed >= FMath::Max(FacingMinSpeed, 0.0f) && !RawFacingVelocity.IsNearlyZero())
	{
		const FVector TargetFacingDirection = RawFacingVelocity.GetSafeNormal();
		if (SmoothedFacingDirection.IsNearlyZero())
		{
			SmoothedFacingDirection = TargetFacingDirection;
		}
		else
		{
			const float DotValue = FMath::Clamp(FVector::DotProduct(SmoothedFacingDirection, TargetFacingDirection), -1.0f, 1.0f);
			const float DeltaAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotValue));
			const FVector EffectiveTargetDirection = (DeltaAngleDegrees <= FMath::Max(FacingDeadZoneDegrees, 0.0f))
				? SmoothedFacingDirection
				: TargetFacingDirection;

			if (FacingTurnSpeedDegPerSec > 0.0f && DeltaTime > 0.0f)
			{
				SmoothedFacingDirection = FMath::VInterpNormalRotationTo(
					SmoothedFacingDirection,
					EffectiveTargetDirection,
					DeltaTime,
					FacingTurnSpeedDegPerSec);
			}
			else
			{
				SmoothedFacingDirection = EffectiveTargetDirection;
			}
		}

		// /Engine/BasicShapes/Cone 的尖端在本地 +Z 轴。
		const FVector VisualTipAxis = FVector(0.0f, 0.0f, 1.0f);
		const FQuat TargetQuat = FQuat::FindBetweenNormals(VisualTipAxis, SmoothedFacingDirection.GetSafeNormal());
		// 可选平滑旋转，避免方向瞬间跳变。
		const FQuat NewQuat = (RotationInterpSpeed > 0.0f && DeltaTime > 0.0f)
			? FMath::QInterpTo(GetActorQuat(), TargetQuat, DeltaTime, RotationInterpSpeed)
			: TargetQuat;
		SetActorRotation(NewQuat, ETeleportType::TeleportPhysics);
	}
}

void ABoidAgent::SetCamera1Highlighted(bool bHighlighted)
{
	if (!VisualMeshComponent || bCamera1Highlighted == bHighlighted)
	{
		return;
	}

	bCamera1Highlighted = bHighlighted;

	if (!bCamera1Highlighted)
	{
		// 退出“相机1控制鱼”时恢复默认材质。
		if (DefaultMaterial)
		{
			VisualMeshComponent->SetMaterial(0, DefaultMaterial);
		}
		ApplyVisualSettings();
		return;
	}

	if (!DefaultMaterial)
	{
		DefaultMaterial = VisualMeshComponent->GetMaterial(0);
	}

	if (!HighlightMaterialInstance)
	{
		// 高亮优先使用可调色的 BasicShapeMaterial，避免“父材质没有颜色参数导致看不出红色”。
		UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!ParentMaterial)
		{
			ParentMaterial = DefaultMaterial ? DefaultMaterial.Get() : VisualMeshComponent->GetMaterial(0);
		}
		if (ParentMaterial)
		{
			HighlightMaterialInstance = UMaterialInstanceDynamic::Create(ParentMaterial, this);
		}
	}

	if (!HighlightMaterialInstance)
	{
		return;
	}

	// 兼容常见参数名：不同材质父类可能使用不同颜色参数名。
	const FLinearColor HighlightColor(0.95f, 0.10f, 0.10f, 1.0f);
	HighlightMaterialInstance->SetVectorParameterValue(TEXT("Color"), HighlightColor);
	HighlightMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), HighlightColor);
	HighlightMaterialInstance->SetVectorParameterValue(TEXT("Tint"), HighlightColor);
	HighlightMaterialInstance->SetVectorParameterValue(TEXT("BodyColor"), HighlightColor);
	HighlightMaterialInstance->SetVectorParameterValue(TEXT("DiffuseColor"), HighlightColor);
	HighlightMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), HighlightColor * 2.0f);
	VisualMeshComponent->SetMaterial(0, HighlightMaterialInstance);
	const FVector HighlightColorVec(HighlightColor.R, HighlightColor.G, HighlightColor.B);
	VisualMeshComponent->SetVectorParameterValueOnMaterials(TEXT("Color"), HighlightColorVec);
	VisualMeshComponent->SetVectorParameterValueOnMaterials(TEXT("BaseColor"), HighlightColorVec);
	ApplyVisualSettings();
}
