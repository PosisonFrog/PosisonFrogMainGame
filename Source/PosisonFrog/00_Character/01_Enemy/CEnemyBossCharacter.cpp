#include "00_Character/01_Enemy/CEnemyBossCharacter.h"

#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/00_BossPattern/CBossPattern_BasicAttack.h"
#include "00_Character/02_Component/01_EnemyComponent/00_BossPattern/CBossPattern_Barrage.h"
#include "00_Character/02_Component/01_EnemyComponent/00_BossPattern/CBossPattern_Rush.h"
#include "00_Character/02_Component/01_EnemyComponent/00_BossPattern/CBossPattern_Slam.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "05_System/01_Sound/CSoundManagerSubsystem.h"
#include "05_System/01_Sound//CSoundDataAsset.h"
#include "00_Character/CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "02_MainMenu/01_Widget/CCutsceneWidget.h"
#include "02_MainMenu/01_Widget/CMainMenuWidget.h"
#include "03_Combat/Damage/DamageType_FuryCountable.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"

ACEnemyBossCharacter::ACEnemyBossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	
	HealthComponent = CreateDefaultSubobject<UCEnemyHealthComponent>(TEXT("HealthComponent"));
	BossPhaseComponent = CreateDefaultSubobject<UCEnemyBossPhaseComponent>(TEXT("BossPhaseComponent"));
	WeaponComponent = CreateDefaultSubobject<UCEnemyWeaponComponent>(TEXT("WeaponComponent"));
	PatternManager = CreateDefaultSubobject<UCBossPatternManager>(TEXT("PatternManager"));
	
	// 패턴 컴포넌트들 생성 (ActorComponent로 변경됨)
	BasicAttackPattern = CreateDefaultSubobject<UCBossPattern_BasicAttack>(TEXT("BasicAttackPattern"));
	BarragePattern = CreateDefaultSubobject<UCBossPattern_Barrage>(TEXT("BarragePattern"));
	RushPattern = CreateDefaultSubobject<UCBossPattern_Rush>(TEXT("RushPattern"));
	SlamPattern = CreateDefaultSubobject<UCBossPattern_Slam>(TEXT("SlamPattern"));
	
	// Tag 설정으로 패턴 찾기 용이하도록
	if (BasicAttackPattern)
	{
		BasicAttackPattern->ComponentTags.Add(FName("BasicAttack"));
	}
	if (BarragePattern)
	{
		BarragePattern->ComponentTags.Add(FName("Barrage"));
	}
	if (RushPattern)
	{
		RushPattern->ComponentTags.Add(FName("Rush"));
	}
	if (SlamPattern)
	{
		SlamPattern->ComponentTags.Add(FName("Slam"));
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		MeshComp->SetCollisionResponseToAllChannels(ECR_Overlap);
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);
		

	}

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		const float DesiredSeparation = CapsuleComp->GetScaledCapsuleRadius() * 2.f + 5.f;
        
		CapsuleComp->SetGenerateOverlapEvents(true);
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		CapsuleComp->SetCollisionResponseToAllChannels(ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
		CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);

	}
}

void ACEnemyBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Error, TEXT("========================================"));
	UE_LOG(LogTemp, Error, TEXT("[Boss] BeginPlay - bAutoStartBattle = %s"), bAutoStartBattle ? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogTemp, Error, TEXT("========================================"));

	Tags.AddUnique(FName("Boss"));
	
	if (bAutoStartBattle)
	{
		APawn* Player = GetWorld()->GetFirstPlayerController()
					  ? GetWorld()->GetFirstPlayerController()->GetPawn() 
					  : nullptr;
		if (Player)
		{
			UE_LOG(LogTemp, Error, TEXT("[Boss] AUTO START BATTLE TRIGGERED IN BEGINPLAY!"));
			StartBossBattle(false);
		}
	}
	
	bIsBossDead = false;

	if (USkeletalMeshComponent* MeshComp = this->GetMesh())
	{
		MeshComp->SetVisibility(false);
	}

	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDeath.AddDynamic(this, &ACEnemyBossCharacter::HandleBossDeath);
	}

	SaveInitialTransform();
}

void ACEnemyBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Warning, TEXT("[Boss] EndPlay called"));
	
	// 패턴 매니저 클린업
	if (IsValid(PatternManager))
	{
		PatternManager->CleanupAllPatterns();
	}
	
	// 모든 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDeath.RemoveDynamic(this, &ACEnemyBossCharacter::HandleBossDeath);
	}
	
	Super::EndPlay(EndPlayReason);
}

float ACEnemyBossCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] TakeDamage 호출됨! 데미지: %.1f, 공격자: %s"), 
		   *GetName(), DamageAmount, *GetNameSafe(DamageCauser));
	
	if (bIsBossDead)
	{
		UE_LOG(LogTemp, Log, TEXT("[Boss] Already dead, ignoring damage"));
		return 0.0f;
	}
	
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (AppliedDamage <= 0.f)
	{
		return AppliedDamage;
	}

	const bool bCountsForFury = DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UDamageType_FuryCountable::StaticClass());

	if (bCountsForFury && EventInstigator)
	{
		if (APawn* InstPawn = EventInstigator->GetPawn())
		{
			if (UCFuryGaugeComponent* Fury = InstPawn->FindComponentByClass<UCFuryGaugeComponent>())
				Fury->AddStack(1);
		}
	}
	
	if (HealthComponent)
	{
		float OldHealth = HealthComponent->GetHealth();
		HealthComponent->Damage(AppliedDamage);
		float NewHealth = HealthComponent->GetHealth();
		
		UE_LOG(LogTemp, Warning, TEXT("[%s] 체력 변화: %.1f -> %.1f"), 
			   *GetName(), OldHealth, NewHealth);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] HealthComponent를 찾을 수 없음!"), *GetName());
	}
	
	const bool bIsDead = IsValid(HealthComponent) && HealthComponent->IsDead();
	if (bIsDead)
	{
		bIsBossDead = true;
		return AppliedDamage;
	}
	
	if (BossPhaseComponent)
	{
		if (BossPhaseComponent->GetCurrentState() == EBossBattleState::Dead)
		{
			return AppliedDamage;
		}

		if (!BossPhaseComponent->IsBattleStarted())
		{
			UE_LOG(LogTemp, Error, TEXT("[Boss] AUTO START BATTLE TRIGGERED BY TAKEDAMAGE!"));
			BossPhaseComponent->StartBattle();
		}

		if (const UBossPhaseDataAsset* PhaseData = BossPhaseComponent->PhaseData)
		{
			BossPhaseComponent->AddPower(PhaseData->PowerGainOnHit);
		}
	}

	return AppliedDamage;
}

void ACEnemyBossCharacter::StartBossBattle(bool bSkipIntro)
{
	UE_LOG(LogTemp, Error, TEXT("========================================"));
	UE_LOG(LogTemp, Error, TEXT("[Boss] StartBossBattle CALLED! SkipIntro=%s"), bSkipIntro ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Error, TEXT("[Boss] Call Location: %s"), ANSI_TO_TCHAR(__FUNCTION__));
	UE_LOG(LogTemp, Error, TEXT("========================================"));
	
	if (bIsBossDead)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Boss] StartBossBattle ignored because boss is dead"));
		return;
	}
	
	if (BossPhaseComponent)
	{
		BossPhaseComponent->StartBattle(bSkipIntro);
	}
	
}

void ACEnemyBossCharacter::ForcePattern(FName PatternId)
{
	if (bIsBossDead)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Boss] ForcePattern(%s) ignored because boss is dead"), *PatternId.ToString());
		return;
	}
	if (BossPhaseComponent)
	{
		BossPhaseComponent->ForceNextPattern(PatternId);
	}
}

void ACEnemyBossCharacter::ResetBossBattleState()
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] ResetBossBattleState"));
	
	bIsBossDead = false;

	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("[Boss] Position Reset to Initial Location"));
	
	if (PatternManager)
	{
		PatternManager->CleanupAllPatterns();
	}
	
	if (BossPhaseComponent)
	{
		BossPhaseComponent->ResetBattleState();
	}
	
	if (HealthComponent)
	{
		HealthComponent->ResetHealth();
	}
	
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
			
		if (ABossAIController* BossAI = Cast<ABossAIController>(AIController))
		{
			BossAI->SetChaseEnabled(false);
		}
	}
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->Velocity = FVector::ZeroVector;
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.0f);
		}
	}
	
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	SetCanBeDamaged(true);

	SetActorTransform(GetInitialTransform());
}

void ACEnemyBossCharacter::SaveInitialTransform()
{
	InitialTransform = GetActorTransform();
	UE_LOG(LogTemp, Log, TEXT("[Boss] Initial Transform Saved: %s"), *InitialTransform.GetLocation().ToString());
}

void ACEnemyBossCharacter::InitializeBossBindings()
{
	if (!BossPhaseComponent)
	{
		return;
	}

	BossPhaseComponent->OnPhaseChanged.AddDynamic(this, &ACEnemyBossCharacter::HandlePhaseChanged);
	BossPhaseComponent->OnPatternStarted.AddDynamic(this, &ACEnemyBossCharacter::HandlePatternStarted);
	BossPhaseComponent->OnPatternFinished.AddDynamic(this, &ACEnemyBossCharacter::HandlePatternFinished);
	BossPhaseComponent->OnShoutStarted.AddDynamic(this, &ACEnemyBossCharacter::HandleShoutStarted);
	BossPhaseComponent->OnShoutFinished.AddDynamic(this, &ACEnemyBossCharacter::HandleShoutFinished);
}

