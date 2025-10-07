#include "00_Character/02_Component/CWeaponComponent.h"

#include "00_Character/02_Component/CUltimateBuffComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "99_Util/CLog.h"
#include "Kismet/GameplayStatics.h"

UCWeaponComponent::UCWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 이벤트 드리븐
}

void UCWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerChar = Cast<ACharacter>(GetOwner());
    if (!IsValid(OwnerChar))
    {
        CLog::Log(TEXT("[WeaponComp] OwnerCharacter invalid"));
        return;
    }
    
    if (!HammerClass)
    {
        CLog::Log(TEXT("[WeaponComp] HammerClass not set"));
        return;
    }

    SpawnWeapon();

    if (Hammer)
    {
        Hammer->OnHammerHit.AddDynamic(this, &UCWeaponComponent::HandleHammerHit);
    }
}

void UCWeaponComponent::HandleHammerHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit)
{
    float Before = Damage;

    if (IBuffable* Buffable = Cast<IBuffable>(OwnerChar))
    {
        if (Buffable->IsBuffActive())
        {
            const float OutMul = Buffable->GetOutgoingDamageMultiplier();
            Damage *= OutMul;

            UE_LOG(LogTemp, Log, TEXT("[ULT][Weapon] Hit=%s, Base=%.1f, Mul=%.2f, Final=%.1f"), *GetNameSafe(HitActor), Before, OutMul, Damage);
        }
    }

    // 만약 여기 적이 WeaponComponent를 가지게 된다면
    // WeaponComponentBase를 만들고 상속 받아서 Player하고 Enemy 따로 Component를 만들어줘야 작업이 편해짐
    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(GetOwner()))
    {
        if (IsValid(HitActor) && HitActor != OwnerChar)
        {
            const float gain = PlayerChar->GetMaxUltimateGauge() * AddUltGaugeMul;
            PlayerChar->AddUltimateGain(gain);
        }
    }

    AController* InstigatorCtrl = InstigatorActor ? InstigatorActor->GetInstigatorController() : nullptr;

    // HitInfo가 유효하면 포인트 데미지로 위치/노멀 전달
    if (Hit.bBlockingHit)
    {
        UGameplayStatics::ApplyPointDamage(
            HitActor,
            Damage,
            Hit.TraceStart.IsNearlyZero() ? FVector::ZeroVector : (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal(),
            Hit,
            InstigatorCtrl,
            InstigatorActor,
            UDamageType::StaticClass());
    }
    else
    {
        UGameplayStatics::ApplyDamage(
        HitActor,
        Damage,
        InstigatorCtrl,
        InstigatorActor,
        UDamageType::StaticClass());
    }
    
    OnWeaponHit.Broadcast(HitActor, Damage);
}

/* ============ 무기 스폰/부착 ============ */
void UCWeaponComponent::SpawnWeapon()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !IsValid(OwnerChar))
        return;

    FActorSpawnParameters Params;
    Params.Owner = OwnerChar;
    Params.Instigator = OwnerChar;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    Hammer = World->SpawnActor<ACHammer>(HammerClass,
        OwnerChar->GetActorLocation(),
        OwnerChar->GetActorRotation(),
        Params);
    if (!IsValid(Hammer))
    {
        CLog::Log(TEXT("[WeaponComp] SpawnWeapon failed"));
        return;
    }

    Hammer->DeactivateDamage();
    AttachWeaponToCharacter();
}

void UCWeaponComponent::AttachWeaponToCharacter()
{
    if (!IsValid(Hammer) || !IsValid(OwnerChar)) return;

    USkeletalMeshComponent* Mesh = OwnerChar->GetMesh();
    if (!IsValid(Mesh))
    {
        CLog::Log(TEXT("[WeaponComp] Owner mesh invalid"));
        return;
    }

    if (AttachSocketName.IsNone() || !Mesh->DoesSocketExist(AttachSocketName))
    {
        CLog::Log(FString::Printf(TEXT("[WeaponComp] Socket not found: %s"),
            *AttachSocketName.ToString()));
        Hammer->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
        return;
    }

    const bool bOk = Hammer->AttachToComponent(
        Mesh,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        AttachSocketName);

    if (!bOk)
    {
        CLog::Log(TEXT("[WeaponComp] Attach failed, fallback KeepRelative"));
        Hammer->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
    }
}

/* ============ 공격/콤보 ============ */

