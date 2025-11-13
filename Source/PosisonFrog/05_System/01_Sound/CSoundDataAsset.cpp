#include "CSoundDataAsset.h"

const FCharacterSoundCollection* UCSoundDataAsset::GetCharacterSounds(FName CharacterType) const
{
	static const FName PlayerName(TEXT("Player"));
	static const FName RiotRobotName(TEXT("RiotRobot"));
	static const FName RangedSkirmisherName(TEXT("RangedSkirmisher"));
	static const FName TankerBruteName(TEXT("TankerBrute"));
	static const FName BossName(TEXT("Boss"));

	if (CharacterType == PlayerName)
	{
		return &PlayerSounds;
	}
	else if (CharacterType == RiotRobotName)
	{
		return &RiotRobotSounds;
	}
	else if (CharacterType == RangedSkirmisherName)
	{
		return &RangedSkirmisherSounds;
	}
	else if (CharacterType == TankerBruteName)
	{
		return &TankerBruteSounds;
	}
	else if (CharacterType == BossName)
	{
		return &BossSounds;
	}

	return nullptr;
}