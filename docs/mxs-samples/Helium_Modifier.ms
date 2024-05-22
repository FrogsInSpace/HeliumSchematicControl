plugin SimpleMod Helium_Mod
	name:"Helium Mod"
	classID:#(0x508b6e4A, 0x11cd0615)
	invisible:false
	version:1
	(
	local emptydummylocal

	parameters heliumParams rollout:heliumModRol
		(
		)

	rollout heliumModRol "heLium" autolayoutonresize:true
		(
		SchematicControl s "View" height:200 offset:[-8,0]

		function refresh =
			(
			if heliumOps.targetControl != s then
				(
				heliumOps.targetRollout = heliumModRol
				heliumOps.targetControl = heliumModRol.s
				heliumOps.useMousePos = false
				)
			)

		on s lbuttondown do
			(
			refresh()
			)

		on s rbuttonup do
			(
			refresh()
			popupmenu HeliumMenu pop:mouse.screenpos
			)
		)--end rollout

	)--end plugin
