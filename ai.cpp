#include "stdafx.h"
#include "common.h"
#include "ai.h"
#include "d20.h"
#include "combat.h"
#include "obj.h"
#include "action_sequence.h"
#include "spell.h"
#include "temple_functions.h"
#include "pathfinding.h"
#include "objlist.h"

#pragma region AI System Implementation
#include "location.h"
#include "critter.h"
#include "weapon.h"
struct AiSystem aiSys;


const auto TestSizeOfAiTactic = sizeof(AiTactic); // should be 2832 (0xB10 )
const auto TestSizeOfAiStrategy = sizeof(AiStrategy); // should be 808 (0x324)

struct AiSystemAddresses :AddressTable
{
	int (__cdecl*UpdateAiFlags)(objHndl obj, int aiFightStatus, objHndl target, int * soundMap);
	AiSystemAddresses()
	{
		rebase(UpdateAiFlags, 0x1005DA00);
	}
}addresses;

AiSystem::AiSystem()
{
	combat = &combatSys;
	d20 = &d20Sys;
	actSeq = &actSeqSys;
	spell = &spellSys;
	pathfinding = &pathfindingSys;
	rebase(aiStrategies,0x11868F3C); 
	rebase(aiStrategiesNum,0x11868F40); 
	rebase(aiTacticDefs,0x102E4398); 
	rebase(_AiRemoveFromList, 0x1005A070);
	rebase(_FleeAdd, 0x1005DE60);
	rebase(_ShitlistAdd, 0x1005CC10);
	rebase(_StopAttacking, 0x1005E6A0);
	rebase(_AiSetCombatStatus, 0x1005DA00);
}

void AiSystem::aiTacticGetConfig(int tacIdx, AiTactic* aiTacOut, AiStrategy* aiStrat)
{
	SpellPacketBody * spellPktBody = &aiTacOut->spellPktBody;
	spell->spellPacketBodyReset(&aiTacOut->spellPktBody);
	aiTacOut->aiTac = aiStrat->aiTacDefs[tacIdx];
	aiTacOut->field4 = aiStrat->field54[tacIdx];
	aiTacOut->tacIdx = tacIdx;
	int32_t spellEnum = aiStrat->spellsKnown[tacIdx].spellEnum;
	spellPktBody->spellEnum = spellEnum;
	spellPktBody->spellEnumOriginal = spellEnum;
	if (spellEnum != -1)
	{
		aiTacOut->spellPktBody.objHndCaster = aiTacOut->performer;
		aiTacOut->spellPktBody.casterClassCode = aiStrat->spellsKnown[tacIdx].classCode;
		aiTacOut->spellPktBody.spellKnownSlotLevel = aiStrat->spellsKnown[tacIdx].spellLevel;
		spell->spellPacketSetCasterLevel(spellPktBody);
		d20->D20ActnSetSpellData(&aiTacOut->d20SpellData, spellEnum, spellPktBody->casterClassCode, spellPktBody->spellKnownSlotLevel, 0xFF, spellPktBody->metaMagicData);
	}
}

uint32_t AiSystem::AiStrategyParse(objHndl objHnd, objHndl target)
{
	AiTactic aiTac;
	combat->enterCombat(objHnd);
	AiStrategy* aiStrat = &(*aiStrategies)[objects.getInt32(objHnd, obj_f_critter_strategy)];
	if (!actSeq->TurnBasedStatusInit(objHnd)) return 0;
	
	actSeq->curSeqReset(objHnd);
	d20->GlobD20ActnInit();
	spell->spellPacketBodyReset(&aiTac.spellPktBody);
	aiTac.performer = objHnd;
	aiTac.target = target;

	if (d20Sys.d20Query(aiTac.performer, DK_QUE_Disarmed))
	{
		hooked_print_debug_message("\n%s attempting to pickup weapon...\n", description._getDisplayName(objHnd, objHnd));
		if (PickUpWeapon(&aiTac))
		{
			actSeq->sequencePerform();
			return 1;
		}
	}

	for (uint32_t i = 0; i < aiStrat->numTactics; i++)
	{
		aiTacticGetConfig(i, &aiTac, aiStrat);
		hooked_print_debug_message("\n%s attempting %s...\n", description._getDisplayName(objHnd, objHnd), aiTac.aiTac->name);
		auto aiFunc = aiTac.aiTac->aiFunc;
		if (!aiFunc) continue;
		if (aiFunc(&aiTac)) {
			actSeq->sequencePerform();
			return 1;
		}
	}

	// if none of those work, use default
	aiTac.aiTac = &aiTacticDefs[0];
	aiTac.field4 = 0;
	aiTac.tacIdx = -1;
	hooked_print_debug_message("\n%s attempting %s...\n", description._getDisplayName(objHnd, objHnd), aiTac.aiTac->name);
	assert(aiTac.aiTac != nullptr);
	if (aiTac.aiTac->aiFunc(&aiTac))
	{
		actSeq->sequencePerform();
		return 1;
	}
	objHndl pathablePartyMember = pathfinding->canPathToParty(objHnd);
	if (pathablePartyMember)
	{
		if (aiTac.aiTac->aiFunc(&aiTac))
		{
			actSeq->sequencePerform();
			return 1;
		}
	}
	return 0;
}

