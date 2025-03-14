#include "PS_Schematic.h"		// create DLL + class definitions

/* ===========================================================================
	Create DLL:
   =========================================================================== */

HMODULE hInstance = NULL;

BOOL APIENTRY
DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	static BOOL controlsInit = FALSE;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// Hang on to this DLL's instance handle:
		hInstance = hModule;
		DisableThreadLibraryCalls(hModule);
		break;
	}
	return(TRUE);
}

__declspec(dllexport) void LibInit()
{
	// do any setup here:
	SchematicInit();			// register our new rollout control
}

__declspec(dllexport) const TCHAR * LibDescription() { return _T("Maxscript Schematic Control"); }

__declspec(dllexport) ULONG LibVersion() { return VERSION_3DSMAX; }

/* ===========================================================================
	Schematic Control UI:
   =========================================================================== */

   // ----------------------------- constructor ----------------------------------------
   // Constructor:
SchematicControl::SchematicControl(Value* name, Value* caption, Value** keyparms, int keyparm_count)
	: RolloutControl(name, caption, keyparms, keyparm_count)
{
	tag = class_tag(SchematicControl);	// macro
	m_hWnd = NULL;
	// set default scene data:
	sceneData = new c_sceneData;

	backgroundColor = ColorMan()->GetColor(kBackground);
	hbrBkGnd = CreateSolidBrush(backgroundColor);
//	controller = NULL;
	m_hBitmap = NULL;
	m_maxBitMap = NULL;
	bitmapPath = _T("");
	connectionSocket = NULL;
	selectedSocketOut = NULL;

	schematicNodes = new Tab<schematicNode*>;
	schematicNodes->SetCount(0);
}

void SchematicControl::cleanUpNodes()
{
	if (schematicNodes != NULL)
	{
		int nc = schematicNodes->Count();
		for (int i = 0; i < nc; i++)
		{
			int rI = nc - 1 - i;						// we want to reverse this loop to select top nodes first, -1 due to 0-based
			delete (*schematicNodes)[rI];				// schematic node's deconstructor will cleanup in/out sockets. (*schematicNodes) de-refference (go to the data that is at the memory address, since a pointer is an address)
			schematicNodes->Delete(rI, 1);
		}
		schematicNodes->SetCount(0);
	}
}

// --------------------------------de constructor -----------------------------------
// DeConstructor:
SchematicControl::~SchematicControl()
{
	cleanUpNodes();
	delete schematicNodes;
	schematicNodes = NULL;

	delete sceneData;

	if (m_hBitmap) DeleteObject(m_hBitmap);
	if (m_maxBitMap) m_maxBitMap = NULL;

	DeleteObject(hbrBkGnd);
	DeleteObject(hbmMem);
	DeleteDC(hdcMem);
}

void SchematicControl::collect()
{
	delete this;
}

