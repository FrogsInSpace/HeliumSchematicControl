

plugin material HeliumMaterial
name:"HeliumMaterial"
classID:#(0x41a0a7fc, 0x5b52da25)
extends:Standard
--replaceUI:true 
	(
	parameters heLiumParams rollout:heLiumRol
		(
		)

	rollout heLiumRol "Helium Material" 
		(

		SchematicControl s "View" height:320 offset:[-10,-2]

		function refresh =
			(
			if heliumOps.targetControl != s then
				(
				heliumOps.targetRollout = heLiumRol
				heliumOps.targetControl = heLiumRol.s
				heliumOps.useMousePos = false
				)
			)

-- NOTE: use  object change handler (when <objects>) to catch material changes. see MXS help

		on s lbuttondown do
			(
			refresh()
			)

		on s rbuttonup do
			(
			reFresh()
			popupmenu HeliumMenu pop:mouse.screenpos
			)
		)

	)
