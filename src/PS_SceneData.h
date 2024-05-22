
// Storage class that holds scene data we want all classes to have access to

#ifndef _PSSCENEDATA_
#define _PSSCENEDATA_

class c_sceneData
{
	friend class SchematicControl;
	friend class heliumController;

private:

	~c_sceneData() {};

public:
	c_sceneData()
	{
		LMBPressed = false;
		MMBPressed = false;
		panAmount = Point2(0.0f, 0.0f);
		panBefore = Point2(0.0f, 0.0f);
		MMBDownPos = Point2(0.0f, 0.0f);
		zoomScale = 1.0f;
		zoomDelta = 0.1f;
		createSocketMode = false;
		previousMousePos = Point2(0.0f, 0.0f);
		toMousePos = Point2(0.0f, 0.0f);
		connectionsDrawn = false;
		regionSelection = false;
		regionSelectionPending = false;
		activeNodeIndex = -1;
		activeSocketIndex = -1;
		allowUIDelete = true;
		allowZoom = true;
		allowPan = true;
		tileBG = false;
		showInfo = true;
		drawWeights = false;
		allowRegionSelect = true;
		drawShadows = false;
		drawLastIndex = -1;
		drawMenuBars = false;
		zoomAboutMouse = true;
		nodeTransparency = -1.0f;
		infoText = _T("Helium - www.lumonix.net - (use setInfo to change this text)");
		useDrawLayers = false;

		// !! REMEMBER TO ADD NEW PROPERTIES TO SAVE / LOAD
		//	  The new property might also be copied when property .storageController is set

	};

	bool LMBPressed;						// keeps track of left mouse button state
	bool MMBPressed;						// keeps track of middle mouse button state
	Point2 MMBDownPos;						// keep track of the position where the MMB was pressed down (for panning)
	Point2 LMBDownPos;						// keep track of the position where the LMB was pressed down (for moving nodes)
	Point2 panAmount;						// how much panning is there
	Point2 panBefore;						// keep track of the pan amount before interactive panning started
	float zoomScale;						// how much zoom is there
	float zoomDelta;						// how much zoom is increased woith every mouse scroll
	bool createSocketMode;					// true when the user is creating a socket
	Point2 previousMousePos;				// keeps track of previous mouse pos for redrawing screen during connection draw
	Point2 toMousePos;						// keeps track of where to draw interactive connection to, for paint event
	bool connectionsDrawn;					// keeps track if connections where reDrawn, if so we do a full redraw on mouseUp
	bool regionSelection;					// keeps track of region selection mode
	bool regionSelectionPending;			// if LMBDown we might go into region select mode if the mouse is also moved.
	Point2 regionPos;						// keeps track of region selection start pos
	Point2 previousRegion;					// keeps track of previous end of region for redrawing
	int activeNodeIndex;					// keeps track of the active node's index. (note the active node does not mean selected node)
	int activeSocketIndex;					// keeps track of the active socket
	bool allowUIDelete;						// wheter to allow deleting of nodes in UI or only via MXS
	Point2 socketValPos;					// keep track of where the middle mouse was pressed for changing socket values
	bool allowZoom;							// view may be zoomed
	bool allowPan;							// view may be panned
	bool tileBG;							// tile background image
	bool showInfo;							// show copyright information
	bool drawWeights;						// if true, draw connection weights
	bool allowRegionSelect;					// if false, region selects are not allowed
	bool drawShadows;						// if true, draws shadows for each node
	int drawLastIndex;						// if we want a certain node to be drawn last, we can feed the index here
	bool drawMenuBars;						// if true, draws menubar at top of nodes
	bool zoomAboutMouse;					// if true, adjust pan so we zoom around the mouse cursor
	float nodeTransparency;					// if bigger or equal to 0.0 then, nodes will be drawn with transparency. Set to -1 for no transparency
	TSTR infoText;							// text to draw on bottom of screen
	Rect windowRect;						// size of window
	bool useDrawLayers;						// draw nodes based on their drawLayer integer or not
};

#endif