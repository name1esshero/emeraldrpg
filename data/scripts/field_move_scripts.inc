@ Interact with cuttable tree
EventScript_CutTree:: @ 82906BB
	lockall
	checkitem ITEM_AXE, 1
	compare VAR_RESULT, 0
	goto_if_eq EventScript_CheckTreeCantCut
	msgbox Text_PlayerUsedAxe, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_CUT_ON_TREE
	waitstate
	goto EventScript_CutTreeDown
	end

@ Use cut from party menu
EventScript_UseCut:: @ 8290705
	lockall
	dofieldeffect FLDEFF_USE_CUT_ON_TREE
	waitstate
	goto EventScript_CutTreeDown
	end

EventScript_CutTreeDown:: @ 8290710
	applymovement VAR_LAST_TALKED, Movement_CutTreeDown
	waitmovement 0
	removeobject VAR_LAST_TALKED
	releaseall
	end

Movement_CutTreeDown: @ 829071F
	cut_tree
	step_end

EventScript_CheckTreeCantCut:: @ 8290721
	msgbox Text_CantCut, MSGBOX_DEFAULT
	releaseall
	end

EventScript_CancelCut:: @ 829072B
	closemessage
	releaseall
	end

Text_PlayerUsedAxe:
	.string "{PLAYER} used the AXE!$"

Text_CantCut: @ 829077D
	.string "This tree looks like it can be\n"
	.string "CUT down!$"

@ Interact with smashable rock
EventScript_RockSmash:: @ 82907A6
	lockall
	checkitem ITEM_HAMMER, 1
	compare VAR_RESULT, 0
	goto_if_eq EventScript_CantSmashRock
	msgbox Text_PlayerUsedHammer, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_ROCK_SMASH
	waitstate
	goto EventScript_SmashRock
	end

@ Use rock smash from party menu
EventScript_UseRockSmash:: @ 82907F0
	lockall
	dofieldeffect FLDEFF_USE_ROCK_SMASH
	waitstate
	goto EventScript_SmashRock
	end

EventScript_SmashRock:: @ 82907FB
	applymovement VAR_LAST_TALKED, Movement_SmashRock
	waitmovement 0
	removeobject VAR_LAST_TALKED
	specialvar VAR_RESULT, TryUpdateRusturfTunnelState
	compare VAR_RESULT, TRUE
	goto_if_eq EventScript_EndSmash
	special RockSmashWildEncounter
	compare VAR_RESULT, FALSE
	goto_if_eq EventScript_EndSmash
	waitstate
	releaseall
	end

EventScript_EndSmash:: @ 8290829
	releaseall
	end

Movement_SmashRock: @ 829082B
	rock_smash_break
	step_end

EventScript_CantSmashRock:: @ 829082D
	msgbox Text_CantSmash, MSGBOX_DEFAULT
	releaseall
	end

EventScript_CancelSmash:: @ 8290837
	closemessage
	releaseall
	end

Text_CantSmash: @ 829087F
	.string "It's a rugged rock, but a HAMMER\n"
	.string "may be able to smash it.$"
	
Text_PlayerUsedHammer:
	.string "{PLAYER} used the HAMMER!$"	

EventScript_StrengthBoulder:: @ 82908BA
	lockall
	goto_if_set FLAG_SYS_USE_STRENGTH, EventScript_CheckActivatedBoulder
	checkitem ITEM_POWER_GLOVE, 1
	compare VAR_RESULT, 0
	goto_if_eq EventScript_CantStrength
	msgbox Text_PlayerUsedGlove, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_STRENGTH
	waitstate
	goto EventScript_ActivateStrength
	end

EventScript_UseStrength:: @ 82908FD
	lockall
	dofieldeffect FLDEFF_USE_STRENGTH
	waitstate
	goto EventScript_ActivateStrength
	end

EventScript_ActivateStrength:: @ 8290908
	setflag FLAG_SYS_USE_STRENGTH
	releaseall
	end

