/*  -------------------------------------------------------------------------------------
	Helium FX Material
	
	Version 1.0

	Copyright Kees Rijnen
	All Rights Reserved
	www.KeesRijnen.com
	
	You may edit and re-distribute this file to your liking, however please do not remove
	the original header (you may add aditional information to the header).

	To the best of my knowledge, Neil Hazzard wrote the Default FX Shader, which
	is what this node based representation uses. Copyrights to Neil and Autodesk.
	
	Special Thanks go to Larry Minton, Rejean Poirier and Shawn Patapoff

*/  -------------------------------------------------------------------------------------












/*  -------------------------------------------------------------------------------------
	HELIUM STRUCTURE
*/  -------------------------------------------------------------------------------------


Struct c_HeliumFXStruct
	(
	targetControl,

	baseInProps = 	( 
						#(
						"k_a",
						"k_d",
						"k_s",
						"n",
						"g_AlphaVertex",
						"g_AddVertexColor",
						"g_UseParallax",
						"g_BumpScale",
						"g_ParallaxScale",
						"g_ParallaxBias",
						"g_AmbientOccEnable",
						"g_AmbientOccTexture",
						"g_AmbientOccTexturemapChannel",
						"g_TopDiffuseEnable",
						"g_TopTexture",
						"g_TopTexturemapChannel",
						"g_BottomDiffuseEnable",
						"g_BottomTexture",
						"g_BottomTexturemapChannel",
						"g_SpecularEnable",
						"g_SpecularTexture",
						"g_SpecularTexturemapChannel",
						"g_NormalEnable",
						"g_NormalTexture",
						"g_NormalTexturemapChannel",
						"g_ReflectionEnable",
						"g_ReflectionTexture"	
						) 
					),

	baseInNames = 	( 
						#(
						"Ambient ",
						"Diffuse ",
						"Specular ",
						"Specular Power ",
						"Vertex Alpha ",
						"Add Vertex Color ",
						"Use Parallax ",
						"Bump Amount ",
						"Parallax Scale ",
						"Parallax Bias ",
						"Use Ambient Occlusion ",
						"Ambient Occl. Texture ",
						"Ambient Occl Channel ",
						"Top Diffuse Enable ",
						"Top Texture ",
						"Top Channel ",
						"Use Bottom Diffuse ",
						"Bottom Texture ",
						"Bottom Channel ",
						"Use Specular ",
						"Specular Texture ",
						"Specular Channel ",
						"Use Normal ",
						"Normal Texture ",
						"Normal Channel ",
						"Use Reflection ",
						"Reflection Texture "
						) 
					),

	baseInClass = 	( 
						#(
						"Color",
						"Color",
						"Color",
						"Integer",
						"Boolean",
						"Boolean",
						"Boolean",
						"Float",
						"Float",
						"Float",
						"Boolean",
						"Bitmap",
						"Integer",
						"Boolean",
						"Bitmap",
						"Integer",
						"Boolean",
						"Bitmap",
						"Integer",
						"Boolean",
						"Bitmap",
						"Integer",
						"Boolean",
						"Bitmap",
						"Integer",
						"Boolean",
						"Bitmap"
						) 
					),




	/*-----------------------------------------------------
	# Description  : Adds default base material node to view
	# Parameters   : 
	# Returns      : ok
	*/-----------------------------------------------------
	function addBase =
		(
		local index = targetControl.addNode
		targetControl.activeNode = index
		targetControl.nodeName = "Standard FX"
		targetControl.nodeSize = [140,30]

		for i=1 to baseInNames.count do
			(
			local si = targetControl.addInSocket
			targetControl.activeSocket = si
			targetControl.socketName = baseInNames[i]
			targetControl.socketDataClass = baseInClass[i]
			targetControl.activeSocketFlipped = true
			)

		targetControl.redrawView
		return ok
		),


	/*-----------------------------------------------------
	# Description  : Add a new Node of varying class
	# Parameters   : 
	# Returns      : ok
	*/-----------------------------------------------------
	function addNode nName outSockets outSocketClasses outSocketValues inSockets inSocketClasses =
		(
		local index = targetControl.addNode
		targetControl.activeNode = index
		targetControl.nodeName = nName

		for i=1 to outSockets.count do
			(
			local si = targetControl.addOutSocket
			targetControl.activeSocket = si
			targetControl.socketName = outSockets[i]
			targetControl.socketDataClass = outSocketClasses[i]
			targetControl.activeSocketFlipped = true
			targetControl.activeSocketValue = outSocketValues[i]
			)
		for i=1 to inSockets.count do
			(
			local si = targetControl.addInSocket
			targetControl.activeSocket = si
			targetControl.socketName = inSockets[i]
			targetControl.socketDataClass = inSocketClasses[i]
			targetControl.activeSocketFlipped = true
			)

		targetControl.redrawView
		return ok
		),

	endOfStruct	
	)-- end struct
