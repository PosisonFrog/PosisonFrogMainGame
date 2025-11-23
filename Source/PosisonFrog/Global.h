#pragma once

#include "DrawDebugHelpers.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "99_Util/CLog.h"
#include "99_Util/CHelpers.h"

namespace PF
{
	namespace Collision
	{
		constexpr ECollisionChannel PlayerBody = ECollisionChannel::ECC_GameTraceChannel1;
		constexpr ECollisionChannel EnemyBody = ECollisionChannel::ECC_GameTraceChannel2;
		constexpr ECollisionChannel RiotEnemy = ECollisionChannel::ECC_GameTraceChannel3;
		constexpr ECollisionChannel Projectile = ECollisionChannel::ECC_GameTraceChannel4;
		constexpr ECollisionChannel BossCharacter = ECollisionChannel::ECC_GameTraceChannel5;
	}
}