EventScript_CantStrength:: @ 8290915
	msgbox Text_CantStrength, MSGBOX_DEFAULT
	releaseall
	end

EventScript_CheckActivatedBoulder:: @ 829091F
	msgbox Text_StrengthActivated, MSGBOX_DEFAULT
	releaseall
	end

EventScript_CancelStrength:: @ 8290929
	closemessage
	releaseall
	end

Text_CantStrength: @ 82909D6
	.string "It's a big boulder, but its too heavy\n"
	.string "to push it aside.$"

Text_StrengthActivated: @ 8290A16
	.string "Its now possible to move boulders\n"
	.string "around.$"
	
Text_PlayerUsedGlove:
	.string "{PLAYER} used the POWER GLOVE!$"

EventScript_UseWaterfall:: @ 8290A49
	lockall
	msgbox Text_PlayerUsedWaterfall, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_WATERFALL
	goto EventScript_EndWaterfall

EventScript_CannotUseWaterfall:: @ 8290A83
	lockall
	msgbox Text_CantWaterfall, MSGBOX_DEFAULT

EventScript_EndWaterfall:: @ 8290A8C
	releaseall
	end

Text_CantWaterfall: @ 8290A8E
	.string "A wall of water is crashing down with\n"
	.string "a mighty roar.$"

Text_PlayerUsedWaterfall: @ 8290AFC
	.string "{PLAYER} climbed the watefall!$"
	
EventScript_DiveSpot:: @ 8290B0F
	lockall
	checkitem ITEM_DIVING_SUIT, 1
	compare VAR_RESULT, 0
	goto_if_eq EventScript_CantDive
	msgbox Text_PlayerUsedDive, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_DIVE
	goto EventScript_EndDive
	end
	
EventScript_UseDive::
	lockall
	dofieldeffect FLDEFF_USE_DIVE
	goto EventScript_EndDive
	end	

EventScript_UseDive::
	lockall
	dofieldeffect FLDEFF_USE_DIVE
	goto EventScript_EndDive
	end

EventScript_CantDive:: @ 8290B4E
	msgbox Text_CantDive, MSGBOX_DEFAULT
	releaseall
	end

EventScript_EndDive:: @ 8290B58
	releaseall
	end

EventScript_DiveUnderwater:: @ 8290B5A
	lockall
	checkitem ITEM_DIVING_SUIT, 1
	compare VAR_RESULT, 0
	goto_if_eq EventScript_CantSurface
	msgbox Text_Surfaced, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_DIVE
	goto EventScript_EndSurface
	end
	
EventScript_UseDiveUnderwater:: @ 8290B5A
	lockall
	dofieldeffect FLDEFF_USE_DIVE
	goto EventScript_EndSurface
	end
	
EventScript_UseDiveUnderwater:: @ 8290B5A
	lockall
	dofieldeffect FLDEFF_USE_DIVE
	goto EventScript_EndSurface
	end		

EventScript_CantSurface:: @ 8290B99
	lockall
	msgbox Text_CantSurface, MSGBOX_DEFAULT
	goto EventScript_EndSurface
	end

EventScript_EndSurface:: @ 8290BA8
	releaseall
	end

Text_CantDive: @ 8290BAA
	.string "The sea is deep here. A SUIT\n"
	.string "may be able to go underwater.$"

Text_PlayerUsedDive: @ 8290C1A
	.string "{PLAYER} put on the DIVING SUIT!$"
	
Text_Surfaced: @ 8290C1A
	.string "{PLAYER} surfaced!$"

Text_CantSurface: @ 8290C28
	.string "Light is filtering down from above.$"

EventScript_FailSweetScent:: @ 8290CAE
	msgbox Text_FailSweetScent, MSGBOX_SIGN
	end

Text_FailSweetScent: @ 8290CB7
	.string "Looks like there's nothing here…$"

Text_MonUsedFieldMove: @ 8290771
	.string "{STR_VAR_1} used {STR_VAR_2}!$"
