#include "stdafx.h"
#include "util/fixes.h"
#include "dispatcher.h"
#include "condition.h"

#include "gamesystems/objects/objsystem.h"

#define CONDFIX(fname) static int fname ## (DispatcherCallbackArgs args);
#define HOOK_ORG(fname) static int (__cdecl* org ##fname)(DispatcherCallbackArgs) = replaceFunction<int(__cdecl)(DispatcherCallbackArgs)>

class GeneralConditionFixes : public TempleFix {
public:
	void apply() override {

		{ // Fix for duplicate Blackguard tooltip when fallen paladin
			SubDispDefNew sdd;
			sdd.dispKey = DK_NONE;
			sdd.dispType = dispTypeEffectTooltip;
			sdd.dispCallback = [](DispatcherCallbackArgs args) {
				
				GET_DISPIO(dispIOTypeEffectTooltip, DispIoEffectTooltip);

				if (objects.StatLevelGet(args.objHndCaller, stat_level_blackguard))
					return 0;
				dispIo->Append(args.GetData1(), -1, nullptr);
				return 0;
			};

			write(0x102E7400, &sdd, sizeof(sdd));
		}
	}
} genCondFixes;