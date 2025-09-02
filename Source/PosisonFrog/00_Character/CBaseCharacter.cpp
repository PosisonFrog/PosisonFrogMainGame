// Fill out your copyright notice in the Description page of Project Settings.


#include "CBaseCharacter.h"

// Sets default values
ACBaseCharacter::ACBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ACBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