bool AiSystem::HasAiFlag(objHndl npc, AiFlag flag) {
	auto aiFlags = templeFuncs.Obj_Get_Field_64bit(npc, obj_f_npc_ai_flags64);
	auto flagBit = (uint64_t)flag;
	return (aiFlags & flagBit) != 0;
}

void AiSystem::SetAiFlag(objHndl npc, AiFlag flag) {
	auto aiFlags = templeFuncs.Obj_Get_Field_64bit(npc, obj_f_npc_ai_flags64);
	aiFlags |= (uint64_t) flag;
	templeFuncs.Obj_Set_Field_64bit(npc, obj_f_npc_ai_flags64, aiFlags);
}

void AiSystem::ClearAiFlag(objHndl npc, AiFlag flag) {
	auto aiFlags = templeFuncs.Obj_Get_Field_64bit(npc, obj_f_npc_ai_flags64);
	aiFlags &= ~ (uint64_t)flag;
	templeFuncs.Obj_Set_Field_64bit(npc, obj_f_npc_ai_flags64, aiFlags);
}

void AiSystem::ShitlistAdd(objHndl npc, objHndl target) {
	_ShitlistAdd(npc, target);
}

void AiSystem::ShitlistRemove(objHndl npc, objHndl target) {
	_AiRemoveFromList(npc, target, 0); // 0 is the shitlist

	auto focus = GetCombatFocus(npc);
	if (focus == target) {
		SetCombatFocus(npc, 0);
	}
	auto hitMeLast = GetWhoHitMeLast(npc);
	if (hitMeLast == target) {
		SetWhoHitMeLast(npc, 0);
	}

	_AiSetCombatStatus(npc, 0, 0, 0); // I think this means stop attacking?
}

void AiSystem::FleeAdd(objHndl npc, objHndl target) {
	_FleeAdd(npc, target);
}

void AiSystem::StopAttacking(objHndl npc) {
	_StopAttacking(npc);
}

objHndl AiSystem::GetCombatFocus(objHndl npc) {
	return templeFuncs.Obj_Get_Field_ObjHnd__fastout(npc, obj_f_npc_combat_focus);
}

objHndl AiSystem::GetWhoHitMeLast(objHndl npc) {
	return templeFuncs.Obj_Get_Field_ObjHnd__fastout(npc, obj_f_npc_who_hit_me_last);
}

void AiSystem::SetCombatFocus(objHndl npc, objHndl target) {
	templeFuncs.Obj_Set_Field_ObjHnd(npc, obj_f_npc_combat_focus, target);
}

void AiSystem::SetWhoHitMeLast(objHndl npc, objHndl target) {
	templeFuncs.Obj_Set_Field_ObjHnd(npc, obj_f_npc_who_hit_me_last, target);
}

int AiSystem::TargetClosest(AiTactic* aiTac)
{
	objHndl target; 
	LocAndOffsets performerLoc; 
	float dist = 0.0;

	locSys.getLocAndOff(aiTac->performer, &performerLoc);
	if (combatSys.GetClosestEnemy(aiTac->performer, &performerLoc, &target, &dist, 0x21))
	{
		aiTac->target = target;
		
	}
	return 0;
}

int AiSystem::Approach(AiTactic* aiTac)
{
	int d20aNum; 

	d20aNum = (*actSeqSys.actSeqCur)->d20ActArrayNum;
	if (!aiTac->target)
		return 0;
	if (combatSys.IsWithinReach(aiTac->performer, aiTac->target))
		return 0;
	actSeqSys.curSeqReset(aiTac->performer);
	d20Sys.GlobD20ActnInit();
	d20Sys.GlobD20ActnSetTypeAndData1(D20A_UNSPECIFIED_MOVE, 0);
	d20Sys.GlobD20ActnSetTarget(aiTac->target, 0);
	actSeqSys.ActionAddToSeq();
	if (actSeqSys.ActionSequenceChecksWithPerformerLocation())
	{
		actSeqSys.ActionSequenceRevertPath(d20aNum);
		return 0;
	}
	return 1;
}