// ------------------------------ wndProc ------------------------------------
// window message processing:
LRESULT CALLBACK SchematicControl::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// create new pointer to schematic control:

	SchematicControl *ctrl = DLGetWindowLongPtr<SchematicControl*>(hWnd);

	if (ctrl == NULL && msg != WM_CREATE)													// if control wasn't created yet
		return DefWindowProc(hWnd, msg, wParam, lParam);									// The DefWindowProc function calls the default window procedure to provide default processing for any window messages that an application does not process. This function ensures that every message is processed.

	switch (msg)
	{
	case WM_CREATE:	// message send during creation of this control, we grab ctrl here for use for messages later
	{
		LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lParam;
		ctrl = (SchematicControl*)lpcs->lpCreateParams;

		DLSetWindowLongPtr(hWnd, ctrl);

		break;
	}

	case WM_SETFOCUS:
	{
		DisableAccelerators();	// stop max from responding to keyboard
		return false;
	}

	case WM_KILLFOCUS:
	{
		// ReleaseCapture(); // this appears to cause problems when switching between helium and say a spinner
		EnableAccelerators();	// return control of keyboard to max
		return false;
	}

	case WM_KEYDOWN:
	{
		if (wParam == VK_F12) //VK_RETURN //VK_ESCAPE
			MessageBox(NULL, _T("Helium, www.lumonix.net"), _T("Information"), 0);

		ctrl->c_keyboardToMXS(wParam, false);
		return (LRESULT)1; // we processed message
	}

	case WM_KEYUP:
	{
		if (wParam == VK_DELETE) //VK_RETURN //VK_ESCAPE
			ctrl->deleteSelectedNodes();	// this could also delete the selected connection, if one exists

		ctrl->c_keyboardToMXS(wParam, true);
		return (LRESULT)1; // we processed message
	}

	case WM_ERASEBKGND:
		return (LRESULT)1; // Say we handled it, to prevent flicker.

	case WM_LBUTTONDBLCLK:
		return ctrl->LButtonDblclk((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_LBUTTONDOWN:
		return ctrl->LButtonDown((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_LBUTTONUP:
		return ctrl->LButtonUp((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_MBUTTONDOWN:
		return ctrl->MButtonDown((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_MBUTTONUP:
		return ctrl->MButtonUp((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_MOUSEMOVE:
		return ctrl->mouseMoved((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_MOUSEWHEEL: //0x020A:
		return ctrl->mouseScroll((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_RBUTTONDOWN:
		return ctrl->RButtonDown((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_RBUTTONUP:
		return ctrl->RButtonUp((short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);

	case WM_PAINT:
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);		// prepair for painting and prep info for painting (i.e hWnd)
		ctrl->Paint(ps);			// call paint function below
		EndPaint(hWnd, &ps);
		return FALSE;				// we processed paint message

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return FALSE;
}

// ---------------------------- left button down --------------------------------------
LRESULT SchematicControl::LButtonDblclk(int xPos, int yPos, int fwKeys)
{
	SetFocus(m_hWnd);	// make sure we receive keyboard input
	sceneData->LMBPressed = false;

	bool controlPressed = false;								// used for adding to selection
	if (GetKeyState(VK_CONTROL) < 0) { controlPressed = true; }	// getKeyState returns a high-order bit.
	bool altPressed = false;									// used for deslecting nodes
	if (GetKeyState(VK_MENU) < 0) { altPressed = true; }
	bool shiftPressed = false;									// used for flipping sockets
	if (GetKeyState(VK_SHIFT) < 0) { shiftPressed = true; }

	if (altPressed == false && controlPressed == false && shiftPressed == false) // we likely did not intend to collapse the node if one of those buttons was held
	{
		Point2 mPos;
		mPos.x = (float)xPos;
		mPos.y = (float)yPos;

		// COLLAPSE:
		for (int i = 0; i < schematicNodes->Count(); i++) 	// node hit-check:
		{
			int rI = schematicNodes->Count() - 1 - i; // we want to reverse this loop to select top nodes first, -1 due to 0-based
			bool nodeHit = nodeHitCheck((*schematicNodes)[rI], mPos);
			if (nodeHit == true) // we do not allow connections to be made with collapsed nodes
			{
				if ((*schematicNodes)[rI]->collapsible == true)
				{
					if ((*schematicNodes)[rI]->collapsed == true)
						(*schematicNodes)[rI]->collapsed = false;
					else
						(*schematicNodes)[rI]->collapsed = true;
					Invalidate();
				}
				break;
			}
		}
	}

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_LButtonDblclk, NULL, 0);

	return TRUE;
}

#define		hmax(a,b)  (((a) > (b)) ? (a) : (b))	// MAX function, pick whichever value is highest
// Test if a point is on a line
//Given two points P and Q and a test point T 
//	return 0 if T is not on the (infinite) line PQ 
//			1 if T is on the open ray P 
//			2 if T is within the line segment PQ 
//			3 if T is on the open ray Q 
int pointOnLine2D(int Px, int Py, int Qx, int Qy, int Tx, int Ty)
{
	if (ABS((Qy - Py) * (Tx - Px) - (Ty - Py) * (Qx - Px)) >= hmax(ABS(Qx - Px), ABS(Qy - Py))) return 0;
	if ((Qx < Px && Px < Tx) || (Qy < Py && Py < Ty)) return 1;
	if ((Tx < Px && Px < Qx) || (Ty < Py && Py < Qy)) return 1;
	if ((Px < Qx && Qx < Tx) || (Py < Qy && Qy < Ty)) return 3;
	if ((Tx < Qx && Qx < Px) || (Ty < Qy && Qy < Py)) return 3;
	return 2;
}

// ---------------------------- left button down --------------------------------------
LRESULT SchematicControl::LButtonDown(int xPos, int yPos, int fwKeys)
{
	SetFocus(m_hWnd);
	sceneData->regionSelection = false;
	sceneData->regionSelectionPending = false;

	sceneData->LMBPressed = true;
	sceneData->LMBDownPos.x = (float)xPos;
	sceneData->LMBDownPos.y = (float)yPos;

	sceneData->createSocketMode = false;
	sceneData->connectionsDrawn = false;

	Point2 mPos;
	mPos.x = (float)xPos;
	mPos.y = (float)yPos;

	connectionSocket = NULL;
	selectedSocketOut = NULL;

	sceneData->regionPos = mPos;
	sceneData->previousRegion = mPos;

	bool controlPressed = false;								// used for adding to selection
	if (GetKeyState(VK_CONTROL) < 0) { controlPressed = true; }	// getKeyState returns a high-order bit.

	bool altPressed = false;									// used for deslecting nodes
	if (GetKeyState(VK_MENU) < 0) { altPressed = true; }

	bool shiftPressed = false;									// used for flipping sockets
	if (GetKeyState(VK_SHIFT) < 0) { shiftPressed = true; }

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_LButtonDown, NULL, 0);

	// do hit check on schematic nodes:
	// the right select / move behaviour we are after is:
	// click/move on a selected nodes and all selected nodes moves.
	// click/move on a unselected node, this node gets selected, all others get unselected
	// control click/move adds hit node to selection and moves all selected nodes.
	// if no nodes were hit, we unselect all nodes
	RECT drawRect;
	int nodesHit = 0;
	int nodeHitIndex = -1;

	bool inSocketHit = false;
	bool outSocketHit = false;

	int Lstart = 0;
	if (sceneData->useDrawLayers) { Lstart = 2; }

	for (int L = Lstart; L >= 0; L--)	// loop each draw layer (if used)
	{
		for (int i = 0; i < schematicNodes->Count(); i++)	// loop all nodes
		{
			int rI = schematicNodes->Count() - 1 - i; // we want to reverse this loop to select top nodes first, -1 due to 0-based

			// first check if we want to use Layers and if we do, don't bother hit-checking any node not in this layer:
			if (sceneData->useDrawLayers && (*schematicNodes)[rI]->drawLayer != L) continue;

			bool nodeHit = nodeHitCheck((*schematicNodes)[rI], mPos);
			if (nodeHit == true && (*schematicNodes)[rI]->collapsed == false)	// we do not allow new connections to be made from collapsed nodes
			{
				// SOCKET HIT CHECK:

				// now do a hitcheck on the nodes sockets to check wheter the node was selected, or a sockets was clicked for
				// creating a new connection:
				for (int s = 0; s < (*schematicNodes)[rI]->inSockets.Count(); s++)
				{
					inSocketHit = (*schematicNodes)[rI]->socketHitCheck((*schematicNodes)[rI]->inSockets[s], mPos);
					if (inSocketHit == true)
					{
						connectionSocket = (*schematicNodes)[rI]->inSockets[s];

						if (shiftPressed == false && altPressed == false && controlPressed == false)	// shift is used for flipping sockets, alt is used for hidden all nodes connected to this socket, control used for unhiding
							sceneData->createSocketMode = true;
						else if (shiftPressed == true)
						{
							(*schematicNodes)[rI]->flipSocket(connectionSocket);
							Invalidate(); //redraw screen
						}
						else if (altPressed == true || controlPressed == true)
						{
							if (connectionSocket->toSockets.Count() > 0)
							{
								(*schematicNodes)[rI]->hideFromSocket(connectionSocket, connectionSocket->owner, altPressed, true);
								Invalidate(); //redraw screen
							}
						}

						break;
					}
				}
				for (int s = 0; s < (*schematicNodes)[rI]->outSockets.Count(); s++)
				{
					outSocketHit = (*schematicNodes)[rI]->socketHitCheck((*schematicNodes)[rI]->outSockets[s], mPos);
					if (outSocketHit == true)
					{
						connectionSocket = (*schematicNodes)[rI]->outSockets[s];

						if (shiftPressed == false)	// shift is used for flipping sockets
							sceneData->createSocketMode = true;
						else
						{
							(*schematicNodes)[rI]->flipSocket(connectionSocket);
							Invalidate(); //redraw screen
						}

						break;
					}
				}

				// if we started a new connection, send message to mxs:
				if (sceneData->createSocketMode)
				{
					int fromNodeIndex = rI + 1;	// +1 for MXS
					int fromSocketID = connectionSocket->ID + 1;
					int socketType = connectionSocket->type;
					ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
					Value**		arg_list;
					value_local_array(arg_list, 3);
					arg_list[0] = Integer::intern(fromNodeIndex);
					arg_list[1] = Integer::intern(fromSocketID);
					arg_list[2] = Integer::intern(socketType);
					run_event_handler(parent_rollout, n_connectionStarted, arg_list, 3);
				}

			}//end if nodeHit

			if (nodeHit == true && inSocketHit == false && outSocketHit == false) // only select node if socket were not hit
			{
				nodeHitIndex = rI;
				nodesHit += 1;

				//SELECT:
				// hit node was unselected:
				if ((*schematicNodes)[rI]->isSelected == false && altPressed == false)
				{
					(*schematicNodes)[rI]->isSelected = true; // select hit node
					drawRect = (*schematicNodes)[rI]->getNodeRect((*schematicNodes)[rI]);	// redraw:
					InvalidateRect(m_hWnd, &drawRect, FALSE);

					if (controlPressed == false)		 // unselect all others
					{
						for (int n = 0; n < schematicNodes->Count(); n++)
						{
							if (n != rI)
							{
								(*schematicNodes)[n]->isSelected = false;
								drawRect = (*schematicNodes)[n]->getNodeRect((*schematicNodes)[n]);	// redraw:
								InvalidateRect(m_hWnd, &drawRect, FALSE);
							}
						}
						break;
					}
				}//end if schem

			// DE-SELECT:
				else if (altPressed == true)
				{
					(*schematicNodes)[rI]->isSelected = false; // select hit node
					drawRect = (*schematicNodes)[rI]->getNodeRect((*schematicNodes)[rI]);	// redraw:
					InvalidateRect(m_hWnd, &drawRect, FALSE);
				}

			}//end if nodeHit

		// one of the sockets was hit:
			else if (inSocketHit == true || outSocketHit == true)
			{
				sceneData->toMousePos = mPos;
				sceneData->previousMousePos = mPos;
				break; // stop searching if a socket was hit
			}

		}//end for i
		if (nodeHitIndex != -1) break;	// stop looping the draw layers if a node was hit
	}//end for L

	if (nodeHitIndex != -1) // only select node if socket were not hit
	{
		c_NodeClicked(nodeHitIndex);		// return node index to MXS
	}

	// check if connection was hit:	
	bool connectionHit = false;
	if (inSocketHit == false && outSocketHit == false)	// even if a in/out socket was clicked we deselect so that the nodes don't get moved. This could also be checked for onMouseMove, but this works ok for now
	{
		if (controlPressed == false && altPressed == false)
		{
			// check if we hit a connection between sockets:
			// this is a little trickier. We have to go through all nodes and all its outSockets and see
			// if the mouse click happened on the curve between 2 sockets.
			for (int i = 0; i < schematicNodes->Count(); i++)
			{
				if ((*schematicNodes)[i]->isHidden == false)	// skip searching hidden nodes, eventhough nodes can be 'grouped' that way, we skip collision detection on grouped nodes, for now anyway
				{
					for (int s = 0; s < (*schematicNodes)[i]->outSockets.Count(); s++)
					{
						hsocket *oSock = (*schematicNodes)[i]->outSockets[s];
						for (int t = 0; t < oSock->toSockets.Count(); t++)
						{
							hsocket *tSock = oSock->toSockets[t];

							Point2 tp = tSock->getSocketPosition(tSock->owner, tSock);
							Point2 op = oSock->getSocketPosition(oSock->owner, oSock);
							// we actually pretend our connection curve is linear, we do a fast 2D point-on-line test:
							// we'll search within a thresHold and see if we hit the curve:
							int hitRes = 0;
							int TH = 8;
							for (int a = -TH; a < TH; a++)	// threshold searching
							{
								for (int b = -TH; b < TH; b++)
								{
									int X = (int)(mPos.x + a);
									int Y = (int)(mPos.y + b);
									int curveHit = pointOnLine2D((int)tp.x, (int)tp.y, (int)op.x, (int)op.y, X, Y);
									if (curveHit == 2) { hitRes = 2; break; }
								}
							}
							if (hitRes == 2 && connectionHit == false && nodesHit == 0) // a hit happened!
							{
								if (oSock->selectedConnectionIndex != t)	// only select the connection if it wasn't already
								{
									oSock->selectedConnectionIndex = t;
									selectedSocketOut = oSock;
									int toNodeIndex = getSocketOwnerID(tSock);
									if ((*schematicNodes)[toNodeIndex]->isHidden == false)	// again, only accept hit if the to node was also not hidden
									{
										connectionHit = true;
										Invalidate();
										c_ConnectionSelectionChanged(i + 1, toNodeIndex + 1, tSock->ID + 1, oSock->ID + 1);
										break;
									}
								}
							}
							// reset this sockets selectionIndex if we reach the end and no sockets were hit:
							if (t == (oSock->toSockets.Count() - 1) && oSock->selectedConnectionIndex != -1)
							{
								oSock->selectedConnectionIndex = -1;	// no hit, so make sure line is set to not be selected
								Invalidate();
							}
						}//end for t
					}//end for s
				}//end if ishidden
			}//end for i
		}
	}

	// UNSELECT ALL + REGION SELECTION:
	// unselect all nodes if no nodes where hit:
	if (nodesHit == 0 && inSocketHit == false && outSocketHit == false)	// even if a in/out socket was clicked we deselect so that the nodes don't get moved. This could also be checked for onMouseMove, but this works ok for now
	{
		if (controlPressed == false && altPressed == false)
		{
			// deselect all nodes:
			for (int i = 0; i < schematicNodes->Count(); i++)
			{
				(*schematicNodes)[i]->isSelected = false;
				drawRect = (*schematicNodes)[i]->getNodeRect((*schematicNodes)[i]);	// redraw:
				InvalidateRect(m_hWnd, &drawRect, FALSE);
			}
		}

		if (connectionHit == false && sceneData->allowRegionSelect == true)
			sceneData->regionSelectionPending = true;
	}

	SetCapture(m_hWnd);
	return TRUE;
}

// ---------------------------- left button up --------------------------------------
LRESULT SchematicControl::LButtonUp(int xPos, int yPos, int fwKeys)
{
	sceneData->LMBPressed = false;
	sceneData->regionSelectionPending = false;

	Point2 mPos;
	mPos.x = (float)xPos;
	mPos.y = (float)yPos;

	// socket hit check:
	if (sceneData->createSocketMode == true)
	{

		bool dragToEmpty = true;	// we send a special event to MXS if the user was dragging a connection, but let go in an empty area

		// node hit-check:
		for (int i = 0; i < schematicNodes->Count(); i++)
		{
			int rI = schematicNodes->Count() - 1 - i; // we want to reverse this loop to select top nodes first, -1 due to 0-based
			bool nodeHit = nodeHitCheck((*schematicNodes)[rI], mPos);
			if (nodeHit == true && (*schematicNodes)[rI]->collapsed == false) // we do not allow connections to be made with collapsed nodes
			{
				dragToEmpty = false;

				// we only allow inSockets to be connected to outSockets and visa versa:
				if (connectionSocket->type == 1)	// the socket we draw from is an OUT SOCKET:
				{
					connectionSocket->selectedConnectionIndex = -1;
					for (int s = 0; s < (*schematicNodes)[rI]->inSockets.Count(); s++)
					{
						// this is a connection from our selected out-socket to a in-socket:
						bool inSocketHit = (*schematicNodes)[rI]->socketHitCheck((*schematicNodes)[rI]->inSockets[s], mPos);
						if (inSocketHit == true)
						{
							// check if connection already exists, if so, break it:
							bool connectionExists = findAndDeleteConnection(connectionSocket, (*schematicNodes)[rI]->inSockets[s]);
							//store socket connections both in from and to nodes:
							if (connectionExists == false)
							{
								connectionSocket->toSockets.Append(1, &(*schematicNodes)[rI]->inSockets[s]);
								(*schematicNodes)[rI]->inSockets[s]->toSockets.Append(1, &connectionSocket);
								int cw = 100;
								connectionSocket->connectionWeights.Append(1, &cw);		// default connectionWeight to 100, only added to outSockets
							}
							// send msg to MXS:
							int status = connectionExists ? 0 : 1;
							int fromNodeIndex = getSocketOwnerID(connectionSocket);
							c_ConnectionChanged(fromNodeIndex + 1, rI + 1, (*schematicNodes)[rI]->inSockets[s]->ID + 1, connectionSocket->ID + 1, status, (*schematicNodes)[rI]->inSockets[s]->toSockets.Count());
							break;
						}
					}
				}
				else if (connectionSocket->type == 0)	// the socket we draw from is an IN SOCKET:
				{
					for (int s = 0; s < (*schematicNodes)[rI]->outSockets.Count(); s++)
					{
						// this connection needs to be reversed as our selected socket is an in-socket:
						bool outSocketHit = (*schematicNodes)[rI]->socketHitCheck((*schematicNodes)[rI]->outSockets[s], mPos);
						if (outSocketHit == true)
						{
							(*schematicNodes)[rI]->outSockets[s]->selectedConnectionIndex = -1;
							// check if connection already exists, if so, break it:
							bool connectionExists = findAndDeleteConnection(connectionSocket, (*schematicNodes)[rI]->outSockets[s]);

							//store socket connections both in from and to nodes:
							if (connectionExists == false)
							{
								(*schematicNodes)[rI]->outSockets[s]->toSockets.Append(1, &connectionSocket);
								int cw = 100;
								(*schematicNodes)[rI]->outSockets[s]->connectionWeights.Append(1, &cw);			// default connectionWeight to 100, only added to outSockets
								connectionSocket->toSockets.Append(1, &(*schematicNodes)[rI]->outSockets[s]);
							}
							// send msg to MXS:
							int status = connectionExists ? 0 : 1;
							int toNodeIndex = getSocketOwnerID(connectionSocket);
							c_ConnectionChanged(rI + 1, toNodeIndex + 1, connectionSocket->ID + 1, (*schematicNodes)[rI]->outSockets[s]->ID + 1, status, connectionSocket->toSockets.Count());
							break;
						}
					}
				}
				break;
			}
		}//end for

	// redraw screen
		Invalidate(); // A complete redraw of the view is ok here.

		if (dragToEmpty)	// user dragged connection to empty area, send event to MXS:
		{
			int fromNodeIndex = getSocketOwnerID(connectionSocket) + 1;	// +1 for MXS
			int fromSocketID = connectionSocket->ID + 1;
			int socketType = connectionSocket->type;
			ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
			Value**		arg_list;
			value_local_array(arg_list, 3);
			arg_list[0] = Integer::intern(fromNodeIndex);
			arg_list[1] = Integer::intern(fromSocketID);
			arg_list[2] = Integer::intern(socketType);
			run_event_handler(parent_rollout, n_connectToEmpty, arg_list, 3);
		}
	}

	// Region Selection:
	else if (sceneData->regionSelection == true)
	{
		selectedSocketOut = NULL;	// reset this in case delete is pressed right after to delete nodes, not connections

		bool addToSelection = true;
		if (GetKeyState(VK_MENU) < 0) { addToSelection = false; }

		Point2 topLeft = sceneData->regionPos;
		Point2 bottomRight = mPos;

		// since a region selection can be made from bottom to top as well, we need to check:
		if (sceneData->regionPos.x > mPos.x)
		{
			topLeft.x = mPos.x;
			bottomRight.x = sceneData->regionPos.x;
		}
		if (sceneData->regionPos.y > mPos.y)
		{
			topLeft.y = mPos.y;
			bottomRight.y = sceneData->regionPos.y;
		}

		// select any node that is within our region selection:
		for (int i = 0; i < schematicNodes->Count(); i++)
		{
			if ((*schematicNodes)[i]->selectable == true)
			{
				Point2 nodePos = (*schematicNodes)[i]->getNodePosition((*schematicNodes)[i]);
				Point2 nodeSize = (*schematicNodes)[i]->getNodeSize((*schematicNodes)[i]);

				// check if node is entirely inside selection rectangle:
				if ((nodePos.x + nodeSize.x) < bottomRight.x &&
					nodePos.x > topLeft.x &&

					nodePos.y > topLeft.y &&
					(nodePos.y + nodeSize.y) < bottomRight.y)
				{
					(*schematicNodes)[i]->isSelected = addToSelection;
				}
			}
		}
		Invalidate();

		// Send to Maxscript:
		run_event_handler(parent_rollout, n_regionSelected, NULL, 0);
	}

	// redraw screen if connections where redrawn to make sure we don't leave any pixels behind:
	else if (sceneData->connectionsDrawn == true)
	{
		Invalidate();
	}

	// restore:
	sceneData->createSocketMode = false;
	sceneData->regionSelection = false;
	connectionSocket = NULL;

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_LButtonUp, NULL, 0);

	ReleaseCapture();
	return TRUE;
}

// ---------------------------- middle button down --------------------------------------
LRESULT SchematicControl::MButtonDown(int xPos, int yPos, int fwKeys)
{
	if (sceneData->createSocketMode == true) return FALSE;	// we'd crash if the user pressed the MM when creating a connection

	SetFocus(m_hWnd);	// make sure we receive keyboard input

	// do a node and socket hit check
	// if a outsocket or it's label was hit, we want to change the stored value by dragging the mouse
	Point2 mPos;
	mPos.x = (float)xPos;
	mPos.y = (float)yPos;

	connectionSocket = NULL;

	bool outSocketHit = false;
	for (int i = 0; i < schematicNodes->Count(); i++)
	{
		int rI = schematicNodes->Count() - 1 - i; // we want to reverse this loop to select top nodes first, -1 due to 0-based
		bool nodeHit = nodeHitCheck((*schematicNodes)[rI], mPos);
		if (nodeHit == true && (*schematicNodes)[rI]->collapsed == false)	// we do not allow new connections to be made from collapsed nodes
		{
			// SOCKET HIT CHECK:

			// now do a hitcheck on the nodes sockets to check wheter the node was selected, or a sockets was clicked for
			// creating a new connection:
			for (int s = 0; s < (*schematicNodes)[rI]->outSockets.Count(); s++)
			{
				outSocketHit = (*schematicNodes)[rI]->socketHitCheck((*schematicNodes)[rI]->outSockets[s], mPos);
				if (outSocketHit == true)
				{
					connectionSocket = (*schematicNodes)[rI]->outSockets[s];
					sceneData->socketValPos = mPos;
					break;
				}
			}
		}
	}//end for

	if (outSocketHit == false)
	{
		sceneData->MMBDownPos.x = (float)xPos; // store for panning
		sceneData->MMBDownPos.y = (float)yPos;
		sceneData->MMBPressed = true;
		sceneData->panBefore = sceneData->panAmount;
	}

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_MButtonDown, NULL, 0);

	SetCapture(m_hWnd);	// makes sure that our window still gets the mouse messages, even if the mouse is over another window
	return TRUE;
}

// ---------------------------- middle button up --------------------------------------
LRESULT SchematicControl::MButtonUp(int xPos, int yPos, int fwKeys)
{
	if (sceneData->createSocketMode == true) return FALSE;	// we'd crash if the user pressed the MM when creating a connection

	sceneData->MMBPressed = false;
	sceneData->panBefore = sceneData->panAmount;

	connectionSocket = NULL;

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_MButtonUp, NULL, 0);

	ReleaseCapture();	// return mouse messages back to normal
	return TRUE;
}

// ---------------------------- mouse move --------------------------------------
LRESULT SchematicControl::mouseMoved(int xPos, int yPos, int fwKeys)
{
	// REGION SELECT:
	if (sceneData->regionSelectionPending == true)
	{
		sceneData->regionSelection = true;
		sceneData->toMousePos.x = (float)xPos;	// stored for Paint event which will draw the new rectangle
		sceneData->toMousePos.y = (float)yPos;

		// clear previous drawn region:
		InvalidateSelectionRegion();
		// drawing the selection rectangle is done in Paint
	}

	// DRAW CONNECTION:
	else if (sceneData->createSocketMode == true)
	{
		if (connectionSocket != NULL)
		{
			sceneData->toMousePos.x = (float)xPos;	// stored for Paint event which will draw the new line
			sceneData->toMousePos.y = (float)yPos;

			// clear previous drawn line:
			RECT preRect;
			Point2 conPos = connectionSocket->getSocketPosition(connectionSocket->owner, connectionSocket);
			preRect.left = (LONG)conPos.x;
			preRect.right = (LONG)sceneData->previousMousePos.x;
			preRect.top = (LONG)conPos.y;
			preRect.bottom = (LONG)sceneData->previousMousePos.y;

			InvalidateArea(preRect, 20);	// Paint event will then draw new line, use threshold to compensate for edge pixels
		}
	}

	// MOVE:
	else if (sceneData->LMBPressed == true)
	{
		RECT preRect = { 0, 0, 0, 0 };
		RECT postRect;

		// move selected nodes:
		Point2 LPos;
		LPos.x = (float)xPos;
		LPos.y = (float)yPos;

		// compensation for moving nodes with zoom:
		float zoomComp = 1.0f / sceneData->zoomScale;
		Point2 posDif = (LPos - sceneData->LMBDownPos) * zoomComp;

		// check to see how many nodes are selected
		// if we have more then a few, redrawing the entire screen will be faster and so we will:
		int selNodeCount = 0;
		for (int i = 0; i < schematicNodes->Count(); i++)
		{
			if ((*schematicNodes)[i]->isSelected == true) selNodeCount += 1;
		}
		bool drawEachNode = true;
		int nodeDrawTH = NODEMOVEDRAWTH;
		if (selNodeCount > nodeDrawTH) drawEachNode = false;

		// invalidate areas in which moved nodes where (both old position and new position):
		for (int i = 0; i < schematicNodes->Count(); i++)
		{
			if ((*schematicNodes)[i]->children.Count() > 0) drawEachNode = false;	// redraw entire screen because children also need to be moved

			if ((*schematicNodes)[i]->isSelected == true)
			{
				if (drawEachNode == true)
				{
					preRect = (*schematicNodes)[i]->getNodeRect((*schematicNodes)[i]);	// redraw:
				}

				// make sure children dont move twice the amount because both them and their parent are selected:
				bool moveChild = true;
				if ((*schematicNodes)[i]->parent != NULL && (*schematicNodes)[i]->parent->isSelected == true) moveChild = false;

				if ((*schematicNodes)[i]->posLock == false && moveChild == true)
					(*schematicNodes)[i]->position += posDif;

				if (drawEachNode == true)
				{
					postRect = (*schematicNodes)[i]->getNodeRect((*schematicNodes)[i]);
					InvalidateArea(preRect, 3);
					InvalidateArea(postRect, 3);
				}

				// invalidate to and from connection areas as well
				// connections to inSocket:
				for (int s = 0; s < (*schematicNodes)[i]->inSockets.Count(); s++)
				{
					// first invalidate all the connection from the in Socket:
					for (int f = 0; f < (*schematicNodes)[i]->inSockets[s]->toSockets.Count(); f++)
					{
						sceneData->connectionsDrawn = true;
						/*	clear previous drawn line:
							note we are redrawing the new area where the connection is, not where it was before moving.
							this isn't really correct and really fast movement can skip pixels
							the right solution would be to store the pre-move position for all sockets and connections
							involved and redraw that area, but we take the easy way out for now.
							we redraw the whole screen on mouse up anyway, so it's not so bad.
							plus small movement will still be redrawn ok because we have a draw threshold. */
						if (drawEachNode == true)
						{
							hsocket* fromSocket = (*schematicNodes)[i]->inSockets[s]->toSockets[f];
							hsocket* toSocket = (*schematicNodes)[i]->inSockets[s];
							RECT connectionArea = fromSocket->getConnectionRect(fromSocket, toSocket);
							InvalidateArea(connectionArea, 20);	// Paint event will then draw new line, use threshold to compensate for edge pixels
						}
					}
				}
				// connections to outSocket:
				for (int s = 0; s < (*schematicNodes)[i]->outSockets.Count(); s++)
				{
					// first invalidate all the connection from the in Socket:
					for (int f = 0; f < (*schematicNodes)[i]->outSockets[s]->toSockets.Count(); f++)
					{
						sceneData->connectionsDrawn = true;
						// clear previous drawn line:
						// (see above for additional details on drawing)
						if (drawEachNode == true)
						{
							hsocket* fromSocket = (*schematicNodes)[i]->outSockets[s]->toSockets[f];
							hsocket* toSocket = (*schematicNodes)[i]->outSockets[s];
							RECT connectionArea = fromSocket->getConnectionRect(fromSocket, toSocket);
							InvalidateArea(connectionArea, 20);	// Paint event will then draw new line, use threshold to compensate for edge pixels
						}
					}
				}
			}
		}

		if (drawEachNode == false) Invalidate(); // if we did not redraw each node, then invalidate entire screen
		// reset mouse pos:
		sceneData->LMBDownPos.x = (float)xPos;
		sceneData->LMBDownPos.y = (float)yPos;
	}

	// ADJUST SOCKET VALUE:
	else if (connectionSocket != NULL)
	{
		bool controlPressed = false;
		if (GetKeyState(VK_CONTROL) < 0) { controlPressed = true; }

		float addVal = (xPos - sceneData->socketValPos.x) + (yPos - sceneData->socketValPos.y);
		float divVal = 100.0f;
		if (controlPressed == true) divVal = 1.0f;

		// either set the value for the socket, or the controller if we have one:
		if (connectionSocket->inputController == NULL || connectionSocket->inputController->SuperClassID() != SClass_ID(CTRL_FLOAT_CLASS_ID))
		{
			float newVal = (addVal / divVal);
			connectionSocket->floatVal += newVal;
			// we have to changed the number of digits here so we don't get funky numbers like 1.923454e-009 etc.

			float mult = pow(10.0f, 2);	// maximum of 2 digits accuracy is enough?
			connectionSocket->floatVal = (floorf((connectionSocket->floatVal * mult) + 0.5f)) / mult;
		}
		else // we only update the controller value if it is a float controller:
		{
			float curFloat;
			connectionSocket->inputController->GetValue((GetCOREInterface()->GetTime()), &curFloat, FOREVER);
			curFloat += addVal / divVal;
			connectionSocket->inputController->SetValue((GetCOREInterface()->GetTime()), &curFloat, 1, CTRL_ABSOLUTE);
			// maybe some sort of message needs to be send here to update the controller
			// I thought notifydependents would be it and it might be, but the view is not redrawn
			// not sure why, maybe because our window has focus?
			connectionSocket->inputController->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
		}

		sceneData->socketValPos.x = (float)xPos;
		sceneData->socketValPos.y = (float)yPos;
		RECT postRect = connectionSocket->owner->getNodeRect(connectionSocket->owner);
		InvalidateArea(postRect, 3);

		// send msg to MXS:
		ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
		Value**	arg_list;
		value_local_array(arg_list, 2);
		arg_list[0] = Integer::intern((getSocketOwnerID(connectionSocket) + 1));	// +1 to keep mxs 1-based
		arg_list[1] = Integer::intern((connectionSocket->ID + 1));				// +1 to keep mxs 1-based
		run_event_handler(parent_rollout, n_socketValueChanged, arg_list, 2);
	}

	// PAN:
	else if (sceneData->MMBPressed == true && sceneData->allowPan == true)
	{
		sceneData->panAmount.x = sceneData->panBefore.x + (xPos - sceneData->MMBDownPos.x);
		sceneData->panAmount.y = sceneData->panBefore.y + (yPos - sceneData->MMBDownPos.y);
		Invalidate();
	}

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_mouseMoved, NULL, 0);

	return TRUE;
}

// ---------------------------- mouse move --------------------------------------
LRESULT SchematicControl::mouseScroll(int xPos, int yPos, int fwKeys)
{
	if (sceneData->allowZoom == true)
	{
		// increase / decrease zoom:
		int wheelDir = (fwKeys / WHEEL_DELTA);	// direction of wheel pos or neg.

		float oldZoom = sceneData->zoomScale;

		if (wheelDir > 0) sceneData->zoomScale += sceneData->zoomDelta;	// roll up
		else sceneData->zoomScale -= sceneData->zoomDelta; // roll fown

		if (sceneData->zoomScale < 0.19f) sceneData->zoomScale = 0.2f;
		else if (sceneData->zoomAboutMouse)
		{
			// need pan compensation to try and keep nodes sort of near the mouse here //
			// we need to figure out the mouse position relative to the rollout control
			// then we need to add the current PAN to the mouse position and that new point
			// is the position we use

			POINT mPoint;
			mPoint.x = xPos;
			mPoint.y = yPos;
			ScreenToClient(m_hWnd, &mPoint); // convert screen mouse to our window

			float zoomCompOld = 1.0f + (1.0f - oldZoom);
			float zoomCompNew = 1.0f + (1.0f - sceneData->zoomScale);

			Point2 oldP;
			oldP.x = ((mPoint.x - sceneData->panAmount.x) / oldZoom);
			oldP.y = ((mPoint.y - sceneData->panAmount.y) / oldZoom);

			Point2 newP;
			newP.x = ((mPoint.x - sceneData->panAmount.x) / sceneData->zoomScale);
			newP.y = ((mPoint.y - sceneData->panAmount.y) / sceneData->zoomScale);

			Point2 dif = (newP - oldP);

			sceneData->panAmount += dif * sceneData->zoomScale;
		}

		Invalidate();	// redraw
	}

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_mouseScroll, NULL, 0);

	return TRUE;
}

// ---------------------------- right button Down --------------------------------------
LRESULT SchematicControl::RButtonDown(int xPos, int yPos, int fwKeys)
{
	SetFocus(m_hWnd);

	sceneData->LMBPressed = false;
	sceneData->MMBPressed = false;
	sceneData->regionSelection = false;
	sceneData->regionSelectionPending = false;
	sceneData->createSocketMode = false;
	connectionSocket = NULL;

	Invalidate();

	// Send to Maxscript:
	run_event_handler(parent_rollout, n_rButtonDown, NULL, 0);

	SetCapture(m_hWnd);
	return TRUE;
}

// ---------------------------- right button up --------------------------------------
LRESULT SchematicControl::RButtonUp(int xPos, int yPos, int fwKeys)
{
	// Send to Maxscript:

	// example of passing arguments:
	// ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
	//	one_value_local(arg);
	//	vl.arg = Float::intern(m_degrees);
	//	run_event_handler(parent_rollout, n_changed, &vl.arg, 1);

	run_event_handler(parent_rollout, n_rButtonUp, NULL, 0);

	ReleaseCapture();
	return TRUE;
}

// ---------------------------- invalide --------------------------------------
void SchematicControl::Invalidate()
{
	// as alternative we could draw a big rectangle to clear the background with FillRect (faster then rectangle)
	if (m_hWnd == NULL) return;
	// RECT rect;
	// GetClientRect(m_hWnd, &rect);										//retrieves the coordinates of a window's client area
	// MapWindowPoints(m_hWnd, parent_rollout->page, (POINT*)&rect, 2);	//converts (maps) a set of points from a coordinate space relative to one window to a coordinate space relative to another window
	// InvalidateRect(parent_rollout->page, &rect, TRUE);				// we no longer have to do this because we use fillRect to draw to and offScreen bitmap
	InvalidateRect(m_hWnd, NULL, FALSE);									// NULL invalidates entire window
}

// ---------------------------- invalide area --------------------------------------
// invalidates an area of the window, and also redraws background of rollout in that area (some invalidateRect does not)
void SchematicControl::InvalidateArea(RECT area, int threshold)
{
	if (m_hWnd == NULL) return;

	// first make sure that the rect values are correct (left smaller then right etc):
	if (area.left >= area.right)
	{
		int tLeft = area.left;
		area.left = area.right;
		area.right = tLeft;
	}
	if (area.top >= area.bottom)
	{
		int tBottom = area.bottom;
		area.bottom = area.top;
		area.top = tBottom;
	}

	if (threshold != 0)	// sometimes a threshold is useful, for example (drawing lines depends on the angle, lots of pixels can touch the edge of the rect)
	{
		area.bottom += threshold;
		area.top -= threshold;
		area.left -= threshold;
		area.right += threshold;
	}

	// as alternative we could draw a big rectangle to clear the background with FillRect (faster then rectangle)
	// MapWindowPoints(m_hWnd, parent_rollout->page, (POINT*)&area, 2);	//converts (maps) a set of points from a coordinate space relative to one window to a coordinate space relative to another window
	// InvalidateRect(parent_rollout->page, &area, FALSE);				// we no longer have to do this because we use fillRect to draw to and offScreen bitmap
	InvalidateRect(m_hWnd, &area, FALSE);								// NULL invalidates entire window
}

// ---------------------------- paint ------------------------------------
LRESULT SchematicControl::Paint(PAINTSTRUCT ps)
{
	if (schematicNodes != NULL)
	{
		HBITMAP hbmOld;
		// Select the bitmap into the off-screen DC:
		hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

		// Erase the background:
		if (m_hBitmap)
		{
			// Get the info for the bitmap.
			BITMAP bm;
			GetObject(m_hBitmap, sizeof(bm), &bm);
			int width = bm.bmWidth;
			int height = bm.bmHeight;
			HDC hMemDC = CreateCompatibleDC(hdcMem);	// create memory dc (bitmap is not yet drawn)
			SelectObject(hMemDC, m_hBitmap);
			// Draw memory bitmap to screen (resize if neccesairy):
			if (sceneData->tileBG == false)
				StretchBlt(hdcMem, windowRect.left, windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, hMemDC, 0, 0, width, height, SRCCOPY);
			else
			{
				//created a twice tiled bitmap so we can properly use bitBlt with shifting further below:
				HDC tiledDC = CreateCompatibleDC(hMemDC);
				HBITMAP tiledBitmap = CreateCompatibleBitmap(hMemDC, width * 2, height * 2);
				SelectObject(tiledDC, tiledBitmap);
				for (int x = 0; x < width * 2; x += width)
				{
					for (int y = 0; y < height * 2; y += height)
						BitBlt(tiledDC, x, y, width, height, hMemDC, 0, 0, SRCCOPY);
				}

				// do some mod here to shift our texture over depending on pan:
				int shiftMapX = -(int)sceneData->panAmount.x % width;
				int shiftMapY = -(int)sceneData->panAmount.y % height;
				if (shiftMapX < 0) shiftMapX += width;
				if (shiftMapY < 0) shiftMapY += height;

				// tile bitmap:
				for (int x = 0; x < windowRect.right; x += width)
				{
					for (int y = 0; y < windowRect.bottom; y += height)
						BitBlt(hdcMem, x, y, width, height, tiledDC, shiftMapX, shiftMapY, SRCCOPY);
				}

				DeleteDC(tiledDC);
				DeleteObject(tiledBitmap);
			}

			DeleteDC(hMemDC);
		}
		else
			FillRect(hdcMem, &windowRect, hbrBkGnd);

		// Render the image into the offscreen DC:

			// Version number and program label:
		if (sceneData->showInfo == true)
		{
			if (schematicNodes->Count() > 0)
			{
				Point2 wndSize;
				wndSize.x = (float)(windowRect.right - windowRect.left);
				wndSize.y = (float)(windowRect.bottom - windowRect.top);
				//(*schematicNodes)[0]->drawLabel(hdcMem, wndSize - Point2(350.0,20.0) , wndSize, sceneData->infoText, DT_RIGHT, false); // Point2(300.0,20.0) is my estimate for the width and height of the text
				(*schematicNodes)[0]->drawLabel(hdcMem, Point2(0.0, wndSize.y - 20.0), wndSize, sceneData->infoText, DT_RIGHT, false);
			}
		}

		// draw the schematic node shadows:
		if (sceneData->drawShadows == true)
		{
			for (int i = 0; i < schematicNodes->Count(); i++)
			{
				(*schematicNodes)[i]->drawNode(hdcMem, true);	// draws shadow shape only
			}
		}

		// draw connection lines BEFORE nodes if nodes are transparent:
		if (sceneData->nodeTransparency >= 0.0f)
		{
			// draw connections:
			for (int i = 0; i < schematicNodes->Count(); i++)
			{
				for (int s = 0; s < (*schematicNodes)[i]->outSockets.Count(); s++)
				{
					(*schematicNodes)[i]->outSockets[s]->drawConnections(hdcMem, (*schematicNodes)[i]->outSockets[s]);	// nodes will draw sockets, sockets will NOT draw connections, because connections need to drawn on top of all nodes
				}
			}
		}

		// draw the schematic nodes:
		if (sceneData->useDrawLayers == false)
		{
			for (int i = 0; i < schematicNodes->Count(); i++)
			{
				(*schematicNodes)[i]->drawNode(hdcMem, false);	// nodes will draw sockets, sockets will NOT draw connections, because connections need to drawn on top of all nodes
			}
		}
		else
		{
			// draw the nodes in custom order based on drawLayer integer specified in nodes.
			// we have 3 layers, background (0), mid (1) and foreground (2):
			for (int c = 0; c < 3; c++)
			{
				for (int i = 0; i < schematicNodes->Count(); i++)
				{
					if ((*schematicNodes)[i]->drawLayer == c)
					{
						(*schematicNodes)[i]->drawNode(hdcMem, false);
					}
				}
			}
		}

		// draw one single node last if requested by user:
		if (sceneData->drawLastIndex > -1)
		{
			if (sceneData->drawLastIndex < schematicNodes->Count())
			{
				(*schematicNodes)[sceneData->drawLastIndex]->drawNode(hdcMem, false);
			}
		}

		// draw connection lines AFTER nodes if nodes are NOT transparent:
		if (sceneData->nodeTransparency < 0.0f)
		{
			// draw connections:
			for (int i = 0; i < schematicNodes->Count(); i++)
			{
				for (int s = 0; s < (*schematicNodes)[i]->outSockets.Count(); s++)
				{
					(*schematicNodes)[i]->outSockets[s]->drawConnections(hdcMem, (*schematicNodes)[i]->outSockets[s]);	// nodes will draw sockets, sockets will NOT draw connections, because connections need to drawn on top of all nodes
				}
			}
		}

		// REGION SELECT:
		if (sceneData->regionSelection == true)
		{
			HGDIOBJ oldPen = SelectObject(hdcMem, GetStockObject(WHITE_PEN));
			HGDIOBJ oldBrush = SelectObject(hdcMem, GetStockObject(HOLLOW_BRUSH));
			Rectangle(hdcMem, (int)sceneData->regionPos.x, (int)sceneData->regionPos.y, (int)sceneData->toMousePos.x, (int)sceneData->toMousePos.y);

			SelectObject(hdcMem, oldPen);
			SelectObject(hdcMem, oldBrush);
			sceneData->previousRegion.x = sceneData->toMousePos.x;
			sceneData->previousRegion.y = sceneData->toMousePos.y;
		}

		// draw interactive connection:
		else if (sceneData->createSocketMode == true)
		{
			// draw new line:
			connectionSocket->drawInteractiveConnection(hdcMem, sceneData->toMousePos);
			sceneData->previousMousePos = sceneData->toMousePos;
		}

		// Blt the changes to the screen DC:
		BitBlt(ps.hdc,
			windowRect.left,
			windowRect.top,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			hdcMem,
			0,
			0,
			SRCCOPY);

		// Done with off-screen bitmap and DC:
		SelectObject(hdcMem, hbmOld);
	}// end if NULL

	return FALSE; // return false to notify we processed the message.
}

// ------------------------------ add control ---------------------------------
// what to do when the control gets added to a rollout:
void SchematicControl::add_control(Rollout *ro, HWND parent, HINSTANCE hInstan, int& current_y)
{
	// caption and control_ID are default rolloutControl values, see rollout.h
	caption = caption->eval();
	const TCHAR *text = caption->eval()->to_string();
	control_ID = next_id();
	parent_rollout = ro;

	// define rollout control properties:
	//	Value *val;
	//	val = control_param(backcolor);
	//	if(val != &unsupplied)
	//		m_style = val;

	// default rolloutControl functions, I asume they calculate positions of the control within the rollout, but not sure:
	layout_data pos;
	setup_layout(ro, &pos, current_y);
	process_layout_params(ro, &pos, current_y);

	// The CreateWindow function creates an overlapped, pop-up, or child window. It specifies the window class, 
	// window title, window style, and (optionally) the initial position and size of the window.
	// The function also specifies the window's parent or owner, if any, and the window's menu:
	m_hWnd = CreateWindow(
		SCHEMATIC_WINDOWCLASS,
		text,
		WS_VISIBLE | WS_CHILD | WS_GROUP,
		MAXScript::GetUIScaledValue(pos.left), MAXScript::GetUIScaledValue(pos.top), 
		MAXScript::GetUIScaledValue(pos.width), MAXScript::GetUIScaledValue(pos.height),
		parent, (HMENU)control_ID, hInstance, this);

	// Get the size of the client rectangle:
	GetClientRect(m_hWnd, &windowRect);
	sceneData->windowRect = windowRect;

	DeleteObject(hbmMem);
	DeleteDC(hdcMem);
	HDC hDC = GetDC(m_hWnd);
	// Create a compatible DC:
	hdcMem = CreateCompatibleDC(hDC);
	// Create a bitmap big enough for our client rectangle:
	hbmMem = CreateCompatibleBitmap(hDC,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top);

	ReleaseDC(m_hWnd, hDC);

	// 17-03-06:	clear our array :
	//				If we don't clear it here, we'll keep drawing the nodes of the previous used UI
	cleanUpNodes();

	// we could set the window transparent, but found it not very ueful:
	//SetWindowLongPtr(parent, GWL_EXSTYLE,GetWindowLong(parent, GWL_EXSTYLE) | WS_EX_LAYERED);
	//SetLayeredWindowAttributes(parent, 0, 255 * 0.8f, LWA_ALPHA);

	// The SendDlgItemMessage function sends a message to the specified control in a dialog box:
	SendDlgItemMessage(parent, control_ID, WM_SETFONT, (WPARAM)ro->font, 0L);
}//end

void SchematicControl::adjust_control(int& current_y)
{

	// default rolloutControl functions, I asume they calculate positions of the control within the rollout, but not sure:
	layout_data pos;
	setup_layout(parent_rollout, &pos, current_y);
	process_layout_params(parent_rollout, &pos, current_y);

	HWND	hWnd = GetDlgItem(parent_rollout->page, control_ID);
	RECT	rect;
	GetWindowRect(hWnd, &rect);
	MapWindowPoints(NULL, parent_rollout->page, (LPPOINT)&rect, 2);
	SetWindowPos(hWnd, NULL, rect.left, rect.top, MAXScript::GetUIScaledValue(pos.width), rect.bottom - rect.top, SWP_NOZORDER);
	// Get the size of the client rectangle:
	GetClientRect(m_hWnd, &windowRect);
	sceneData->windowRect = windowRect;
	DeleteObject(hbmMem);
	DeleteDC(hdcMem);
	HDC hDC = GetDC(m_hWnd);
	// Create a compatible DC:
	hdcMem = CreateCompatibleDC(hDC);
	// Create a bitmap big enough for our client rectangle:
	hbmMem = CreateCompatibleBitmap(hDC,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top);

	ReleaseDC(m_hWnd, hDC);
}

// ------------------------------ handle message -------------------------------
// I believe this handles messages from default controls such as spinners, for example CC_SPINNER_CHANGE, see tester.cpp
BOOL SchematicControl::handle_message(Rollout *ro, UINT message, WPARAM wParam, LPARAM lParam)
{
	//if (message == WM_CLOSE)
	//	cleanUpNodes();

	// All messages are handled in the dialog proc
	return FALSE;
}

// ---------------------------- get property --------------------------------
// Get properties of this control via maxscript:
// NOTE Properties is an interesting thing, depending on the amount of arguments a decision is made to either get or set
//      the property. No arguments mean 'get' an '= value' argument means set_property gets run instead.
//      we can hijack this system for i.e. addNode. We need no arguments, yet we still 'set a property' (we add a new node)

Value* SchematicControl::get_property(Value** arg_list, int count)
{
	Value* prop = arg_list[0];

	// n_width and n_height are def_names that are already declared somewhere else (probably generic rollout prop)

	if (parent_rollout && parent_rollout->page)
	{
		// GET WIDTH:
		if (prop == n_width)
		{
			HWND hWnd = GetDlgItem(parent_rollout->page, control_ID);
			RECT rect;
			GetWindowRect(hWnd, &rect);
			MapWindowPoints(NULL, parent_rollout->page, (LPPOINT)&rect, 2);
			return Integer::intern(MAXScript::GetValueUIUnscaled(rect.right - rect.left));
		}

		// GET HEIGHT:
		else if (prop == n_height)
		{
			HWND hWnd = GetDlgItem(parent_rollout->page, control_ID);
			RECT rect;
			GetWindowRect(hWnd, &rect);
			MapWindowPoints(NULL, parent_rollout->page, (LPPOINT)&rect, 2);
			return Integer::intern(MAXScript::GetValueUIUnscaled(rect.bottom - rect.top));
		}
	}

	// ADD NODE:
	if (prop == n_addNode)
	{
		c_addNode(); // should return node index back to mxs?
		return Integer::intern(schematicNodes->Count());	// intern creates correct value for maxscript. 1-based back to MXS
	}

	// DELETE NODE:
	else if (prop == n_deleteActiveNode)
	{
		bool nodeDeleted = c_deleteActiveNode(); // should return node index back to mxs?
		return nodeDeleted ? &true_value : &false_value;
	}

	// ADD IN SOCKET:
	else if (prop == n_addInSocket)
	{
		bool socketAdded = c_addSocket(0);
		if (socketAdded == true)
		{
			int totalSockets = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count() + (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();
			return Integer::intern(totalSockets);
		}
		else return &false_value;
	}

	// ADD OUT SOCKET:
	else if (prop == n_addOutSocket)
	{
		bool socketAdded = c_addSocket(1);
		if (socketAdded == true)
		{
			int totalSockets = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count() + (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();
			return Integer::intern(totalSockets);
		}
		else return &false_value;
	}

	// DELETE SOCKET:
	else if (prop == n_deleteActiveSocket)
	{
		bool socketDeleted = c_deleteActiveSocket(); // should return node index back to mxs?
		return socketDeleted ? &true_value : &false_value;
	}

	// GET ACTIVE NODE:
	else if (prop == n_activeNode)
	{
		return Integer::intern(sceneData->activeNodeIndex + 1);
	}

	// GET ACTIVE SOCKET:
	else if (prop == n_activeSocket)
	{
		return Integer::intern(sceneData->activeSocketIndex + 1);
	}

	// GET SELECTED NODE INDEX
	else if (prop == n_selectedNodeIndex)
	{
		return Integer::intern((c_getSelectedNodeIndex()) + 1);
	}

	// GET SELECTION COUNT
	else if (prop == n_getSelectionCount)
	{
		return Integer::intern(c_getSelectionCount());
	}

	// GET SOCKET COUNT
	else if (prop == n_getSocketCount)
	{
		return Integer::intern(c_getSocketCount());
	}

	// GET NODE NAME:
	else if (prop == n_nodeName)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
			return new String((*schematicNodes)[sceneData->activeNodeIndex]->label);
		else return &false_value;
	}

	// GET SOCKET NAME:
	else if (prop == n_socketName)
	{
		TSTR socketName = c_getSocketName();
		return new String(socketName);
	}

	// GET NODE BITMAP:
	else if (prop == n_nodeBitmap)
	{
		// we'll have to implement this one day
		// the problem is that our MAXBitmap gets removed by GC, because we do not implement GC_TRACE
		// this caused serious stability issues.
		/*
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1 )
			return (*schematicNodes)[sceneData->activeNodeIndex]->m_maxBitMap ? (*schematicNodes)[sceneData->activeNodeIndex]->m_maxBitMap : &undefined;
		else return &false_value;
		*/
		return &ok;
	}

	// GET NODE POSITION:
	else if (prop == n_activeNodePos)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
			return new Point2Value((*schematicNodes)[sceneData->activeNodeIndex]->position);
		else return &false_value;
	}

	// GET NODE COLLAPSE:
	else if (prop == n_activeNodeCollapsed)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
			return (*schematicNodes)[sceneData->activeNodeIndex]->collapsed ? &true_value : &false_value;
		else return &false_value;
	}

	// REDRAW VIEW:
	else if (prop == n_redrawView)
	{
		Invalidate();
		return &ok;
	}

	// GET ZOOM:
	else if (prop == n_zoom)
	{
		return Float::intern(sceneData->zoomScale);
	}

	// GET PAN:
	else if (prop == n_pan)
	{
		return new Point2Value(sceneData->panAmount);
	}

	// REDRAW NODE:
	else if (prop == n_redrawNode)
	{
		c_redrawNode();
		return &ok;
	}

	// NODE COLOR:
	else if (prop == n_nodeColor)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
			return new ColorValue((*schematicNodes)[sceneData->activeNodeIndex]->color);
		else return &false_value;
	}

	// NODE SIZE:
	else if (prop == n_nodeSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			Point2 nSize;
			nSize.x = (float)(*schematicNodes)[sceneData->activeNodeIndex]->width;
			nSize.y = (float)(*schematicNodes)[sceneData->activeNodeIndex]->height;
			return new Point2Value(nSize);
		}
		else return &false_value;
	}

	// SOCKET FLIPPED:
	else if (prop == n_activeSocketFlipped)
	{
		int flipInt = c_socketFlipped(2); // feeding it anything but 0 or 1 will simply return the flip state
		return (flipInt == 1) ? &true_value : &false_value;
	}

	// GET NODE COUNT:
	else if (prop == n_getNodeCount)
	{
		return Integer::intern(schematicNodes->Count());
	}

	// GET SOCKET VALUE:
	else if (prop == n_activeSocketValue)
	{
		float retVal = c_socketValue(0.0, false); // we feed it value 0.0, but we don't actually set it
		return Float::intern(retVal);
	}

	// GET SHOW VALUE ACTIVE SOCKET:
	else if (prop == n_activeSocketShowValue)
	{
		bool retVal = c_socketShowValue(true, false); // we only get the value here
		return (retVal == true) ? &true_value : &false_value;
	}

	// GET ISHIDDEN:
	else if (prop == n_isHidden)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool hid = (*schematicNodes)[sceneData->activeNodeIndex]->isHidden;
			return (hid == true) ? &true_value : &false_value;
		}
		else return &false_value;
	}

	// GET ACTIVE SOCKET TYPE:
	else if (prop == n_getActiveSocketType)
	{
		int retVal = c_getActiveSocketType();
		return Integer::intern(retVal);
	}

	// GET ACTIVE SOCKET CONNECTION INDICES:
	else if (prop == n_getActiveSocketConnectingNodeIndices)
	{
		Tab<int> nodeIndices;
		nodeIndices = c_getActiveSocketConnectionIndices(true); // true returns node indices
		//MXS array:
		one_typed_value_local(Array* ar);
		vl.ar = new Array((nodeIndices.Count()));
		for (int s = 0; s < (nodeIndices.Count()); s++)
		{
			vl.ar->append((Integer::intern(nodeIndices[s] + 1))); // +1 MXS is 1-based
		}
		return_value(vl.ar);
	}

	// GET ACTIVE SOCKET CONNECTION INDICES:
	else if (prop == n_getActiveSocketConnectingSocketIndices)
	{
		Tab<int> socketIndices;
		socketIndices = c_getActiveSocketConnectionIndices(false); // false returns socket ids
		//MXS array:
		one_typed_value_local(Array* ar);
		vl.ar = new Array((socketIndices.Count()));
		for (int s = 0; s < (socketIndices.Count()); s++)
		{
			vl.ar->append((Integer::intern(socketIndices[s] + 1))); // +1 MXS is 1-based
		}
		return_value(vl.ar);
	}

	// GET BITMAP PATH:
	else if (prop == n_bitmapPath)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			TSTR path = (*schematicNodes)[sceneData->activeNodeIndex]->bitmapPath;
			return new String(path);
		}
		else return &false_value;
	}

	// GET ALLOW UI DELETE:
	else if (prop == n_allowUIDelete)
	{
		return (sceneData->allowUIDelete == true) ? &true_value : &false_value;
	}

	// GET SCENE NODE
	else if (prop == n_sceneNode)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			INode* outNode = (*schematicNodes)[sceneData->activeNodeIndex]->inputNode;
			if (outNode == NULL) return &false_value;
			return MAXNode::intern(outNode);
		}
		else return &false_value;
	}

	// GET CONTROLLER
	else if (prop == n_sceneController)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			return MAXControl::intern(sock->inputController);
		}
		else return &false_value;
	}

	// GET NODE INFO:
	else if (prop == n_nodeInfo)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			return new String((*schematicNodes)[sceneData->activeNodeIndex]->info);
		}
		else return &false_value;
	}

	// GET SOCKET POSITION:
	else if (prop == n_socketPosition)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			return new Point2Value(sock->position);
		}
		return &false_value;
	}

	// GET SOCKET INFO:
	else if (prop == n_socketInfo)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
			return new String(sock->info);
		else return &false_value;
	}

	// GET NODE POSITION LOCK:
	else if (prop == n_posLocked)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool lock = (*schematicNodes)[sceneData->activeNodeIndex]->posLock;
			return (lock == true) ? &true_value : &false_value;
		}
		else return &undefined;
	}

	// GET LABEL OFFSET:
	else if (prop == n_nodeNameOffset)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			return new Point2Value((*schematicNodes)[sceneData->activeNodeIndex]->labelOffset);
		}
		else return &false_value;
	}

	// GET CONNECTION COLOR:
	else if (prop == n_connectionColor)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			return new ColorValue(sock->penColor);
		}
		return &false_value;
	}

	// GET NODE SELECTABLE:
	else if (prop == n_selectable)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool sel = (*schematicNodes)[sceneData->activeNodeIndex]->selectable;
			return (sel == true) ? &true_value : &false_value;
		}
		else return &undefined;
	}

	// GET NODE IS SELECTED:
	else if (prop == n_isselected)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool sel = (*schematicNodes)[sceneData->activeNodeIndex]->isSelected;
			return (sel == true) ? &true_value : &false_value;
		}
		else return &undefined;
	}

	// GET SOCKET POSITION OFFSET:
	else if (prop == n_socketLabelOffset)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			return new Point2Value(sock->labelOffset);
		}
		return &false_value;
	}

	// GET NODE COLLAPSED SIZE:
	else if (prop == n_nodeCollapsedSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			Point2 nSize;
			nSize.x = (float)(*schematicNodes)[sceneData->activeNodeIndex]->cWidth;
			nSize.y = (float)(*schematicNodes)[sceneData->activeNodeIndex]->cHeight;
			return new Point2Value(nSize);
		}
		else return &false_value;
	}

	// GET NODE EDGE SIZE:
	else if (prop == n_edgeSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			return Float::intern((*schematicNodes)[sceneData->activeNodeIndex]->edgeVal);
		}
		else return &false_value;
	}

	// GET SOCKET DATA CLASS:
	else if (prop == n_socketDataClass)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			return new String(sock->dataClass);
		}
		return &false_value;
	}

	// GET NODE SHOW BITMAP ONLY:
	else if (prop == n_showBitmapOnly)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool shw = (*schematicNodes)[sceneData->activeNodeIndex]->showBitmapOnly;
			return (shw == true) ? &true_value : &false_value;
		}
		else return &undefined;
	}

	// GET NODE IS COLLAPSIBLE:
	else if (prop == n_isCollapsible)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool shw = (*schematicNodes)[sceneData->activeNodeIndex]->collapsible;
			return (shw == true) ? &true_value : &false_value;
		}
		else return &undefined;
	}

	// GET BACKGROUND BITMAP:
	else if (prop == n_backgroundBitmap)
	{
		// we'll have to implement this one day
		// the problem is that our MAXBitmap gets removed by GC, because we do not implement GC_TRACE
		// this caused serious stability issues.
		return &ok;
	}

	// GET BACKGROUND PATH:
	else if (prop == n_backgroundPath)
	{
		return new String(bitmapPath);
	}

	// GET ALLOW ZOOM:
	else if (prop == n_allowZoom)
	{
		return (sceneData->allowZoom == true) ? &true_value : &false_value;
	}

	// GET ALLOW PAN:
	else if (prop == n_allowPan)
	{
		return (sceneData->allowPan == true) ? &true_value : &false_value;
	}

	// GET TILE BACKGROUND:
	else if (prop == n_tileBackground)
	{
		return (sceneData->tileBG == true) ? &true_value : &false_value;
	}

	// GET ACTIVE SOCKET SELECTED CONNECTION INDEX:
	else if (prop == n_getSelectedConnectionIndex)
	{
		hsocket* sock = getActiveSocket();
		int selIndex = 0;
		if (sock != NULL)
			selIndex = (sock->selectedConnectionIndex + 1);	// MXS is 1-based
		return Integer::intern(selIndex);
	}

	// GET PARENT:
	else if (prop == n_parentNode)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			schematicNode *par = (*schematicNodes)[sceneData->activeNodeIndex]->parent;
			int parIndex = -1;
			if (par != NULL)
				parIndex = c_getNodeIndex(par);	// MXS is 1-based
			return Integer::intern(parIndex + 1);
		}
		else return &undefined;
	}

	// GET SHOWINFO:
	else if (prop == n_showInfo)
	{
		bool shwinfo = sceneData->showInfo;
		return (shwinfo == true) ? &true_value : &false_value;
	}

	// GET CHILD COUNT:
	else if (prop == n_getChildCount)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			schematicNode *par = (*schematicNodes)[sceneData->activeNodeIndex];
			int parIndex = par->children.Count();
			return Integer::intern(parIndex);
		}
		else return &undefined;
	}

	// GET DRAW CONNECTIONS FROM PARENT:
	else if (prop == n_drawConnectionsFromParent)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool outVal = (*schematicNodes)[sceneData->activeNodeIndex]->drawConnectionsFromParent;
			return (outVal == true) ? &true_value : &false_value;
		}
		else return &false_value;
	}

	// GET DRAWWEIGHTS:
	else if (prop == n_drawWeights)
	{
		bool drw = sceneData->drawWeights;
		return (drw == true) ? &true_value : &false_value;
	}

	// GET SELECTED CONNECTION NODE AND SOCKET:
	else if (prop == n_getSelectedConnectionNodeAndSocket)
	{
		if (selectedSocketOut != NULL)
		{
			int nodeIndex = c_getNodeIndex(selectedSocketOut->owner) + 1;	// +1 since MXS is 1-based
			int socketIndex = selectedSocketOut->ID + 1;					// +1 since MXS is 1-based
			return new Point2Value(Point2((float)nodeIndex, (float)socketIndex));
		}
		else return &false_value;
	}

	// GET ALLOW REGION SELECT:
	else if (prop == n_allowRegionSelect)
	{
		return (sceneData->allowRegionSelect == true) ? &true_value : &false_value;
	}

	// GET NODE TEXTALIGN:
	else if (prop == n_textAlign)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int ta = (*schematicNodes)[sceneData->activeNodeIndex]->textAlign;
			return Integer::intern(ta + 1);	// +1 since MXS is 1-based
		}
		else return &false_value;
	}

	// GET DELETED NODE COUNT:
	else if (prop == n_getDeletedNodeCount)
	{
		return Integer::intern(lastDeletedNodesIndices.Count());
	}

	// GET BITMAP SCALE:
	else if (prop == n_bitmapSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			return new Point2Value((*schematicNodes)[sceneData->activeNodeIndex]->bitmapSize);
		}
		else return &false_value;
	}

	// GET SOCKET IS HIDDEN:
	else if (prop == n_socketIsHidden)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			bool hid = sock->isHidden;
			return (hid == true) ? &true_value : &false_value;
		}
		return &false_value;
	}

	// GET DRAW SHADOW:
	else if (prop == n_drawShadows)
	{
		return (sceneData->drawShadows == true) ? &true_value : &false_value;
	}

	// GET DRAW LAST INDEX:
	else if (prop == n_drawLastIndex)
	{
		return Integer::intern(sceneData->drawLastIndex + 1);	// mxs is 1 based, we are 0-based
	}

	// DRAW MENUBAR:
	else if (prop == n_drawMenuBars)
	{
		return (sceneData->drawMenuBars == true) ? &true_value : &false_value;
	}

	// GET DRAW SEPERATOR:
	else if (prop == n_drawSeperator)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			bool bval = sock->drawSeperator;
			return (bval == true) ? &true_value : &false_value;
		}
		return &false_value;
	}

	// GET ZOOM ABOUT MOUSE:
	else if (prop == n_zoomAboutMouse)
	{
		return (sceneData->zoomAboutMouse == true) ? &true_value : &false_value;
	}

	// GET NODE TRANSPARENCY:
	else if (prop == n_nodeTransparency)
	{
		return Float::intern(sceneData->nodeTransparency);
	}

	// GET INFO TEXT:
	else if (prop == n_setInfo)
	{
		return new String(sceneData->infoText);
	}

	// GET VIEWALIGN:
	else if (prop == n_viewAlign)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int va = (*schematicNodes)[sceneData->activeNodeIndex]->viewAlign;
			return Integer::intern(va);
		}
		else return &undefined;
	}

	// GET VIEW ALIGN OFFSET:
	else if (prop == n_viewAlignOffset)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			return new Point2Value((*schematicNodes)[sceneData->activeNodeIndex]->viewAlignOffset);
		}
		else return &false_value;
	}

	// GET SHOW MINI BITMAP:
	else if (prop == n_showMiniBitmap)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool bval = (*schematicNodes)[sceneData->activeNodeIndex]->showMiniBitmap;
			return (bval == true) ? &true_value : &false_value;
		}
		else return &undefined;
	}

	// CUSTOM DRAW ORDER:
	else if (prop == n_useDrawLayers)
	{
		return (sceneData->useDrawLayers == true) ? &true_value : &false_value;
	}

	// GET DRAW LAYER:
	else if (prop == n_drawLayer)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int va = (*schematicNodes)[sceneData->activeNodeIndex]->drawLayer;
			return Integer::intern(va + 1);  // +1 since MXS is 1-based
		}
		else return &undefined;
	}

	// !! REMEMBER TO ADD NEW PROPERTIES TO SAVE / LOAD

	if (parent_rollout && parent_rollout->page)
		return RolloutControl::get_property(arg_list, count);
	else return &undefined;
}

