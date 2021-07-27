EventScript_UseFlash:: @ 82926F8
	msgbox Text_PlayerUsedLantern, MSGBOX_DEFAULT
	closemessage
	animateflash 1
	setflashradius 1
	end

Text_PlayerUsedLantern:
	.string "{PLAYER} used the LANTERN!$"