void UCWeaponComponent::DoAttack()
{
    if (!IsValid(OwnerChar) || PlayerComboMontages.Num() == 0)
        return;

    if (!IsValid(Hammer) || HammerComboMontages.Num() == 0)
        return;

    // 이미 공격 중: 창이 열려 있으면 즉시 다음 스텝, 아니면 큐잉
    if (bIsAttacking)
    {
       	if (bCanNextCombo && CurrentCombo < PlayerComboMontages.Num() - 1 && CurrentCombo < HammerComboMontages.Num() - 1)
       	{
       	    StepToNextCombo();
       	}
        else
        {
            bQueuedNextInput = true; // 창 열릴 때 자동 처리
        }
        return;
    }

    // 첫타 시작
    bIsAttacking = true;
    bQueuedNextInput = false;
    CurrentCombo = 0;
    PlayComboAttack();
}

void UCWeaponComponent::PlayComboAttack()
{
    if (!PlayerComboMontages.IsValidIndex(CurrentCombo))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: invalid index"));
        ResetCombo();
        return;
    }

    UAnimMontage* PlayerMontage = PlayerComboMontages[CurrentCombo];
    UAnimMontage* HammerMontage = HammerComboMontages[CurrentCombo];

    if (!IsValid(PlayerMontage))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: Player montage null"));
        ResetCombo();
        return;
    }

    if (!IsValid(HammerMontage))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: Hammer montage null"));
        ResetCombo();
        return;
    }

    UAnimInstance* PlayerAnimInst = (OwnerChar && OwnerChar->GetMesh())
        ? OwnerChar->GetMesh()->GetAnimInstance()
        : nullptr;

    UAnimInstance* HammerAnimInst = (Hammer && Hammer->GetHammerMesh())
        ? Hammer->GetHammerMesh()->GetAnimInstance()
        : nullptr;
    if (!PlayerAnimInst || !HammerAnimInst)
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: AnimInstance null"));
        ResetCombo();
        return;
    }
 
    PlayerAnimInst->Montage_Play(PlayerMontage);
    HammerAnimInst->Montage_Play(HammerMontage);
    
    // 종료시 정리(인터럽트/블렌드아웃 포함)
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCWeaponComponent::OnMontageEnded);
    PlayerAnimInst->Montage_SetEndDelegate(EndDelegate, PlayerMontage);
    HammerAnimInst->Montage_SetEndDelegate(EndDelegate, HammerMontage);
    
    // 콤보 리셋 타이머
    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
    GetWorld()->GetTimerManager().SetTimer(ComboResetTimer, this, &UCWeaponComponent::ResetCombo, ComboResetTime, false);
}

void UCWeaponComponent::StepToNextCombo()
{
    if (CurrentCombo >= PlayerComboMontages.Num() - 1)
        return;

    ++CurrentCombo;
    bQueuedNextInput = false; // 소비
    bCanNextCombo = false;    // 창 닫힘으로 간주(다음 애님에서 다시 열림)
    PlayComboAttack();
}

void UCWeaponComponent::ResetCombo()
{
    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);

    bIsAttacking = false;
    bCanNextCombo = false;
    bQueuedNextInput = false;
    CurrentCombo = 0;

    // 안전: 히트창 닫기
    DisableAttackBoxCollider();
}

void UCWeaponComponent::OnMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
    if (bInterrupted)
        return;
    
    // 애님이 어떤 이유로 끝나면 항상 정리
    ResetCombo();
}

/* ============ 상태기/노티에서 호출 ============ */

void UCWeaponComponent::BeginAction()
{
    bIsAttacking = true;
    
    if (ACharacter* Ch = OwnerChar)
    {
        if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(Ch))
            PC->OnAttackStarted();
    }
}
void UCWeaponComponent::EndAction()
{
    ResetCombo();
    if (ACharacter* Ch = OwnerChar)
    {
        if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(Ch))
            PC->OnAttackEnded();
    }
}

void UCWeaponComponent::EnableComboInput()
{
    bCanNextCombo = true;

    // 입력이 미리 들어와 있으면 즉시 다음 스텝으로
    if (bQueuedNextInput && bIsAttacking && CurrentCombo < PlayerComboMontages.Num() - 1)
    {
        StepToNextCombo();
    }
}

void UCWeaponComponent::DisableComboInput()
{
    bCanNextCombo = false;
}

void UCWeaponComponent::EnableAttackBoxCollider()
{
    if (IsValid(Hammer)) Hammer->ActivateDamage();
}

void UCWeaponComponent::DisableAttackBoxCollider()
{
    if (IsValid(Hammer)) Hammer->DeactivateDamage();
}