// -------------------------------- set property --------------------------------
// Set properties of this control via maxscript:
Value* SchematicControl::set_property(Value** arg_list, int count)
{
	Value* val = arg_list[0];
	Value* prop = arg_list[1];

	// n_width and n_height are def_names that are already declared somewhere else (probably generic rollout prop)

	if (parent_rollout && parent_rollout->page)
	{
		// SET WIDTH:
		if (prop == n_width)
		{
			int width = val->to_int();
			HWND	hWnd = GetDlgItem(parent_rollout->page, control_ID);
			RECT	rect;
			GetWindowRect(hWnd, &rect);
			MapWindowPoints(NULL, parent_rollout->page, (LPPOINT)&rect, 2);
			SetWindowPos(hWnd, NULL, rect.left, rect.top, MAXScript::GetUIScaledValue(width), rect.bottom - rect.top, SWP_NOZORDER);
			// Get the size of the client rectangle:
			GetClientRect(m_hWnd, &windowRect);
			sceneData->windowRect = windowRect;
			DeleteObject(hbmMem);
			DeleteDC(hdcMem);
			HDC hDC = GetDC(m_hWnd);
			// Create a compatible DC:
			hdcMem = CreateCompatibleDC(hDC);
			// Create a bitmap big enough for our client rectangle:
			hbmMem = CreateCompatibleBitmap(hDC,
				windowRect.right - windowRect.left,
				windowRect.bottom - windowRect.top);

			ReleaseDC(m_hWnd, hDC);
			return &true_value;
		}

		// SET HEIGHT:
		else if (prop == n_height)
		{
			int height = val->to_int();
			HWND	hWnd = GetDlgItem(parent_rollout->page, control_ID);
			RECT	rect;
			GetWindowRect(hWnd, &rect);
			MapWindowPoints(NULL, parent_rollout->page, (LPPOINT)&rect, 2);
			SetWindowPos(hWnd, NULL, rect.left, rect.top, rect.right - rect.left, MAXScript::GetUIScaledValue(height), SWP_NOZORDER);
			// Get the size of the client rectangle:
			GetClientRect(m_hWnd, &windowRect);
			sceneData->windowRect = windowRect;
			DeleteObject(hbmMem);
			DeleteDC(hdcMem);
			HDC hDC = GetDC(m_hWnd);
			// Create a compatible DC:
			hdcMem = CreateCompatibleDC(hDC);
			// Create a bitmap big enough for our client rectangle:
			hbmMem = CreateCompatibleBitmap(hDC,
				windowRect.right - windowRect.left,
				windowRect.bottom - windowRect.top);

			ReleaseDC(m_hWnd, hDC);
			return &true_value;
		}
	}

	// SET ACTIVE NODE:
	if (prop == n_activeNode)
	{
		int index = val->to_int();
		bool indexSet = c_setActiveNode((index - 1)); // Note: since MXS is 1-based and our code is 0-based we subtract 1 from incoming MXS values
		return indexSet ? &true_value : &false_value;
	}

	// SET ACTIVE SOCKET:
	else if (prop == n_activeSocket)
	{
		int index = val->to_int();
		bool indexSet = c_setActiveSocket((index - 1)); // Note: since MXS is 1-based and our code is 0-based we subtract 1 from incoming MXS values
		return indexSet ? &true_value : &false_value;
	}

	// SET NODE NAME:
	else if (prop == n_nodeName)
	{
		TSTR nodeName = val->to_string();
		bool nameSet = c_setNodeName(nodeName);
		return nameSet ? &true_value : &false_value;
	}

	// SET SOCKET NAME:
	else if (prop == n_socketName)
	{
		TSTR socketName = val->to_string();
		bool nameSet = c_setSocketName(socketName);
		return nameSet ? &true_value : &false_value;
	}

	// SET BITMAP:
	else if (prop == n_nodeBitmap)
	{
		bool bitmapSet = c_setBitmap(val);
		return bitmapSet ? &true_value : &false_value;
	}

	// SET NODE POSITION:
	else if (prop == n_activeNodePos)
	{
		Point2 pos = val->to_point2();
		bool posSet = c_setNodePosition(pos, false);
		return posSet ? &true_value : &false_value;
	}

	// SET NODE POSITION RELATIVE TO VIEW (PAN AND ZOOM):
	else if (prop == n_activeNodePosRelative)
	{
		Point2 pos = val->to_point2();
		bool posSet = c_setNodePosition(pos, true);
		return posSet ? &true_value : &false_value;
	}

	// SET NODE COLLAPSE:
	else if (prop == n_activeNodeCollapsed)
	{
		bool col = val->to_bool() == 1;
		c_collapseActiveNode(col);
		return &ok;
	}

	// TOGGLE CONNECTION:
	else if (prop == n_toggleConnection)
	{
		Point2 inVal = val->to_point2();
		bool conMade = c_makeConnection((int)(inVal.x - 1), (int)(inVal.y - 1)); // -1 since MXS is 1-based
		return conMade ? &true_value : &false_value;
	}

	// FIND NODE BY POS:
	else if (prop == n_findNodeByPos)
	{
		Point2 inVal = val->to_point2();
		int nodeIndex = c_findNodeByPos(inVal);
		return Integer::intern((nodeIndex + 1)); // +1 because MXS index is kept 1-based
	}

	// NODE COLOR:
	else if (prop == n_nodeColor)
	{
		COLORREF nCol = val->to_colorref();
		c_setNodeColor(nCol);
		return &ok;
	}

	// SET NODE SIZE:
	else if (prop == n_nodeSize)
	{
		Point2 inVal = val->to_point2();
		c_nodeSize(inVal);
		return &ok;
	}

	// SET SELECTED NODE INDEX:
	else if (prop == n_selectedNodeIndex)
	{
		int inVal = val->to_int();
		bool selSet = c_setSelectedNodeIndex((inVal - 1)); // -1 because MXS is 1-based
		return selSet ? &true_value : &false_value;
	}

	// SOCKET FLIPPED:
	else if (prop == n_activeSocketFlipped)
	{
		bool inVal = val->to_bool() == 1;
		int flipInt = (inVal == true) ? 1 : 0;
		int returnInt = c_socketFlipped(flipInt);
		return (returnInt != -1) ? &true_value : &false_value;
	}

	// SET SOCKET VALUE:
	else if (prop == n_activeSocketValue)
	{
		float inVal = val->to_float();
		float retVal = c_socketValue(inVal, true);
		return Float::intern(retVal);
	}

	// SET SHOW VALUE ACTIVE SOCKET:
	else if (prop == n_activeSocketShowValue)
	{
		bool inVal = val->to_bool() == 1;
		c_socketShowValue(inVal, true);
		return &ok;
	}

	// SET ISHIDDEN:
	else if (prop == n_isHidden)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->isHidden = inVal;
			return &ok;
		}
		else return &false_value;
	}

	// SET BITMAP PATH:
	else if (prop == n_bitmapPath)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			TSTR path = val->to_string();
			(*schematicNodes)[sceneData->activeNodeIndex]->bitmapPath = path;
			return &ok;
		}
		else return &false_value;
	}

	// SET ALLOW UI DELETE:
	else if (prop == n_allowUIDelete)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->allowUIDelete = inVal;
		return &ok;
	}

	// SET SCENE NODE
	else if (prop == n_sceneNode)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			INode* inNode = val->to_node();
			(*schematicNodes)[sceneData->activeNodeIndex]->inputNode = inNode;
			return &ok;
		}
		else return &false_value;
	}

	// SET CONTROLLER
	else if (prop == n_sceneController)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			Control* inCont = val->to_controller();
			// removed the restriction of inputController having to be a float controller
			// socket only displays value if this inputController is a float though.
			// if (inCont->SuperClassID() == SClass_ID(CTRL_FLOAT_CLASS_ID))
			//	{
			sock->inputController = inCont;
			return &ok;
			//	}
		}
		return &false_value;
	}

	// SET NODE INFO:
	else if (prop == n_nodeInfo)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			TSTR cInfo = val->to_string();
			(*schematicNodes)[sceneData->activeNodeIndex]->info = cInfo;
			return &true_value;
		}
		else return &false_value;
	}

	// SET ZOOM:
	else if (prop == n_zoom)
	{
		float inVal = val->to_float();
		if (inVal < 0.1f) inVal = 0.1f;
		sceneData->zoomScale = inVal;
		return &true_value;
	}

	// SET PAN:
	else if (prop == n_pan)
	{
		Point2 inVal = val->to_point2();
		sceneData->panAmount = inVal;
		return &true_value;
	}

	// SET SOCKET POSITION:
	else if (prop == n_socketPosition)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			Point2 inVal = val->to_point2();
			sock->position = inVal;
			return &ok;
		}
		return &false_value;
	}

	// SET SOCKET INFO:
	else if (prop == n_socketInfo)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			TSTR cInfo = val->to_string();
			sock->info = cInfo;
			return &true_value;
		}
		return &false_value;
	}

	// SET NODE POSITION LOCK:
	else if (prop == n_posLocked)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->posLock = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET LABEL OFFSET:
	else if (prop == n_nodeNameOffset)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			Point2 inVal = val->to_point2();
			(*schematicNodes)[sceneData->activeNodeIndex]->labelOffset = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET CONNECTION COLOR:
	else if (prop == n_connectionColor)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			COLORREF nCol = val->to_colorref();
			sock->penColor = nCol;
			return &true_value;
		}
		return &false_value;
	}

	// SET NODE SELECTABLE:
	else if (prop == n_selectable)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->selectable = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET NODE IS SELECTED:
	else if (prop == n_isselected)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->isSelected = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET SOCKET POSITION OFFSET:
	else if (prop == n_socketLabelOffset)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			Point2 inVal = val->to_point2();
			sock->labelOffset = inVal;
			return &true_value;
		}
		return &false_value;
	}

	// SET NODE COLLAPSED SIZE:
	else if (prop == n_nodeCollapsedSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			Point2 inVal = val->to_point2();
			(*schematicNodes)[sceneData->activeNodeIndex]->cWidth = (int)inVal.x;
			(*schematicNodes)[sceneData->activeNodeIndex]->cHeight = (int)inVal.y;
			return &true_value;
		}
		else return &false_value;
	}

	// SET NODE EDGE SIZE:
	else if (prop == n_edgeSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			float inVal = val->to_float();
			(*schematicNodes)[sceneData->activeNodeIndex]->edgeVal = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET SOCKET DATA CLASS:
	else if (prop == n_socketDataClass)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			TSTR dClass = val->to_string();
			sock->dataClass = dClass;
			return &true_value;
		}
		return &false_value;
	}

	// SET NODE SHOW BITMAP ONLY:
	else if (prop == n_showBitmapOnly)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->showBitmapOnly = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET NODE IS COLLAPSIBLE:
	else if (prop == n_isCollapsible)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->collapsible = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET BACKGROUND BITMAP:
	else if (prop == n_backgroundBitmap)
	{
		HWND hWnd = MAXScript_interface->GetMAXHWnd();
		MAXBitMap *mbm = (MAXBitMap*)val;
		type_check(mbm, MAXBitMap, _T("backgroundBitmap"));
		m_maxBitMap = val;
		HDC hDC = GetDC(hWnd);
		//mbm->bi.SetCustomFlag( BMM_CUSTOM_GAMMA );
		//mbm->bi.SetCustomGamma(2.2f);
		PBITMAPINFO bmi = mbm->bm->ToDib(32, NULL, FALSE, TRUE);
		if (m_hBitmap) DeleteObject(m_hBitmap);
		m_hBitmap = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmi->bmiColors, bmi, DIB_RGB_COLORS);
		LocalFree(bmi);
		ReleaseDC(hWnd, hDC);
		return &true_value;
	}

	// SET BACKGROUND PATH:
	else if (prop == n_backgroundPath)
	{
		TSTR path = val->to_string();
		bitmapPath = path;
		return &ok;
	}

	// SET ALLOW ZOOM:
	else if (prop == n_allowZoom)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->allowZoom = inVal;
		return &ok;
	}

	// SET ALLOW PAN:
	else if (prop == n_allowPan)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->allowPan = inVal;
		return &ok;
	}

	// SET TILE BACKGROUND:
	else if (prop == n_tileBackground)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->tileBG = inVal;
		return &ok;
	}

	// SET PARENT:
	else if (prop == n_parentNode)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int inVal = (val->to_int()) - 1;	// mxs is 1 based, we are 0-based
			if (inVal < schematicNodes->Count())
			{
				schematicNode *child = (*schematicNodes)[sceneData->activeNodeIndex];
				schematicNode *par = NULL;
				if (inVal > -1) par = (*schematicNodes)[inVal];
				child->setParent(child, par, true);
				return &true_value;
			}
			else return &false_value;
		}
		else return &false_value;
	}

	// SET SHOWINFO:
	else if (prop == n_showInfo)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->showInfo = inVal;
		return &ok;
	}

	// CHILD INDEX:           (Note this is a Get function)
	else if (prop == n_childIndex)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int inVal = val->to_int();
			schematicNode *par = (*schematicNodes)[sceneData->activeNodeIndex];
			if (inVal > 0 && (inVal - 1) < par->children.Count())
			{
				int childIndex = c_getNodeIndex(par->children[(inVal - 1)]);	// -1 since MXS is 1-based
				return Integer::intern((childIndex + 1));
			}
			else return &false_value;
		}
		else return &undefined;
	}

	// SET DRAW CONNECTIONS FROM PARENT:
	else if (prop == n_drawConnectionsFromParent)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool inVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->drawConnectionsFromParent = inVal;
			return &ok;
		}
		else return &false_value;
	}

	// SET DRAWWEIGHTS:
	else if (prop == n_drawWeights)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->drawWeights = inVal;
		return &ok;
	}

	// GETCONNECTIONWEIGHT:			(Note this is a Get function, set function below)
	else if (prop == n_getConnectionWeight)
	{
		// [connectionIndex, newWeightValue]
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			int inVal = val->to_int() - 1;				// subtract 1 from connectionIndex since MXS is 1-based
			if (inVal < sock->connectionWeights.Count())
			{
				int curW = sock->connectionWeights[inVal];
				return Integer::intern(curW);
			}
			else return &false_value;
		}
		return &false_value;
	}

	// SET CONNECTIONWEIGHT:
	else if (prop == n_setConnectionWeight)
	{
		// [connectionIndex, newWeightValue]
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			Point2 inVal = val->to_point2() - Point2(1.0, 0.0);	// substract 1 from connectionIndex since MXS is 1-based
			if (inVal.x < sock->connectionWeights.Count())
			{
				sock->connectionWeights[(int)inVal.x] = (int)inVal.y;
				return &true_value;
			}
			else return &false_value;
		}
		return &false_value;
	}

	// SET ALLOW REGION SELECT:
	else if (prop == n_allowRegionSelect)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->allowRegionSelect = inVal;
		return &ok;
	}

	// SET NODE TEXTALIGN:
	else if (prop == n_textAlign)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int inVal = (val->to_int()) - 1;	// mxs is 1 based, we are 0-based
			if (inVal > -1 && inVal < 3)
			{
				(*schematicNodes)[sceneData->activeNodeIndex]->textAlign = inVal;
				return &true_value;
			}
			else return &false_value;
		}
		else return &false_value;
	}

	// GET DELETED NODE INDEX:
	else if (prop == n_getDeletedNodeIndex)
	{
		int inVal = (val->to_int()) - 1;	// mxs is 1 based, we are 0-based
		if (inVal < lastDeletedNodesIndices.Count())
			return Integer::intern((lastDeletedNodesIndices[inVal] + 1)); // +1 for MXS
		else return &false_value;
	}

	// SET RENDER BALL:
	else if (prop == n_setRenderBall)
	{
		/* MXS sample code:
		m = getMeditMaterial 1
		m.diffuse = (color 0 0 0)
		heliumOps.targetControl.activeNode = 1
		heliumOps.targetControl.setRenderBall = m
		*/
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			MtlBase *mb = val->to_mtlbase();
			mb->CreatePStamp(PS_LARGE, TRUE); // create and render a large pstamp
			PStamp *ps = mb->GetPStamp(PS_LARGE);
			if (ps)
			{
				int d = PSDIM(PS_LARGE);
				int scanw = ByteWidth(d);
				int nb = scanw * d;
				UBYTE *workImg = new UBYTE[nb];
				if (workImg == NULL) return &undefined;

				ps->GetImage(workImg);
				Rect rect;
				rect.left = 0;
				rect.top = 0;
				rect.right = 0 + d;
				rect.bottom = 0 + d;

				// get HDC from our rollout:
				HDC hMainDC = GetDC(m_hWnd);

				// create memory dc (bitmap is not yet drawn):
				HDC hdcBMP = CreateCompatibleDC(hMainDC);

				// select schematic node HBITMAP into hdc, so we draw in the bitmap:
				SelectObject(hdcBMP, (*schematicNodes)[sceneData->activeNodeIndex]->m_hBitmap);

				// draw the map into the hdc (effectively drawing into our schematic node bitmap):
				GetGPort()->DisplayMap(hdcBMP, rect, 0, 0, workImg, scanw);

				// clean up:
				ReleaseDC(m_hWnd, hMainDC);
				DeleteDC(hdcBMP);
				delete[] workImg;
				return &true_value;
			}
		}

		// ideally we would return the renderBall bitmap, but have no idea how to convert HBITMAP to MAXBitMap
		//return new MAXBitMap(bi, bmp);;
		return &false_value;
	}

	// SET BITMAP SCALE:
	else if (prop == n_bitmapSize)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			Point2 inVal = val->to_point2();
			(*schematicNodes)[sceneData->activeNodeIndex]->bitmapSize = inVal;
			return &true_value;
		}
		else return &false_value;
	}

	// SET SOCKET IS HIDDEN:
	else if (prop == n_socketIsHidden)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			bool hid = (val->to_bool() == 1);
			sock->isHidden = hid;
			return &true_value;
		}
		return &false_value;
	}

	// SET DRAW SHADOW:
	else if (prop == n_drawShadows)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->drawShadows = inVal;
		return &ok;
	}

	// SET DRAW LAST INDEX:
	else if (prop == n_drawLastIndex)
	{
		int inVal = (val->to_int()) - 1;	// mxs is 1 based, we are 0-based
		sceneData->drawLastIndex = inVal;
		return &ok;
	}

	// DRAW MENUBARS:
	else if (prop == n_drawMenuBars)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->drawMenuBars = inVal;
		return &ok;
	}

	// SET DRAW SEPERATOR:
	else if (prop == n_drawSeperator)
	{
		hsocket* sock = getActiveSocket();
		if (sock != NULL)
		{
			bool bval = (val->to_bool() == 1);
			sock->drawSeperator = bval;
			return &true_value;
		}
		return &false_value;
	}

	// SET ZOOM ABOUT MOUSE:
	else if (prop == n_zoomAboutMouse)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->zoomAboutMouse = inVal;
		return &ok;
	}

	// SET NODE TRANSPARENCY:
	else if (prop == n_nodeTransparency)
	{
		float inVal = val->to_float();
		sceneData->nodeTransparency = inVal;
		return &ok;
	}

	// SET INFO TEXT
	else if (prop == n_setInfo)
	{
		TSTR str = val->to_string();
		sceneData->infoText = str;
		return &ok;
	}

	// SET VIEWALIGN:
	else if (prop == n_viewAlign)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int inVal = val->to_int();
			(*schematicNodes)[sceneData->activeNodeIndex]->viewAlign = inVal;
			return &ok;
		}
		else return &false_value;
	}

	// SET VIEW ALIGN OFFSET:
	else if (prop == n_viewAlignOffset)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			Point2 pos = val->to_point2();
			(*schematicNodes)[sceneData->activeNodeIndex]->viewAlignOffset = pos;
			return &ok;
		}
		else return &false_value;
	}

	// SET SHOW MINI BITMAP:
	else if (prop == n_showMiniBitmap)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			bool bVal = val->to_bool() == 1;
			(*schematicNodes)[sceneData->activeNodeIndex]->showMiniBitmap = bVal;
			return &ok;
		}
		else return &false_value;
	}

	// CUSTOM DRAW ORDER
	else if (prop == n_useDrawLayers)
	{
		bool inVal = val->to_bool() == 1;
		sceneData->useDrawLayers = inVal;
		return &ok;
	}

	// SET DRAW LAYER:
	else if (prop == n_drawLayer)
	{
		if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
		{
			int inVal = val->to_int();
			inVal -= 1; // substract 1 since MXS is 1-based
			inVal = std::min(std::max(0, inVal), 2);	// cap layer between 0-2
			(*schematicNodes)[sceneData->activeNodeIndex]->drawLayer = inVal;
			return &ok;
		}
		else return &false_value;
	}

	// !! REMEMBER TO ADD NEW PROPERTIES TO SAVE / LOAD

	else
		return RolloutControl::set_property(arg_list, count);
	return val;
}

