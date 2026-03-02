// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BoidStartMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/SlateTypes.h"

TSharedRef<SWidget> UBoidStartMenuWidget::RebuildWidget()
{
	if (WidgetTree)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Root;

		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		// 深海幕布：更冷、更深的蓝黑底。
		Backdrop->SetBrushColor(FLinearColor(0.01f, 0.03f, 0.08f, 0.82f));
		if (UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop))
		{
			BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BackdropSlot->SetOffsets(FMargin(0.0f));
		}

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
		CardSize->SetWidthOverride(760.0f);
		CardSize->SetHeightOverride(460.0f);

		UBorder* CardShadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CardShadow"));
		CardShadow->SetBrushColor(FLinearColor(0.05f, 0.22f, 0.34f, 0.42f));
		CardShadow->SetPadding(FMargin(2.0f));
		CardSize->AddChild(CardShadow);

		UBorder* CardBody = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CardBody"));
		// 玻璃卡片：半透明深蓝。
		CardBody->SetBrushColor(FLinearColor(0.03f, 0.11f, 0.19f, 0.78f));
		CardBody->SetPadding(FMargin(36.0f, 30.0f));
		CardShadow->AddChild(CardBody);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
		CardBody->AddChild(Content);

		UTextBlock* Badge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Badge"));
		Badge->SetText(FText::FromString(TEXT("CS523A1 / DEEP SEA MODE")));
		Badge->SetColorAndOpacity(FSlateColor(FLinearColor(0.52f, 0.90f, 1.0f, 1.0f)));
		{
			FSlateFontInfo Font = Badge->GetFont();
			Font.Size = 15;
			Badge->SetFont(Font);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Badge))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartTitle"));
		Title->SetText(FText::FromString(TEXT("Deep Sea Boid Simulation")));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.98f, 1.0f, 1.0f)));
		{
			FSlateFontInfo Font = Title->GetFont();
			Font.Size = 46;
			Title->SetFont(Font);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Title))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}

		UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartSubtitle"));
		Subtitle->SetText(FText::FromString(TEXT("Drift, split, and regroup. Control your primary fish after entering.")));
		Subtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.90f, 1.0f, 1.0f)));
		{
			FSlateFontInfo Font = Subtitle->GetFont();
			Font.Size = 19;
			Subtitle->SetFont(Font);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Subtitle))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		}

		USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DividerSize"));
		DividerSize->SetHeightOverride(2.0f);
		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Divider"));
		Divider->SetBrushColor(FLinearColor(0.32f, 0.85f, 1.0f, 0.82f));
		DividerSize->AddChild(Divider);
			if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(DividerSize))
			{
				Slot->SetSize(ESlateSizeRule::Automatic);
				Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
			}

		UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
		Hint->SetText(FText::FromString(TEXT("Controls\n- Mouse: orbit camera\n- W/A/S/D: move fish\n- Space/Ctrl: ascend/descend\n- Press 2: top camera")));
		Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.93f, 1.0f, 1.0f)));
		{
			FSlateFontInfo Font = Hint->GetFont();
			Font.Size = 18;
			Hint->SetFont(Font);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Hint))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
		}

		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("ActionSpacer"));
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Spacer))
		{
			Slot->SetSize(ESlateSizeRule::Fill);
		}

		StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
		FButtonStyle ButtonStyle = StartButton->GetStyle();
		// 玻璃按钮：青蓝高光。
		ButtonStyle.Normal.TintColor = FSlateColor(FLinearColor(0.10f, 0.60f, 0.84f, 0.96f));
		ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.16f, 0.73f, 0.96f, 1.0f));
		ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.45f, 0.64f, 1.0f));
		StartButton->SetStyle(ButtonStyle);

		UTextBlock* StartLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartLabel"));
		StartLabel->SetText(FText::FromString(TEXT("Start Dive")));
		StartLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		StartLabel->SetJustification(ETextJustify::Center);
		{
			FSlateFontInfo Font = StartLabel->GetFont();
			Font.Size = 30;
			StartLabel->SetFont(Font);
		}
			StartButton->AddChild(StartLabel);

			USizeBox* StartButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StartButtonSize"));
			StartButtonSize->SetWidthOverride(360.0f);
			StartButtonSize->SetHeightOverride(64.0f);
			StartButtonSize->AddChild(StartButton);

			if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(StartButtonSize))
			{
				Slot->SetHorizontalAlignment(HAlign_Center);
				Slot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
			}

		UTextBlock* StartHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartHint"));
		StartHint->SetText(FText::FromString(TEXT("Press Enter / Space or click the button")));
		StartHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.90f, 1.0f, 0.95f)));
		StartHint->SetJustification(ETextJustify::Center);
		{
			FSlateFontInfo Font = StartHint->GetFont();
			Font.Size = 15;
			StartHint->SetFont(Font);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(StartHint))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		if (UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(CardSize))
		{
			CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CardSlot->SetPosition(FVector2D::ZeroVector);
			CardSlot->SetAutoSize(true);
		}
	}

	return Super::RebuildWidget();
}

void UBoidStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	StartButton = Cast<UButton>(WidgetTree ? WidgetTree->FindWidget(TEXT("StartButton")) : nullptr);
	if (!StartButton)
	{
		return;
	}

	StartButton->OnClicked.RemoveDynamic(this, &UBoidStartMenuWidget::HandleStartClicked);
	StartButton->OnClicked.AddDynamic(this, &UBoidStartMenuWidget::HandleStartClicked);
	SetKeyboardFocus();
}

FReply UBoidStartMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey() || Key == EKeys::SpaceBar)
	{
		HandleStartClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBoidStartMenuWidget::HandleStartClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xC523A103), 1.5f, FColor::Green, TEXT("Starting game: entering Camera 1"));
	}
	OnStartClicked.Broadcast();
}
