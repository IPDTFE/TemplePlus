
#include "stdafx.h"
#include "ui.h"
#include "config/config.h"
#include "tig/tig_mes.h"
#include "tig/tig_font.h"
#include "temple_functions.h"
#include "tig/tig_tokenizer.h"
#include "ui_item_creation.h"
#include "combat.h"
#include "obj.h"
#include "radialmenu.h"
#include "common.h"
#include "ui_render.h"
#include <python/python_integration_obj.h>
#include <python/python_object.h>
#include <condition.h>
#include <critter.h>
#include <util/fixes.h>
//#include <condition.h>
#include "gamesystems/objects/objsystem.h"
#include <gamesystems/gamesystems.h>
#include <infrastructure/keyboard.h>
#include <infrastructure/elfhash.h>
#include <tig/tig_tabparser.h>
#include <d20_level.h>
#include <party.h>

#define NUM_ITEM_ENHANCEMENT_SPECS 41
#define MAX_CRAFTED_BONUSES 9 // number of bonuses that can be applied on item creation
#define CRAFT_EFFECT_INVALID -1
#define MAA_EFFECT_BUTTONS_COUNT 10
#define MAA_TEXTBOX_MAX_LENGTH 60
#define NUM_DISPLAYED_CRAFTABLE_ITEMS_MAX 21
#define MAA_NUM_ENCHANTABLE_ITEM_WIDGETS 5
#define NUM_ITEM_CREATION_ENTRY_WIDGETS 21 // number of "buttons" (clickable text items) in the normal crafting menu

const std::unordered_map<std::string, uint32_t> ItemEnhSpecFlagDict = { 
	{"iesf_enabled", IESF_ENABLED},
	{"iesf_weapon",IESF_WEAPON } ,
	{"iesf_armor",IESF_ARMOR },
	{"iesf_shield",IESF_SHIELD },
	{"iesf_ranged",IESF_RANGED },

	{"iesf_melee",IESF_MELEE },
	{"iesf_thrown",IESF_THROWN },
	{"iesf_unk100",IESF_UNK100 },
	{"iesf_plus_bonus",IESF_PLUS_BONUS }
};

int WandCraftCostCp=0;

ItemCreation itemCreation;

struct UiItemCreationAddresses : temple::AddressTable
{
	int (__cdecl* Sub_100F0200)(objHndl a1, RadialMenuEntry *entry);
	ItemCreationType * itemCreationType;
	objHndl* crafter;
	int *craftInsufficientXP;
	int * craftInsufficientFunds;
	int*craftSkillReqNotMet;
	int*dword_10BEE3B0;
	char**itemCreationUIStringSkillRequired;
	char**itemCreationUIStringItemCost;
	char**itemCreationUIStringXPCost;
	char**itemCreationUIStringValue;
	TigTextStyle*itemCreationTextStyle;
	TigTextStyle*itemCreationTextStyle2;
	ItemEnhancementSpec *itemEnhSpecs; // size 41
	UiItemCreationAddresses()
	{
		rebase(Sub_100F0200, 0x100F0200);
		rebase(itemCreationType, 0x10BEDF50);
		rebase(crafter, 0x10BECEE0);
		rebase(craftInsufficientXP,0x10BEE3A4);
		rebase(craftInsufficientFunds, 0x10BEE3A8);
		rebase(craftSkillReqNotMet, 0x10BEE3AC);
		rebase(dword_10BEE3B0, 0x10BEE3B0);

		rebase(itemCreationUIStringSkillRequired, 0x10BED6D4);
		rebase(itemCreationUIStringItemCost, 0x10BEDB50);
		rebase(itemCreationUIStringXPCost, 0x10BED8A4);
		rebase(itemCreationUIStringValue, 0x10BED8A8);

		rebase(itemCreationTextStyle, 0x10BEE338);
		rebase(itemCreationTextStyle2, 0x10BED938);
		rebase(itemEnhSpecs, 0x102967C8);
	}
	
} itemCreationAddresses;




static MesHandle mesItemCreationText;
static MesHandle mesItemCreationRules;
static MesHandle mesItemCreationNamesText;
static ImgFile *background = nullptr;
//static ButtonStateTextures acceptBtnTextures;
//static ButtonStateTextures declineBtnTextures;
static int disabledBtnTexture;

class ItemCreationHooks : public TempleFix {
public:
	

	static void HookedGetLineForMaaAppend(MesHandle, MesLine*); // ensures the crafted item name doesn't overflow

	void apply() override {
		// auto system = UiSystem::getUiSystem("ItemCreation-UI");		
		// system->init = systemInit;
		
		// System Init
		replaceFunction<int(__cdecl)(GameSystemConf&)>(0x10154BA0, [](GameSystemConf& conf) {
			return itemCreation.UiItemCreationInit(conf);
		});


		replaceFunction(0x10150DA0, CraftScrollWandPotionSetItemSpellData);
		
		replaceFunction<int(__cdecl)(objHndl, objHndl)>(0x10152690, [](objHndl crafter, objHndl item) {
			return itemCreation.CreateItemResourceCheck(crafter, item);
		});

		replaceFunction(0x10152930, UiItemCreationCraftingCostTexts);


		// MAA Window Message Handler
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10153110, [](int widId, TigMsg* msg)	{
			return itemCreation.MaaWndMsg(widId, msg);
		});

		// MAA Textbox
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10151890, [](int widId, TigMsg* msg) {
			return itemCreation.MaaTextboxMsg(widId, msg);
		});
		replaceFunction<bool(__cdecl)(int , objHndl)>(0x10151C10, [](int widId, objHndl item)
		{
			return itemCreation.MaaRenderText(widId, item);
		});

		// MAA selected item for crafting
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10152E40, [](int widId, TigMsg* msg) {
			return itemCreation.MaaSelectedItemMsg(widId, msg); }
		);

		// MAA Effect "buttons"
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10153250, [](int widId, TigMsg* msg) {
			return itemCreation.MaaEffectMsg(widId, msg); }
		);
		replaceFunction<void(__cdecl)(int)>(0x10153990, [](int widId) {
			return itemCreation.MaaEffectRender(widId); }
		);
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10152ED0, [](int widId, TigMsg* msg) {
			return itemCreation.MaaEffectAddMsg(widId, msg);
		});
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10152FE0, [](int widId, TigMsg* msg) {
			return itemCreation.MaaEffectRemoveMsg(widId, msg);
		});
		replaceFunction<bool(__cdecl)(int, TigMsg*)>(0x10151EF0, [](int widId, TigMsg* msg) {
			return itemCreation.MaaAppliedBtnMsg(widId, msg);
		});

		// CreateBtnMsg
		replaceFunction<bool(int, TigMsg * )>(0x10153F60, [](int widId, TigMsg* msg){
			return itemCreation.CreateBtnMsg(widId, msg);
		});

		// CancelBtnMsg
		replaceFunction<bool(int, TigMsg *)>(0x10153820, [](int widId, TigMsg* msg) {
			return itemCreation.CancelBtnMsg(widId, msg);
		});

		
		/*auto writeval = &UiItemCreationInit;
		write(0x102F6C10 + 9 * 4 * 26 + 4, &writeval, sizeof(void*));*/

		redirectCall(0x1015221B, HookedGetLineForMaaAppend);
		replaceFunction<void(_cdecl)(char)>(0x10150C10, [](char newChar) {
			auto craftedName = temple::GetPointer<char>(0x10BED758);
			auto& craftedNameCurPos = temple::GetRef<int>(0x10BECE7C);
			craftedName[63] = 0; // ensure string termination


			auto currStrLen = strlen(craftedName) + 1;
			if (currStrLen < 62) {
				for (int i = craftedNameCurPos; i < currStrLen; currStrLen--)
				{
					craftedName[currStrLen] = craftedName[currStrLen - 1];
				}
				craftedName[craftedNameCurPos] = newChar;
				craftedNameCurPos++;
			}


		});
	}

} itemCreationHooks;



int CraftedWandSpellLevel(objHndl objHndItem)
{
	auto spellData = objSystem->GetObject(objHndItem)->GetSpell(obj_f_item_spell_idx, 0);
	uint32_t spellLevelBasic = spellData.spellLevel;
	uint32_t spellLevelFinal = spellData.spellLevel;


	int casterLevelSet = (int) d20Sys.d20QueryReturnData(*itemCreationAddresses.crafter, DK_QUE_Craft_Wand_Spell_Level, 0, 0);
	casterLevelSet = 2 * ((casterLevelSet + 1) / 2) - 1;
	if (casterLevelSet < 1)
		casterLevelSet = 1;

	int slotLevelSet = 1 + (casterLevelSet -1)/ 2;
	if (spellLevelBasic == 0 && casterLevelSet <= 1)
		slotLevelSet = 0;
		
	

	// get data from caster - make this optional!

	uint32_t classCodes[SPELL_ENUM_MAX] = { 0, };
	uint32_t spellLevels[SPELL_ENUM_MAX] = { 0, };
	uint32_t spellFoundNum = 0;
	int casterKnowsSpell = spellSys.spellKnownQueryGetData(*itemCreationAddresses.crafter, spellData.spellEnum, classCodes, spellLevels, &spellFoundNum);
	if (casterKnowsSpell){
		uint32_t spellClassFinal = classCodes[0];
		spellLevelFinal = 0;
		uint32_t isClassSpell = classCodes[0] & (0x80);

		if (isClassSpell){
			spellLevelFinal = spellSys.GetMaxSpellSlotLevel(*itemCreationAddresses.crafter, static_cast<Stat>(classCodes[0] & 0x7F), 0);
		};
		if (spellFoundNum > 1){
			for (uint32_t i = 1; i < spellFoundNum; i++){
				if (spellLevels[i] > spellLevelFinal){
					spellData.classCode = classCodes[i];
					spellLevelFinal = spellLevels[i];
				}
			}
			spellData.spellLevel = spellLevelFinal;

		}
		spellData.spellLevel = spellLevelFinal; // that's the max possible at this point
		if (slotLevelSet && slotLevelSet <= spellLevelFinal && slotLevelSet >= spellLevelBasic)
			spellData.spellLevel = slotLevelSet;
		else if (slotLevelSet  > spellLevelFinal)
			spellData.spellLevel = spellLevelFinal;
		else if (slotLevelSet < spellLevelBasic)
			spellData.spellLevel = spellLevelBasic;
		else if (spellLevelBasic == 0)
		{
			spellData.spellLevel = spellLevelBasic;
		} 

		//templeFuncs.Obj_Set_IdxField_byPtr(objHndItem, obj_f_item_spell_idx, 0, &spellData);
		spellLevelFinal = spellData.spellLevel;

	}
	return spellLevelFinal;
}

int CraftedWandCasterLevel(objHndl item)
{
	int result = CraftedWandSpellLevel(item);
	if (result <= 1)
		return 1;
	return (result * 2) - 1;
}