// ---------------------------- set enable ------------------------------------
void SchematicControl::set_enable()	// each rolloutControl can be enabled or disabled, we handle enabling here:
{
	if (parent_rollout != NULL && parent_rollout->page != NULL)
	{
		EnableWindow(m_hWnd, enabled);			// The EnableWindow function enables or disables mouse and keyboard input to the specified window or control
		InvalidateRect(m_hWnd, NULL, TRUE);		// The InvalidateRect function adds a rectangle to the specified window's update region. The update region represents the portion of the window's client area that must be redrawn
	}
}

/* ----------------------------- Plug-in Init ----------------------------- */
void SchematicInit()
{
	// initialize variables to hold names used as keyword parameters:
#include <maxscript\macros\define_implementations.h>

#include "PS_Events.h"

	static BOOL registered = FALSE;
	if (!registered)
	{
		// The WNDCLASSEX structure contains window class information
		// The GetClassLongPtr function retrieves the specified value from the WNDCLASSEX structure associated with the specified window
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = SchematicControl::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = NULL;
		wcex.hIconSm = NULL;
		wcex.hCursor = LoadCursor(0, IDC_ARROW);
		wcex.hbrBackground = NULL;
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = SCHEMATIC_WINDOWCLASS;

		if (!RegisterClassEx(&wcex))
			return;
		registered = TRUE;
	}

	// make new rolloutControl available to maxscript rollouts:
	install_rollout_control(Name::intern( _M(MXS_EXPORT_NAME)), SchematicControl::create);
}

