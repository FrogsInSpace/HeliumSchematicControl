plugin TrackViewUtility HeliumUtility
name:"Helium"
category:"Scripted TV Utilities"
classID:#(0x41bf5e72, 0x57ae3a77)
	(
	on Init val do
		(
		unregisterViewWindow helium
		createDialog helium parent:(val.ui.hwnd)
		--Register the dialog as Dialog Bar, make it child of the Track View:
		cui.registerDialogBar helium parent:val.ui.hwnd
		)
	)