int32_t ItemCreation::CreateItemResourceCheck(objHndl obj, objHndl objHndItem){
	bool canCraft = 1;
	bool xpCheck = 0;
	int32_t * insuffXp = itemCreationAddresses.craftInsufficientXP;
	int32_t * insuffCp = itemCreationAddresses.craftInsufficientFunds;
	int32_t *insuffSkill = itemCreationAddresses.craftSkillReqNotMet;
	int32_t *insuffPrereqs = itemCreationAddresses.dword_10BEE3B0;
	//uint32_t crafterLevel = objects.StatLevelGet(obj, stat_level);
	//uint32_t crafterXP = objSystem->GetObject(obj)->GetInt32(obj_f_critter_experience);
	uint32_t surplusXP = d20LevelSys.GetSurplusXp(obj);
	uint32_t craftingCostCP = 0;
	uint32_t partyMoney = party.GetMoney();

	*insuffXp = 0;
	*insuffCp = 0;
	*insuffSkill = 0;
	*insuffPrereqs = 0;
	int itemWorth = objects.getInt32(objHndItem, obj_f_item_worth);

	auto itemCreationType = *itemCreationAddresses.itemCreationType;
	// Check GP Section
	if (itemCreationType == ItemCreationType::CraftMagicArmsAndArmor){
		craftingCostCP = MaaCpCost( CRAFT_EFFECT_INVALID );
	}
	else
	{
		// current method for crafting stuff:
		craftingCostCP =  itemWorth / 2;

		// TODO: create new function
		if (itemCreationType == ItemCreationType::CraftWand)
		{

			itemWorth = ItemWorthAdjustedForCasterLevel(objHndItem, CraftedWandCasterLevel(objHndItem));
			craftingCostCP = itemWorth / 2;
		}
			
	};

	if ( ( (uint32_t)partyMoney ) < craftingCostCP){
		*insuffCp = 1;
		canCraft = 0;
	};


	// Check XP section (and maybe spell prerequisites too? explore sub_10152280)
	if ( itemCreationType != CraftMagicArmsAndArmor){
		// check requirements from rules\\item_creation.mes
		if ( temple::GetRef<int(__cdecl)(objHndl, objHndl)>(0x10152280)(obj, objHndItem) == 0){ 
			*insuffPrereqs = 1;
			canCraft = 0;
		};
		// check XP
		// TODO make XP cost calculation take applied caster level into account
		uint32_t itemXPCost = itemWorth / 2500;
		xpCheck = surplusXP >= itemXPCost;
	} else 
	{
		uint32_t magicArmsAndArmorXPCost = MaaXpCost(CRAFT_EFFECT_INVALID);
		xpCheck = surplusXP >= magicArmsAndArmorXPCost;
	}
		
	if (xpCheck){
		return canCraft;
	} else
	{
		*insuffXp = 1;
		return 0;
	};

}

const char* ItemCreation::GetItemCreationMesLine(int lineId){
	MesLine line;
	line.key = lineId;

	mesFuncs.GetLine_Safe(mItemCreationMes, &line);
	return line.value;
}

char const* ItemCreation::ItemCreationGetItemName(objHndl itemHandle) const
{
	if (!itemHandle)
		return nullptr;
	auto itemObj = gameSystems->GetObj().GetObject(itemHandle);

	int protoNum = 0;
	if (itemObj->IsProto()) {
		protoNum = itemObj->id.GetPrototypeId();
	}
	else
		protoNum = itemObj->protoId.GetPrototypeId();

	auto itemCreationNames = temple::GetRef<MesHandle>(0x10BEDB4C);
	MesLine line;
	line.key = protoNum;
	if (mesFuncs.GetLine(itemCreationNames, &line))
		return line.value;
	else
		return description.getDisplayName(itemHandle, itemHandle);
}

objHndl ItemCreation::MaaGetItemHandle(){
	if (craftingItemIdx < 0 || craftingItemIdx >= numItemsCrafting[itemCreationType])
		return 0i64;
	return craftedItemHandles[itemCreationType][craftingItemIdx];
}

bool ItemCreation::IsWeaponBonus(int effIdx)
{
	if (effIdx < 0)
		return false;

	if ( (itemEnhSpecs[effIdx].flags & IESF_PLUS_BONUS) 
		&& (itemEnhSpecs[effIdx].flags & IESF_WEAPON)){
		return true;
	}
	return false;
}

bool ItemCreation::ItemEnhancementIsApplicable(int effIdx)
{
	auto itEnh = itemEnhSpecs[effIdx];
	if (!(itEnh.flags & IESF_ENABLED))
		return false;

	if (craftingItemIdx >= 0 && craftingItemIdx < numItemsCrafting[itemCreationType])
	{
		auto itemHandle = craftedItemHandles[itemCreationType][craftingItemIdx];
		if (itemHandle){
			auto itemObj = gameSystems->GetObj().GetObject(itemHandle);
			if (itemObj->type == obj_t_weapon && !(itEnh.flags & IESF_WEAPON))
				return false;
			if (itemObj->type == obj_t_armor)
			{
				auto armorFlags = itemObj->GetInt32(obj_f_armor_flags);
				auto armorType = inventory.GetArmorType(armorFlags);
				if (armorType == ArmorType::ARMOR_TYPE_SHIELD)	{

					if (!(itEnh.flags & IESF_SHIELD))
						return false;
				} 
				else{

					if (!(itEnh.flags & IESF_ARMOR))
						return false;
				}
			}
		}
	}


	return true;
}

int ItemCreation::GetEffIdxFromWidgetIdx(int widIdx){

	// auto scrollbar2Y = temple::GetRef<int>(0x10BECDA8);
	auto adjIdx = mMaaApplicableEffectsScrollbarY + widIdx; // this is the overall index for the effect
	auto validCount = 0;
	for (auto it: itemEnhSpecs){
		if (ItemEnhancementIsApplicable(it.first))
		{
			if (validCount == adjIdx)
			{
				return it.first;
			}
			validCount++;
		}
			
	}
	return CRAFT_EFFECT_INVALID;
}

int ItemCreation::HasPlusBonus(int effIdx)
{
	auto itEnh = itemEnhSpecs[effIdx];
	if (itEnh.data.enhBonus == 1 && (itEnh.flags & IESF_PLUS_BONUS) ) {
		return TRUE;	
	}
	for (auto it : appliedBonusIndices)
	{
		itEnh = itemEnhSpecs[it];
		if ((itEnh.flags & IESF_PLUS_BONUS)
			&& itEnh.data.enhBonus >= 1 )
			return TRUE;
	}

	return 0;
}

bool ItemCreation::ItemWielderCondsContainEffect(int effIdx, objHndl item)
{
	if (effIdx == CRAFT_EFFECT_INVALID)
		return false;

	auto itemObj = gameSystems->GetObj().GetObject(item);

	auto condId = ElfHash::Hash(itemEnhSpecs[effIdx].condName);
	auto condArray = itemObj->GetInt32Array(obj_f_item_pad_wielder_condition_array);

	if (condArray.GetSize() <= 0)
		return false;


	if (!IsWeaponBonus(effIdx)){  // a +x weapon bonus

		for (int i = 0; i < condArray.GetSize(); i++)
		{
			if (condArray[i] == condId)
			{
				if (itemEnhSpecs[i].flags & IESF_PLUS_BONUS){
					return itemEnhSpecs[i].data.enhBonus <= inventory.GetItemWieldCondArg(item, condId, 0);
				}
				else
					return true;
			}	
		}

		return false;
	}

	// else, do the weapon +x bonus

	auto toHitBonusId = ElfHash::Hash("To Hit Bonus");
	auto damageBonusId = ElfHash::Hash("Damage Bonus");
	auto damBonus = 0;
	auto toHitBonus = 0;

	for (int i = 0; i < condArray.GetSize();i++)
	{
		auto wielderCondId = condArray[i];
		if (wielderCondId == condId){
			return itemEnhSpecs[effIdx].data.enhBonus <= inventory.GetItemWieldCondArg(item, condId, 0);
		}

		if (wielderCondId == toHitBonusId)
		{
			toHitBonus = inventory.GetItemWieldCondArg(item, toHitBonusId, 0);
			if (toHitBonus == damBonus)
				return itemEnhSpecs[effIdx].data.enhBonus <= toHitBonus;
		}

		if (wielderCondId == damageBonusId)
		{
			damBonus = inventory.GetItemWieldCondArg(item, toHitBonusId, 0);
			if (toHitBonus == damBonus)
			{
				return itemEnhSpecs[effIdx].data.enhBonus <= toHitBonus;
			}
		}
	}

	return false;
};

void CraftScrollWandPotionSetItemSpellData(objHndl objHndItem, objHndl objHndCrafter){

	// the new and improved Wands/Scroll Property Setting Function

	auto obj = objSystem->GetObject(objHndItem);
	auto itemCreationType = *itemCreationAddresses.itemCreationType;

	if (itemCreationType == CraftWand){
		
		auto spellData = obj->GetSpell(obj_f_item_spell_idx, 0);
		uint32_t spellLevelBasic = spellData.spellLevel;
		uint32_t spellLevelFinal = CraftedWandSpellLevel(objHndItem);
		spellData.spellLevel = spellLevelFinal;				
		obj->SetSpell(obj_f_item_spell_idx, 0, spellData);
		
		auto args = PyTuple_New(3);
			
		int casterLevelFinal = spellLevelFinal * 2 - 1;
		if (casterLevelFinal < 1)
			casterLevelFinal = 1;
		PyTuple_SET_ITEM(args, 0, PyObjHndl_Create(objHndItem));
		PyTuple_SET_ITEM(args, 1, PyObjHndl_Create(objHndCrafter));
		PyTuple_SET_ITEM(args, 2, PyInt_FromLong(casterLevelFinal));

		auto wandNameObj = pythonObjIntegration.ExecuteScript("crafting", "craft_wand_new_name", args);
		char * wandNewName = PyString_AsString(wandNameObj);
			
		objects.setInt32(objHndItem, obj_f_description, objects.description.CustomNameNew(wandNewName)); // TODO: create function that appends effect of caster level boost			
		objects.setInt32(objHndItem, obj_f_item_spell_charges_idx, 50); // as god intended it to be!
		Py_DECREF(wandNameObj);
		Py_DECREF(args);
		
		return;

	}
	if (itemCreationType == ScribeScroll){
		// do scroll specific stuff
		// templeFuncs.Obj_Set_Field_32bit(objHndItem, obj_f_description, templeFuncs.CustomNameNew("Scroll of LOL"));
	};

	if (itemCreationType == BrewPotion){
		// do potion specific stuff
		// templeFuncs.Obj_Set_Field_32bit(objHndItem, obj_f_description, templeFuncs.CustomNameNew("Potion of Commotion"));
		// TODO: change it so it's 0xBAAD F00D just like spawned / mobbed potions
	};
	
	auto numItemSpells = obj->GetSpellArray(obj_f_item_spell_idx).GetSize();

	// loop thru the item's spells (can be more than one in principle, like Keoghthem's Ointment)

	// Current code - change this at will...
	for (int n = 0; n < numItemSpells; n++){
		auto spellData = obj->GetSpell(obj_f_item_spell_idx, n);

		// get data from caster - make this optional!

		uint32_t classCodes[SPELL_ENUM_MAX] = { 0, };
		uint32_t spellLevels[SPELL_ENUM_MAX] = { 0, };
		uint32_t spellFoundNum = 0;
		int casterKnowsSpell = spellSys.spellKnownQueryGetData(objHndCrafter, spellData.spellEnum, classCodes, spellLevels, &spellFoundNum);
		if (casterKnowsSpell){
			uint32_t spellClassFinal = classCodes[0];
			uint32_t spellLevelFinal = 0;
			uint32_t isClassSpell = classCodes[0] & (0x80);

			if (isClassSpell){
				spellLevelFinal = spellSys.GetMaxSpellSlotLevel(objHndCrafter, static_cast<Stat>(classCodes[0] & 0x7F), 0);
				spellData.classCode = spellClassFinal;
			};
			if (spellFoundNum > 1){
				for (uint32_t i = 1; i < spellFoundNum; i++){
					if (spellLevels[i] > spellLevelFinal){
						spellData.classCode = classCodes[i];
						spellLevelFinal = spellLevels[i];
					}
				}
				spellData.spellLevel = spellLevelFinal;

			}
			spellData.spellLevel = spellLevelFinal;
			obj->SetSpell(obj_f_item_spell_idx, n, spellData);

		}

	}
};


