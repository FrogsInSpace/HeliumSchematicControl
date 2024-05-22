	struct heliumExampleStruct
		(

		function refresh rol control =
			(
			if heliumOps.targetControl != control then
				(
				heliumOps.targetRollout = rol
				heliumOps.targetControl = control
				heliumOps.useMousePos = false
				)
			),

		function rolloutPreOpen ca rol control =
			(
			-- if you attach helium to a material, you must make sure that here the material editor is open:
			-- max mtledit
			print "preOpen"
			),

		function rolloutPostOpen ca rol control = (print "postOpen"),
		function rolloutClose ca rol control = (print "Rollout close"),
		function resized ca rol control size = (print "reSized"),
		function rolledUp ca rol control state = (print "rolledUp"),


		function lbuttondown ca rol control = 
			(
			print "lbuttondown"
			refresh rol control
			),

		function lbuttonup ca rol control = (print "lbuttonup"),
		function lbuttonDblclk ca rol control = (print "lbuttonDblclk"),
		function mbuttonup ca rol control = (print "mbuttonup"),
		function mbuttondown ca rol control = (print "mbuttondown"),
		function rbuttondown ca rol control = (print "rbuttondown"),
		function rbuttonup ca rol control = 
			(
			print "rbuttonup"
			refresh rol control
			popupmenu HeliumMenu pop:mouse.screenpos
			),	

		function mouseMoved ca rol control = (/*print "mouseMoved"*/),
		function mouseScroll ca rol control = (print "mouseScroll"),

		function nodeClicked ca rol control index = (print "nodeClicked"),
		function nodesDeleted ca rol control = (print "nodesDeleted"),
		function connectionChanged ca rol control sourceNode targetNode inSocket outSocket status connectionCount = (print "connectionChanged"),

		endOfStruct
		)





-- example for adding helium UI to a custom attribute:

heliumPar_string = 		( "parameters helium_pBlock rollout:heliumRollout \n")
append heliumPar_string ( "		(\n" )
append heliumPar_string ( "		heliumStruct type:#string \n" )
append heliumPar_string ( "		)\n" )


append heliumPar_string ( "rollout heliumRollout \"Helium\"  autolayoutonresize:true \n")
append heliumPar_string ( "		(\n" )
append heliumPar_string ( "		schematicControl s \"\" height:300 \n" )

append heliumPar_string ( "		on heliumRollout open do \n")
append heliumPar_string ( "			( \n")
append heliumPar_string ( "			if hStruct == undefined then hStruct = execute(heliumStruct) \n")
append heliumPar_string ( "			if (isProperty hStruct \"rolloutPreOpen\") then hStruct.rolloutPreOpen this heliumRollout s \n")
append heliumPar_string ( "			if (isProperty hStruct \"rolloutPostOpen\") then hStruct.rolloutPostOpen this heliumRollout s \n")
append heliumPar_string ( "			) \n")

append heliumPar_string ( "		on heliumRollout close do ( if (isProperty hStruct \"rolloutClose\") then hStruct.rolloutClose this heliumRollout s ) \n")
append heliumPar_string ( "		on heliumRollout resized size do ( if (isProperty hStruct \"resized\") then hStruct.resized this heliumRollout s size )  \n")
append heliumPar_string ( "		on heliumRollout rolledUp state do ( if (isProperty hStruct \"rolledUp\") then hStruct.rolledUp this heliumRollout s state )  \n")


append heliumPar_string ( "		on s lbuttondown do ( if (isProperty hStruct \"lbuttondown\") then hStruct.lbuttondown this heliumRollout s )  \n")
append heliumPar_string ( "		on s lbuttonup do ( if (isProperty hStruct \"lbuttonup\") then hStruct.lbuttonup this heliumRollout s )  \n")
append heliumPar_string ( "		on s lbuttonDblclk do ( if (isProperty hStruct \"lbuttonDblclk\") then hStruct.lbuttonDblclk this heliumRollout s )  \n")
append heliumPar_string ( "		on s mbuttondown do ( if (isProperty hStruct \"mbuttondown\") then hStruct.mbuttondown this heliumRollout s )  \n")
append heliumPar_string ( "		on s mbuttonup do ( if (isProperty hStruct \"mbuttonup\") then hStruct.mbuttonup this heliumRollout s )  \n")
append heliumPar_string ( "		on s rbuttondown do ( if (isProperty hStruct \"rbuttondown\") then hStruct.rbuttondown this heliumRollout s )  \n")
append heliumPar_string ( "		on s rbuttonup do ( if (isProperty hStruct \"rbuttonup\") then hStruct.rbuttonup this heliumRollout s )  \n")

append heliumPar_string ( "		on s mouseMoved do ( if (isProperty hStruct \"mouseMoved\") then hStruct.mouseMoved this heliumRollout s )  \n")
append heliumPar_string ( "		on s mouseScroll do ( if (isProperty hStruct \"mouseScroll\") then hStruct.mouseScroll this heliumRollout s )  \n")

append heliumPar_string ( "		on s nodeClicked index do ( if (isProperty hStruct \"nodeClicked\") then hStruct.nodeClicked this heliumRollout s index )  \n")
append heliumPar_string ( "		on s nodesDeleted do ( if (isProperty hStruct \"nodesDeleted\") then hStruct.nodesDeleted this heliumRollout s )  \n")
append heliumPar_string ( "		on s connectionChanged sourceNode targetNode inSocket outSocket status connectionCount do ( if (isProperty hStruct \"connectionChanged\") then hStruct.connectionChanged this heliumRollout s sourceNode targetNode inSocket outSocket status connectionCount )  \n")

append heliumPar_string ( "		)\n" )



--create CA string:
heliumCA_string = ""
append heliumCA_string "MCA = attributes heliumCA\n"
append heliumCA_string "(\n"
append heliumCA_string "local unusedDummyVar\n"			-- prevent a bug in old max versions
append heliumCA_string "local hStruct \n"
append heliumCA_string heliumPar_string
append heliumCA_string ")\n"
myheliumCA = execute(heliumCA_string)






if selection.count > 0 then
	(

	custAttributes.add $ myHeliumCA #unique
	$.heliumStruct = "heliumExampleStruct()"

	)
-- end selection.count



