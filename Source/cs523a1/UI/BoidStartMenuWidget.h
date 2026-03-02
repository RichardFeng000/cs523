// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "BoidStartMenuWidget.generated.h"

class UButton;
class SWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBoidStartClicked);

/**
 * 纯 C++ 开场菜单：
 * - 居中显示“开始”按钮；
 * - 点击后广播 OnStartClicked，交给 PlayerController 处理相机切换。
 */
UCLASS()
class CS523A1_API UBoidStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Boid|UI")
	FOnBoidStartClicked OnStartClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleStartClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;
};