void ItemCreation::CreateItemDebitXPGP(objHndl crafter, objHndl objHndItem){
	uint32_t crafterXP = objects.getInt32(crafter, obj_f_critter_experience);
	uint32_t craftingCostCP = 0;
	uint32_t craftingCostXP = 0;

	auto itemCreationType = *itemCreationAddresses.itemCreationType;

	if (itemCreationType == CraftMagicArmsAndArmor){ // magic arms and armor
		craftingCostCP = MaaCpCost(CRAFT_EFFECT_INVALID);
		craftingCostXP = MaaXpCost(CRAFT_EFFECT_INVALID);
	}
	else
	{
		// TODO make crafting costs take applied caster level into account
		// currently this is what ToEE does	
		int itemWorth;
		if (itemCreationType == ItemCreationType::CraftWand)
			itemWorth = ItemWorthAdjustedForCasterLevel(objHndItem, CraftedWandCasterLevel(objHndItem));
		else
			itemWorth = objects.getInt32(objHndItem, obj_f_item_worth);
		craftingCostCP = itemWorth / 2;
		craftingCostXP = itemWorth / 2500;

	};

	templeFuncs.DebitPartyMoney(0, 0, 0, craftingCostCP);
	objects.setInt32(crafter, obj_f_critter_experience, crafterXP - craftingCostXP);
};

void __cdecl UiItemCreationCraftingCostTexts(objHndl objHndItem){
	// prolog
	int32_t widgetId;
	int32_t * insuffXp;
	int32_t * insuffCp;
	int32_t *insuffSkill;
	int32_t *insuffPrereq;
	uint32_t craftingCostCP;
	uint32_t craftingCostXP;
	TigRect rect;
	char * prereqString;
	__asm{
		mov widgetId, ebx; // widgetId is passed in ebx
	};


	uint32_t casterLevelNew = -1; // h4x!
	
	auto itemCreationType = *itemCreationAddresses.itemCreationType;

	if (itemCreationType == CraftWand)
	{
		casterLevelNew = CraftedWandCasterLevel(objHndItem);
	}
	

	rect.x = 212;
	rect.y = 157;
	rect.width = 159;
	rect.height = 10;

	insuffXp = itemCreationAddresses.craftInsufficientXP;
	insuffCp = itemCreationAddresses.craftInsufficientFunds;
	insuffSkill = itemCreationAddresses.craftSkillReqNotMet;
	insuffPrereq = itemCreationAddresses.dword_10BEE3B0;

	//old method
	/* 
	craftingCostCP = templeFuncs.Obj_Get_Field_32bit(objHndItem, obj_f_item_worth) / 2;
	craftingCostXP = templeFuncs.Obj_Get_Field_32bit(objHndItem, obj_f_item_worth) / 2500;
	*/

	craftingCostCP = ItemWorthAdjustedForCasterLevel(objHndItem, casterLevelNew) / 2;
	craftingCostXP = ItemWorthAdjustedForCasterLevel(objHndItem, casterLevelNew) / 2500;
	
	string text;
	// "Item Cost: %d"
	if (*insuffXp || *insuffCp || *insuffSkill || *insuffPrereq){
		
		text = format("{} @{}{}", *itemCreationAddresses.itemCreationUIStringItemCost, *(insuffCp)+1, craftingCostCP / 100);
	} else {
		//_snprintf(text, 128, "%s @3%d", *itemCreationAddresses.itemCreationUIStringItemCost, craftingCostCP / 100);
		text = format("{} @3{}", *itemCreationAddresses.itemCreationUIStringItemCost, craftingCostCP / 100);
	};


	UiRenderer::DrawTextInWidget(widgetId, text, rect, *itemCreationAddresses.itemCreationTextStyle);
	rect.y += 11;



	// "Experience Cost: %d"  (or "Skill Req: " for alchemy - WIP)

	if (itemCreationType == IC_Alchemy){ 
		// placeholder - they do similar bullshit in the code :P but I guess it can be modified easily enough!
		if (*insuffXp || *insuffCp || *insuffSkill || *insuffPrereq){
			text = format("{} @{}{}", *itemCreationAddresses.itemCreationUIStringSkillRequired, *insuffSkill + 1, craftingCostXP);
		}
		else {
			text = format("{} @3{}", *itemCreationAddresses.itemCreationUIStringSkillRequired, craftingCostXP);
		};
	}
	else
	{
		if (*insuffXp || *insuffCp || *insuffSkill || *insuffPrereq){
			text = format("{} @{}{}", *itemCreationAddresses.itemCreationUIStringXPCost, *(insuffXp) + 1, craftingCostXP);
		}
		else {
			text = format("{} @3{}", *itemCreationAddresses.itemCreationUIStringXPCost, craftingCostXP);
		};
	};

	UiRenderer::DrawTextInWidget(widgetId, text, rect, *itemCreationAddresses.itemCreationTextStyle);
	rect.y += 11;

	// "Value: %d"
	//_snprintf(text, 128, "%s @1%d", * (itemCreationUIStringValue.ptr() ), templeFuncs.Obj_Get_Field_32bit(objHndItem, obj_f_item_worth) / 100);
	text = format("{} @1{}", *itemCreationAddresses.itemCreationUIStringValue, ItemWorthAdjustedForCasterLevel(objHndItem, casterLevelNew) / 100);

	UiRenderer::DrawTextInWidget(widgetId, text, rect, *itemCreationAddresses.itemCreationTextStyle2);

	// Prereq: %s
	rect.x = 210;
	rect.y = 200;
	rect.width = 150;
	rect.height = 105;
	prereqString = templeFuncs.ItemCreationPrereqSthg_sub_101525B0(*itemCreationAddresses.crafter, objHndItem);
	if (prereqString){
		UiRenderer::DrawTextInWidget(widgetId, prereqString, rect, *itemCreationAddresses.itemCreationTextStyle);
	}
	
	if (itemCreationType == ItemCreationType::CraftWand)
	{
		rect.x = 210;
		rect.y = 250;
		rect.width = 150;
		rect.height = 105;
		char asdf[1000];
		sprintf(asdf, "Crafted Caster Level: " );
		
		text = format("{} @3{}", asdf, casterLevelNew);
		if (prereqString){
			UiRenderer::DrawTextInWidget(widgetId, text, rect, *itemCreationAddresses.itemCreationTextStyle);
		}
	}
	
	
};


void ItemCreation::GetMaaSpecs()
{
	itemCreationType = temple::GetRef<int>(0x10BEDF50);
	itemCreationCrafter = temple::GetRef<objHndl>(0x10BECEE0);
	craftingItemIdx = temple::GetRef<int>(0x10BEE398);
	mItemCreationMes = temple::GetRef<MesHandle>(0x10BEDFD0);
	
	mCreateBtnId = temple::GetRef<int>(0x10BED8B0);

	//MAA
	maaSelectedEffIdx = temple::GetRef<int>(0x10BECD74);
	mMaaWndId = temple::GetRef<int>(0x10BEDA64);
	mMaaWnd = temple::GetPointer<WidgetType1>(0x10BEDB58);
	mMaaCraftableItemsScrollbarId = temple::GetRef<int>(0x10BED8A0);
	mMaaApplicableEffectsScrollbarId = temple::GetRef<int>(0x10BECD78);

	// Normal Item Creation
	mItemCreationWnd = temple::GetPointer<WidgetType1>(0x10BEE040);
	mItemCreationWndId = temple::GetRef<int>(0x10BEDA60);
	mItemCreationScrollbarId = temple::GetRef<int>(0x10BED9F4);
	mItemCreationScrollbarY = temple::GetRef<int>(0x10BECDA4);
	
	
	memcpy(maaBtnIds, temple::GetPointer<int>(0x10BECD7C), sizeof maaBtnIds);

	struct MaaSpecTabEntry
	{
		char * id;
		char * condName;
		char * flags;
		char * effBonus;
		char * enhBonus;
		char * classReq; // class req
		char * charReqs; // Character Level, Alignment
		char * spellReqs;
		char * featReqs; // TODO

	};
	auto maaSpecLineParser = [](const TigTabParser*, int lineIdx, char ** cols)
	{
		auto& tabEntry = *reinterpret_cast<MaaSpecTabEntry*>(cols);
//		auto& tabEntry = *_tabEntry;
		

		auto effIdx = atol(tabEntry.id);
		auto condName = tabEntry.condName;
		
		
		// get flags
		uint32_t flags = 0;
		{
			StringTokenizer flagTok(tabEntry.flags);
			while (flagTok.next()) {
				auto& tok = flagTok.token();
				if (tok.type != StringTokenType::Identifier)
					continue;
				
				auto flagLookup = ItemEnhSpecFlagDict.find(tok.text);
				if (flagLookup != ItemEnhSpecFlagDict.end()){
					flags |= flagLookup->second;
				}
				
				
			}
		}
		
		
		auto effBonus = atol(tabEntry.effBonus);
		auto enhBonus = atol(tabEntry.enhBonus);

		itemCreation.itemEnhSpecs[effIdx] = ItemEnhancementSpec(condName, flags, effBonus, enhBonus);

		// get class req
		if (tabEntry.classReq)
		{
			// TODO (right now only Weapon Ki Focus uses it and it's not enabled anyway)
		}

		// get charReqs
		if (tabEntry.charReqs)
		{
			StringTokenizer charReqTok(tabEntry.charReqs);
			while (charReqTok.next())
			{
				auto& tok = charReqTok.token();
				if (tok.type != StringTokenType::Identifier)
					continue;
				if (toupper(tok.text[0]) == 'A')
				{
					if (!_stricmp(&tok.text[1], "good"))
						itemCreation.itemEnhSpecs[effIdx].reqs.alignment = Alignment::ALIGNMENT_GOOD;
					else if (!_stricmp(&tok.text[1], "lawful"))
						itemCreation.itemEnhSpecs[effIdx].reqs.alignment = Alignment::ALIGNMENT_LAWFUL;
					else if (!_stricmp(&tok.text[1], "evil"))
						itemCreation.itemEnhSpecs[effIdx].reqs.alignment = Alignment::ALIGNMENT_EVIL;
					else if (!_stricmp(&tok.text[1], "chaotic"))
						itemCreation.itemEnhSpecs[effIdx].reqs.alignment = Alignment::ALIGNMENT_CHAOTIC;
				} 
				else if (toupper(tok.text[0]) == 'C')
				{
					itemCreation.itemEnhSpecs[effIdx].reqs.minLevel = atol(&tok.text[1]);
				}
				
			}
		}

		// get spellReqs
		if (tabEntry.spellReqs)
		{
			StringTokenizer spellReqTok(tabEntry.spellReqs);
			while (spellReqTok.next())
			{
				auto& tok = spellReqTok.token();
				if (tok.type != StringTokenType::QuotedString)
					continue;
				auto spellEnum = spellSys.getSpellEnum(tok.text);
				if (spellEnum){
					itemCreation.itemEnhSpecs[effIdx].reqs.spells[0].push_back(spellEnum);
				}
				// TODO: implement AND/OR support (currently just Brilliant Radiance has this, not yet implemented anyway)
			}
		}
		
		return 0;
	};

	TigTabParser maaSpecsTab;
	maaSpecsTab.Init(maaSpecLineParser);
	maaSpecsTab.Open("tprules\\craft_maa_specs.tab");
	maaSpecsTab.Process();
	maaSpecsTab.Close();
}

