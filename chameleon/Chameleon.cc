/*

	Chameleon -- basic skin changer for CS:GO on 64-bit Linux.
	Copyright (C) 2016, aixxe. (www.aixxe.net)
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Chameleon. If not, see <http://www.gnu.org/licenses/>.

*/

#include <iostream>
#include <memory.h>

#include "SDK.h"
#include "vmthook/vmthook.h"

/* game interface pointers */
CHLClient* clientdll = nullptr;
IVModelInfo* modelinfo = nullptr;
IVEngineClient* engine = nullptr;
IClientEntityList* entitylist = nullptr;
IGameEventManager2* gameevents = nullptr;

/* virtual table hooks */
VMTHook* clientdll_hook = nullptr;
VMTHook* gameevents_hook = nullptr;

/* replacement FrameStageNotify function */
void hkFrameStageNotify(void* thisptr, ClientFrameStage_t stage) {
	/* perform replacements during postdataupdate */
	while (stage == ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_START) {
		/* get our player entity */
		int localplayer_idx = engine->GetLocalPlayer();
		C_BasePlayer* localplayer = reinterpret_cast<C_BasePlayer*>(entitylist->GetClientEntity(localplayer_idx));

		if (!localplayer || localplayer->GetLifeState() != LIFE_ALIVE)
			break;

		/* get information about our player */
		player_info_t localplayer_info;
		
		if (!engine->GetPlayerInfo(localplayer_idx, &localplayer_info))
			break;

		/* get a list of weapon we're holding */
		int* weapons = localplayer->GetWeapons();

		if (!weapons)
			break;

		for (int i = 0; i < 64; i++) {
			/* check if the handle is invalid */
			if (weapons[i] == -1)
				continue;

			C_BaseAttributableItem* weapon = reinterpret_cast<C_BaseAttributableItem*>(entitylist->GetClientEntity(weapons[i] & 0xFFF));

			/* check if the weapon pointer is invalid */
			if (!weapon)
				continue;

			switch (*weapon->GetItemDefinitionIndex()) {
				/* AWP | Dragon Lore */
				case WEAPON_AWP:
					*weapon->GetFallbackPaintKit() = 344; break;

				/* AK-47 | Redline */
				case WEAPON_AK47:
					*weapon->GetFallbackPaintKit() = 282; break;

				/* M4A4 | Howl */
				case WEAPON_M4A1:
					*weapon->GetFallbackPaintKit() = 309; break;

				/* Desert Eagle | Conspiracy */
				case WEAPON_DEAGLE:
					*weapon->GetFallbackPaintKit() = 351; break;

				/* Glock-18 | Fade */
				case WEAPON_GLOCK:
					*weapon->GetFallbackPaintKit() = 38; break;

				/* USP-S | Stainless */
				case WEAPON_USP_SILENCER:
					*weapon->GetFallbackPaintKit() = 277; break;

				/* Karambit | Tiger Tooth */
				case WEAPON_KNIFE:
					*weapon->GetItemDefinitionIndex() = WEAPON_KNIFE_KARAMBIT;
					*weapon->GetFallbackPaintKit() = 409; break;

				/* M9 Bayonet | Crimson Web */
				case WEAPON_KNIFE_T:
					*weapon->GetItemDefinitionIndex() = WEAPON_KNIFE_M9_BAYONET;
					*weapon->GetFallbackPaintKit() = 12; break;
			}

			/* write to weapon name tag */
			snprintf(weapon->GetCustomName(), 32, "%s", "chameleon-x64 // aixxe.net");

			/* remove all wear */
			*weapon->GetFallbackWear() = 0.f;

			/* use stattrak on everything */
			*weapon->GetEntityQuality() = 9;
			*weapon->GetFallbackStatTrak() = 133337;
			*weapon->GetAccountID() = localplayer_info.xuidlow;

			/* force our fallback values to be used */
			*weapon->GetItemIDHigh() = -1;
		}

		/* viewmodel replacements */
		C_BaseViewModel* viewmodel = reinterpret_cast<C_BaseViewModel*>(entitylist->GetClientEntity(localplayer->GetViewModel() & 0xFFF));

		if (!viewmodel)
			break;

		C_BaseCombatWeapon* active_weapon = reinterpret_cast<C_BaseCombatWeapon*>(entitylist->GetClientEntity(viewmodel->GetWeapon() & 0xFFF));

		if (!active_weapon)
			break;

		switch (*active_weapon->GetItemDefinitionIndex()) {
			case WEAPON_KNIFE_KARAMBIT:
				*viewmodel->GetModelIndex() = modelinfo->GetModelIndex("models/weapons/v_knife_karam.mdl"); break;
			case WEAPON_KNIFE_M9_BAYONET:
				*viewmodel->GetModelIndex() = modelinfo->GetModelIndex("models/weapons/v_knife_m9_bay.mdl"); break;
		}

		break;
	}

	/* call original function after we've made our changes */
	return clientdll_hook->GetOriginalFunction<FrameStageNotifyFn>(36)(thisptr, stage);
}

/* replacement FireEventClientSide function */
bool hkFireEventClientSide(void* thisptr, IGameEvent* event) {
	while (event) {
		/* only make changes to player_death events */
		if (!strcmp(event->GetName(), "player_death")) {
			/* continue if we were the attacker */
			int attacker_uid = event->GetInt("attacker");

			if (!attacker_uid)
				break;

			if (engine->GetPlayerForUserID(attacker_uid) != engine->GetLocalPlayer())
				break;

			/* check the weapon id and perform replacements */
			const char* weapon = event->GetString("weapon");

			if (!strcmp(weapon, "knife_default_ct")) {
				/* Knife (CT) -> Karambit */
				event->SetString("weapon", "knife_karambit");
			} else if (!strcmp(weapon, "knife_t")) {
				/* Knife (T) -> M9 Bayonet */
				event->SetString("weapon", "knife_m9_bayonet");
			}
		}

		break;
	}
	
	/* call original function after we've made our changes */
	return gameevents_hook->GetOriginalFunction<FireEventClientSideFn>(9)(thisptr, event);
};

/* called when the library is loading */
int __attribute__((constructor)) chameleon_init() {
	/* obtain pointers to game interface classes */
	clientdll = GetInterface<CHLClient>("./csgo/bin/linux64/client_client.so", CLIENT_DLL_INTERFACE_VERSION);
	modelinfo = GetInterface<IVModelInfo>("./bin/linux64/engine_client.so", VMODELINFO_CLIENT_INTERFACE_VERSION);
	engine = GetInterface<IVEngineClient>("./bin/linux64/engine_client.so", VENGINE_CLIENT_INTERFACE_VERSION);
	entitylist = GetInterface<IClientEntityList>("./csgo/bin/linux64/client_client.so", VCLIENTENTITYLIST_INTERFACE_VERSION);
	gameevents = GetInterface<IGameEventManager2>("./bin/linux64/engine_client.so", INTERFACEVERSION_GAMEEVENTSMANAGER2);
	
	/* hook CHLClient::FrameStageNotify */
	clientdll_hook = new VMTHook(clientdll);
	clientdll_hook->HookFunction((void*)hkFrameStageNotify, 36);

	/* hook IGameEventManager2::FireEventClientSide */
	gameevents_hook = new VMTHook(gameevents);
	gameevents_hook->HookFunction((void*)hkFireEventClientSide, 9);

	return 0;
}

void __attribute__((destructor)) chameleon_shutdown() {
	/* restore virtual tables to normal */
	delete clientdll_hook;
	delete gameevents_hook;
}