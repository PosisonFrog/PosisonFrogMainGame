#include "00_Character/00_Player/00_Notify/CAnimNotifyState_PlayerAttack.h"

#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

// 로컬 로그 카테고리 (원하시면 프로젝트 공용 로그 사용)
DEFINE_LOG_CATEGORY_STATIC(LogPF_PlayerAttackNotify, Log, All);

/* static */ TMap<TWeakObjectPtr<UCPlayerWeaponComponent>, int32> UCAnimNotifyState_PlayerAttack::RefTable;

static bool PF_IsGameWorldSafe(const USkeletalMeshComponent* MeshComp, bool bAllowPreview)
{
    if (!MeshComp) return false;
    const UWorld* World = MeshComp->GetWorld();
    if (!World) return false;
    if (bAllowPreview) return true;
    return World->IsGameWorld();
}

/* static */ int32 UCAnimNotifyState_PlayerAttack::GetRefCount(UCPlayerWeaponComponent* Comp)
{
    if (!IsValid(Comp)) return 0;
    if (int32* Found = RefTable.Find(Comp))
        return *Found;
    return 0;
}

/* static */ int32 UCAnimNotifyState_PlayerAttack::IncRef(UCPlayerWeaponComponent* Comp)
{
    if (!IsValid(Comp)) return 0;
    int32& Cnt = RefTable.FindOrAdd(Comp);
    Cnt = FMath::Max(0, Cnt + 1);
    return Cnt;
}

/* static */ int32 UCAnimNotifyState_PlayerAttack::DecRef(UCPlayerWeaponComponent* Comp)
{
    if (!IsValid(Comp)) return 0;
    int32& Cnt = RefTable.FindOrAdd(Comp);
    Cnt = FMath::Max(0, Cnt - 1);
    if (Cnt == 0)
    {
        // 0이 되면 맵에서 제거(가비지와 weak키 정리)
        RefTable.Remove(Comp);
    }
    return Cnt;
}

/* static */ void UCAnimNotifyState_PlayerAttack::EnableIfFirst(UCPlayerWeaponComponent* Comp, bool bDebug)
{
    if (!IsValid(Comp)) return;

    const int32 After = IncRef(Comp);
    if (After == 1)
    {
        Comp->EnableAttackBoxCollider();
        if (bDebug)
        {
            UE_LOG(LogPF_PlayerAttackNotify, Log, TEXT("[Enable] %s (Ref=1)"),
                *GetNameSafe(Comp));
        }
    }
    else if (bDebug)
    {
        UE_LOG(LogPF_PlayerAttackNotify, Verbose, TEXT("[Enable-Skip] %s (Ref=%d)"),
            *GetNameSafe(Comp), After);
    }
}

/* static */ void UCAnimNotifyState_PlayerAttack::DisableIfNone(UCPlayerWeaponComponent* Comp, bool bDebug)
{
    if (!IsValid(Comp)) return;

    const int32 After = DecRef(Comp);
    if (After == 0)
    {
        Comp->DisableAttackBoxCollider();
        if (bDebug)
        {
            UE_LOG(LogPF_PlayerAttackNotify, Log, TEXT("[Disable] %s (Ref=0)"),
                *GetNameSafe(Comp));
        }
    }
    else if (bDebug)
    {
        UE_LOG(LogPF_PlayerAttackNotify, Verbose, TEXT("[Disable-Skip] %s (Ref=%d)"),
            *GetNameSafe(Comp), After);
    }
}

void UCAnimNotifyState_PlayerAttack::NotifyBegin(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    float TotalDuration,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    if (!PF_IsGameWorldSafe(MeshComp, bAllowInPreviewEditor))
        return;

    ACharacter* OwnerCharacter = MeshComp ? MeshComp->GetOwner<ACharacter>() : nullptr;
    if (!OwnerCharacter) return;

    // 서버 권한에서만 실제 충돌 On (권장)
    if (bServerOnlyCollision && !OwnerCharacter->HasAuthority())
        return;

    UCPlayerWeaponComponent* WeaponComp = OwnerCharacter->FindComponentByClass<UCPlayerWeaponComponent>();
    if (!WeaponComp) return;

    LastWeaponComp = WeaponComp;

    // Ref-Count 방식: 첫 Begin에서만 Enable
    EnableIfFirst(WeaponComp, bDebugLog);

    if (AActor* Owner = MeshComp->GetOwner())
    {
        if (UCPlayerWeaponComponent* WeaponComponent = Owner->FindComponentByClass<UCPlayerWeaponComponent>())
        {
            WeaponComponent->BeginAction();
        }
    }
}

void UCAnimNotifyState_PlayerAttack::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    if (!PF_IsGameWorldSafe(MeshComp, bAllowInPreviewEditor))
        return;

    ACharacter* OwnerCharacter = MeshComp ? MeshComp->GetOwner<ACharacter>() : nullptr;
    if (!OwnerCharacter) return;

    if (bServerOnlyCollision && !OwnerCharacter->HasAuthority())
        return;

    // 가능하면 같은 무기 기준으로 Ref-Count 감소
    UCPlayerWeaponComponent* WeaponComp = nullptr;

    if (LastWeaponComp.IsValid())
    {
        WeaponComp = LastWeaponComp.Get();
    }
    else if (AActor* Owner = OwnerCharacter)
    {
        WeaponComp = Owner->FindComponentByClass<UCPlayerWeaponComponent>();
    }

    if (!WeaponComp) return;

    // Ref-Count 방식: 마지막 End에서만 Disable
    DisableIfNone(WeaponComp, bDebugLog);

    if (AActor* Owner = MeshComp->GetOwner())
    {
        if (UCPlayerWeaponComponent* WeaponComponent = Owner->FindComponentByClass<UCPlayerWeaponComponent>())
        {
            WeaponComponent->EndAction();
        }
    }
}

//이후 디버그 스윕 라인 그리기, 공격 태그/데미지 배율 파라미터화 등 확장하면 좋을것 같습니당
