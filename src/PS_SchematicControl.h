/*
Schematic Maxscript UI control
Copyright (C) 2005 Kees Rijnen (kees@lumonix.net)
www.lumonix.net

This is a new UI widget that can be added to maxscript rollouts and controlled via maxscript.
It represents a generic schematic UI tool for 3dsmax that can be used for various scripted tools.

Special Thanks to:
- Larry Minton
- Simon Feltman (for sharing source like imgTag)
- All companies that have bought the source code over the years.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

class SchematicControl : public RolloutControl
{

protected:

	HWND     m_hWnd;

public:

	// Default functions for Rollout Controls:

	SchematicControl(Value* name, Value* caption, Value** keyparms, int keyparm_count);	//constructor
	~SchematicControl();	// deConstructor

	static RolloutControl* create(Value* name, Value* caption, Value** keyparms, int keyparm_count)
	{
		return new SchematicControl(name, caption, keyparms, keyparm_count);
	}

	classof_methods(SchematicControl, RolloutControl);	// macro

	// we do not supply this function and let a default implementation take over
	// see ps_schematic.cpp for details:
	void		collect(); // { delete this; }
	void		sprin1(CharStream* s) { s->printf(_T("SchematicControl:%s"), name->to_string()); }
	void		add_control(Rollout *ro, HWND parent, HINSTANCE hInstance, int& current_y);
	void		adjust_control(int& current_y);
	LPCTSTR		get_control_class() { return SCHEMATIC_WINDOWCLASS; }
	void		compute_layout(Rollout *ro, layout_data* pos) { }
	BOOL		handle_message(Rollout *ro, UINT message, WPARAM wParam, LPARAM lParam);
	Value*		get_property(Value** arg_list, int count);
	Value*		set_property(Value** arg_list, int count);
	void		set_enable();

	// My Functions:

	Tab<schematicNode*> *schematicNodes;		// pointer to an array of schematic nodes

	c_sceneData* sceneData;					// holds scene data
	hsocket* connectionSocket;				// if a socket is clicked to initiate a new connection, we store that socket here, also used for changing socket values with MMB
	int backgroundColor;					// max background color
	HBRUSH hbrBkGnd;						// background colored brush
	RECT windowRect;						// used to store size of window, needs to be updated on a resize
	HBITMAP m_hBitmap;						// background image
	Value *m_maxBitMap;
	TSTR bitmapPath;
	hsocket* selectedSocketOut;				// selected connection outSocket, since we also know the 'selectedConnectionIndex', we can get to the inSocket of the connection
	Tab<int> lastDeletedNodesIndices;		// keeps track of the last set of nodes that were deleted

	// offScreen drawing (for flicker free drawing:
	HDC hdcMem;
	HBITMAP hbmMem;

	// window procedure:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Event handlers:
	LRESULT			LButtonDblclk(int xPos, int yPos, int fwKeys);
	LRESULT			LButtonDown(int xPos, int yPos, int fwKeys);
	LRESULT			LButtonUp(int xPos, int yPos, int fwKeys);
	LRESULT			MButtonUp(int xPos, int yPos, int fwKeys);
	LRESULT			MButtonDown(int xPos, int yPos, int fwKeys);
	LRESULT			mouseMoved(int xPos, int yPos, int fwKeys);
	LRESULT			mouseScroll(int xPos, int yPos, int fwKeys);
	LRESULT			RButtonUp(int xPos, int yPos, int fwKeys);
	LRESULT			RButtonDown(int xPos, int yPos, int fwKeys);

	LRESULT			Paint(PAINTSTRUCT ps);

	// schematic functions:
	bool			c_addNode();									// add a new schematic node to array
	bool			nodeHitCheck(schematicNode* sNode, Point2 pos); // checks wheter a node was hit by the mouse click, mouse click pos relative to control
	void			Invalidate();									// redraws entire schematic view and rollout
	void			InvalidateArea(RECT area, int threshold = 0);					// redraws areas of the schematic view and rollout
	bool			findAndDeleteConnection(hsocket* socketA, hsocket* socketB);	// checks if a connection exists between to sockets and deletes the connection (both ways)
	void			InvalidateSelectionRegion();								// redraws only the outer edges of the selection region
	hsocket*		getActiveSocket();											// uses activeSocketIndex to find the belonging socket
	void			deleteSelectedNodes();										// delete selected nodes or selected connection via UI
	void			cleanUpNodes();

	// M X S  and  others:
	bool			c_setActiveNode(int index);									// set active node by index, the active node is the node to which all MXS functions are applied, 0-based
																				// note that active node does not mean selected node.
	bool			c_deleteActiveNode();										// delete the active node
	bool			c_addSocket(int type);										// add new socket to active node
	bool			c_setActiveSocket(int index);								// set index of active socket (for active node)
	bool			c_deleteActiveSocket();										// delete active socket
	int				c_getSelectedNodeIndex();									// returns index of selected node
	bool			c_setSelectedNodeIndex(int index);							// set selection by providing an index
	int				c_getSelectionCount();										// returns number of nodes selected
	int				c_getSocketCount();											// return number of sockets in active node
	bool			c_setSocketName(TSTR sLabel);								// set name for active socket
	TSTR			c_getSocketName();											// get name of active socket
	bool			c_setNodeName(TSTR sLabel);									// set name for active node
	bool			c_setBitmap(Value* val);									// set bitmap to active node
	bool			c_setNodePosition(Point2 pos, bool viewSpace = false);		// set node position via maxScript
	void			c_collapseActiveNode(bool col);								// collapse active node
	void			c_NodeClicked(int nodeIndex);								// returns clicked node to MXS
	void			c_ConnectionChanged(int fromNodeIndex, int toNodeIndex, int fromSocketID, int toSocketID, int status, int toSocketCount);	// sends msg to MXS if a connection is changed
	int				getSocketOwnerID(hsocket* sock);							// finds the owner index of a socket by searching schematicNodes
	bool			c_makeConnection(int targetIndex, int targetSocketIndex);	// make connection between active node and socket to supplied indices, via MXS
	void			c_redrawNode();												// redraw a single node via mxs
	int				c_findNodeByPos(Point2 pos);								// find a nodes by supplying a position
	void			c_setNodeColor(COLORREF col);								// set color of node via mxs
	void			c_nodeSize(Point2 size);									// set width and height of active node
	int				c_socketFlipped(int getOrSet);								// get or set Flipped state of socket
	float			c_socketValue(float inVal, bool set = true);					// get or set socket value
	bool			c_socketShowValue(bool show, bool set = true);				// Show value for socket
	int				c_getActiveSocketType();									// return type of active socket
	Tab<int>		c_getActiveSocketConnectionIndices(bool nodes);				// gets node indices of active socket connections (since we can't return an array. we pass it the array to fill up
	void			c_ConnectionSelectionChanged(int fromNodeIndex, int toNodeIndex, int fromSocketID, int toSocketID);		// sends msg to MXS if a connection Selection is changed
	int				c_getNodeIndex(schematicNode *node);						// find index of provided node
	void			c_keyboardToMXS(WPARAM code, bool up);						// send keyboard to MXS
	int				c_getUnhiddenNodeCount();									// returns unhidden node count (for shaderFX demo)
};

