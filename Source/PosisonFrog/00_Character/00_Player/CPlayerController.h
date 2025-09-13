// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CPlayerController.generated.h"

struct FInputActionValue;
class UInputMappingContext;
class UCInputConfig;
class UCPlayerWidget;
class ACPlayerCharacter;
class UCHealthComponent;
class UCEnhancedInputComponent;
/**
 * 
 */
UCLASS()
class POSISONFROG_API ACPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	
	
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);
	void UpdateHpUI() const;
	
	UFUNCTION()
	void HandleMove(const FInputActionValue& Value);
    
	UFUNCTION()
	void HandleLook(const FInputActionValue& Value);
    
	UFUNCTION()
	void HandleDashStart();
    
	UFUNCTION()
	void HandleAttack();

private:
	void SetupInputBindings();

	UFUNCTION()
	bool ShouldCreatePlayerWidget() const;

	void CreatePlayerWidget();
protected:
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

	TSubclassOf<UInputComponent>InputComponentClass;

	UPROPERTY()
	UCPlayerWidget* PlayerWidget = nullptr;

	UPROPERTY()
	TObjectPtr<ACPlayerCharacter> OwnerCharacter = nullptr;
	
	UPROPERTY()
	TObjectPtr<UCHealthComponent> HealthComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UCInputConfig* InputConfig = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext = nullptr;

	UCEnhancedInputComponent* CEnhancedInputComponent;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bClearPreviousMappings = true;
};