int AiSystem::PickUpWeapon(AiTactic* aiTac)
{
	int d20aNum;

	d20aNum = (*actSeqSys.actSeqCur)->d20ActArrayNum;

	if (inventory.ItemWornAt(aiTac->performer, 203) || inventory.ItemWornAt(aiTac->performer, 204))
	{
		return 0;
	}
	if (!d20Sys.d20Query(aiTac->performer, DK_QUE_Disarmed))
		return 0;
	objHndl weapon = d20Sys.d20QueryReturnData(aiTac->performer, DK_QUE_Disarmed, 0, 0);
	if (weapon && !inventory.GetParent(weapon))
	{
		aiTac->target = weapon;
	} else
	{
		LocAndOffsets loc;
		locSys.getLocAndOff(aiTac->performer, &loc);

		ObjList objList;
		objList.ListTile(loc.location, OLC_WEAPON);

		if (objList.size() > 0)
		{
			weapon = objList.get(0);
			aiTac->target = weapon;
		}
	}
	
	
	
	
	if (!aiTac->target)
		return 0;

	if (!combatSys.IsWithinReach(aiTac->performer, aiTac->target))
		return 0;
	actSeqSys.curSeqReset(aiTac->performer);
	d20Sys.GlobD20ActnInit();
	d20Sys.GlobD20ActnSetTypeAndData1(D20A_DISARMED_WEAPON_RETRIEVE, 0);
	d20Sys.GlobD20ActnSetTarget(aiTac->target, 0);
	actSeqSys.ActionAddToSeq();
	if (actSeqSys.ActionSequenceChecksWithPerformerLocation())
	{
		actSeqSys.ActionSequenceRevertPath(d20aNum);
		return 0;
	}
	return 1;
}

int AiSystem::CoupDeGrace(AiTactic* aiTac)
{
	int actNum; 
	objHndl origTarget = aiTac->target;
	signed int result; // eax@2

	actNum = (*actSeqSys.actSeqCur)->d20ActArrayNum;
	aiTac->target = 0i64;
	combatSys.GetClosestEnemy(aiTac, 1);
	if (aiTac->target)
	{
		if (Approach(aiTac)
			|| (d20Sys.GlobD20ActnInit(),
			d20Sys.GlobD20ActnSetTypeAndData1(D20A_COUP_DE_GRACE, 0),
			d20Sys.GlobD20ActnSetTarget(aiTac->target, 0),
			actSeqSys.ActionAddToSeq(),
			!actSeqSys.ActionSequenceChecksWithPerformerLocation()))
		{
			return 1;
		}
		actSeqSys.ActionSequenceRevertPath(actNum);
		return 0;
		
	}

	aiTac->target = origTarget;
	return 0;


}

void AiSystem::UpdateAiFightStatus(objHndl objIn, int* aiStatus, objHndl* target)
{
	int critterFlags;
	AiFlag aiFlags;

	if (objects.getInt32(objIn, obj_f_critter_flags) & OCF_FLEEING)
	{
		if (target)
			*target = objects.getObjHnd(objIn, obj_f_critter_fleeing_from);
		*aiStatus = 2;
	}
	else
	{
		critterFlags = objects.getInt32(objIn, obj_f_critter_flags);
		if (critterFlags & OCF_SURRENDERED)                     // OCF_SURRENDERED (maybe mixed up with OCF_SPELL_FLEE?)
		{
			if (target)
				*target = objects.getObjHnd(objIn, obj_f_critter_fleeing_from);
			*aiStatus = 3;
		}
		else
		{
			aiFlags = (AiFlag)objects.getInt64(objIn, obj_f_npc_ai_flags64);
			if ((uint64_t)aiFlags & (uint64_t)AiFlag::Fighting)
			{
				if (target)
					*target = objects.getObjHnd(objIn, obj_f_npc_combat_focus);
				*aiStatus = 1;
			}
			else if ((uint64_t)aiFlags & (uint64_t)AiFlag::FindingHelp)
			{
				if (target)
					*target = objects.getObjHnd(objIn, obj_f_npc_combat_focus);
				*aiStatus = 4;
			}
			else
			{
				if (target)
					*target = 0i64;
				*aiStatus = 0;
			}
		}
	}
}

int AiSystem::UpdateAiFlags(objHndl obj, int aiFightStatus, objHndl target, int* soundMap)
{
	return addresses.UpdateAiFlags(obj, aiFightStatus, target, soundMap);
}
#pragma endregion

#pragma region AI replacement functions

uint32_t _aiStrategyParse(objHndl objHnd, objHndl target)
{
	return aiSys.AiStrategyParse(objHnd, target);
}

int _AiCoupDeGrace(AiTactic* aiTac)
{
	return aiSys.CoupDeGrace(aiTac);
}

int _AiApproach(AiTactic* aiTac)
{
	return aiSys.Approach(aiTac);
}

class AiReplacements : public TempleFix
{
public: 
	const char* name() override { return "AI Function Replacements";} 
	void apply() override 
	{
		logger->info("Replacing AI functions...");
		replaceFunction(0x100E48D0, _AiApproach);
		replaceFunction(0x100E50C0, _aiStrategyParse); 
		replaceFunction(0x100E5DB0, _AiCoupDeGrace);
		
	}
} aiReplacements;
#pragma endregion 
AiPacket::AiPacket(objHndl objIn)
{
	target = 0i64;
	obj = objIn;
	aiFightStatus = 0;
	aiState2 = 0;
	field18 = 10000;
	skillEnum = (SkillEnum)-1;
	scratchObj = 0i64;
	leader = critterSys.GetLeader(objIn);
	aiSys.UpdateAiFightStatus(objIn, &this->aiFightStatus, &this->target);
	field30 = -1;
}