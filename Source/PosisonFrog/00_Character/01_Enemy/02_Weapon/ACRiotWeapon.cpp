// Fill out your copyright notice in the Description page of Project Settings.


#include "ACRiotWeapon.h"


// Sets default values
AACRiotWeapon::AACRiotWeapon()
{
	Damage = RiotWeaponDamage;

	DamageBoxSocketName = RiotDamageBoxSocketName; // 무기 메시에 정의된 소켓 이름
	DamageBoxExtent = RiotDamageBoxExtent; // 무기 형태에 맞는 적절한 크기
	DamageBoxRelativeLocation = RiotDamageBoxRelativeLocation; // 필요 시 소켓 기준 위치 조정
}