// -------------------------------- Add Node --------------------------------
// Adds a new node to the schematic:
bool SchematicControl::c_addNode()
{
	schematicNode* sNode = new schematicNode;	// will set default props, such as height and label.
	schematicNodes->Append(1, &sNode);			// add node to array

	sNode->label = _T("node");
	//!!
	sNode->sceneData = &this->sceneData; //new c_sceneData;

	/*
	// redraw: // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
	if (schematicNodes->Count() > 1)
		{
		RECT drawRect = sNode->getNodeRect(sNode);
		InvalidateRect(m_hWnd, &drawRect, FALSE); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
		}
	else
		Invalidate(); // first node we redraw entire screen to also draw program name and version
	*/
	return TRUE;	// we currently allow creation of nodes at any time, but in the future we need to check with maxscript if it's ok to create the node
}

// -------------------------------- hit Check Node --------------------------------
// checks wheter a node was hit by the mouse
bool SchematicControl::nodeHitCheck(schematicNode* sNode, Point2 pos)
{
	if (sNode->selectable == true)
	{
		Point2 nodeSize = sNode->getNodeSize(sNode);
		Point2 nodePos = sNode->getNodePosition(sNode);
		if (
			nodePos.x < pos.x &&							// nodePos smaller then mousePos
			(nodePos.x + nodeSize.x) > pos.x &&		// nodePos + width bigger then mousePos
			nodePos.y < pos.y &&
			(nodePos.y + nodeSize.y) > pos.y
			)
		{
			return true;
		}
	}
	return false;
}