void ACEnemyBossCharacter::HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData)
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] Phase changed to %s (Index %d)"), *PhaseData.PhaseName.ToString(), PhaseIndex);
}

void ACEnemyBossCharacter::HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] Pattern %s started (Phase %d, Power %.1f)"), *PatternId.ToString(), PhaseIndex, RemainingPower);

	if (bIsBossDead || (IsValid(BossPhaseComponent) && BossPhaseComponent->GetCurrentState() == EBossBattleState::Dead))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Boss] Pattern %s ignored because boss is dead"), *PatternId.ToString());
		return;
	}
	
	if (IsValid(HealthComponent) && HealthComponent->IsDead())
	{
		bIsBossDead = true;
		UE_LOG(LogTemp, Verbose, TEXT("[Boss] Pattern %s ignored due to dead health state"), *PatternId.ToString());
		return;
	}
}

void ACEnemyBossCharacter::HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] Pattern %s finished (Phase %d, Power %.1f)"), *PatternId.ToString(), PhaseIndex, RemainingPower);
}

void ACEnemyBossCharacter::HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration)
{
	UE_LOG(LogTemp, Warning, TEXT("[Boss] Shout %s started (Phase %d, Duration %.2fs)"), *ShoutId.ToString(), PhaseIndex, Duration);
}

void ACEnemyBossCharacter::HandleShoutFinished(int32 PhaseIndex, FName ShoutId, float Duration)
{
	UE_LOG(LogTemp, Warning, TEXT("[Boss] Shout %s finished (Phase %d)"), *ShoutId.ToString(), PhaseIndex);
}

void ACEnemyBossCharacter::HandleBossDeath(AActor* DeadActor)
{
	if (DeadActor != this)
	{
		return;
	}
  
	UE_LOG(LogTemp, Error, TEXT("[Boss] ========== BOSS DEATH =========="));
	
	bIsBossDead = true;

	if (IsValid(PatternManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Boss] Cleaning up pattern manager"));
		PatternManager->CleanupAllPatterns();
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		
		if (ABossAIController* BossAI = Cast<ABossAIController>(AIController))
		{
			BossAI->SetChaseEnabled(false);
			BossAI->SetTargetPlayer(nullptr);
		}
	}
	
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	
	SetActorTickEnabled(false);

	if (USkeletalMeshComponent* mesh = GetMesh())
	{
		if (UAnimInstance* AnimInst = mesh->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.f); 
			
			if (DeathMontage)
			{
				AnimInst->Montage_Play(DeathMontage, 1.0f);
				UE_LOG(LogTemp, Warning, TEXT("[Boss] Playing death montage: %s"), *DeathMontage->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Boss] No death montage assigned!"));
			}
		}
	}


	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(PC->GetPawn()))
		{
			PlayerChar->SetHUDVisibility(false);
			UE_LOG(LogTemp, Log, TEXT("[Boss] Player HUD Hidden"));
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this); // 기존 타이머 정리
        
		World->GetTimerManager().SetTimer(
			TimerHandle_CutsceneStart,
			this,
			&ACEnemyBossCharacter::StartDeathCutscene,
			3.0f, // 0.5초 딜레이
			false
		);
	}
}

void ACEnemyBossCharacter::OnImageCutsceneFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] Cutscene Finished -> Moving to MainMenu"));
    
	// 위젯 제거
	if (ImageCutsceneWidget)
	{
		ImageCutsceneWidget->RemoveFromParent();
		ImageCutsceneWidget = nullptr;
	}
    
	// [수정] MainMenu 레벨로 이동
	// "MainMenu"는 프로젝트의 실제 메인 메뉴 레벨 이름과 일치해야 합니다.
	UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}


void ACEnemyBossCharacter::StartDeathCutscene()
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] Starting Death Cutscene..."));

	if (ImageCutsceneWidgetClass)
	{
		// 플레이어 컨트롤러 가져오기 (위젯 소유자용)
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        
		ImageCutsceneWidget = CreateWidget<UCCutsceneWidget>(PC, ImageCutsceneWidgetClass);
		if (ImageCutsceneWidget)
		{
			ImageCutsceneWidget->AddToViewport(200); // Z-Order 높게
			ImageCutsceneWidget->InitializeAndStart();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Boss] Failed to create Cutscene Widget"));
			OnImageCutsceneFinished(); // 위젯 생성 실패 시 바로 이동
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Boss] ImageCutsceneWidgetClass is null"));
		OnImageCutsceneFinished(); // 설정 안 되어 있으면 바로 이동
	}
}

