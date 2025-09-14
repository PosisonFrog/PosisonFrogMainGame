

#include "CPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

// 프로젝트 컴포넌트/유틸
#include "00_Character/02_Component/CDashComponent.h"
#include "00_Character/02_Component/CWeaponComponent.h"
#include "00_Character/02_Component/CHealthComponent.h"
#include "00_Character/02_Component/CEnhancedInputComponent.h"   // 커스텀 강화 입력
#include "00_Character/02_Component/CGameplayTags.h"             // InputTag_Move/Look/Dash/Attack
#include "01_Widget/CPlayerWidget.h"
#include "99_Util/CLog.h"

// ============================================================================
// 생성자
// ============================================================================
ACPlayerCharacter::ACPlayerCharacter()
{
	// 캡슐 크기
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// 필수 서브오브젝트 생성 (생성자에서 보장)
	DashComponent = CreateDefaultSubobject<UCDashComponent>(TEXT("DashComponent"));
	check(DashComponent);

	WeaponComponent = CreateDefaultSubobject<UCWeaponComponent>(TEXT("WeaponComponent"));
	check(WeaponComponent);

	HealthComponent = CreateDefaultSubobject<UCHealthComponent>(TEXT("HealthComponent"));
	check(HealthComponent);

	// 캐릭터 회전/이동 기본값
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

		// 가속 및 제동
		Move->MinAnalogWalkSpeed = 20.f;
		Move->BrakingDecelerationWalking = 2000.f;
		Move->BrakingDecelerationFalling = 1500.f;
	}

	// 카메라 붐
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 400.0f;
	SpringArm->bUsePawnControlRotation = true;

	// 추적 카메라
	PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	PlayerCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	PlayerCamera->bUsePawnControlRotation = false;
}

// ============================================================================
// BeginPlay
// ============================================================================
void ACPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkingSpeed;
	}

	// 생성자에서 보장되지만, 회귀 방지를 위한 런타임 체크
	ensureMsgf(DashComponent, TEXT("DashComponent is missing"));
	ensureMsgf(WeaponComponent, TEXT("WeaponComponent is missing"));
	ensureMsgf(HealthComponent, TEXT("HealthComponent is missing"));

	// === Health UI 연동 ===
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &ACPlayerCharacter::HandleHealthChanged);
	}

	if (PlayerWidgetClass)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC)
		{
			if (UWorld* World = GetWorld())
				PC = World->GetFirstPlayerController();
		}

		PlayerWidget = CreateWidget<UCPlayerWidget>(PC, PlayerWidgetClass);
		if (PlayerWidget)
		{
			PlayerWidget->AddToViewport();
			UpdateHpUI(); // 초기 수치 반영
		}
	}
}

// ============================================================================
// 입력 바인딩 (커스텀 UCEnhancedInputComponent + 태그 기반)
// ============================================================================
void ACPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UCEnhancedInputComponent* EIC = Cast<UCEnhancedInputComponent>(PlayerInputComponent);
	checkf(EIC, TEXT("UCEnhancedInputComponent가 필요합니다. 입력 세팅을 확인하세요."));
	checkf(InputConfig, TEXT("InputConfig(UCInputConfig)가 설정되지 않았습니다."));

	// Move / Look / Dash / Attack
	EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Move);
	EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Look);
	EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Dash, ETriggerEvent::Started, this, &ACPlayerCharacter::DashStart);
	EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Attack, ETriggerEvent::Started, this, &ACPlayerCharacter::Attack);
}

// ============================================================================
// 이동/시야
// ============================================================================
void ACPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) return;

	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void ACPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) return;

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

// ============================================================================
// 액션: 대시 / 공격
// ============================================================================
void ACPlayerCharacter::DashStart()
{
	if (IsValid(DashComponent))
	{
		CLog::Log(TEXT("대시 시작 - 컴포넌트 사용 가능"));
		DashComponent->StartDash(); // 내부에서 쿨다운/가속/물리감 처리
	}
	else
	{
		CLog::Log(TEXT("오류: DashComponent를 찾을 수 없습니다!"));
		// 필요 시 FindComponentByClass<UCDashComponent>()로 보정 가능
	}
}

void ACPlayerCharacter::Attack()
{
	if (IsValid(WeaponComponent))
	{
		CLog::Log(TEXT("공격 시작 - 컴포넌트 사용 가능"));
		WeaponComponent->DoAttack(); // 내부에서 콤보/애님 노티 윈도우 처리
	}
}

// ============================================================================
// HP UI 연동
// ============================================================================
void ACPlayerCharacter::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	if (PlayerWidget)
	{
		PlayerWidget->UpdateHpBar(CurrentHealth, MaxHealth);
	}
}

void ACPlayerCharacter::UpdateHpUI() const
{
	if (PlayerWidget && HealthComponent)
	{
		PlayerWidget->UpdateHpBar(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
	}
}
/* 중요 : UCEnhancedInputComponent, UCInputConfig, CGameplayTags, UCPlayerWidget, UCDashComponent, UCWeaponComponent, UCHealthComponent가 들어가야 프로젝트 적으로 안정성이 커집니다. 이 코드도 이게 존재한다고 가정하고 만들어진 코드입니다만 */