uint32_t ItemWorthAdjustedForCasterLevel(objHndl objHndItem, uint32_t casterLevelNew){
	auto obj = objSystem->GetObject(objHndItem);
	auto numItemSpells = obj->GetSpellArray(obj_f_item_spell_idx).GetSize();
	auto itemWorthBase = obj->GetInt32(obj_f_item_worth);
	if (casterLevelNew == -1){
		return itemWorthBase;
	}

	uint32_t itemSlotLevelBase = 0;

	// loop thru the item's spells (can be more than one in principle, like Keoghthem's Ointment)

	for (uint32_t n = 0; n < numItemSpells; n++){
		auto spellData = obj->GetSpell(obj_f_item_spell_idx, n);
		if (spellData.spellLevel > itemSlotLevelBase){
			itemSlotLevelBase = spellData.spellLevel;
		}
	};

	int casterLevelOld = itemSlotLevelBase * 2 - 1;
	if (casterLevelOld < 1)
		casterLevelOld = 1;

	if (itemSlotLevelBase == 0 && casterLevelNew > casterLevelOld){
		return itemWorthBase * casterLevelNew;
	}
	if (casterLevelNew > casterLevelOld)
	{
		return itemWorthBase * casterLevelNew / casterLevelOld;
	}
	return itemWorthBase;

	
}

static vector<uint64_t> craftingProtoHandles[8];

const char *getProtoName(uint64_t protoHandle) {
	/*
	 // gets item creation proto id
  if ( sub_1009C950((objHndl)protoHandle) )
    v1 = sub_100392E0(protoHandle);
  else
    v1 = sub_10039320((objHndl)protoHandle);

  line.key = v1;
  if ( tig_mes_get_line(ui_itemcreation_names, &line) )
    result = line.value;
  else
    result = objects.description._getDisplayName((objHndl)protoHandle, (objHndl)protoHandle);
  return result;
  */

	return objects.description._getDisplayName(protoHandle, protoHandle);
}

static void loadProtoIds(MesHandle mesHandle) {

	for (uint32_t i = 0; i < 8; ++i) {
		auto protoLine = mesFuncs.GetLineById(mesHandle, i);
		if (!protoLine) {
			continue;
		}

		auto &protoHandles = craftingProtoHandles[i];

		StringTokenizer tokenizer(protoLine);
		while (tokenizer.next()) {
			auto handle = templeFuncs.GetProtoHandle(tokenizer.token().numberInt);
			protoHandles.push_back(handle);
		}

		// Sort by prototype name
		sort(protoHandles.begin(), protoHandles.end(), [](uint64_t a, uint64_t b)
		{
			auto nameA = getProtoName(a);
			auto nameB = getProtoName(b);
			return _strcmpi(nameA, nameB);
		});
		logger->info("Loaded {} prototypes for crafting type {}", craftingProtoHandles[i].size(), i);
	}
	
}




uint32_t ItemCreationBuildRadialMenuEntry(DispatcherCallbackArgs args, ItemCreationType itemCreationType, char* helpSystemString, MesHandle combatMesLine)
{
	if (combatSys.isCombatActive()) { return 0; }
	MesLine mesLine;
	RadialMenuEntryAction radEntry(combatMesLine, D20A_ITEM_CREATION, itemCreationType, helpSystemString);
	radEntry.AddChildToStandard(args.objHndCaller, RadialMenuStandardNode::Feats);
	
	return 0;
};

#pragma region ItemCreation Radial Menu Dispatcher Callbacks

uint32_t BrewPotionRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, BrewPotion, "TAG_BREW_POTION", 5066);
};

uint32_t ScribeScrollRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, ScribeScroll, "TAG_SCRIBE_SCROLL", 5067);
};

int CraftWandRadialMenu(DispatcherCallbackArgs args)
{
	
	if (combatSys.isCombatActive()) { return 0; }
	MesLine mesLine;
	RadialMenuEntry radMenuCraftWand;
	mesLine.key = 5068;
	mesFuncs.GetLine_Safe(*combatSys.combatMesfileIdx, &mesLine);
	radMenuCraftWand.text = (char*)mesLine.value;
	radMenuCraftWand.d20ActionType = D20A_ITEM_CREATION;
	radMenuCraftWand.d20ActionData1 = CraftWand;
	radMenuCraftWand.helpId = ElfHash::Hash("TAG_CRAFT_WAND");
	
	int newParent = radialMenus.AddParentChildNode(args.objHndCaller, &radMenuCraftWand, radialMenus.GetStandardNode(RadialMenuStandardNode::Feats));

	RadialMenuEntry useCraftWand;
	RadialMenuEntry setWandLevel;
	
	setWandLevel.minArg = 1;
	setWandLevel.maxArg = min(20, critterSys.GetCasterLevel(args.objHndCaller));
	
	setWandLevel.field4 = (int)combatSys.GetCombatMesLine(6019);
	setWandLevel.type = RadialMenuEntryType::Slider;
	setWandLevel.actualArg = (int)conds.CondNodeGetArgPtr(args.subDispNode->condNode, 0);
	setWandLevel.callback = (BOOL (__cdecl*)(objHndl, RadialMenuEntry*))itemCreationAddresses.Sub_100F0200;
	setWandLevel.text = combatSys.GetCombatMesLine(6017);
	setWandLevel.helpId = ElfHash::Hash("TAG_CRAFT_WAND");
	radialMenus.AddChildNode(args.objHndCaller, &setWandLevel, newParent);

	useCraftWand.type = RadialMenuEntryType::Action;
	useCraftWand.text = combatSys.GetCombatMesLine(6018);
	useCraftWand.helpId = ElfHash::Hash("TAG_CRAFT_WAND");
	useCraftWand.d20ActionType = D20A_ITEM_CREATION;
	useCraftWand.d20ActionData1 = CraftWand;
	radialMenus.AddChildNode(args.objHndCaller, &useCraftWand, newParent);



	return 0;
	
	//return ItemCreationBuildRadialMenuEntry(args, CraftWand, "TAG_CRAFT_WAND", 5068);
};

int CraftWandOnAdd(DispatcherCallbackArgs args)
{
	//vector< int> condArgs(2);
	//condArgs[0] = 1;
	//condArgs[1] = 0;
	conds.AddTo(args.objHndCaller, "Craft Wand Level Set", { 1, 0 });
	//Dispatcher * dispatcher = objects.GetDispatcher(args.objHndCaller);
	//CondStruct * condStruct = conds.hashmethods.GetCondStruct(conds.hashmethods.StringHash("Craft Wand Level Set") );
	//conds.ConditionAddDispatchArgs(dispatcher, &dispatcher->conditions, condStruct, condArgs);
	return 0;
}

uint32_t CraftRodRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, CraftRod, "TAG_CRAFT_ROD", 5069);
};

uint32_t CraftWondrousRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, CraftWondrous, "TAG_CRAFT_WONDROUS", 5070);
};

uint32_t CraftStaffRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, CraftStaff, "TAG_CRAFT_STAFF", 5103);
};

uint32_t ForgeRingRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, ForgeRing, "TAG_FORGE_RING", 5104);
};

uint32_t CraftMagicArmsAndArmorRadialMenu(DispatcherCallbackArgs args)
{
	return ItemCreationBuildRadialMenuEntry(args, CraftMagicArmsAndArmor, "TAG_CRAFT_MAA", 5071);
};

#pragma endregion
/*
static int __cdecl systemInit(const GameSystemConf *conf) {

	mesFuncs.Open("mes\\item_creation.mes", &mesItemCreationText);
	mesFuncs.Open("mes\\item_creation_names.mes", &mesItemCreationNamesText);
	mesFuncs.Open("rules\\item_creation.mes", &mesItemCreationRules);
	loadProtoIds(mesItemCreationRules);

	acceptBtnTextures.loadAccept();
	declineBtnTextures.loadDecline();
	ui.GetAsset(UiAssetType::Generic, UiGenericAsset::DisabledNormal, disabledBtnTexture);

	background = ui.LoadImg("art\\interface\\item_creation_ui\\item_creation.img");

	// TODO !sub_10150F00("rules\\item_creation.mes")

	/*
	tig_texture_register("art\\interface\\item_creation_ui\\craftarms_0.tga", &dword_10BEE38C)
    || tig_texture_register("art\\interface\\item_creation_ui\\craftarms_1.tga", &dword_10BECEE8)
    || tig_texture_register("art\\interface\\item_creation_ui\\craftarms_2.tga", &dword_10BED988)
    || tig_texture_register("art\\interface\\item_creation_ui\\craftarms_3.tga", &dword_10BECEEC)
    || tig_texture_register("art\\interface\\item_creation_ui\\invslot_selected.tga", &dword_10BECDAC)
    || tig_texture_register("art\\interface\\item_creation_ui\\invslot.tga", &dword_10BEE038)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button.tga", &dword_10BEE334)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button_grey.tga", &dword_10BED990)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button_hover.tga", &dword_10BEE2D8)
    || tig_texture_register("art\\interface\\item_creation_ui\\add_button_press.tga", &dword_10BED79C) )
	

	return 0;
}
*/
static void __cdecl systemReset() {
}

static void __cdecl systemExit() {
}


ItemCreation::ItemCreation(){

	for (int i = 0; i < 30; i++) {
		GoldCraftCostVsEffectiveBonus[i] = 1000 * i*i;
		GoldBaseWorthVsEffectiveBonus[i] = GoldCraftCostVsEffectiveBonus[i] * 2;
	}

	craftedItemExistingEffectiveBonus = -1; // stores the crafted item existing (pre-crafting) effective bonus
	//craftingItemIdx = -1;

	memset(numItemsCrafting, 0, sizeof(numItemsCrafting));
	memset(craftedItemHandles, 0, sizeof(craftedItemHandles));
	memset(craftedItemName, 0, sizeof craftedItemName);
	craftedItemNamePos = 0;
	craftingWidgetId = -1;

	InitItemEnhSpecs();
}

