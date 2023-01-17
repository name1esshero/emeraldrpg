EventScript_UseSurf:: @ 8271EA0
	checkitem ITEM_SURFBOARD, 1
	compare VAR_RESULT, 0
	goto_if_eq EventScript_EndUseSurf
	lockall
	msgbox gText_PlayerUsedSurf, MSGBOX_DEFAULT
	dofieldeffect FLDEFF_USE_SURF
EventScript_ReleaseUseSurf::
	releaseall
EventScript_EndUseSurf::
	end
