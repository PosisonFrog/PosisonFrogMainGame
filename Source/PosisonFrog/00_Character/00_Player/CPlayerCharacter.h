// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "00_Character/CBaseCharacter.h"
#include "CPlayerCharacter.generated.h"

class UCWeaponComponent;
class USpringArmComponent;
class UCameraComponent;
class UCDashComponent;
class UCHealthComponent;
class UCInputConfig;
class UCPlayerWidget;
struct FInputActionValue;


UCLASS(config=Game)
class POSISONFROG_API ACPlayerCharacter : public ACBaseCharacter
{
	GENERATED_BODY()

public:
	ACPlayerCharacter();
public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return SpringArm; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return PlayerCamera; }

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void DashStart();

	void Attack();
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UCInputConfig* InputConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MovementSpeed")
	float WalkingSpeed = 400.0f;

	// UI
protected:
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

	UPROPERTY()
	UCPlayerWidget* PlayerWidget = nullptr;

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);
	
	void UpdateHpUI() const;
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Component")
	UCDashComponent* DashComponent;

	UPROPERTY(VisibleAnywhere, Category = "Component")
	UCWeaponComponent* WeaponComponent;

	UPROPERTY(VisibleAnywhere, Category = "Component")
	UCHealthComponent* HealthComponent;
	
	// 카메라 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* PlayerCamera;
};

