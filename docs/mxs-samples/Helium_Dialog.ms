
try(detroyDialog helium)catch()

rollout helium "Helium" width:506 height:506
	(
	SchematicControl s "View" width:500 height:500 offset:[-10,-2]


	function refresh =
		(
		if heliumOps.targetControl != s then
			(
			heliumOps.targetRollout = helium
			heliumOps.targetControl = helium.s
			heliumOps.useMousePos = true
			)
		)

	on helium open do
		(
		
		)

	on helium close do
		(
		helium = undefined
		freeSceneBitmaps()
		gc()
		)

	on s regionSelected do
		(
		print "Region Selected"
		)

	on s nodesDeleted do
		(
		print "Node(s) deleted via UI"
		for i=1 to s.getDeletedNodeCount do
			(
			format "Node Index deleted:% \n" (s.getDeletedNodeIndex = i)
			)
		)

	on s connectionChanged sourceNode targetNode inSocket outSocket status connectionCount do
		(
		-- status == 1 then we made a connection
		-- status == 0 then we broke a connection
		if status == 1 then format "Connected: src:% (socket:%)  to  trg:% (socket:%). Input socket now has: % connecting sockets\n" sourceNode outSocket targetNode inSocket connectionCount
		else if status == 0 then format "Disconnected: src:% (socket:%)  from  trg:% (socket:%). Input socket now has: % connecting sockets\n" sourceNode outSocket targetNode inSocket connectionCount
		)

	on s connectionSelectionChanged sourceNode targetNode inSocket outSocket do
		(
		format "Connection Selected: src:% (socket:%)  to  trg:% (socket:%).\n" sourceNode outSocket targetNode inSocket
		heliumOps.lastSelectedConnectionNodeIndex = sourceNode
		heliumOps.lastSelectedConnectionOutSocketIndex = outSocket
		)

	on s nodeClicked index do
		(
		format "Node hit index:% - selInd:%\n" index s.selectedNodeIndex
		)

	on s socketValueChanged nodeIndex socketID do
		(
		--format "Socket value changed: Socket ID: %, node Index: %\n" socketID nodeIndex
		forceCompleteRedraw()
		)

	on s LButtonDblclk do
		(
		--print "double click"
		)

	on s lbuttondown do
		(
		refresh()
		--print "left down"
		)

	on s lbuttonup do
		(
		heliumOps.connectionActive = false
		-- check if any connections are selected:
		if heliumOps.lastSelectedConnectionOutSocketIndex != 0 and heliumOps.lastSelectedConnectionOutSocketIndex != 0 then
			(
			s.activeNode = heliumOps.lastSelectedConnectionNodeIndex
			s.activeSocket = heliumOps.lastSelectedConnectionOutSocketIndex
			local selectedIndex = s.getSelectedConnectionIndex
			if selectedIndex == 0 then format "No Connection selected \n"
			)
		--print "left up"
		)

	on s mbuttondown do
		(
		--print "middle down"
		)

	on s mbuttonup do
		(
		--print "middle up"
		)

	on s mouseMoved do
		(
		if heliumOps.connectionActive == true then print "Moving during connection"
		--print "mouse moved"
		)

	on s mouseScroll do
		(
		--print "mouse Scroll"
		)

	on s rbuttondown do
		(
		--print "right down"
		)

	on s rbuttonup do
		(
		heliumOps.connectionActive = false	-- since we are poping-up a right click menu, this will cancel our connection.
		reFresh()
		-- below is only accurate if you get the right position, which the code below might not always get
		-- so use it as an example only for using 'findNodeByPos':
		--local heliumPos = (getDialogPos helium)
		--format "Node index found at right click mouse position: %\n" ( helium.s.findNodeByPos = (mouse.screenPos - heliumPos - [5,20]) ) -- the last point2 is to compensate for nodeWidth/Height and dialog borders etc. Can be different in your scripts
		popupmenu HeliumMenu pos:mouse.screenpos
		)

	on s keyboardDown keycode do
		(
		print ("down: " + (keycode as string))
		)

	on s keyboardUp keycode do
		(
		print ("up: " + (keycode as string))
		)

	on helium resized size do
		(
		s.width = (size.x - 6)
		s.height = (size.y - 6)
		)

	on s connectionStarted sourceNode Socket socketType do
		(
		heliumOps.connectionActive = true
		print (sourceNode as string + " - " + socket as string + " - " + socketType as string)
		)
	)



createDialog helium style:#(#style_toolwindow, #style_sysmenu, #style_resizing)
registerViewWindow helium