// -------------------------------- find And Delete Connection --------------------------------

// !! NOTE: if you alter this function, you might also need to alter the one in heliumController.cpp

// checks if a connection exists between to sockets and deletes the connection (both ways)
// return false if a connection did not exist, true if it did
bool SchematicControl::findAndDeleteConnection(hsocket* socketA, hsocket* socketB)
{
	// check if connection already exists, if so, break it:
	int sc = socketA->toSockets.Count();
	for (int t = 0; t < sc; t++)
	{
		int rT = sc - 1 - t;	// reverse search because we are deleting
		if (socketA->toSockets[rT] == socketB)
		{
			// delete socket from toSocket list:
			socketA->toSockets.Delete(rT, 1);

			// remove weight Data from socket if socket is outSocket:
			if (socketA->type == 1)	socketA->connectionWeights.Delete(rT, 1);

			// if this connection is true, there is also a connection the other way so, delete that too:
			findAndDeleteConnection(socketB, socketA);	// same function but reverse
			return true;
		}
	}
	return false;
}

// -------------------------------- Get ID of owner of a socket --------------------------------

// !! NOTE: if you alter this function, you might also need to alter the one in heliumController.cpp

// finds the index of the schematicNode that owns the socket (used to send index back to MXS after connection)
int SchematicControl::getSocketOwnerID(hsocket* sock)
{
	int index = -1;
	for (int i = 0; i < schematicNodes->Count(); i++)
	{
		// search in sockets:
		for (int t = 0; t < (*schematicNodes)[i]->inSockets.Count(); t++)
		{
			if ((*schematicNodes)[i]->inSockets[t] == sock)
			{
				index = i;
				break;
			}
		}
		if (index == -1) // if not found, search outSockets:
		{
			for (int t = 0; t < (*schematicNodes)[i]->outSockets.Count(); t++)
			{
				if ((*schematicNodes)[i]->outSockets[t] == sock)
				{
					index = i;
					break;
				}
			}
		}
	}
	return index;
}

