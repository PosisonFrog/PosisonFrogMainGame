#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CAnimNotifyState_PlayerAttack.generated.h"

class UCWeaponComponent;

/**
 * 플레이어 공격 히트창(NotifyState)
 * - Begin ~ End 사이 무기 콜라이더를 안전하게 On/Off
 * - 중첩/중복 NotifyState를 고려해 컴포넌트 단위 Ref-Count로 관리
 * - 서버 권한에서만 실제 충돌 On/Off (클라는 코스메틱)
 * - 에디터 미리보기(Persona)에서는 기본적으로 동작하지 않도록 가드
 */
UCLASS(meta = (DisplayName = "PF_PlayerAttackWindow"))
class POSISONFROG_API UCAnimNotifyState_PlayerAttack : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    /** 서버 권한에서만 실제 콜라이더 On/Off (권장) */
    UPROPERTY(EditAnywhere, Category = "PF|HitWindow")
    bool bServerOnlyCollision = true;

    /** 에디터 미리보기(Anim Preview)에서도 작동시키고 싶다면 켜세요 (기본 Off) */
    UPROPERTY(EditAnywhere, Category = "PF|HitWindow", meta = (EditCondition = "true"))
    bool bAllowInPreviewEditor = false;

    /** 개발 중 로그(출입/RefCount)를 찍습니다 */
    UPROPERTY(EditAnywhere, Category = "PF|Debug")
    bool bDebugLog = false;

public:
    // AnimNotifyState API
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

private:
    /** 현재 Notify 인스턴스가 마지막에 참조한 무기 컴포넌트(디버그용) */
    UPROPERTY()
    TWeakObjectPtr<UCWeaponComponent> LastWeaponComp;

private:
    // ----- Ref-Count 관리: 컴포넌트별로 Begin/End 중첩을 안전하게 처리 -----
    static int32 GetRefCount(UCWeaponComponent* Comp);
    static int32 IncRef(UCWeaponComponent* Comp);
    static int32 DecRef(UCWeaponComponent* Comp);

    static void EnableIfFirst(UCWeaponComponent* Comp, bool bDebug);
    static void DisableIfNone(UCWeaponComponent* Comp, bool bDebug);

private:
    /** 전용 Ref-Count 테이블 (Weak 포인터 키) */
    static TMap<TWeakObjectPtr<UCWeaponComponent>, int32> RefTable;
};

/*요 코드의 핵심은 “겹치는 공격 히트창을 안전하게” 처리하기 위해

노티파이마다 켰다 끄는 단순 방식 대신,

컴포넌트별 참조 카운트(Ref-Count) 를 이 NotifyState 내부에서 정적 맵으로 관리하여

첫 Begin에서만 켜고,

마지막 End에서만 끄도록 만든 것 입니다.
(이 방식은 UCWeaponComponent를 수정하지 않아도 동작합니다. 기존의 EnableAttackBoxCollider / DisableAttackBoxCollider API만 있으면 됩니다.)*/