int ItemCreation::GetSurplusXp(objHndl crafter){
	auto level = objects.StatLevelGet(crafter, stat_level);
	auto xpReq = d20LevelSys.GetXpRequireForLevel(level);
	return gameSystems->GetObj().GetObject(crafter)->GetInt32(obj_f_critter_experience) - xpReq;
}

void ItemCreation::InitItemEnhSpecs()
{
	//auto idx = 0;
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Enhancement Bonus", IESF_ENABLED | IESF_WEAPON | IESF_PLUS_BONUS, 1, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Enhancement Bonus", IESF_ENABLED | IESF_WEAPON | IESF_PLUS_BONUS, 1, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Enhancement Bonus", IESF_ENABLED | IESF_WEAPON | IESF_PLUS_BONUS, 1, 3);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Defending Bonus", IESF_ENABLED | IESF_WEAPON, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Flaming", IESF_ENABLED | IESF_WEAPON, 1);
	//
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Frost", IESF_ENABLED | IESF_WEAPON, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Shock", IESF_ENABLED | IESF_WEAPON, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Ghost Touch", IESF_WEAPON, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Keen", IESF_ENABLED | IESF_WEAPON | IESF_UNK100, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Mighty Cleaving", IESF_ENABLED | IESF_WEAPON, 1);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Spell Storing", IESF_WEAPON, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Throwing", IESF_WEAPON, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Bane", IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Disruption", IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Flaming Burst", IESF_ENABLED | IESF_WEAPON, 2);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Icy Burst", IESF_ENABLED | IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Shocking Burst", IESF_ENABLED | IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Thundering",  IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Wounding", IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Holy", IESF_ENABLED | IESF_WEAPON, 2);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Unholy", IESF_ENABLED | IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Lawful", IESF_ENABLED | IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Chaotic", IESF_ENABLED | IESF_WEAPON, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Enhancement Bonus", IESF_ENABLED | IESF_WEAPON | IESF_PLUS_BONUS, 1, 4);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Weapon Enhancement Bonus", IESF_ENABLED | IESF_WEAPON | IESF_PLUS_BONUS, 1, 5);

	//// armor bonuses
	//idx = 200;

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Enhancement Bonus", IESF_ENABLED | IESF_ARMOR | IESF_PLUS_BONUS, 1, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Enhancement Bonus", IESF_ENABLED | IESF_ARMOR | IESF_PLUS_BONUS, 1, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Enhancement Bonus", IESF_ENABLED | IESF_ARMOR | IESF_PLUS_BONUS, 1, 3);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Light Fortification", IESF_ARMOR , 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Glamered", IESF_ARMOR, 1);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Slick", IESF_ARMOR, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Shadow", IESF_ARMOR, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Silent Moves", IESF_ENABLED | IESF_ARMOR, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Spell Resistance", IESF_ENABLED | IESF_ARMOR, 2, 13);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Enhancement Bonus", IESF_ENABLED | IESF_ARMOR, 1, 4);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Armor Enhancement Bonus", IESF_ENABLED | IESF_ARMOR, 1, 5);


	//// shield bonuses
	//idx = 400;
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Enhancement Bonus", IESF_ENABLED | IESF_SHIELD | IESF_PLUS_BONUS, 1, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Enhancement Bonus", IESF_ENABLED | IESF_SHIELD | IESF_PLUS_BONUS, 1, 2);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Enhancement Bonus", IESF_ENABLED | IESF_SHIELD | IESF_PLUS_BONUS, 1, 3);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Bashing", IESF_SHIELD, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Blinding", IESF_SHIELD, 1);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Light Fortification", IESF_SHIELD , 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Arrow Deflection", IESF_SHIELD, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Animated", IESF_SHIELD, 1);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Spell Resistance", IESF_ENABLED | IESF_SHIELD, 2, 13);
	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Enhancement Bonus", IESF_ENABLED | IESF_SHIELD | IESF_PLUS_BONUS, 1, 4);

	//itemEnhSpecs[idx++] = ItemEnhancementSpec("Shield Enhancement Bonus", IESF_ENABLED | IESF_SHIELD | IESF_PLUS_BONUS, 1, 5);
}

ItemEnhancementSpec::ItemEnhancementSpec(const char* CondName, uint32_t Flags, int EffcBonus, int enhBonus)
	:condName(CondName),flags(Flags),effectiveBonus(EffcBonus){
	data.enhBonus = enhBonus;
}

BOOL ItemCreation::ItemCreationShow(objHndl crafter, ItemCreationType icType)
{
	if (icType == itemCreationType)
		return TRUE;

	if (itemCreationType <= ItemCreationType::ForgeRing && itemCreationType >= ItemCreationType::IC_Alchemy )
	{
		if (itemCreationResourceCheckResults)
			free(itemCreationResourceCheckResults);
		ui.WidgetSetHidden(mItemCreationWndId, 1);
		ui.WidgetCopy(mItemCreationWndId, mItemCreationWnd);
	} else if (itemCreationType == ItemCreationType::CraftMagicArmsAndArmor)
	{
		ui.WidgetSetHidden(mMaaWndId, 1);
	}

	itemCreationType = icType;
	itemCreationCrafter = crafter;

	if (icType <= ItemCreationType::ForgeRing)
	{
		ButtonStateInit(mItemCreationWndId);
	} else if (icType == ItemCreationType::CraftMagicArmsAndArmor)
	{
		MaaInitCrafter(crafter);
		MaaInitWnd(mMaaWndId);
		return TRUE;
	}

	return TRUE;
}

void ItemCreation::ButtonStateInit(int wndId){
	ui.WidgetSetHidden(wndId, 0);
	ui.WidgetCopy(wndId, mItemCreationWnd);
	itemCreationResourceCheckResults = new bool[numItemsCrafting[itemCreationType]];
	for (int i = 0; i < (int)numItemsCrafting[itemCreationType];i++)
	{
		auto itemHandle = craftedItemHandles[itemCreationType][i];
		if (itemHandle)	{
			itemCreationResourceCheckResults[i] = CreateItemResourceCheck(itemCreationCrafter, itemHandle);
		}
	}

	if (craftingItemIdx >= 0 && craftingItemIdx < (int)numItemsCrafting[itemCreationType]){
		if (CreateItemResourceCheck(itemCreationCrafter, craftedItemHandles[itemCreationType][craftingItemIdx]))
			ui.ButtonSetButtonState(mCreateBtnId, 0);
		else
			ui.ButtonSetButtonState(mCreateBtnId, 4);
	}

	ui.ScrollbarSetYmax(mItemCreationScrollbarId, numItemsCrafting[itemCreationType] - NUM_DISPLAYED_CRAFTABLE_ITEMS_MAX  < 0 ? 0 : numItemsCrafting[itemCreationType] - NUM_DISPLAYED_CRAFTABLE_ITEMS_MAX);
	ui.ScrollbarSetY(mItemCreationScrollbarId, 0);
	temple::GetRef<int>(0x10BEDB54) = 0; // scrollbarCurY
	ui.WidgetBringToFront(wndId);
}

void ItemCreation::MaaInitCraftedItem(objHndl itemHandle){

	craftedItemName[0] = 0;
	craftedItemExistingEffectiveBonus = 0;
	if (!itemHandle){
		appliedBonusIndices.clear();
		return;
	}

	strcpy(craftedItemName, ItemCreationGetItemName(itemHandle));
	for (auto it: itemEnhSpecs)
	{
		if (ItemWielderCondsContainEffect(it.first,itemHandle))	{
			craftedItemExistingEffectiveBonus += it.second.effectiveBonus;
			appliedBonusIndices.push_back(it.first);
		}
	}
}

void ItemCreation::MaaInitCrafter(objHndl crafter){
	mMaaCraftableItemList.clear();
	auto crafterObj = gameSystems->GetObj().GetObject(crafter);
	auto crafterInvenNum = crafterObj->GetInt32(obj_f_critter_inventory_num);
	auto inventoryArr = crafterObj->GetObjectIdArray(obj_f_critter_inventory_list_idx);
	auto itemCanBeEnchantedWithMaa = temple::GetRef<BOOL(__cdecl)(objHndl, objHndl)>(0x1014F190);
	for (int i = 0; i < crafterInvenNum;i++){
		auto itemHandle = inventoryArr[i];
		if (itemCanBeEnchantedWithMaa(crafter, itemHandle))
		{
			mMaaCraftableItemList.push_back(itemHandle);
		}
	}
	MaaInitCraftedItem(craftedItemHandles[itemCreationType][0]);
}

void ItemCreation::MaaInitWnd(int wndId){
	maaSelectedEffIdx = -1;
	mMaaActiveAppliedWidIdx = -1;
	objHndl itemHandle = 0;
	if (craftingItemIdx >= 0 && mCraftingItemIdx < (int)numItemsCrafting[itemCreationType]) {
		itemHandle = craftedItemHandles[itemCreationType][craftingItemIdx];
	}

	MaaInitCraftedItem(itemHandle);
	ui.WidgetSetHidden(wndId, 0);
	ui.WidgetCopy(wndId, mMaaWnd);
	ui.WidgetBringToFront(wndId);
	// auto scrollbarId =  mMaaCraftableItemsScrollbarId;// temple::GetRef<int>(0x10BED8A0);
	ui.ScrollbarSetYmax(mMaaCraftableItemsScrollbarId, mMaaCraftableItemList.size() < 5 ? 0 : mMaaCraftableItemList.size() - 5);
	ui.ScrollbarSetY(mMaaCraftableItemsScrollbarId, 0);
	itemCreationScrollbarY = 0;

	auto numApplicableEffects = 0;
	for (auto it : itemEnhSpecs) {
		if (ItemEnhancementIsApplicable(it.first)) {
			numApplicableEffects++;
		}
	}

	ui.ScrollbarSetYmax(mMaaApplicableEffectsScrollbarId, numApplicableEffects < MAA_EFFECT_BUTTONS_COUNT ? 0 : numApplicableEffects - MAA_EFFECT_BUTTONS_COUNT);
	ui.ScrollbarSetY(mMaaApplicableEffectsScrollbarId, 0);

	mMaaApplicableEffectsScrollbarY = 0;
	craftingItemIdx = -1;

	if (!mMaaCraftableItemList.size()) {
		auto title = combatSys.GetCombatMesLine(6009);
		auto helpId = ElfHash::Hash("TAG_CRAFT_MAGIC_ARMS_ARMOR_POPUP");
		auto popupType0 = temple::GetRef<int(__cdecl)(int, int(__cdecl*)(), const char*)>(0x100E6F10);
		popupType0(helpId, []() { return itemCreation.ItemCreationShow(0, ItemCreationType::Inactive); }, title);
	}
	craftedItemNamePos = strlen(craftedItemName);
}

bool ItemCreation::CreateBtnMsg(int widId, TigMsg* msg)
{
	if (msg->type == TigMsgType::WIDGET && msg->arg2 == 1)
	{
		craftedItemNamePos = strlen(craftedItemName);
		craftingWidgetId = widId;
		if (craftingItemIdx >= 0 )
		{
			auto itemHandle = craftedItemHandles[itemCreationType][craftingItemIdx];
			if ( CreateItemResourceCheck(itemCreationCrafter, itemHandle) )
			{
				CreateItemDebitXPGP(itemCreationCrafter, itemHandle);
				CreateItemFinalize(itemCreationCrafter, itemHandle);
			}
		}
	}
	return false;
}