// ---------------------------- invalide area --------------------------------------
// invalidates an area of the window, and also redraws background of rollout in that area (some invalidateRect does not)
void SchematicControl::InvalidateSelectionRegion()
{
	// we draw 4 rects around the lines of the rectangle for less flicker:
	RECT rect;
	// top line:
	rect.left = (LONG)sceneData->regionPos.x;
	rect.right = (LONG)sceneData->previousRegion.x;
	rect.top = (LONG)sceneData->previousRegion.y;
	rect.bottom = (LONG)sceneData->previousRegion.y;
	InvalidateArea(rect, 6);
	// bottom line:
	rect.left = (LONG)sceneData->regionPos.x;
	rect.right = (LONG)sceneData->previousRegion.x;
	rect.top = (LONG)sceneData->regionPos.y;
	rect.bottom = (LONG)sceneData->regionPos.y;
	InvalidateArea(rect, 6);
	// left line:
	rect.left = (LONG)sceneData->regionPos.x;
	rect.right = (LONG)sceneData->regionPos.x;
	rect.top = (LONG)sceneData->regionPos.y;
	rect.bottom = (LONG)sceneData->previousRegion.y;
	InvalidateArea(rect, 6);
	// right line:
	rect.left = (LONG)sceneData->previousRegion.x;
	rect.right = (LONG)sceneData->previousRegion.x;
	rect.top = (LONG)sceneData->regionPos.y;
	rect.bottom = (LONG)sceneData->previousRegion.y;
	InvalidateArea(rect, 6);
}

/*
					M X S
*/

// ---------------------------- set Active Node (MXS) --------------------------------------
// MXS is 1-based, our function is 0-based, but compensation already happened in set_property
bool SchematicControl::c_setActiveNode(int index)
{
	if (schematicNodes->Count() > index)
	{
		sceneData->activeNodeIndex = index;
		return true;
	}
	return false;
}

// ---------------------------- delete Active Node (MXS) --------------------------------------
bool SchematicControl::c_deleteActiveNode()
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		delete (*schematicNodes)[sceneData->activeNodeIndex];		// calls node's deconstructor, which cleans up sockets
		schematicNodes->Delete(sceneData->activeNodeIndex, 1);	// remove from array
		sceneData->activeNodeIndex = -1;						// reset activeNodeIndex
		sceneData->activeSocketIndex = -1;						// reset activeSocketIndex
		// Invalidate(); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
		return true;
	}
	return false;
}

// ---------------------------- add socket (MXS) --------------------------------------
bool SchematicControl::c_addSocket(int type)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		TSTR sLabel = _T("Input");
		if (type == 1) sLabel = _T("Output");
		(*schematicNodes)[sceneData->activeNodeIndex]->addSocket(m_hWnd, (*schematicNodes)[sceneData->activeNodeIndex], type, sLabel);
		return true;
	}
	return false;
}

// ---------------------------- set Active Socket (MXS) --------------------------------------
// MXS is 1-based, our function is 0-based, but compensation already happened in set_property
bool SchematicControl::c_setActiveSocket(int index)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		int totalSockets = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count() + (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();
		if (totalSockets > index)
		{
			sceneData->activeSocketIndex = index;
			return true;
		}
	}
	return false;
}

// ---------------------------- set Active Socket (MXS) --------------------------------------
// MXS is 1-based, our function is 0-based, but compensation already happened in set_property
bool SchematicControl::c_deleteActiveSocket()
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		int totalSockets = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count() + (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();
		if (sceneData->activeSocketIndex < totalSockets)
		{
			(*schematicNodes)[sceneData->activeNodeIndex]->deleteSocket(m_hWnd, (*schematicNodes)[sceneData->activeNodeIndex], sceneData->activeSocketIndex);
			sceneData->activeSocketIndex = totalSockets - 2; // set to last socket, -2 because we deleted 1 node, plus count() is 1-based and we need it 0-based
			// Invalidate(); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
			return true;
		}
	}
	return false;
}

// -------------------------------- get Selected Node Index --------------------------------
// Note that is 0-based, but gets turned into 1-based in get_property
// currently returns the last selected node it finds. There might be multiple selected nodes.
// we can support and array in the future (see MXS help on how to return an array to MXS)
int SchematicControl::c_getSelectedNodeIndex()
{
	int index = -1;
	for (int i = 0; i < schematicNodes->Count(); i++)
	{
		if ((*schematicNodes)[i]->isSelected == true) index = i;
	}
	return index;
}

// -------------------------------- set Selected Node Index --------------------------------
bool SchematicControl::c_setSelectedNodeIndex(int index)
{
	if (index < schematicNodes->Count())
	{
		for (int i = 0; i < schematicNodes->Count(); i++)
		{
			if (i != index) (*schematicNodes)[i]->isSelected = false;
			else (*schematicNodes)[i]->isSelected = true;
		}
		return true;
	}
	return false;
}

// -------------------------------- get Selection count --------------------------------
int SchematicControl::c_getSelectionCount()
{
	int count = 0;
	for (int i = 0; i < schematicNodes->Count(); i++)
	{
		if ((*schematicNodes)[i]->isSelected == true) count += 1;
	}
	return count;
}

// -------------------------------- get Socket count --------------------------------
int SchematicControl::c_getSocketCount()
{
	int count = 0;
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		count = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count() + (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();
	}
	return count;
}

// -------------------------------- set Socket name --------------------------------
bool SchematicControl::c_setSocketName(TSTR sLabel)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		int i = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count();
		int o = (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();

		int totalSockets = i + o;
		if (sceneData->activeSocketIndex < totalSockets)
		{
			// search through all sockets to find the one with this ID:
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->inSockets[s]->ID == sceneData->activeSocketIndex)
				{
					(*schematicNodes)[sceneData->activeNodeIndex]->inSockets[s]->label = sLabel;
					break;
				}
			}
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s]->ID == sceneData->activeSocketIndex)
				{
					(*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s]->label = sLabel;
					break;
				}
			}

			// RECT drawRect = (*schematicNodes)[sceneData->activeNodeIndex]->getNodeRect((*schematicNodes)[sceneData->activeNodeIndex]);
			// InvalidateRect(m_hWnd, &drawRect, FALSE); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
			return true;
		}
	}
	return false;
}

// -------------------------------- get Socket name --------------------------------
TSTR SchematicControl::c_getSocketName()
{
	TSTR socketName = _T("");
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		int i = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count();
		int o = (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();

		int totalSockets = i + o;
		if (sceneData->activeSocketIndex < totalSockets)
		{
			// search through all sockets to find the one with this ID:
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->inSockets[s]->ID == sceneData->activeSocketIndex) return (*schematicNodes)[sceneData->activeNodeIndex]->inSockets[s]->label;
			}
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s]->ID == sceneData->activeSocketIndex) return (*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s]->label;
			}
		}
	}
	return socketName;
}