heliumFXOps = c_HeliumFXStruct()
















/*  -------------------------------------------------------------------------------------
	HELIUM RC MENU
*/  -------------------------------------------------------------------------------------



RCMenu HeliumFXMenu
	(
	subMenu "Add"
		(
		MenuItem addInt "Integer"
		MenuItem addFloat "Float"
		MenuItem addBool "Boolean"
		MenuItem addColor "Color"
		MenuItem addBitmap "Bitmap"
		)

	subMenu "View"
		(
		MenuItem resetZoom "Reset Zoom"
		MenuItem resetPan "Reset Pan"
		seperator sep1
		MenuItem nodeCount "Show Node Count"
		MenuItem unhideAll "Unhide All"
		)

	seperator sep2
	MenuItem delNodes "Delete Selected Nodes"

	on addInt picked do
		(
		heliumFXOps.addNode "Integer" #("Output") #("Integer") #(1.0) #() #()
		)

	on delNodes picked do
		(
		-- make sure selected node is NOT our material (index 1):
		for i=heliumFXOps.targetControl.getNodeCount to 2 by -1 do
			(
			heliumFXOps.targetControl.activeNode = i
			if heliumFXOps.targetControl.isSelected == true then
				heliumFXOps.targetControl.deleteActiveNode
			)
		heliumFXOps.targetControl.redrawView
		)

	on unhideAll picked do
		(
		for i=1 to heliumFXOps.targetControl.getNodeCount do
			(
			heliumFXOps.targetControl.activeNode = i
			heliumFXOps.targetControl.isHidden = false
			)
		heliumFXOps.targetControl.redrawView
		)

	on nodeCount picked do
		(
		messagebox (heliumFXOps.targetControl.getNodeCount as string)
		)

	on resetZoom picked do
		(
		heliumFXOps.targetControl.zoom = 1.0
		heliumFXOps.targetControl.redrawView
		)

	on resetPan picked do
		(
		heliumFXOps.targetControl.pan = [0,0]
		heliumFXOps.targetControl.redrawView
		)

	)-- end RCMenu













/*  -------------------------------------------------------------------------------------
	HELIUM MATERIAL
*/  -------------------------------------------------------------------------------------


plugin material HeliumFXMaterial
name:"HeliumFXMaterial"
classID:#(0x614719d7, 0x2d3f42a0)
extends:DirectX_9_Shader
replaceUI:true 
	(
	rollout heLiumRol "Helium FX Material"
		(
		SchematicControl s "View" height:500 offset:[-10,-2]

		on heliumRol open do
			(
			heliumFXOps.targetControl = s

			if s.getNodeCount == 0 then 
				heliumFXOps.addBase()

			s.allowUIDelete = false
			)

		on s rbuttonup do
			(
			popupmenu HeliumFXMenu pop:mouse.screenpos
			)

		)-- end rollout



	)--end plugin