// Item Creation UI
void ItemCreation::CreateItemFinalize(objHndl crafter, objHndl item){

	auto icType = *itemCreationAddresses.itemCreationType;
	auto effBonus = 0;
	auto crafterObj = gameSystems->GetObj().GetObject(crafter);
	auto altPressed = infrastructure::gKeyboard.IsKeyPressed(VK_LMENU) || infrastructure::gKeyboard.IsKeyPressed(VK_RMENU);

	//auto appliedBonusIndices = temple::GetRef<int[9]>(0x10BED908);

	if (icType == ItemCreationType::CraftMagicArmsAndArmor){

		auto itemObj = gameSystems->GetObj().GetObject(item);
		for (auto it : appliedBonusIndices){
			auto effIdx = it;
			if (effIdx == CRAFT_EFFECT_INVALID)
				continue;

			auto& itEnh = itemEnhSpecs[effIdx];

			effBonus += itEnh.effectiveBonus;
			int arg0 = 0, arg1 = 0;


			auto condId = ElfHash::Hash(itEnh.condName);
			auto wielderConds = itemObj->GetInt32Array(obj_f_item_pad_wielder_condition_array);
			auto wielderArgs = itemObj->GetInt32Array(obj_f_item_pad_wielder_argument_array);

			// if it's a plus bonus greater than +1, then it means the condition should already be there
			if ( (itEnh.flags & IESF_PLUS_BONUS) && itemEnhSpecs[effIdx].data.enhBonus >= 2)
			{
				arg0 = itEnh.data.enhBonus;
				arg1 = 0;

				auto condArgIdx = 0;
				for (size_t i = 0; i < wielderConds.GetSize(); i++)
				{
					auto wieldCond = conds.GetById(wielderConds[i]);
					if (wielderConds[i] == condId)
					{
						wielderArgs.Set(condArgIdx, arg0);
						wielderArgs.Set(condArgIdx + 1, arg1);
					}
					condArgIdx += wieldCond->numArgs;
				}

			}
			else if (!ItemWielderCondsContainEffect(effIdx, item)){
			
				arg0 = itEnh.data.enhBonus;
				arg1 = 0;

				auto appliedCond = conds.GetByName(itEnh.condName);
				wielderConds.Set(wielderConds.GetSize(), ElfHash::Hash(appliedCond->condName));

				if (appliedCond->numArgs > 0)
					wielderArgs.Append(arg0);
				if (appliedCond->numArgs > 1)
					wielderArgs.Append(arg1);
				for (int i = 2; i < appliedCond->numArgs; i++)
					wielderArgs.Append(0);

				// applying +1 shield/armor bonus, so remove the masterwork condition
				if ((itEnh.flags & (IESF_ARMOR | IESF_SHIELD) ) && itEnh.data.enhBonus == 1){
					auto armorMWCondId = ElfHash::Hash("Armor Masterwork");
					inventory.RemoveWielderCond(item,armorMWCondId);
				}
			}
		}
		auto itemWorthDelta = 100;
		if (itemObj->type == obj_t_weapon)
		{
			itemWorthDelta *= GoldBaseWorthVsEffectiveBonus[effBonus] - GoldBaseWorthVsEffectiveBonus[craftedItemExistingEffectiveBonus];
		}
		else
		{
			itemWorthDelta *= GoldCraftCostVsEffectiveBonus[effBonus] - GoldCraftCostVsEffectiveBonus[craftedItemExistingEffectiveBonus];
		}

		auto itemWorthRegardIdentified = temple::GetRef<int(__cdecl)(objHndl)>(0x10067C90)(item);

		itemObj->SetInt32(obj_f_item_worth, itemWorthRegardIdentified + itemWorthDelta);

		auto itemDesc = itemObj->GetInt32(obj_f_description);
		if (description.DescriptionIsCustom(itemDesc))
		{
			description.CustomNameChange(craftedItemName, itemDesc);
		}
		else
		{
			auto itemDescNew = description.CustomNameNew(craftedItemName);
			itemObj->SetInt32(obj_f_description, itemDescNew);
		}

		ui.WidgetSetHidden(temple::GetRef<int>(0x10BEDA64), 1);
		itemCreationType = ItemCreationType::Inactive;
		itemCreationCrafter = 0i64;

		return;
	}

	// else, a non-MAA crafting type with the simpler window

	// create the new item
	auto newItemHandle = gameSystems->GetObj().CreateObject(item, crafterObj->GetLocation());
	if (!newItemHandle)
		return;

	auto itemObj = gameSystems->GetObj().GetObject(newItemHandle);

	// make it Identified
	itemObj->SetItemFlag(ItemFlag::OIF_IDENTIFIED, true);


	// set its spell properties
	if (itemCreationType == ItemCreationType::CraftWand
		|| itemCreationType == ItemCreationType::ScribeScroll
		|| itemCreationType == ItemCreationType::BrewPotion) {
		CraftScrollWandPotionSetItemSpellData(newItemHandle, crafter);
	}

	inventory.ItemGet(newItemHandle, crafter, 8);

	if (itemCreationType != ItemCreationType::Inactive) {
		//infrastructure::gKeyboard.Update();
		// if ALT is pressed, keep the window open for more crafting!

		//auto& itemCreationResourceCheckResults = temple::GetRef<char*>(0x10BEE330);
		if (altPressed) {

			// refresh the resource checks
			auto numItemsCrafting = temple::GetRef<int[8]>(0x11E76C7C); // array containing number of protos
			static auto craftingHandles = temple::GetRef<objHndl*[8]>(0x11E76C3C); // proto handles

			for (int i = 0; i < numItemsCrafting[itemCreationType]; i++) {
				auto protoHandle = craftingHandles[itemCreationType][i];
				if (protoHandle)
					itemCreationResourceCheckResults[i] = CreateItemResourceCheck(crafter, protoHandle);
			}

			auto icItemIdx = temple::GetRef<int>(0x10BEE398);
			if (icItemIdx >= 0 && icItemIdx < numItemsCrafting[itemCreationType]) {
				auto createBtnId = temple::GetRef<int>(0x10BED8B0);
				if (CreateItemResourceCheck(crafter, item))
				{
					ui.ButtonSetButtonState(createBtnId, UiButtonState::UBS_NORMAL);
				}
				else
				{
					ui.ButtonSetButtonState(createBtnId, UiButtonState::UBS_DISABLED);
				}
			}
			return;
		}

		// else close the window and reset everything
		free(itemCreationResourceCheckResults);
		ui.WidgetSetHidden(temple::GetRef<int>(0x10BEDA60), 1);
		itemCreationType = ItemCreationType::Inactive;
		itemCreationCrafter = 0i64;
	}
}

bool ItemCreation::CancelBtnMsg(int widId, TigMsg* msg) {
	if (msg->type == TigMsgType::WIDGET && msg->arg2 == 1)
	{
		craftedItemNamePos = strlen(craftedItemName);
		craftingWidgetId = widId;

		if (itemCreationType != ItemCreationType::Inactive) {
			if (itemCreationType <= ItemCreationType::ForgeRing){
				free(itemCreationResourceCheckResults);
				ui.WidgetSetHidden(mItemCreationWndId, 1);
				ui.WidgetCopy(mItemCreationWndId, mItemCreationWnd);
			}
			else if (itemCreationType == ItemCreationType::CraftMagicArmsAndArmor)
			{
				ui.WidgetSetHidden(mMaaWndId, 1);
				ui.WidgetCopy(mItemCreationWndId, mMaaWnd);
			}

			itemCreationType = ItemCreationType::Inactive;
		}
		return true;
	}
	return false;
}

bool ItemCreation::MaaCrafterMeetsReqs(int effIdx, objHndl crafter)
{
	if (!crafter)
		return false;
	if (effIdx == CRAFT_EFFECT_INVALID)
		return false;

	auto& itEnh = itemEnhSpecs[effIdx];
	if (objects.StatLevelGet(crafter, stat_caster_level) < itEnh.reqs.minLevel)
		return false;

	if (itEnh.reqs.alignment){
		if (!(objects.StatLevelGet(crafter,stat_alignment) & itEnh.reqs.alignment))
		return false;
	}
		
	// todo: implement AND/OR conditions
	if (itEnh.reqs.spells.size() >= 0){
		auto spellKnown = false;
		for (auto it : itEnh.reqs.spells) {
			for (auto it2 : it.second)
			{
				if (spellSys.spellKnownQueryGetData(crafter, it2, nullptr, nullptr, nullptr))
					spellKnown = true;
			}
		}
		if (!spellKnown)
			return false;
	}
	

	return true;
}

bool ItemCreation::MaaEffectIsInAppliedList(int effIdx){
	for (auto it: appliedBonusIndices){
		if (it == effIdx)
			return true;
	}
	return false;
}

bool ItemCreation::MaaWndMsg(int widId, TigMsg * msg)
{
	if (msg->type == TigMsgType::MOUSE)
		return true;

	if (msg->type == TigMsgType::KEYSTATECHANGE && (msg->arg2 & 0xFF) == 1
		|| msg->type == TigMsgType::KEYDOWN) {
		auto vk = msg->arg1 & 0xFF;
		if (msg->type == TigMsgType::KEYSTATECHANGE)
			vk = infrastructure::gKeyboard.ToVirtualKey(msg->arg1);

		auto crnl = 0;
		switch (vk) {
		case VK_LEFT:
			if (--craftedItemNamePos < 0)
				craftedItemNamePos = 0;
			return true;
		case VK_RIGHT:
			craftedItemNamePos++;
			crnl = strlen(craftedItemName);
			if (craftedItemNamePos > crnl)
				craftedItemNamePos = crnl;
			return true;
		case VK_HOME:
			craftedItemNamePos = 0;
			return true;
		case VK_DELETE:
			crnl = strlen(craftedItemName);
			if (craftedItemNamePos < crnl) {
				memcpy(&craftedItemName[craftedItemNamePos], &craftedItemName[craftedItemNamePos+1], crnl - craftedItemNamePos);
			}
			return true;
		default:
			return true;
			
		}
		return true;
	}
	
	
	if (msg->type == TigMsgType::CHAR) {
		auto key = (char)msg->arg1;
		if (key == '\b') {
			if (--craftedItemNamePos <0)
				craftedItemNamePos=0;
			auto crnl = strlen(craftedItemName);
			if (craftedItemNamePos < crnl) {
				memcpy(&craftedItemName[craftedItemNamePos], &craftedItemName[craftedItemNamePos + 1], crnl - craftedItemNamePos);
			}
		}
		else if (key != '\r' && key >= ' ' && key <= '~'){
			auto curStrLen = strlen(craftedItemName) + 1;
			if (curStrLen < MAA_TEXTBOX_MAX_LENGTH) {
				auto idx = 0;
				for (idx = craftedItemNamePos; idx < curStrLen; --curStrLen) {
					craftedItemName[curStrLen] = craftedItemName[curStrLen - 1];
				}
				craftedItemName[idx] = key;
				craftedItemNamePos = idx + 1;
			}
		}
		return true;
	}

	if (msg->type == TigMsgType::WIDGET) { // scrolling
		if (msg->arg2 == 5) {
			ui.ScrollbarGetY(mMaaCraftableItemsScrollbarId, &mItemCreationScrollbarY);
			ui.ScrollbarGetY(mMaaCraftableItemsScrollbarId, &mMaaApplicableEffectsScrollbarY);;
		}
		return true;
	}
	
	return false;
}