// -------------------------------- set Node name --------------------------------
bool SchematicControl::c_setNodeName(TSTR sLabel)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		(*schematicNodes)[sceneData->activeNodeIndex]->label = sLabel;
		// RECT drawRect = (*schematicNodes)[sceneData->activeNodeIndex]->getNodeRect((*schematicNodes)[sceneData->activeNodeIndex]);
		//InvalidateRect(m_hWnd, &drawRect, FALSE); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
		return true;
	}
	return false;
}

// -------------------------------- set Node bitmap --------------------------------
bool SchematicControl::c_setBitmap(Value* val)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		(*schematicNodes)[sceneData->activeNodeIndex]->setBitmap(m_hWnd, (*schematicNodes)[sceneData->activeNodeIndex], val);
		// Invalidate(); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
		return true;
	}
	return false;
}

// -------------------------------- set Node position --------------------------------
bool SchematicControl::c_setNodePosition(Point2 pos, bool viewSpace)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		if (viewSpace == false)
			(*schematicNodes)[sceneData->activeNodeIndex]->position = pos;
		else
			(*schematicNodes)[sceneData->activeNodeIndex]->position = (pos - sceneData->panAmount) / sceneData->zoomScale;
		// Invalidate(); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
		return true;
	}
	return false;
}

// -------------------------------- collapse active node --------------------------------
void SchematicControl::c_collapseActiveNode(bool col)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		(*schematicNodes)[sceneData->activeNodeIndex]->collapsed = col;
		// Invalidate(); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
	}
}

// -------------------------------- make Connection MXS  --------------------------------
// !! NOTE: if you alter this function, you might also need to alter the one in heliumController.cpp

// make a new connection between the activeNode - activeSocket and the incoming target node and socket
// active socket should be an out socket
// target socket index should belong to an inSocket
bool SchematicControl::c_makeConnection(int targetIndex, int targetSocketIndex)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1 && targetIndex < schematicNodes->Count())
	{
		int ai = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count();
		int ao = (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();
		int totalSocketsA = ai + ao;

		int ti = (*schematicNodes)[targetIndex]->inSockets.Count();
		int to = (*schematicNodes)[targetIndex]->outSockets.Count();
		int totalSocketsT = ti + to;

		if (sceneData->activeSocketIndex < totalSocketsA && targetSocketIndex < totalSocketsT)
		{

			hsocket* targetSocket = NULL;		// inSocket
			hsocket* srcSocket = NULL;			// outSocket

			// find target socket in target node inSockets:
			for (int s = 0; s < (*schematicNodes)[targetIndex]->inSockets.Count(); s++)
			{
				if ((*schematicNodes)[targetIndex]->inSockets[s]->ID == targetSocketIndex) targetSocket = (*schematicNodes)[targetIndex]->inSockets[s];
			}

			// find src socket in outSockets:
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s]->ID == sceneData->activeSocketIndex)
				{
					srcSocket = (*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s];
					break;
				}
			}

			// either socket not found, then abort:
			if (targetSocket == NULL || srcSocket == NULL) return false; // target socket was not an out socket of targetNode or source socket was not found

			bool connectionExists = findAndDeleteConnection(srcSocket, targetSocket);

			if (connectionExists == false)
			{
				targetSocket->toSockets.Append(1, &srcSocket);
				srcSocket->toSockets.Append(1, &targetSocket);
				int cw = 100;
				srcSocket->connectionWeights.Append(1, &cw);	// default connectionWeight to 100
			}

			return true;
		}
	}

	return false;
}

// -------------------------------- set Node color --------------------------------
void SchematicControl::c_setNodeColor(COLORREF col)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		(*schematicNodes)[sceneData->activeNodeIndex]->color = col;
		// Invalidate(); // let user decide to redraw or not (via redrawViews command) in case of batch adding/changes
	}
}

// -------------------------------- redraw active Node --------------------------------
// maxscript access to redraw the active node
void SchematicControl::c_redrawNode()
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		RECT drawRect = (*schematicNodes)[sceneData->activeNodeIndex]->getNodeRect((*schematicNodes)[sceneData->activeNodeIndex]);
		InvalidateRect(m_hWnd, &drawRect, FALSE);
	}
}

// -------------------------------- find node by position --------------------------------
// sees if a node exists at a certain position (hit check for MXS)
int SchematicControl::c_findNodeByPos(Point2 pos)
{
	// incoming position from MXS has not taken into acount scale or pan, so lets convert pos to that first:

	for (int i = 0; i < schematicNodes->Count(); i++)
	{
		bool nodeWasHit = nodeHitCheck((*schematicNodes)[i], pos);
		if (nodeWasHit == true)
			return i; // MXS is 1-based, but we comp for this in get/set property instead of here
	}
	return -1;
}

// -------------------------------- set Node Size --------------------------------
// set node width and height via mxs
void SchematicControl::c_nodeSize(Point2 size)
{
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		(*schematicNodes)[sceneData->activeNodeIndex]->width = (int)size.x;
		(*schematicNodes)[sceneData->activeNodeIndex]->height = (int)size.y;

		// now update x position of outSockets or flipped inSockets
		// note that the correct procedure would be to give the schematic node the correct size first,
		// then add all the sockets. But we'll support a limited function here to alter the size somewhat after:
		for (int i = 0; i < (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count(); i++)
		{
			if ((*schematicNodes)[sceneData->activeNodeIndex]->inSockets[i]->flipped == true)
			{
				(*schematicNodes)[sceneData->activeNodeIndex]->inSockets[i]->position.x = (*schematicNodes)[sceneData->activeNodeIndex]->width - (*schematicNodes)[sceneData->activeNodeIndex]->inSockets[i]->radius * 2;
			}
		}
		for (int i = 0; i < (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count(); i++)
		{
			if ((*schematicNodes)[sceneData->activeNodeIndex]->outSockets[i]->flipped == false)
			{
				(*schematicNodes)[sceneData->activeNodeIndex]->outSockets[i]->position.x = (*schematicNodes)[sceneData->activeNodeIndex]->width - (*schematicNodes)[sceneData->activeNodeIndex]->outSockets[i]->radius * 2;
			}
		}
	}
}

// -------------------------------- get/set Socket Flipped --------------------------------
// get or set Flipped state of socket
// if getOrSet 0 or 1, we set the socket to false or true.
// if getOrSet is not 0 or 1, we return the current state of flipped
int SchematicControl::c_socketFlipped(int getOrSet)
{
	int returnVal = -1;
	hsocket* aSocket = getActiveSocket();
	if (aSocket != NULL)
	{
		if (getOrSet == 0 || getOrSet == 1)
		{
			bool flip = (getOrSet == 1) ? false : true; // the below 2 lines TOGGLEs the flip state, so we make sure we have the opesite here
			aSocket->flipped = flip;
			(*schematicNodes)[sceneData->activeNodeIndex]->flipSocket(aSocket);
		}
		returnVal = aSocket->flipped;
	}
	return returnVal;
}

// -------------------------------- get /set Socket Value --------------------------------
float SchematicControl::c_socketValue(float inVal, bool set)
{
	float returnVal = -1.0;
	hsocket* aSocket = getActiveSocket();
	if (aSocket != NULL)
	{
		if (set == true) { aSocket->floatVal = inVal; }
		returnVal = aSocket->floatVal;
	}
	return returnVal;
}

// -------------------------------- get /set Show socket value --------------------------------
bool SchematicControl::c_socketShowValue(bool show, bool set)
{
	bool returnBool = false;
	hsocket* aSocket = getActiveSocket();
	if (aSocket != NULL)
	{
		if (set == true) { aSocket->showVal = show; }
		return aSocket->showVal;
	}
	return returnBool;
}

// -------------------------------- get active Socket type --------------------------------
int SchematicControl::c_getActiveSocketType()
{
	int returnVal = -1;
	hsocket* aSocket = getActiveSocket();
	if (aSocket != NULL) returnVal = aSocket->type;
	return returnVal;
}

// -------------------------------- get Active Socket Connection Indices --------------------------------
Tab<int> SchematicControl::c_getActiveSocketConnectionIndices(bool Nodes)
{
	Tab<int> theIndices;
	hsocket* aSocket = getActiveSocket();
	if (aSocket != NULL)
	{
		for (int s = 0; s < aSocket->toSockets.Count(); s++)
		{
			if (Nodes == false)
				theIndices.Append(1, &aSocket->toSockets[s]->ID);
			else
			{
				schematicNode* sNode = aSocket->toSockets[s]->owner;
				// find index of sNode:
				for (int i = 0; i < schematicNodes->Count(); i++)
				{
					if ((*schematicNodes)[i] == sNode)
					{
						theIndices.Append(1, &i);
						break;
					}
				}
			}
		}
	}
	return theIndices;
}

// -------------------------------- get Active Socket --------------------------------
hsocket* SchematicControl::getActiveSocket()
{
	hsocket* retSocket = NULL;
	if (sceneData->activeNodeIndex < schematicNodes->Count() && sceneData->activeNodeIndex != -1)
	{
		int i = (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count();
		int o = (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count();

		int totalSockets = i + o;
		if (sceneData->activeSocketIndex < totalSockets)
		{
			// search through all sockets to find the one with this ID:
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->inSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->inSockets[s]->ID == sceneData->activeSocketIndex)
				{
					return (*schematicNodes)[sceneData->activeNodeIndex]->inSockets[s];
				}
			}
			for (int s = 0; s < (*schematicNodes)[sceneData->activeNodeIndex]->outSockets.Count(); s++)
			{
				if ((*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s]->ID == sceneData->activeSocketIndex)
				{
					return (*schematicNodes)[sceneData->activeNodeIndex]->outSockets[s];
				}
			}
		}
	}
	return retSocket;
}

// -------------------------------- get Node Index --------------------------------
// !! NOTE: if you alter this function, you might also need to alter the one in heliumController.cpp
int SchematicControl::c_getNodeIndex(schematicNode *node)
{
	int index = -1;
	if (node != NULL)
	{
		for (int i = 0; i < schematicNodes->Count(); i++)
		{
			if ((*schematicNodes)[i] == node) index = i;
		}
	}
	return index;
}

// -------------------------------- keyboard to MXS --------------------------------
void SchematicControl::c_keyboardToMXS(WPARAM code, bool up)
{
	// Send to Maxscript:
	ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;

	one_value_local(arg);
	vl.arg = Integer::intern((int)code);
	if (up == false)
		run_event_handler(parent_rollout, n_keyboardDown, &vl.arg, 1);
	else
		run_event_handler(parent_rollout, n_keyboardUp, &vl.arg, 1);
}

// ---------------------------- delete selected nodes / selected connection --------------------------------------
void SchematicControl::deleteSelectedNodes()
{
	if (sceneData->allowUIDelete == true)
	{
		if (selectedSocketOut == NULL)	// delete selected nodes:
		{

			// send msg to MXS:
			run_event_handler(parent_rollout, n_preNodesDeleted, NULL, 0);

			lastDeletedNodesIndices.SetCount(0);
			bool anyNodesDeleted = false;
			int nCount = schematicNodes->Count();

			for (int i = 0; i < nCount; i++)
			{
				int rI = nCount - 1 - i; // for delete we go backward through array
				if ((*schematicNodes)[rI]->isSelected == true)
				{
					delete (*schematicNodes)[rI];		// calls node's deconstructor, which cleans up sockets
					schematicNodes->Delete(rI, 1);		// remove from array
					lastDeletedNodesIndices.Append(1, &rI);	// keep track of indices, in case MXS asks for them
					anyNodesDeleted = true;
				}
			}
			sceneData->activeNodeIndex = -1;						// reset activeNodeIndex
			sceneData->activeSocketIndex = -1;						// reset activeSocketIndex
			Invalidate();

			// send msg to MXS:
			if (anyNodesDeleted == true)
			{
				run_event_handler(parent_rollout, n_nodesDeleted, NULL, 0);
			}
		}
		else	// delete selected connection instead:
		{
			// remove connection:
			int i = selectedSocketOut->selectedConnectionIndex;
			hsocket *selectedSocketIn = selectedSocketOut->toSockets[i];
			findAndDeleteConnection(selectedSocketOut, selectedSocketIn);

			// send connection change to MXS:
			int fromNodeIndex = c_getNodeIndex(selectedSocketOut->owner);
			int toNodeIndex = c_getNodeIndex(selectedSocketIn->owner);
			c_ConnectionChanged((fromNodeIndex + 1), (toNodeIndex + 1), (selectedSocketIn->ID + 1), (selectedSocketOut->ID + 1), 0, selectedSocketIn->toSockets.Count());

			// cleanup and redraw:
			selectedSocketOut = NULL;
			Invalidate();
		}
	}
}

// -------------------------------- EVENT Node Clicked --------------------------------
void SchematicControl::c_NodeClicked(int nodeIndex)
{
	// Send to Maxscript:
	ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;

	// pass arguments to MXS:
	one_value_local(arg);
	vl.arg = Integer::intern(nodeIndex + 1); // +1 because MXS is 1-based
	run_event_handler(parent_rollout, n_nodeClicked, &vl.arg, 1);
}

// -------------------------------- EVENT New Connection Made  --------------------------------
// status is 0 (connection broken) or 1 (connection made)
void SchematicControl::c_ConnectionChanged(int fromNodeIndex, int toNodeIndex, int fromSocketID, int toSocketID, int status, int toSocketCount)
{
	// Send to Maxscript:
	ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;

	// pass arguments to MXS:

	Value**		arg_list;
	value_local_array(arg_list, 6);

	arg_list[0] = Integer::intern(fromNodeIndex);
	arg_list[1] = Integer::intern(toNodeIndex);
	arg_list[2] = Integer::intern(fromSocketID);
	arg_list[3] = Integer::intern(toSocketID);
	arg_list[4] = Integer::intern(status);
	arg_list[5] = Integer::intern(toSocketCount);

	run_event_handler(parent_rollout, n_connectionChanged, arg_list, 6);
}

// -------------------------------- EVENT Connection selection changed  --------------------------------
void SchematicControl::c_ConnectionSelectionChanged(int fromNodeIndex, int toNodeIndex, int fromSocketID, int toSocketID)
{
	// Send to Maxscript:
	ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;

	// pass arguments to MXS:

	Value**		arg_list;
	value_local_array(arg_list, 4);

	arg_list[0] = Integer::intern(fromNodeIndex);
	arg_list[1] = Integer::intern(toNodeIndex);
	arg_list[2] = Integer::intern(fromSocketID);
	arg_list[3] = Integer::intern(toSocketID);

	run_event_handler(parent_rollout, n_connectionSelectionChanged, arg_list, 4);
}

// -------------------------------- UNHIDDEN node count for shaderFX  --------------------------------
int SchematicControl::c_getUnhiddenNodeCount()
{
	int total = 0;

	int nCount = schematicNodes->Count();
	for (int i = 0; i < nCount; i++)
	{
		if ((*schematicNodes)[i]->isHidden == false)
			total += 1;
	}

	return total;
}

