
#ifndef _heliumNodes_
#define _heliumNodes_

/* ===========================================================================
	Includes and defines
   =========================================================================== */
#include <max.h>

#include <maxscript/maxscript.h>
#include <maxscript/maxwrapper/bitmaps.h>

#include "PS_SceneData.h"

   // default defines for sockets and nodes:
#define SOCKETRADIUS 9.0						// default radius (size) of sockets
#define SOCKETINCOLOR RGB(255,30,30)			// default color for in sockets
#define SOCKETOUTCOLOR RGB(30,255,30)			// default color for out sockets
#define SOCKETCOLLAPSEDCOLOR RGB(80,80,80)		//RGB(30,30,255);	// default color for out sockets that have hidden nodes attached to them
#define SOCKETHEIGHT 16							// default height for one socket
#define SOCKETTHRESHOLD 4						// threshold aroud socket for easier selecting

#define SCHEMATICNODECOLOR RGB(224,198,87)		// default color for schematic nodes
#define SCHEMATICNODESELCOLOR RGB(255,255,255)	// default selected color for schematic nodes
#define NODEWIDTH 100							// default node width
#define NODEHEIGHT 24							// default node height
#define NODEEDGEVAL 10.0						// default value for rounded edges for nodes
#define CONNECTIONCOLOR RGB(0,0,0)				// default color of connections
#define SELCONNECTIONCOLOR RGB(255,255,255)		// selected connection color
#define CONNECTIONPENSIZE 1						// default size for connectionPen
#define NODEEDGECOLOR RGB(0,0,0)				// default color node edges
#define EDGEWIDTH 1								// width of edge around nodes
#define NODEPOS Point2(0.0f,0.0f)				// default position for nodes
#define NODELABEL _T("")						// default label for nodes
#define FONTHEIGHT 14							// default height of font
#define ARROWWIDTH 10							// default width of arrow head
#define ARROWHEIGHT 4							// default height of arrow head
#define MINIBITMAPSIZE 40						// default size for mini bitmap
#define BITMAPYOFFSET 6							// Y offset for bitmap placement (and sockets when there is a bitmap)

/* ===========================================================================
	Class Definitions:
   =========================================================================== */

   // ---------------------------- rotatePoly --------------------------------------
   // rotate poly class/function (used for rotating arrow head)
class rotatePoly
{
	float m_x0, m_y0, m_dx, m_dy;

public:
	int m_ds;

	rotatePoly(int x0, int y0, int x1, int y1) // constructor
	{
		m_x0 = (float)x0;
		m_y0 = (float)y0;
		m_dx = (float)(x1 - x0);
		m_dy = (float)(y1 - y0);
		m_ds = (int)sqrt(m_dx * m_dx + m_dy * m_dy);
		if (m_ds == 0) m_ds = 1;
	}

	int RotateX(int x, int y)
	{
		return (int)(x * m_dx / m_ds - y * m_dy / m_ds);
	}

	int RotateY(int x, int y)
	{
		return (int)(x * m_dy / m_ds + y * m_dx / m_ds);
	}
};

/* ----------------------------- Socket -------------------------- */
// This class implements socket node

class schematicNode;								// make sure schematicNode exists before it's used in socket:

class hsocket
{
protected:

	HBRUSH collapsedSocketBrush;				// not used but left in here for 'memory packing' reasons

public:

	hsocket();									// constructor
	~hsocket();									// deconstructor

	int ID;										// keeps track of index of socket, unique per node. 0-based
	int type;									// type of socket. For example 0 = inputSocket, 1 = outputSocket.
	bool flipped;								// swap between drawing the socket on the right or left (type stays the same)
	float radius;								// radius of socket
	schematicNode* owner;						// node that owns this socket
	Tab<hsocket*> toSockets;					// other sockets this socket is connecting to
	Point2 position;							// relative to baseNode that owns this socket
	COLORREF inColor;							// color of in sockets
	COLORREF outColor;							// color of out sockets
	COLORREF collapsedColor;					// color of out collapsed sockets
	COLORREF penColor;							// color of connections
	TSTR label;									// label of socket
	float floatVal;								// Value for this socket (user can store a float or ignore this)
	bool showVal;								// if true, show values behind name of socket
	Control* inputController;					// each socket can store a controller (per max session)
	Point2 labelOffset;							// offset for label
	TSTR dataClass;								// keep track of what class the socket inputs/outputs
	int selectedConnectionIndex;				// Keeps track of wheter a connection coming out of this socket is selected (only outsockets keep track of this since they draw connections)
	TSTR info;									// custom info user can store on node
	Tab<int> connectionWeights;					// keep track of outSocket connection weights 
	bool isHidden;								// if true socket is hidden
	bool drawSeperator;							// if true, draws a line above the socket

// !! REMEMBER TO ADD NEW PROPERTIES TO SAVE / LOAD

