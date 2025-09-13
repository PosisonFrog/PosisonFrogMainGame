// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "00_Character/02_Component/CEnhancedInputComponent.h"
#include "00_Character/02_Component/CGameplayTags.h"
#include "00_Character/02_Component/CHealthComponent.h"
#include "PosisonFrog/00_Character/00_Player/01_Widget/CPlayerWidget.h"

#include "Global.h"

ACPlayerController::ACPlayerController()
{
}


void ACPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (auto* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			if (bClearPreviousMappings)
				Subsys->ClearAllMappings();

			if (DefaultMappingContext)
			{
				Subsys->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
	// 위젯 생성 로직을 OnPossess로 이동
	if (ShouldCreatePlayerWidget())
	{
		CreatePlayerWidget();
	}

}

void ACPlayerController::OnPossess(APawn* InPawn)
{
	
	Super::OnPossess(InPawn);

	OwnerCharacter = Cast<ACPlayerCharacter>(InPawn);
	if (!OwnerCharacter)
	{
		CLog::Log(TEXT("플레이어 컨트롤러가 캐릭터를 소유하지 못했습니다."));
		return;
	}

	CLog::Log(TEXT("플레이어 컨트롤러가 캐릭터를 성공적으로 소유했습니다."));
	
	HealthComponent = OwnerCharacter->GetComponentByClass<UCHealthComponent>();
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &ACPlayerController::HandleHealthChanged);
		// 여기서 UI 업데이트 (안전한 타이밍)
		if (PlayerWidget)
		{
			UpdateHpUI();
		}
	}
}


void ACPlayerController::OnUnPossess()
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &ACPlayerController::HandleHealthChanged);
		HealthComponent = nullptr;
	}
	OwnerCharacter = nullptr;
	Super::OnUnPossess();
}




void ACPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// 강제로 UCEnhancedInputComponent 사용
	CEnhancedInputComponent = Cast<UCEnhancedInputComponent>(InputComponent);
	if (!CEnhancedInputComponent)
	{
		// 캐스팅 실패시 새로 생성
		CEnhancedInputComponent = NewObject<UCEnhancedInputComponent>(this);
		InputComponent = CEnhancedInputComponent;
	}
    
	if (!InputConfig)
	{
		CLog::Log(TEXT("InputConfig이 설정되지 않았습니다."));
		return;
	}

    SetupInputBindings();
}


void ACPlayerController::SetupInputBindings()
{

	check(InputConfig);
	check(CEnhancedInputComponent);

	// 기본 이동 및 시야 입력 - PlayerController에서 처리
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Move, ETriggerEvent::Triggered,this, &ACPlayerController::HandleMove);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACPlayerController::HandleLook);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Dash, ETriggerEvent::Started, this, &ACPlayerController::HandleDashStart);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Attack, ETriggerEvent::Started, this, &ACPlayerController::HandleAttack);
}

bool ACPlayerController::ShouldCreatePlayerWidget() const
{
	// MainMenu나 다른 UI 전용 레벨에서는 PlayerWidget을 생성하지 않음
	if (!PlayerWidgetClass)
	{
		return false;
	}
	
	// 현재 레벨이 게임플레이 레벨인지 확인
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	
	FString LevelName = World->GetMapName();

	// MainMenu 레벨에서는 PlayerWidget 생성하지 않음
	if (LevelName.Contains(TEXT("MainMenuLevel")) || 
		LevelName.Contains(TEXT("Setting")))
	{
		return false;
	}
	return true;
}

void ACPlayerController::CreatePlayerWidget()
{
	if (PlayerWidgetClass && !PlayerWidget)
	{
		PlayerWidget = CreateWidget<UCPlayerWidget>(this, PlayerWidgetClass);
		if (PlayerWidget)
		{
			PlayerWidget->AddToViewport();
		}
	}
}


void ACPlayerController::HandleMove(const FInputActionValue& Value)
{
	if (OwnerCharacter)
	{
		OwnerCharacter->Move(Value); 
	}
}

void ACPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (OwnerCharacter)
	{
		OwnerCharacter->Look(Value); 
	}
}

void ACPlayerController::HandleDashStart()
{
	if (OwnerCharacter)
	{
		OwnerCharacter->DashStart(); 
	}
}

void ACPlayerController::HandleAttack()
{
	if (OwnerCharacter)
	{
		OwnerCharacter->Attack();
	}
}

void ACPlayerController::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	if (PlayerWidget)
	{
		PlayerWidget->UpdateHpBar(CurrentHealth, MaxHealth);
	}
}

void ACPlayerController::UpdateHpUI() const
{
	if (PlayerWidget)
	{
		PlayerWidget->UpdateHpBar(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
	}
}



