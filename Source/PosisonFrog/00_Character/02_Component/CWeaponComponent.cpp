#include "00_Character/02_Component/CWeaponComponent.h"

#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "99_Util/CLog.h"

UCWeaponComponent::UCWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 이벤트 드리븐
}

void UCWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
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
    OnWeaponHit.Broadcast(HitActor, Damage);
}

/* ============ 무기 스폰/부착 ============ */
void UCWeaponComponent::SpawnWeapon()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !IsValid(OwnerCharacter))
        return;

    FActorSpawnParameters Params;
    Params.Owner = OwnerCharacter;
    Params.Instigator = OwnerCharacter;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    Hammer = World->SpawnActor<ACHammer>(HammerClass,
        OwnerCharacter->GetActorLocation(),
        OwnerCharacter->GetActorRotation(),
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
    if (!IsValid(Hammer) || !IsValid(OwnerCharacter)) return;

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
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
    if (!IsValid(OwnerCharacter) || ComboMontages.Num() == 0)
        return;


    // 이미 공격 중: 창이 열려 있으면 즉시 다음 스텝, 아니면 큐잉
    if (bIsAttacking)
    {
       	if (bCanNextCombo && CurrentCombo < ComboMontages.Num() - 1)
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
    if (!ComboMontages.IsValidIndex(CurrentCombo))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: invalid index"));
        ResetCombo();
        return;
    }

    UAnimMontage* Montage = ComboMontages[CurrentCombo];
    if (!IsValid(Montage))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: montage null"));
        ResetCombo();
        return;
    }

    UAnimInstance* AnimInst = (OwnerCharacter && OwnerCharacter->GetMesh())
        ? OwnerCharacter->GetMesh()->GetAnimInstance()
        : nullptr;
    if (!AnimInst)
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: AnimInstance null"));
        ResetCombo();
        return;
    }
 
    AnimInst->Montage_Play(Montage);
    
    // 종료시 정리(인터럽트/블렌드아웃 포함)
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCWeaponComponent::OnMontageEnded);
    AnimInst->Montage_SetEndDelegate(EndDelegate, Montage);

    // 콤보 리셋 타이머
    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
    GetWorld()->GetTimerManager().SetTimer(ComboResetTimer, this, &UCWeaponComponent::ResetCombo, ComboResetTime, false);
}

void UCWeaponComponent::StepToNextCombo()
{
    if (CurrentCombo >= ComboMontages.Num() - 1)
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

void UCWeaponComponent::BeginAction() { bIsAttacking = true; }
void UCWeaponComponent::EndAction() { ResetCombo(); }

void UCWeaponComponent::EnableComboInput()
{
    bCanNextCombo = true;

    // 입력이 미리 들어와 있으면 즉시 다음 스텝으로
    if (bQueuedNextInput && bIsAttacking && CurrentCombo < ComboMontages.Num() - 1)
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
