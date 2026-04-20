// Fill out your copyright notice in the Description page of Project Settings.
#include "99_Util/CLog.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(Game, Display, All)


void CLog::Log(int32 InValue)
{
#if !UE_BUILD_SHIPPING
	//GLog->Log("Game", ELogVerbosity::Display, FString::FromInt(InValue));

	UE_LOG(Game, Display, TEXT("%d"), InValue);
#endif
}

void CLog::Log(float InValue)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(Game, Display, TEXT("%f"), InValue);
#endif
}

void CLog::Log(const FString & InValue)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(Game, Display, TEXT("%s"), *InValue);
#endif
}

void CLog::Log(const FVector & InValue)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(Game, Display, TEXT("%s"), *InValue.ToString());
#endif
}

void CLog::Log(const FRotator & InValue)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(Game, Display, TEXT("%s"), *InValue.ToString());
#endif
}

void CLog::Log(const UObject * InValue)
{
#if !UE_BUILD_SHIPPING
	FString str;

	if (!!InValue)
		str.Append(InValue->GetName());

	str.Append(!!InValue ? " Not Null" : "Null");

	UE_LOG(Game, Display, TEXT("%s"), *str);
#endif
}

void CLog::Print(int32 InValue, int32 InKey, float InDuration, FColor InColor)
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InDuration, InColor, FString::FromInt(InValue));
	}
#endif
}

void CLog::Print(float InValue, int32 InKey, float InDuration, FColor InColor)
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InDuration, InColor, FString::SanitizeFloat(InValue));
	}
#endif
}

void CLog::Print(const FString& InValue, int32 InKey, float InDuration, FColor InColor)
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InDuration, InColor, InValue);
	}
#endif
}

void CLog::Print(const FVector& InValue, int32 InKey, float InDuration, FColor InColor)
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InDuration, InColor, InValue.ToString());
	}
#endif
}

void CLog::Print(const FRotator& InValue, int32 InKey, float InDuration, FColor InColor)
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InDuration, InColor, InValue.ToString());
	}
#endif
}

void CLog::Print(const UObject* InValue, int32 InKey, float InDuration, FColor InColor)
{
#if !UE_BUILD_SHIPPING
	FString str;

	if (!!InValue)
		str.Append(InValue->GetName());

	str.Append(!!InValue ? " Not Null" : "Null");

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, InDuration, InColor, str);
	}
#endif
}