bool ItemCreation::MaaTextboxMsg(int widId, TigMsg* msg){
	if (msg->type != TigMsgType::WIDGET || msg->arg2 != 1)
		return false;

	craftedItemNamePos = strlen(craftedItemName);
	craftingWidgetId = widId;

	return false;
}

bool ItemCreation::MaaRenderText(int widId, objHndl item){
	std::string text;

	// draw the textbox text
	TigRect rect(296, 72,170, 10);
	if (craftedItemNamePos > 0)	{
		text.append(craftedItemName,craftedItemNamePos);
	}
	text.append("|");
	text.append(&craftedItemName[craftedItemNamePos]);

	auto maaTextboxStyle = temple::GetRef<TigTextStyle>(0x10BEDFE8);
	auto measWidth = UiRenderer::MeasureTextSize(text, maaTextboxStyle);
	while (measWidth.width > rect.width)	{
		text.erase(text.begin());
		measWidth = UiRenderer::MeasureTextSize(text, maaTextboxStyle);
	}
	UiRenderer::DrawTextInWidget(widId, text, rect, maaTextboxStyle);

	// draw the enhancement costs
	rect = TigRect(293, 90, 159, 10);
	text.clear();

	auto insuffXp = temple::GetRef<int>(0x10BEE3A4);
	auto insuffGp = temple::GetRef<int>(0x10BEE3A8);
	auto insuffSkill = temple::GetRef<int>(0x10BEE3AC);
	auto insuffPrereqs = temple::GetRef<int>(0x10BEE3B0);
	auto enhCostLabel = temple::GetRef<const char*>(0x10BED798);
	auto cpCost = MaaCpCost(CRAFT_EFFECT_INVALID);
	if (insuffXp || insuffGp || insuffSkill || insuffPrereqs){
		text.append(fmt::format("{} @{}{}", enhCostLabel, insuffGp+1, cpCost/100));
	} 
	else{
		text.append(fmt::format("{} @3{}", enhCostLabel, cpCost / 100));
	}
	auto& textStyle = temple::GetRef<TigTextStyle>(0x10BEE338);
	UiRenderer::DrawTextInWidget(widId, text, rect, textStyle);

	// draw xp cost
	rect.y += 11;
	text.clear();
	auto xpCostLabel = temple::GetRef<const char*>(0x10BED8A4);
	auto xpCost = MaaXpCost(CRAFT_EFFECT_INVALID);
	if (insuffXp || insuffGp || insuffSkill || insuffPrereqs){
		text.append(fmt::format("{} @{}{}", xpCostLabel, insuffXp + 1, xpCost));
	} 
	else	{
		text.append(fmt::format("{} @3{}", xpCostLabel, xpCost ));
	}
	UiRenderer::DrawTextInWidget(widId, text, rect, textStyle);

	rect.y += 11;
	auto itemWorth = gameSystems->GetObj().GetObject(item)->GetInt32(obj_f_item_worth);
	text.clear();
	auto valueLabel = temple::GetRef<const char*>(0x10BED8A8);
	text.append(fmt::format("{} @3{}", valueLabel, itemWorth / 100));

	
	return UiRenderer::DrawTextInWidget(widId, text, rect, textStyle);
}

bool ItemCreation::MaaSelectedItemMsg(int widId, TigMsg* msg)
{
	if (msg->type != TigMsgType::WIDGET || msg->arg2 != 1)
		return false;

	craftedItemNamePos = strlen(craftedItemName);
	craftingWidgetId = widId;
	auto widIdx = 0;
	for (widIdx = 0; widIdx < MAA_NUM_ENCHANTABLE_ITEM_WIDGETS; widIdx++)	{
		if (mMaaCraftableItemList[widIdx] == widId)
			break;
	}
	if (widIdx >= 5)
		widIdx = -1;

	auto itemIdx = mItemCreationScrollbarY + widIdx;


	if (itemIdx >= 0 && itemIdx < numItemsCrafting[itemCreationType]){
		auto itemHandle = craftedItemHandles[itemCreationType][itemIdx];
		craftingItemIdx = itemIdx;
		MaaInitCraftedItem(itemHandle);
	}

	return true;
}

bool ItemCreation::MaaEffectMsg(int widId, TigMsg* msg){

	if (msg->type != TigMsgType::WIDGET || msg->arg2 != 1)
		return false;

	craftedItemNamePos = strlen(craftedItemName);
	craftingWidgetId = widId;

	auto widIdx = 0;
	for (widIdx = 0 ; widIdx < MAA_EFFECT_BUTTONS_COUNT; widIdx++){
		if (maaBtnIds[widIdx] == widId)
			break;
	}
	if (widIdx >= 10)
		return false;

	auto effIdx = GetEffIdxFromWidgetIdx(widIdx);
	if (!MaaCrafterMeetsReqs(effIdx, itemCreationCrafter))
		return true;
	
	if (!MaaEffectIsInAppliedList(effIdx))
		return true;

	int surplusXp = GetSurplusXp(itemCreationCrafter);
	if (MaaXpCost(effIdx) > surplusXp)
		return true;

	auto cpCost = MaaCpCost(effIdx);
	if (cpCost > party.GetMoney())
		return true;

	if (HasPlusBonus(effIdx))
		maaSelectedEffIdx = effIdx;

	return true;
}

void ItemCreation::MaaEffectRender(int widId)
{
	auto idx = 0;
	for (idx = 0; idx < MAA_EFFECT_BUTTONS_COUNT; idx++){
		if (maaBtnIds[idx] == widId)
			break;
	}
	if (idx >= MAA_EFFECT_BUTTONS_COUNT)
		return;

	auto effIdx = GetEffIdxFromWidgetIdx(idx);
	if (effIdx == CRAFT_EFFECT_INVALID)
		return;

	TigRect rect(207, idx * 12 + 152, 106, 12);
	auto effName = GetItemCreationMesLine(1000 + effIdx);

	TigTextStyle* textstyle;
	MaaEffectGetTextStyle(effIdx, itemCreationCrafter, textstyle);

	UiRenderer::DrawTextInWidget(mMaaWndId, effName, rect, *textstyle);
}

void ItemCreation::MaaEffectGetTextStyle(int effIdx, objHndl crafter, TigTextStyle* &style){
	if (!MaaCrafterMeetsReqs(effIdx, crafter)){
		style = temple::GetPointer<TigTextStyle>(0x10BEDE40);
		return;
	}

	for (auto it: appliedBonusIndices)
	{
		if (it == effIdx){
			style = temple::GetPointer<TigTextStyle>(0x10BED998);
			return;
		}
	}

	int surplusXp = d20LevelSys.GetSurplusXp(crafter);
	if (MaaXpCost(effIdx) > surplusXp){
		style = temple::GetPointer<TigTextStyle>(0x10BECDB0);
		return;
	}

	auto cpCost = MaaCpCost(effIdx);
	if (cpCost > party.GetMoney())
	{
		style = temple::GetPointer<TigTextStyle>(0x10BED8B8);
		return;
	}

	if (!HasPlusBonus(effIdx))
	{
		style = temple::GetPointer<TigTextStyle>(0x10BEE2E0);
		return;
	}

	if (effIdx == maaSelectedEffIdx){
		style = temple::GetPointer<TigTextStyle>(0x10BEDF70);
		return;
	}

	style = temple::GetPointer<TigTextStyle>(0x10BEDDF0);
	return;
}

int32_t CreateItemResourceCheck(objHndl ObjHnd, objHndl ObjHndItem)
{
}

bool ItemCreation::MaaEffectAddMsg(int widId, TigMsg* msg)
{
	if (msg->type != TigMsgType::WIDGET || msg->arg2 != 1)
		return false;

	craftedItemNamePos = strlen(craftedItemName);
	auto effIdx = maaSelectedEffIdx;
	craftingWidgetId = widId;

	if (effIdx < 0 || effIdx == CRAFT_EFFECT_INVALID)
		return true;

	objHndl itemHandle = MaaGetItemHandle();
	if (!itemHandle)
		return true;

	if (!MaaCrafterMeetsReqs(effIdx, itemCreationCrafter))
		return true;

	if (!MaaEffectIsInAppliedList(effIdx))
		return true;

	int surplusXp = GetSurplusXp(itemCreationCrafter);
	if (MaaXpCost(effIdx) > surplusXp)
		return true;

	auto cpCost = MaaCpCost(effIdx);
	if (cpCost > party.GetMoney())
		return true;

	if (HasPlusBonus(effIdx))
	{
		MaaAppendEnhancement(effIdx);
		CreateItemResourceCheck(itemCreationCrafter, itemHandle);
	}
	
	return true;
}

void ItemCreation::MaaAppendEnhancement(int effIdx){

	if (!HasPlusBonus(effIdx))
		return;

	appliedBonusIndices.push_back(effIdx);
	const char* icMesLine = GetItemCreationMesLine(1000 + effIdx);
	
	
	auto icLineLen = strlen(icMesLine);
	if (icLineLen + craftedItemNamePos + 1 <= MAA_TEXTBOX_MAX_LENGTH){
		craftedItemName[craftedItemNamePos] = ' ';
		memcpy(&craftedItemName[craftedItemNamePos + 1], icMesLine, icLineLen);
	}
	craftedItemNamePos = strlen(craftedItemName);
}

bool ItemCreation::MaaEffectRemoveMsg(int widId, TigMsg* msg){
	if (msg->type != TigMsgType::WIDGET || msg->arg2 != 1)
		return false;

	craftedItemNamePos = strlen(craftedItemName);

	auto idx = mMaaActiveAppliedWidIdx;
}

bool ItemCreation::MaaAppliedBtnMsg(int widId, TigMsg* msg){
	if (msg->type != TigMsgType::WIDGET || msg->arg2 != 1)
		return false;
	craftedItemNamePos = strlen(craftedItemName);
	craftingWidgetId = widId;
	auto widIdx = 0;
	while (mMaaAppliedBtnIds[widIdx] != widId)
	{
		widIdx++;
		if (widIdx >= MAX_CRAFTED_BONUSES)
			return false;
	}
	mMaaActiveAppliedWidIdx = widIdx;
}

