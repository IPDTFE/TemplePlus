#include "stdafx.h"
#include "common.h"
#include "combat.h"
#include "tig/tig_mes.h"
#include "obj.h"
#include "critter.h"
#include "temple_functions.h"
#include "party.h"
#include "location.h"
#include "bonus.h"
#include "history.h"


class CombatSystemReplacements : public TempleFix
{
public:
	const char* name() override {
		return "Combat System Replacements";
	}
	void apply() override{
		replaceFunction(0x100628D0, _isCombatActive);
		replaceFunction(0x100629B0, _IsCloseToParty);
	}
} combatSysReplacements;

struct CombatSystemAddresses : AddressTable
{
	int(__cdecl* GetEnemiesCanMelee)(objHndl obj, objHndl* canMeleeList);
	CombatSystemAddresses()
	{
		rebase(GetEnemiesCanMelee, 0x100B90C0);
	}
} addresses;

#pragma region Combat System Implementation
CombatSystem combatSys;

bool CombatSystem::isCombatActive()
{
	return *combatSys.combatModeActive != 0;
}

uint32_t CombatSystem::IsCloseToParty(objHndl objHnd)
{
	if (critterSys.IsPC(objHnd))  return 1;
	for (auto i = 0; i < party.GroupListGetLen(); i++)
	{
		auto dude = party.GroupListGetMemberN(i);
		if (locSys.DistanceToObj(objHnd, dude) < COMBAT_ACTIVATION_DISTANCE)
			return 1;
	}
	return 0;
}

int CombatSystem::GetEnemiesCanMelee(objHndl obj, objHndl* canMeleeList)
{
	return addresses.GetEnemiesCanMelee(obj, canMeleeList);
}

objHndl CombatSystem::GetWeapon(AttackPacket* attackPacket)
{
	D20CAF flags = attackPacket->flags;
	objHndl result = 0i64;

	if (!(flags & D20CAF_TOUCH_ATTACK) || flags & D20CAF_THROWN_GRENADE )
		result = attackPacket->weaponUsed;
	return result;
}

bool CombatSystem::DisarmCheck(objHndl attacker, objHndl defender)
{
	objHndl attackerWeapon = inventory.ItemWornAt(attacker, 3);
	objHndl defenderWeapon = inventory.ItemWornAt(defender, 3);
	int attackerRoll = templeFuncs.diceRoll(1, 20, 0);
	int attackerSize = dispatch.DispatchGetSizeCategory(attacker);
	BonusList atkBonlist;
	bonusSys.bonusAddToBonusList(&atkBonlist, (attackerSize - 5 )* 4,0, 316);
	int attackerResult = attackerRoll + bonusSys.getOverallBonus(&atkBonlist);
	
	int defenderRoll = templeFuncs.diceRoll(1, 20, 0);
	int defenderSize = dispatch.DispatchGetSizeCategory(defender);
	BonusList defBonlist;
	bonusSys.bonusAddToBonusList(&defBonlist, (defenderSize - 5) * 4, 0, 316);
	int defenderResult = defenderRoll + bonusSys.getOverallBonus(&defBonlist);
	int attackerSucceeded = attackerResult > defenderResult;
	int rollHistId = histSys.RollHistoryAddType6OpposedCheck(attacker, defender, attackerRoll, defenderRoll, &atkBonlist, &defBonlist, 5062, 144 - attackerSucceeded, 1);
	histSys.CreateRollHistoryString(rollHistId);
	
	return attackerSucceeded;
}

void CombatSystem::enterCombat(objHndl objHnd)
{
	_enterCombat(objHnd);
}
#pragma endregion


uint32_t _isCombatActive()
{
	return *combatSys.combatModeActive;
}

uint32_t _IsCloseToParty(objHndl objHnd)
{
	return combatSys.IsCloseToParty(objHnd);
}

uint32_t Combat_GetMesfileIdx_CombatMes()
{
	return *combatSys.combatMesfileIdx;
}