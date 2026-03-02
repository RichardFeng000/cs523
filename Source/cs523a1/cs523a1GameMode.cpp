// Copyright Epic Games, Inc. All Rights Reserved.

#include "cs523a1GameMode.h"
#include "cs523a1PlayerController.h"

Acs523a1GameMode::Acs523a1GameMode()
{
	// 统一使用项目自定义 PlayerController，确保相机1/2和开始菜单输入逻辑可用。
	PlayerControllerClass = Acs523a1PlayerController::StaticClass();
}