int ItemCreation::UiItemCreationInit(GameSystemConf& conf)
{
	if (!mesFuncs.Open("mes\\item_creation.mes", temple::GetPointer<MesHandle>(0x10BEDFD0)))
		return 0;
	if (!mesFuncs.Open("rules\\item_creation.mes", temple::GetPointer<MesHandle>(0x10BEDA90)))
		return 0;
	if (!mesFuncs.Open("mes\\item_creation_names.mes", temple::GetPointer<MesHandle>(0x10BEDB4C)))
		return 0;

	if (!InitItemCreationRules())
		return 0;

	auto GetAsset = temple::GetRef<int(__cdecl)(UiAssetType assetType, uint32_t assetIndex, int* textureIdOut, int offset) >(0x1004A360);
	GetAsset(UiAssetType::Generic, 1, temple::GetPointer<int>(0x10BED9F0), 0);
	GetAsset(UiAssetType::Generic, 0, temple::GetPointer<int>(0x10BEDA48), 0);
	GetAsset(UiAssetType::Generic, 2, temple::GetPointer<int>(0x10BED9EC), 0);
	GetAsset(UiAssetType::Generic, 6, temple::GetPointer<int>(0x10BEDB48), 0);
	GetAsset(UiAssetType::Generic, 4, temple::GetPointer<int>(0x10BEDA5C), 0);
	GetAsset(UiAssetType::Generic, 3, temple::GetPointer<int>(0x10BEE2D4), 0);
	GetAsset(UiAssetType::Generic, 5, temple::GetPointer<int>(0x10BED6D0), 0);

	auto LoadImgFile = temple::GetRef<ImgFile*(__cdecl)(const char *)>(0x101E8320);
	if ((temple::GetRef<ImgFile*>(0x10BEE388) = LoadImgFile("art\\interface\\item_creation_ui\\item_creation.img")) == 0)
		return 0;

	auto RegisterUiTexture = temple::GetRef<int(__cdecl)(const char*, int*)>(0x101EE7B0);
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\craftarms_0.tga", temple::GetPointer<int>(0x10BEE38C)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\craftarms_1.tga", temple::GetPointer<int>(0x10BECEE8)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\craftarms_2.tga", temple::GetPointer<int>(0x10BED988)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\craftarms_3.tga", temple::GetPointer<int>(0x10BECEEC)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\invslot_selected.tga", temple::GetPointer<int>(0x10BECDAC)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\invslot.tga", temple::GetPointer<int>(0x10BEE038)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\add_button.tga", temple::GetPointer<int>(0x10BEE334)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\add_button_grey.tga", temple::GetPointer<int>(0x10BED990)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\add_button_hover.tga", temple::GetPointer<int>(0x10BEE2D8)))
		return 0;
	if (RegisterUiTexture("art\\interface\\item_creation_ui\\add_button_press.tga", temple::GetPointer<int>(0x10BED79C)))
		return 0;

	if (!ItemCreationWidgetsInit(conf.width, conf.height))
		return 0;

	if (!ItemCreatioMaaWidgetsInit(conf.width, conf.height))
		return 0;

	if (!ItemCreationStringsGet())
		return 0;



	itemCreationType = ItemCreationType::Inactive;

	return 1;
}

bool ItemCreation::InitItemCreationRules(){
	auto fname = "rules\\item_creation.mes";
	MesHandle icrules;
	auto mesFile = mesFuncs.Open(fname, &icrules);
	if (!mesFile) {
		logger->error("Cannot open rules\\item_creation.mes!");
		return false;
	}
	for (auto i = (int)ItemCreationType::IC_Alchemy; i < ItemCreationType::Inactive; i++){
		if (i == ItemCreationType::CraftMagicArmsAndArmor){
			mMaaCraftableItemList.clear();
		}
		else
		{
			MesLine line(i);
			mesFuncs.GetLine_Safe(icrules, &line);
			numItemsCrafting[i] = 0;
			{
				StringTokenizer craftableTok(line.value);
				while (craftableTok.next()) {
					if (craftableTok.token().type == StringTokenType::Number)
						++numItemsCrafting[i];
				}
			}
			craftedItemHandles[i] = new objHndl[numItemsCrafting[i]+1];
			memset(craftedItemHandles[i], 0, sizeof(objHndl) *(numItemsCrafting[i] + 1));
			if (numItemsCrafting[i] > 0){
				int j = 0;
				StringTokenizer craftableTok(line.value);
				while (craftableTok.next()) {
					if (craftableTok.token().type == StringTokenType::Number){
						auto protoHandle = gameSystems->GetObj().GetProtoHandle(craftableTok.token().numberInt);
						if (protoHandle)
							craftedItemHandles[i][j++] = protoHandle;
					}
						
				}
				numItemsCrafting[i] = j; // in case there are invalid protos
				std::sort(craftedItemHandles[i], &craftedItemHandles[i][numItemsCrafting[i]],
					[](objHndl first, objHndl second){
					auto firstName = itemCreation.ItemCreationGetItemName(first);
					auto secondName = itemCreation.ItemCreationGetItemName(second);
					return _stricmp(firstName, secondName);
				});
			}
			

		}
		
	}

	mesFuncs.Close(icrules);

	return true;
}

bool ItemCreation::ItemCreationWidgetsInit(int width, int height){
	mItemCreationWndId;
	auto wndId = temple::GetPointer<int>(0x10BEDA60);
	auto& wnd = temple::GetRef<WidgetType1>(0x10BEE040);
	wnd.WidgetType1Init((width - 404 / 2), (height - 404) / 2, 404, 421);
	wnd.widgetFlags = 1;
	wnd.windowId = 0x7FFFFFFF;
	wnd.render = [](int widId) { itemCreation.ItemCreationRender(widId); };
	wnd.handleMessage = temple::GetRef<bool(__cdecl)(int, TigMsg*)>(0x1014FC20);

	if (ui.AddWindow(temple::GetPointer<WidgetType1>(0x10BEE040), sizeof(WidgetType1),
		wndId, "ui_item_creation.cpp", 2094))
		return false;
	auto icScrollbar = temple::GetPointer<WidgetType3>(0x10BED7A0);
	if (icScrollbar->Init(185, 51, 259))
		return false;
	icScrollbar->x += wnd.x;
	icScrollbar->y += wnd.y;

	auto scrollbarId = temple::GetPointer<int>(0x10BED9F4);
	if (icScrollbar->Add(scrollbarId) || ui.BindButton(*wndId, *scrollbarId))
		return false;

	auto btnY = 55;
	auto& icEntryBtnIds = temple::GetRef<int[NUM_ITEM_CREATION_ENTRY_WIDGETS]>(0x10BECE28);
	for (int i = 0; i < NUM_ITEM_CREATION_ENTRY_WIDGETS; i++){
		WidgetType2 btn(nullptr, *wndId, 32, btnY, 155, 12);
		btn.x += wnd.x;
		btn.y += wnd.y;
		btn.render = temple::GetRef<void(__cdecl)(int)>(0x10151590);
		btn.handleMessage = temple::GetRef<bool(__cdecl)(int, TigMsg*)>(0x10152D70);
		ui.AddButton(&btn, sizeof(WidgetType2), &icEntryBtnIds[i], "ui_item_creation.cpp", 2115);
		if (ui.BindButton(*wndId, icEntryBtnIds[i]))
			return false;
		btnY += 12;
	}
	// create button
	auto createBtnId = temple::GetPointer<int>(0x10BED8B0);
	{
		WidgetType2 btn(nullptr, *wndId, 81, 373, 112, 22);
		btn.x += wnd.x;
		btn.y += wnd.y;
		btn.render = temple::GetRef<void(__cdecl)(int)>(0x1014FD50);
		btn.handleMessage = [](int widId, TigMsg* msg) { return itemCreation.CreateBtnMsg(widId, msg); };

		ui.AddButton(&btn, sizeof(WidgetType2), createBtnId, "ui_item_creation.cpp", 2127);
		ui.SetDefaultSounds(*createBtnId);
		ui.BindButton(*wndId, *createBtnId);
	}
	// cancel button
	auto cancelBtnId = temple::GetPointer<int>(0x10BEDA68);
	
	WidgetType2 btn(nullptr, *wndId, 205, 373, 112, 22);
	btn.x += wnd.x;
	btn.y += wnd.y;
	btn.render = temple::GetRef<void(__cdecl)(int)>(0x1014FEC0);
	btn.handleMessage = [](int widId, TigMsg* msg) { return itemCreation.CancelBtnMsg(widId, msg); };

	ui.AddButton(&btn, sizeof(WidgetType2), cancelBtnId, "ui_item_creation.cpp", 2142);
	ui.SetDefaultSounds(*cancelBtnId);
	return ui.BindButton(*wndId, *cancelBtnId) == 0;
	
}

void ItemCreationHooks::HookedGetLineForMaaAppend(MesHandle handle, MesLine* line)
{
	
	auto result = mesFuncs.GetLine_Safe(handle, line);
	const char emptyString [1] = "";
	
	auto craftedName = temple::GetPointer<char>(0x10BED758);
	craftedName[62] = craftedName[63] =0; // ensure string termination (some users are naughty...)

	auto craftedNameLen = strlen(craftedName);
	auto enhancementNameLen = strlen(line->value);
	if (enhancementNameLen + craftedNameLen > 60){
		line->value = emptyString;
	}
	



}

int ItemCreation::MaaCpCost(int effIdx)
{
	auto effBonus = 0;
	auto icType = itemCreationType;

	if (craftingItemIdx < 0 || craftingItemIdx >= numItemsCrafting[icType])
		return 0;

	auto itemHandle = *craftedItemHandles[icType];
	if (!itemHandle)
		return 0;

	// get the overall effective bonus level
	// first from pre-existing effects
	for (auto it: appliedBonusIndices){
		if (it != CRAFT_EFFECT_INVALID)
			effBonus += itemEnhSpecs[it].effectiveBonus;
	}
	// then from the new applied effect
	if (effIdx >= 0)
		effBonus += itemEnhSpecs[effIdx].effectiveBonus;

	if (effBonus > 29)
		effBonus = 29;

	if (gameSystems->GetObj().GetObject(itemHandle)->type == obj_t_weapon){
		return 50 * (GoldBaseWorthVsEffectiveBonus[effBonus] - GoldBaseWorthVsEffectiveBonus[craftedItemExistingEffectiveBonus]);
	}
	return 50 * (GoldCraftCostVsEffectiveBonus[effBonus] - GoldCraftCostVsEffectiveBonus[craftedItemExistingEffectiveBonus]);
}

int ItemCreation::MaaXpCost(int effIdx){

	auto effBonus = 0;
	auto icType = itemCreationType;

	if (craftingItemIdx < 0 || craftingItemIdx >= numItemsCrafting[icType])
		return 0;

	auto itemHandle = *craftedItemHandles[icType];
	if (!itemHandle)
		return 0;

	// get the overall effective bonus level
	// first from pre-existing effects
	for (auto it : appliedBonusIndices) {
		if (it != CRAFT_EFFECT_INVALID)
			effBonus += itemEnhSpecs[it].effectiveBonus;
	}
	// then from the new applied effect
	if (effIdx >= 0 && effIdx != CRAFT_EFFECT_INVALID)
		effBonus += itemEnhSpecs[effIdx].effectiveBonus;

	if (effBonus > 29)
		effBonus = 29;

	if (gameSystems->GetObj().GetObject(itemHandle)->type == obj_t_weapon) {
		return (GoldBaseWorthVsEffectiveBonus[effBonus] - GoldBaseWorthVsEffectiveBonus[craftedItemExistingEffectiveBonus]) / 25;
	}
	return (GoldCraftCostVsEffectiveBonus[effBonus] - GoldCraftCostVsEffectiveBonus[craftedItemExistingEffectiveBonus]) / 25;
}