	void	drawConnections(HDC hDC, hsocket* sock, int lineType = 0);					// each socket draws connections FROM this socket to another sockets, lineType 0 is bezier, 1 is linear
	void	drawInteractiveConnection(HDC hDC, Point2 pos);				// draws interactive connection while you are attempting a connection
	void	drawSocket(HDC hDC);										// each socket is reponsisble for drawing itself
	Point2	getSocketPosition(schematicNode* owner, hsocket* sock);		// returns target position of this socket
	float	getSocketSize(schematicNode* owner);						// returns size of socket
	RECT	getConnectionRect(hsocket* fromSock, hsocket* toSock);		// gets area in which connection is drawn between 2 sockets
	void	Arrow(HDC hDC, int x0, int y0, int x1, int y1, int width, int height);	// draw arrow head
};

/* ----------------------------- Base Node -------------------------- */
// This class implements schematic node

class schematicNode
{
protected:

	HPEN outlinePen;										// not used but left in here for 'memory packing' reasons

public:

	schematicNode();										// constructor
	~schematicNode();										// deconstructor

	c_sceneData* *sceneData;								// holds scene data
	Tab<hsocket*> inSockets;								// input sockets for this node
	Tab<hsocket*> outSockets;								// output sockets for this node
	Point2 position;										// position relative to rollout control
	TSTR label;												// label of node
	COLORREF color;											// color of node
	COLORREF penColor;										// color of outline
	float edgeVal;											// divide value for rounded edges
	int edgeWidth;											// width of edges around nodes
	int width;												// width of node
	int height;												// height of node
	bool isSelected;										// keeps track of wheter the node is selected or not
	bool collapsed;											// keeps track of wheter the node is collapsed or not
	HBITMAP m_hBitmap;
	Value *m_maxBitMap;
	bool isHidden;											// if true node and all it's connections are hidden
	TSTR bitmapPath;										// store path of bitmap (optional)
	INode* inputNode;										// each schematic node can store a max node (per max session)
	TSTR info;												// custom info user can store on node
	bool posLock;											// if true, node cannot be moved
	Point2 labelOffset;										// position offset for node label
	bool selectable;										// node is selectable or not
	int cWidth;												// collapsed width
	int cHeight;											// collapsed height
	bool showBitmapOnly;									// if true, draw only the bitmap of the node
	bool collapsible;										// if true, node can be collapsed
	schematicNode* parent;									// a node can be parented to another node, the parent is usually the 'group' node
	Tab<schematicNode*> children;							// holds a list of children
	bool drawConnectionsFromParent;							// if a node is parented, but the node is hidden, we dont draw the connections by default. This allows you to still draw them
	int textAlign;											// how to align text, default is DT_VCENTER
	Point2 bitmapSize;										// bitmap scale (offset from default 80x80 size)
	COLORREF labelColor;									// color of node labels
	int viewAlign;											// -1 align to topLeft, +1 align to topRight, 0 draw regular position (default)
	Point2 viewAlignOffset;									// allows an additional offset to be added to the viewAligned nodes
	bool showMiniBitmap;									// if true, shows a mini version of the bitmap when the node is collapsed
	int drawLayer;											// determines the draw order for this node

// !! REMEMBER TO ADD NEW PROPERTIES TO SAVE / LOAD

	void		drawNode(HDC hDC, bool drawShadow);																			// draw shape, color, label, etc. of node (i.e. rectangle)
	hsocket*	addSocket(HWND m_hWnd, schematicNode* sNode, int sType, TSTR sLabel, bool redraw = true);		// adds a new input socket
	RECT		getNodeRect(schematicNode* sNode);															// returns the rectangle that a specified node takes up
	Point2  getNodePosition(schematicNode* sNode, bool viewSpace = true);				// return position of node, viewSpace true means pan and zoom are taken into acount
	Point2	getNodeSize(schematicNode* sNode, bool viewSpace = true);					// returns width and height of node, viewSpace true means pan and zoom are taken into acount
	bool	socketHitCheck(hsocket* sock, Point2 pos);								// checks wheter a socket was hit by the mouse click, mouse click pos relative to control (not node)
	void	drawLabel(HDC hDC, Point2 posTopLeft, Point2 posBottomRight, TSTR sLabel, int align = DT_LEFT, bool useScale = true);		// draw label
	void	flipSocket(hsocket* sock);															// flips a socket left to right
	void	deleteSocket(HWND m_hWnd, schematicNode* sNode, int delID, bool redraw = true);		// delete a socket
	void	setBitmap(HWND m_hWnd, schematicNode* sNode, Value *val);							// adds bitmap to node, supply undefined as val to remove bitmap
	void	hideFromSocket(hsocket* sock, schematicNode* sourceNode, bool hide, bool first);	// hide all nodes from this socket on.
	void	setNode(INode * node);																// store max node
	INode*	getNode();																			// return stored node
	void	setParent(schematicNode* child, schematicNode* par, bool adjustPos = true);			// set a parent node, which this node will be relative to position wise
	schematicNode* getFirstUnhiddenParent(schematicNode* node, schematicNode* firstNode);		// keep searching until we hit an unhidden parent, or NULL. The second node (same node) is used to prevent cyclic loops)

};

#endif