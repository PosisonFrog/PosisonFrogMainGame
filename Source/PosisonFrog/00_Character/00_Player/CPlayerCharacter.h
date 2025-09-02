// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/CBaseCharacter.h"
#include "CPlayerCharacter.generated.h"

class UCInputConfig;
class UCDashComponent;
struct FInputActionValue;

UCLASS()
class POSISONFROG_API ACPlayerCharacter : public ACBaseCharacter
{
	GENERATED_BODY()

public:
	// 생성자 및 기본 함수
	ACPlayerCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// 입력 처리 함수
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	// 기본 오버라이드 함수
	virtual void BeginPlay() override;
	virtual FVector GetPawnViewLocation() const override;
	
	
	// 대시 함수
	void DashStart();


	//=========================================================================================
	// 변수 선언부
	//=========================================================================================
public:
	// 입력 설정
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UCInputConfig* InputConfig;
	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MovementSpeed")
	float WalkingSpeed = 400.0f;
	
	// 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Component")
	UCDashComponent* DashComponent;
	
	// 카메라 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* SpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* PlayerCamera;

	float DefaultFOV;

	// 카메라 회전 속도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;


};
