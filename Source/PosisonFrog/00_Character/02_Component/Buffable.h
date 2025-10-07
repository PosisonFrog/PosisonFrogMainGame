// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Buffable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBuffable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class POSISONFROG_API IBuffable
{
	GENERATED_BODY()

public:
	virtual float GetOutgoingDamageMultiplier() const = 0;
	virtual float GetIncomingDamageScale() const = 0;
	virtual bool IsBuffActive() const = 0;
};
