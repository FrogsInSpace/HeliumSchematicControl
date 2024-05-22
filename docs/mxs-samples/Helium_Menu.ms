struct c_heliumOps
	(
	targetRollout,
	targetControl,
	useMousePos = true,
	lastSelectedConnectionNodeIndex = 0,
	lastSelectedConnectionOutSocketIndex = 0,
	textToUse = "",
	colorToUse,
	connectionActive = false,

	nodeProps = (#(
					"nodeName",
					-- "nodeBitmap", -- saving bitmaps is skipped right now, but you could save the bitmap to disk
					"activeNodePos",
					"activeNodeCollapsed",
					"nodeColor",
					"bitmapPath",
					"isHidden"
					)),

	socketProps = (#(
					"socketName",
					"activeSocketFlipped",
					"activeSocketValue",
					"activeSocketShowValue",
					"connectionColor"
					)),


	-- very basic save function to save nodes in the schematic
	-- we simply store one array for every node in the schematic with all the properties to restore the nodes
	-- the trickiest part is to rebuild the connections between the sockets
	function save outfile:undefined =
		(
		if outfile == undefined then outfile = (getSaveFileName filename:((getDir #export) + "\\") types:"Helium (*.HLM)|*.HLM")
		if outfile != undefined then
			(
			setWaitCursor()
	
			local ostream = (createFile outfile)

			local nc = targetControl.getNodeCount
			format "--Helium_version 0.5\n" to:ostream
			for i=1 to nc do
				(
				format "#(" to:ostream	-- start node array

				targetControl.activeNode = i			-- ! important, whatever node you want to work on should be set to 'active'
				for prop in nodeProps do
					(
					local returnProp = (getProperty targetControl prop)
					if (classof returnProp != String) then
						format "%, " returnProp to:ostream
					else
						format "\"%\", " returnProp to:ostream
					)

				format "%, " targetControl.nodeSize to:ostream			-- keep 2nd last
				format "% " targetControl.getSocketCount to:ostream  	-- !! keep getSocketCount as last property listed here as we need it to build sockets during load
				format ")\n" to:ostream -- close node array


				-- save sockets for this node:
				local sc = targetControl.getSocketCount
				for s=1 to sc do
					(
					format "\t #(" to:ostream	-- start socket array
					targetControl.activeSocket = s		-- ! important, whatever socket you want to work on should be set to 'active'
					for p=1 to socketProps.count do
						(
						local returnProp = (getProperty targetControl socketProps[p])

						if (classof returnProp != String) then
							format "%, " returnProp to:ostream
						else
							format "\"%\", " returnProp to:ostream
						)

					local socketType = targetControl.getActiveSocketType
					if socketType == 1 then	-- we only have to store connections for output sockets
						(
						format "%, " targetControl.getActiveSocketConnectingNodeIndices to:ostream 		-- !! keep 3rd last
						format "%, " targetControl.getActiveSocketConnectingSocketIndices to:ostream 	-- !! keep second last
						)
					format "% " socketType to:ostream 						-- !! keep socket type as last in array as we need it during load
					format " )\n" to:ostream -- close node array
					)

				)--end for i
			close ostream
			setArrowCursor()
			)
		return ok
		),



	-- basic load:
	function load infile:undefined =
		(
		if infile == undefined then infile = (getOpenFileName filename:((getDir #export) + "\\") types:"Helium (*.HLM)|*.HLM")
		if infile != undefined then
			(
			-- delete existing nodes:
			for i=targetControl.getNodeCount to 1 by -1 do	-- reverse the loop when deleting!
				(
				targetControl.activeNode = i
				targetControl.deleteActiveNode
				)

			local istream = (openFile infile)
			local heliumVersion = readline istream
			local nodeConnections = #()	-- we are going to store connections until all nodes are build, then make them.
			while not (EOF istream) do
				(
				local nodeArray = execute(readline istream) -- get the node array

				local nodeIndex = targetControl.addNode
				targetControl.activeNode = nodeIndex		-- ! set active before we can apply properties back

				-- restore node properties:
				for p=1 to nodeProps.count do
					(
					setProperty targetControl nodeProps[p] nodeArray[p]
					)

				-- restore sockets:
				local socketCount = nodeArray[nodeArray.count]
				for s=1 to socketCount do
					(
					local socketArray = execute(readline istream)
					local socketType = socketArray[socketArray.count]
					local socketIndex
					if socketType == 0 then	-- input socket:
						socketIndex = targetControl.addInSocket
					else	-- output socket:
						(
						socketIndex = targetControl.addOutSocket
						-- since we are dealing with an outPut socket, store connections for later:
						for c=1 to socketArray[socketArray.count-2].count do 
							(
							local n = socketArray[socketArray.count-2][c]	-- node index
							local s = socketArray[socketArray.count-1][c]	-- socket index
							append nodeConnections #(nodeIndex, socketIndex, [n,s])  -- #(sourceNode, outputSocket, [targetNode, inputSocket])
							)
						)

					targetControl.activeSocket = socketIndex	-- ! set active before we can apply properties back

					for t=1 to socketProps.count do
						(
						setProperty targetControl socketProps[t] socketArray[t]
						)
					)

				-- restore nodeSize (adding sockets might have altered the size as they automatically increase node size):
				targetControl.nodeSize = nodeArray[nodeArray.count-1]
				)--end if EOF

			-- we cannot restore connections until we know all nodes are build (that's why we do it here)
			-- to make a connection, set the output node and socket to be active then
			-- provide a point2 with the target node index and socket index:
			for con in nodeConnections do
				(
				targetControl.activeNode = con[1]
				targetControl.activeSocket = con[2]
				targetControl.toggleConnection = con[3]	-- [target nodeIndex, inSocket]
				)


			close istream
			targetControl.redrawView
			)
		
		return ok
		),


	endOfStruct
	)

heliumOps = c_heliumOps()


























-- Helium right click menu:

RCMenu HeliumMenu
	(
	MenuItem addNode "Add Node"
	seperator sep1
	subMenu "Node Properties"
		(
		MenuItem addBitmap "Add Bitmap"
		MenuItem deleteBitmap "Delete Bitmap"
		MenuItem setBitmapPath "Set bitmap path \"c:\\myBitmaps\\themap.jpg\""
		MenuItem getBitmapPath "Show bitmap path"
		MenuItem setNodeName "Set Node name to \"Material\""
		MenuItem setRandomColor "Give Node random Color"
		MenuItem increaseNodeSize "Increase Node Size"
		MenuItem decreaseNodeSize "Decrease Node Size"
		MenuItem decreaseNodeColSize "Decrease Node Collapsed Size"
		MenuItem collapseNode "Collapse Active Node Toggle"
		MenuItem nodeHideToggle "Toggle Hide active node"
		MenuItem storeSelNode "Store selected scene node in schematic node"
		MenuItem getSceneNode "Get scene node stored in schematic node"
		MenuItem togglePosLock "Toggle Position Lock for selected node"
		MenuItem moveNodeLabel "Move Node Label Down"
		MenuItem decreaseEdges "Decrease rounded edges"
		MenuItem increaseEdges "Increase rounded edges"
		MenuItem togglebitmapOnly "Toggle Show Bitmap Only"
		MenuItem toggleCollapsible "Toggle Collapsible"
		MenuItem increaseBitmapSize "Increase Bitmap Size"
		MenuItem decreaseBitmapSize "Decrease Bitmap Size"
		seperator sep2
		MenuItem deleteNode "Delete Selected Node"
		)
	subMenu "Socket Properties"
		(
		MenuItem addInSocket "Add Input Socket"
		MenuItem addOutSocket "Add Output Socket"
		MenuItem nameSocket "Set last Socket name to \"Bezier\""
		MenuItem flipSocket "Flip Active Socket"
		MenuItem socketCount "Show Socket Count of selected node"
		MenuItem increaseVal "Increase Last Socket Value"
		MenuItem decreaseVal "Decrease Last Socket Value"
		MenuItem toggleShowVal "Toggle Show Socket Values of last Socket"
		MenuItem editVal "Edit Last Socket Value [..]"
		MenuItem showLastSocketType "Show last Socket type"
		MenuItem getLastSocketNodeIndices "Show connecting node indices of active socket"
		MenuItem getLastSocketSocketIndices "Show connecting socket indices of active socket"
		MenuItem setController "Store Controller of selected node in last socket"
		MenuItem getController "Get stored Controller of last socket"
		MenuItem setSocketPos "set Active socket Position to top"
		MenuItem setConnectionColor "set random color for Active (out) Socket"
		MenuItem moveSocketLabel "Move Socket Label Down and Right"
		MenuItem sDataClass "Set Data Class to Boolean"
		MenuItem setSockInfo "Set socket info"
		MenuItem getSockInfo "Get socket info"
		MenuItem toggleHideSocket "Toggle Hide last Socket"
		seperator sep3
		MenuItem deleteSocket "Delete Last Socket"
		)
	subMenu "Connection"
		(
		MenuItem toggleShowWeight "Toggle Show Weights"
		MenuItem setWeight "Set Weight for Selected Connection"
		)
	subMenu "Group"
		(
		MenuItem groupNodes "Group selected nodes"
		MenuItem ungroupNodes "UnGroup"
		)
	subMenu "View"
		(
		MenuItem resetZoom "Reset Zoom"
		MenuItem resetPan "Reset Pan"
		seperator sep4
		MenuItem nodeCount "Show Node Count"
		MenuItem unhideAll "Unhide All"
		MenuItem allowUIDel "Allow node del via UI"
		MenuItem deleteAllNodes "Delete ALL nodes"
		seperator sep5
		MenuItem save "Save [..]"
		MenuItem load "Load [..]"
		seperator sep6
		subMenu "Kees Debugging"
			(
			MenuItem testMXSConnect "Test MXS Connect"
			MenuItem addLotsOfNodes "Add lots of Nodes"
			MenuItem nodeToGroupBox "Selected Node to Group Box"
			MenuItem setBG "Set background image"
			MenuItem getBGPath "Show background path"
			MenuItem tilebg "Tile Background toggle"
			MenuItem nodeSocketDrawTest "Draw bug"
			MenuItem parent "Parent"
			MenuItem unParent "unParent"
			MenuItem toggleInfo "Toggle copyright"
			)
		)

	on toggleHideSocket picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.socketIsHidden = not heliumOps.targetControl.socketIsHidden
			)
		)

	on decreaseBitmapSize picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.bitmapSize = heliumOps.targetControl.bitmapSize - [10,10]
			heliumOps.targetControl.redrawView
			)
		)

	on increaseBitmapSize picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.bitmapSize = heliumOps.targetControl.bitmapSize + [10,10]
			heliumOps.targetControl.redrawView
			)
		)

	on toggleShowWeight picked do
		(
		heliumOps.targetControl.drawWeights = not heliumOps.targetControl.drawWeights
		heliumOps.targetControl.redrawView
		)

	on setWeight picked do
		(
		local activeConnectionNodeAndSocket = heliumOps.targetControl.getSelectedConnectionNodeAndSocket
		if activeConnectionNodeAndSocket != false then
			(
			heliumOps.targetControl.activeNode = activeConnectionNodeAndSocket[1]
			heliumOps.targetControl.activeSocket = activeConnectionNodeAndSocket[2]
			local connectionIndex = heliumOps.targetControl.getSelectedConnectionIndex
			heliumOps.targetControl.setConnectionWeight = [connectionIndex, (random 1 500)]	-- set random value in weight
			heliumOps.targetControl.redrawView
			format "Node:% , Socket:% , Connection:% \n" activeConnectionNodeAndSocket[1] activeConnectionNodeAndSocket[2] connectionIndex
			)
		else messagebox "Please select a connection first"
		)

	on setSockInfo picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.socketInfo = "This is test info"
			)
		)

	on getSockInfo picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			messagebox heliumOps.targetControl.socketInfo
			)
		)

	on groupNodes picked do
		(
		if heliumOps.targetControl.getSelectionCount < 2 then messagebox "Pick at least 2 nodes to group"
		else
			(
			-- to get to the position of the groupNode, you could walk the whole graph and avarage the position of all selected nodes
			-- however, its faster to just match the groupNode position with the last selectedNode, so lets do that:
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			local gPos = heliumOps.targetControl.activeNodePos

			-- create group node:
			local groupIndex = heliumOps.targetControl.addNode
			heliumOps.targetControl.activeNode = groupIndex
			heliumOps.targetControl.nodeName = "Group"
			heliumOps.targetControl.nodeSize = [50,50]
			heliumOps.targetControl.edgeSize = 50			-- we'll make groupNodes look like circles
			heliumOps.targetControl.nodeNameOffset = [0,18]	-- put the label in the center
			heliumOps.targetControl.activeNodePos = gPos
			heliumOps.targetControl.isCollapsible = false

			-- parent all selected nodes to groupNode:
			for i=1 to heliumOps.targetControl.getNodeCount do
				(
				heliumOps.targetControl.activeNode = i
				if heliumOps.targetControl.isSelected == true then
					(
					heliumOps.targetControl.parentNode = groupIndex
					heliumOps.targetControl.isHidden = true
					heliumOps.targetControl.drawConnectionsFromParent = true	-- make sure connections are still drawn
					)
				)
			heliumOps.targetControl.redrawView
			)
		)

	on ungroupNodes picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Pick 1 group node"
		else
			(
			local groupIndex = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeNode = groupIndex
			-- deleting the group node will automatically unparent all children, but
			-- it does not unhide the children, so instead of the one line below, we walk through the children and unparent and unlink them
			-- heliumOps.targetControl.deleteActiveNode
			for i=heliumOps.targetControl.getChildCount to 1 by -1 do	-- be sure to loop BACKWARDS since we are going to unparent in the loop!
				(
				heliumOps.targetControl.activeNode = groupIndex	-- be sure to restore the activeNode to groupIndex, since it is changed in our loop below
				local childIndex = (heliumOps.targetControl.childIndex = i)
				heliumOps.targetControl.activeNode = childIndex
				heliumOps.targetControl.parentNode = 0
				heliumOps.targetControl.isHidden = false
				heliumOps.targetControl.drawConnectionsFromParent = false
				)

			-- node delete the groupNode:
			heliumOps.targetControl.activeNode = groupIndex
			heliumOps.targetControl.deleteActiveNode

			heliumOps.targetControl.redrawView
			)
		)

	on toggleInfo picked do
		(
		heliumOps.targetControl.showInfo = not heliumOps.targetControl.showInfo
		heliumOps.targetControl.redrawView
		)

	on parent picked do
		(
		if heliumOps.targetControl.getNodeCount > 1 then
			(
			heliumOps.targetControl.activeNode = 2
			heliumOps.targetControl.parentNode = 1
			heliumOps.targetControl.redrawView 
			)
		)

	on unparent picked do
		(
		if heliumOps.targetControl.getNodeCount > 1 then
			(
			heliumOps.targetControl.activeNode = 2
			heliumOps.targetControl.parentNode = 0
			heliumOps.targetControl.redrawView 
			)
		)

	on tilebg picked do
		(
		heliumOps.targetControl.tileBackground = (not heliumOps.targetControl.tileBackground)
		heliumOps.targetControl.backgroundBitmap = (openBitmap "C:\\Documents and Settings\\mr roboto\\My Documents\\Documents\\CPP Code\\PuppetShop\\Script\\Documentation\\grid.jpg")
		heliumOps.targetControl.redrawView
		)

	on nodeSocketDrawTest picked do
		(
		local nodes = 200
		local sockets = 50
		   
		for i = 1 to nodes do
			(
			local index = heliumOps.targetControl.addNode
			heliumOps.targetControl.activeNode = index
			--heliumOps.targetControl.selectedNodeIndex = index
			heliumOps.targetControl.activeNodePos = [random 0.0 2000.0,random 0.0 2000.0]
			for s = 1 to sockets do
				heliumOps.targetControl.addInSocket
			)
		heliumOps.targetControl.redrawView
		)

	on getBGPath picked do
		(
		messagebox heliumOps.targetControl.backgroundPath
		)

	on setBG picked do
		(
		heliumOps.targetControl.backgroundBitmap = (openBitmap "C:\\WINDOWS\\Coffee Bean.bmp")
		heliumOps.targetControl.backgroundPath = "C:\\WINDOWS\\Coffee Bean.bmp"
		heliumOps.targetControl.redrawView 
		)

	on toggleCollapsible picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematic node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.isCollapsible = (not heliumOps.targetControl.isCollapsible)
			heliumOps.targetControl.redrawView 
			)
		)

	on togglebitmapOnly picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.showBitmapOnly = (not heliumOps.targetControl.showBitmapOnly)
			heliumOps.targetControl.redrawView 
			)
		)

	on sDataClass picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.socketDataClass = "Boolean"
			)
		)

	on increaseEdges picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.edgeSize += 2
			heliumOps.targetControl.redrawView -- redraw view instead of node since node has shrunk and old, bigger size is not known
			)
		)

	on decreaseEdges picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.edgeSize -= 2
			heliumOps.targetControl.redrawView -- redraw view instead of node since node has shrunk and old, bigger size is not known
			)
		)

	on decreaseNodeColSize picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeCollapsedSize = heliumOps.targetControl.nodeCollapsedSize - [20,5]
			heliumOps.targetControl.redrawView -- redraw view instead of node since node has shrunk and old, bigger size is not known
			)
		)

	on moveSocketLabel picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.socketLabelOffset += [15,5]
			heliumOps.targetControl.redrawView
			)
		)

	on nodetoGroupBox picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeSize = [110,200]
			heliumOps.targetControl.nodeColor = (255 * (colorman.getColor #background) as color)
			heliumOps.targetControl.selectable = false
			heliumOps.targetControl.nodeName = "Group Box"
			heliumOps.targetControl.isSelected = false
			heliumOps.targetControl.activeNode = 0
			heliumOps.targetControl.redrawView
			)
		)

	on setConnectionColor picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			local col = heliumOps.colorToUse
			if col == undefined then col = (color (random 0 255) (random 0 255) (random 0 255))
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.connectionColor = col
			heliumOps.targetControl.redrawView
			)
		)

	on moveNodeLabel picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeNameOffset += [0,10]
			heliumOps.targetControl.redrawView
			)
		)

	on setSocketPos picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.socketPosition = [50,2]
			heliumOps.targetControl.redrawView
			)
		)

	on togglePosLock picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.posLocked = (not heliumOps.targetControl.posLocked)
			)
		)

	on getController picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select one schematc node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			messagebox (classof heliumOps.targetControl.sceneController as string)
			)
		)

	on setController picked do
		(
		if (heliumOps.targetControl.getSelectionCount != 1 or selection.count < 1) then messagebox "Please select one schematc node and one scene node"
		else
			(
			if classof selection[1].position.controller != position_xyz then messagebox "No pos XYZ controller on scene node"
			else
				(
				heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
				heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
				heliumOps.targetControl.sceneController = selection[1].position.controller.x_position.controller
				)
			)
		)

	on getSceneNode picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			messagebox (heliumOps.targetControl.sceneNode).name
			)
		)

	on storeSelNode picked do
		(
		if selection.count > 0 and heliumOps.targetControl.getSelectionCount == 1 then
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.sceneNode = selection[1]
			)
		else messagebox "Please select only 1 schematic node and one max scene node"
		)


	on allowUIDel picked do
		(
		heliumOps.targetControl.allowUIDelete = (not heliumOps.targetControl.allowUIDelete)
		)


	on setBitmapPath picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.bitmapPath = "c:\\myBitmaps\\themap.jpg"
			)
		)

	on getBitmapPath picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			messagebox heliumOps.targetControl.bitmapPath
			)
		)

	on load picked do
		(
		heliumOps.load()
		)

	on save picked do
		(
		heliumOps.save()
		)

	on getLastSocketSocketIndices picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			local connectingSocketIndices = heliumOps.targetControl.getActiveSocketConnectingSocketIndices
			messagebox (connectingSocketIndices as string)
			)
		)

	on getLastSocketNodeIndices picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			local connectingNodeIndices = heliumOps.targetControl.getActiveSocketConnectingNodeIndices
			messagebox (connectingNodeIndices as string)
			)
		)


	on showLastSocketType picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			local type = heliumOps.targetControl.getActiveSocketType
			if type == 0 then messagebox "Input"
			else if type == 1 then messagebox "Output"
			else messagebox "Not a valid socket"
			)
		)

	on unhideAll picked do
		(
		for i=1 to heliumOps.targetControl.getNodeCount do
			(
			heliumOps.targetControl.activeNode = i
			heliumOps.targetControl.isHidden = false
			)
		heliumOps.targetControl.redrawView
		)

	on nodeHideToggle picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.isHidden = (not heliumOps.targetControl.isHidden)
			heliumOps.targetControl.redrawView
			)
		)

	on deleteAllNodes picked do
		(
		if querybox "Sure?" == true then
			(
			for i=heliumOps.targetControl.getNodeCount to 1 by -1 do	-- reverse the loop when deleting!
				(
				heliumOps.targetControl.activeNode = i
				heliumOps.targetControl.deleteActiveNode
				)
			heliumOps.targetControl.redrawView
			)
		)

	on editVal picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			rollout floatEdit "Edit Value"
				(
				spinner floatSpin "" type:#float range:[-9999999,9999999,0.0]
				on floatEdit open do
					(
					heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
					heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
					floatSpin.value = heliumOps.targetControl.activeSocketValue
					)
				on floatSpin changed val do
					(
					heliumOps.targetControl.activeSocketValue = floatSpin.value
					heliumOps.targetControl.redrawNode
					)
				on floatSpin entered do
					(
					heliumOps.targetControl.activeSocketValue = floatSpin.value
					)
				)
			createDialog floatEdit pos:mouse.screenPos modal:true
			destroyDialog floatEdit
			)
		)

	on toggleShowVal picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.activeSocketShowValue = (not heliumOps.targetControl.activeSocketShowValue)
			heliumOps.targetControl.redrawNode
			)
		)

	on increaseVal picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.activeSocketValue += 0.2
			heliumOps.targetControl.redrawNode
			)
		)

	on decreaseVal picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.activeSocketValue -= 0.33
			heliumOps.targetControl.redrawNode
			)
		)

	on socketCount picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			messagebox (heliumOps.targetControl.getSocketCount as string)
			)
		)

	on nodeCount picked do
		(
		messagebox (heliumOps.targetControl.getNodeCount as string)
		)

	on addLotsOfNodes picked do
		(
		local addBitmaps = querybox "Add bitmaps?"
		-- create the nodes:
		for i=1 to 100 do 
			(
			local index = heliumOps.targetControl.addNode
			heliumOps.targetControl.activeNode = index
			heliumOps.targetControl.activeNodePos = [(random 0 700), (random 0 700)]
			heliumOps.targetControl.nodeColor = (color (random 0 255) (random 0 255) (random 0 255))
			for s=1 to 2 do heliumOps.targetControl.addInSocket
			for s=1 to 2 do 
				(
				heliumOps.targetControl.addOutSocket
				heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
				heliumOps.targetControl.connectionColor = (color (random 0 255) (random 0 255) (random 0 255))
				)
			-- add unique bitmap to each:
			if addBitmaps == true then
				(
				targetMap = (bitmap 96 96 color:(255 * (colorman.getColor #background) as color))
				heliumOps.targetControl.nodeBitmap = targetMap
				)
			)

		-- randomly connect nodes:
		for i=1 to 100 do 
			(
			heliumOps.targetControl.activeNode = i
			targetIndex = (random 1 100)
			if targetIndex == i then targetIndex += 1
			if targetIndex > 100 then targetIndex = 1

			for s=3 to 4 do  -- 3 and 4 because the first 2 are the insockets and the activeNode/sockets has to be connected from it's outSockets
				(
				heliumOps.targetControl.activeSocket = s
				heliumOps.targetControl.toggleConnection = [targetIndex,(random 1 2)]
				)
			)
		
		heliumOps.targetControl.redrawView
		)


	on flipSocket picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.activeSocketFlipped = (not heliumOps.targetControl.activeSocketFlipped)
			heliumOps.targetControl.redrawView
			)
		)

	on increaseNodeSize picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeSize = heliumOps.targetControl.nodeSize + [25,5]
			heliumOps.targetControl.redrawNode
			)
		)

	on decreaseNodeSize picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeSize = heliumOps.targetControl.nodeSize - [25,5]
			heliumOps.targetControl.redrawView -- redraw view instead of node since node has shrunk and old, bigger size is not known
			)
		)

	on setRandomColor picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			local col = heliumOps.colorToUse
			if col == undefined then col = (color (random 0 255) (random 0 255) (random 0 255))
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeColor = col
			heliumOps.targetControl.redrawNode
			)
		)

	on testMXSConnect picked do
		(
		if querybox "This requires 2 nodes, first with outSocket, second with inSocket" == true then
			(
			-- active node (index 1) should have an out socket with index 1
			-- we then connect to a second node (index 2) which should have an in socket with index 1
			heliumOps.targetControl.activeNode = 1
			heliumOps.targetControl.activeSocket = 1
			result = heliumOps.targetControl.toggleConnection = [2,1]	-- connect from an outSocket to an inSocket
			heliumOps.targetControl.redrawView
			if result == false then messagebox "Something was wrong, Connection could not be made"
			)
		)

	on resetZoom picked do
		(
		heliumOps.targetControl.zoom = 1.0
		heliumOps.targetControl.redrawView
		)

	on resetPan picked do
		(
		heliumOps.targetControl.pan = [0,0]
		heliumOps.targetControl.redrawView
		)

	on collapseNode picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeNodeCollapsed = (not heliumOps.targetControl.activeNodeCollapsed)
			heliumOps.targetControl.redrawView
			)
		)

	on addNode picked do
		(
		local index = heliumOps.targetControl.addNode
		heliumOps.targetControl.activeNode = index
		heliumOps.targetControl.selectedNodeIndex = index	-- set selected node to be the new added node (deselects everything else)
		if heliumOps.useMousePos == true then
			(
			local heliumPos = (getDialogPos heliumOps.targetRollout)
			heliumOps.targetControl.activeNodePos = mouse.screenpos - heliumPos - [50,35] -- the last point2 is to compensate for nodeWidth/Height and dialog borders etc. Can be different in your scripts
			)
		heliumOps.targetControl.redrawView --  although we only added 1 node so we could have used redrawNode, in this case we might have also deselected other nodes, so we redraw all
		)

	on deleteNode picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.deleteActiveNode
			heliumOps.targetControl.redrawView
			)
		)

	on addInSocket picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.addInSocket
			heliumOps.targetControl.redrawNode
			)
		)

	on addOutSocket picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.addOutSocket
			heliumOps.targetControl.redrawNode
			)
		)

	on deleteSocket picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.deleteActiveSocket
			heliumOps.targetControl.redrawView -- we are making the node smaller and the old size is not know, so redraw all
			)
		)

	on addBitmap picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			local selNodeIndex = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeNode = selNodeIndex
			targetMap = (bitmap 96 96 color:(255 * (colorman.getColor #background) as color))
			if classof meditMaterials[selNodeIndex+1] == Standardmaterial then
				(
				-- +1 because the first material is likely our helium material
				-- this is only a test.
				-- you likely want to grab .delete.diffuseMap etc.
				if meditMaterials[selNodeIndex+1].diffuseMap != undefined then
					renderMap meditMaterials[selNodeIndex+1].diffuseMap into:targetMap
				)
			heliumOps.targetControl.nodeBitmap = targetMap
			heliumOps.targetControl.redrawNode
			)
		)

	on deleteBitmap picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeBitmap = undefined
			heliumOps.targetControl.redrawView -- node got smaller but we do not know the original size, so redraw entire view
			)
		)

	on setNodeName picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			local txt = heliumOps.textToUse
			if txt == "" then txt = "Material"
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.nodeName = txt
			heliumOps.targetControl.redrawNode
			)
		)

	on nameSocket picked do
		(
		if heliumOps.targetControl.getSelectionCount != 1 then messagebox "Please select only 1 node"
		else
			(
			local txt = heliumOps.textToUse
			if txt == "" then txt = "Bezier"
			heliumOps.targetControl.activeNode = heliumOps.targetControl.selectedNodeIndex
			heliumOps.targetControl.activeSocket = heliumOps.targetControl.getSocketCount
			heliumOps.targetControl.socketName = txt
			heliumOps.targetControl.redrawNode
			)
		)

	)--end rcMenu




