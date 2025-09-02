// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CInputConfig.h"

#include "GameplayTagContainer.h"
#include "InputAction.h"

const UInputAction* UCInputConfig::FindInputActionForTag(const FGameplayTag& InputTag) const
{
	for (const FTaggedInputAction& TaggedInputAction : TaggedInputActions)
	{
		if (TaggedInputAction.InputAction && TaggedInputAction.InputTag == InputTag)
		{
			return TaggedInputAction.InputAction;
		}
	}

	return nullptr;
}
