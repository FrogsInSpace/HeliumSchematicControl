

#include "PS_Nodes.h"		// includes and class definitions

/* ===========================================================================
	Node Classes implementation:
   =========================================================================== */

   // ---------------------------- Socket constructor --------------------------------------
hsocket::hsocket()
{
	ID = -1;							// 0-based
	type = 0;
	flipped = false;
	position = Point2(0.0f, 0.0f);
	radius = SOCKETRADIUS;
	inColor = SOCKETINCOLOR;
	outColor = SOCKETOUTCOLOR;
	penColor = CONNECTIONCOLOR;
	collapsedColor = SOCKETCOLLAPSEDCOLOR;
	label = _T("");
	labelOffset = Point2(0.0f, 0.0f);
	dataClass = _T("");
	selectedConnectionIndex = -1;

	floatVal = 0.0;
	showVal = true;
	inputController = NULL;
	info = _T("");
	isHidden = false;
	drawSeperator = false;
}

// ---------------------------- Socket deconstructor ------------------------------------
hsocket::~hsocket()
{
	inputController = NULL;

	// Clean up toSocket connections:
	int sc = toSockets.Count();
	for (int i = 0; i < sc; i++)
	{
		int tsc = toSockets[i]->toSockets.Count();
		for (int t = 0; t < tsc; t++)	// find toSockets that connect to me
		{
			int rT = tsc - 1 - t;	// reverse search because we are deleting
			if (toSockets[i]->toSockets[rT] == this) 			// if you find this socket in the other socket, delete it from array
				toSockets[i]->toSockets.Delete(rT, 1);
		}
	}
	toSockets.SetCount(0);

	// clear connectionWeights tab:
	connectionWeights.SetCount(0);
}

// ------------------------- Socket getSocketPosition -----------------------------------
Point2 hsocket::getSocketPosition(schematicNode* owner, hsocket* sock)
{
	Point2 nodePos = owner->getNodePosition(owner);
	Point2 socketPos;

	if (owner->collapsed == false)
	{
		socketPos = nodePos + (sock->position * (*owner->sceneData)->zoomScale); // owner pos already has pan in it
		if (owner->m_hBitmap) //(owner->m_maxBitMap && owner->m_hBitmap)
		{
			int bSize = (int)((*owner->sceneData)->zoomScale * owner->bitmapSize.y); //* BITMAPSIZE;
			socketPos.y += bSize;
		}
		if ((sock->type == 0 && sock->flipped == true) || sock->type == 1 && sock->flipped == false) socketPos.x += sock->radius / 2;
	}
	else	// if the node is collapsed, we want all connections to be drawn to the edge of the node, thus we position our sockets there:
	{
		Point2 nodeSize = owner->getNodeSize(owner);
		if ((sock->type == 1 && sock->flipped == false) || (sock->type == 0 && sock->flipped == true))
			socketPos = nodePos + Point2(nodeSize.x - 4.0f, nodeSize.y / 2);	// -4 is just a minor visual correction
		else
			socketPos = nodePos + Point2(-4.0f, nodeSize.y / 2);
	}

	return socketPos;
}

// ------------------------- Socket getSocketSize -----------------------------------
float hsocket::getSocketSize(schematicNode* owner)
{
	float size = radius * (*owner->sceneData)->zoomScale;
	return size;
}

// ---------------------------- Socket Arrow --------------------------------------
// Draw arrow head:
void hsocket::Arrow(HDC hDC, int x0, int y0, int x1, int y1, int width, int height)
{
	rotatePoly rotate(x0, y0, x1, y1);

	POINT arrow[4] =
	{
	x1, y1,
	rotate.RotateX(rotate.m_ds - width,  height) + x0,
	rotate.RotateY(rotate.m_ds - width,  height) + y0,
	rotate.RotateX(rotate.m_ds - width, -height) + x0,
	rotate.RotateY(rotate.m_ds - width, -height) + y0,
	x1, y1
	};

	HBRUSH colBrush = CreateSolidBrush(this->penColor);
	HGDIOBJ hOld = SelectObject(hDC, colBrush); //SelectObject(hDC, GetStockObject(BLACK_BRUSH)); 
	Polygon(hDC, arrow, 4);

	SelectObject(hDC, hOld);
	DeleteObject(colBrush);
}

// -------------------------- Socket drawConnections ------------------------------------
// draw all connections from this socket to other sockets
void hsocket::drawConnections(HDC hDC, hsocket* sock, int lineType)
{
	// are we ok to draw from parent:
	schematicNode *fromParNode = sock->owner->getFirstUnhiddenParent(sock->owner, sock->owner);
	bool fromParent = (sock->owner->drawConnectionsFromParent == true && fromParNode != NULL);

	// should we draw this connection:
	if (sock->owner->isHidden == false || fromParent == true)
	{
		int pSize = CONNECTIONPENSIZE;
		HPEN connectionPen = CreatePen(PS_SOLID, pSize, sock->penColor);
		HPEN oldPen = (HPEN)SelectObject(hDC, connectionPen);
		HPEN selectionPen = nullptr;
//		HPEN selectionPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
		if (sock->selectedConnectionIndex != -1)
		{
			COLORREF selCol = SELCONNECTIONCOLOR;
			selectionPen = CreatePen(PS_SOLID, pSize, selCol);
		}

		float sRad = SOCKETRADIUS;
		sRad *= (*sock->owner->sceneData)->zoomScale;
		sRad = sRad / 2;

		// determine where to draw FROM:
		Point2 FROM_socketPos;
		if (fromParent == false)
		{
			FROM_socketPos = getSocketPosition(sock->owner, sock);						// position of this socket
			FROM_socketPos += Point2(sRad, sRad);
		}
		else
		{
			Point2 parSize = fromParNode->getNodeSize(fromParNode); // / 2;
			FROM_socketPos = fromParNode->getNodePosition(fromParNode);
			FROM_socketPos += Point2(0.0f, parSize.y / 2); //parSize;
		}

		// create Point array and add first point (always the same):
		POINT lpPoints[4];
		if (lineType == 0)
		{
			POINT sPoint;
			sPoint.x = (LONG)FROM_socketPos.x;
			sPoint.y = (LONG)FROM_socketPos.y;
			lpPoints[0] = sPoint;
		}

		for (int i = 0; i < sock->toSockets.Count(); i++)
		{
			// check if we should draw to parent:
			schematicNode *toParNode = sock->toSockets[i]->owner->getFirstUnhiddenParent(sock->toSockets[i]->owner, sock->toSockets[i]->owner);
			bool toParent = (sock->toSockets[i]->owner->drawConnectionsFromParent == true && toParNode != NULL);

			if ((sock->toSockets[i]->owner->isHidden == false || toParent == true) && (fromParNode != toParNode || fromParNode == NULL || toParNode == NULL))
			{
				int startPointX = 0;
				int startPointY = 0;
				int midPointX = 0;
				int midPointY = 0;
				Point2 TO_socketPos;
				if (toParent == false)
				{
					TO_socketPos = getSocketPosition(sock->toSockets[i]->owner, sock->toSockets[i]);		// position of socket we are drawing to
					TO_socketPos += Point2(sRad, sRad);
				}
				else
				{
					Point2 parSize = toParNode->getNodeSize(toParNode); // / 2;
					TO_socketPos = toParNode->getNodePosition(toParNode);
					TO_socketPos += Point2(parSize.x, parSize.y / 2);
				}

				// check if this connection is selected, if so we use a different pen
				if (sock->selectedConnectionIndex != -1 && sock->selectedConnectionIndex == i)
					SelectObject(hDC, selectionPen);
				else
					SelectObject(hDC, connectionPen);

				// draw bezier line:
				if (lineType == 0 && (*sock->owner->sceneData)->zoomScale > 0.4) // when we zoom out really far we switch to linear lines
				{
					POINT controlPointS;
					controlPointS.x = (LONG)(FROM_socketPos.x + (TO_socketPos.x - FROM_socketPos.x) / 4);
					controlPointS.y = (LONG)(FROM_socketPos.y);
					lpPoints[1] = controlPointS;

					POINT controlPointE;
					controlPointE.x = (LONG)(FROM_socketPos.x + (((TO_socketPos.x - FROM_socketPos.x) / 4) * 3));
					controlPointE.y = (LONG)(TO_socketPos.y);
					lpPoints[2] = controlPointE;

					POINT ePoint;
					ePoint.x = (LONG)TO_socketPos.x;
					ePoint.y = (LONG)TO_socketPos.y;
					lpPoints[3] = ePoint;

					PolyBezier(hDC, lpPoints, 4);

					midPointX = lpPoints[0].x + (lpPoints[3].x - lpPoints[0].x) / 2;
					midPointY = lpPoints[0].y + (lpPoints[3].y - lpPoints[0].y) / 2;
					startPointX = lpPoints[0].x;
					startPointY = lpPoints[0].y;
				}

				// draw regular line:
				else if (lineType == 1 || (*sock->owner->sceneData)->zoomScale < 0.4)
				{
					MoveToEx(hDC, (int)FROM_socketPos.x, (int)FROM_socketPos.y, NULL);
					LineTo(hDC, (int)TO_socketPos.x, (int)TO_socketPos.y);					// draws from current position to target

					midPointX = (int)(FROM_socketPos.x + (TO_socketPos.x - FROM_socketPos.x) / 2);
					midPointY = (int)(FROM_socketPos.y + (TO_socketPos.y - FROM_socketPos.y) / 2);
					startPointX = (int)FROM_socketPos.x;
					startPointY = (int)FROM_socketPos.y;
				}

				// draw arrow head in middle of line
				int aWidth = ARROWWIDTH;
				int aHeight = ARROWHEIGHT;
				float sScale = (*sock->owner->sceneData)->zoomScale;
				sock->Arrow(hDC, startPointX, startPointY, midPointX, midPointY, (int)(aWidth*sScale), (int)(aHeight*sScale));

				// draw weight text:
				if ((*sock->owner->sceneData)->drawWeights == true)
				{
					TCHAR buf[9];
					_stprintf(buf, L"%d", sock->connectionWeights[i]);
					// draw weight value inbetween middle and start (we do this to minimize overlapping text):
					Point2 wPos = FROM_socketPos + (TO_socketPos - FROM_socketPos) / 3;
					sock->owner->drawLabel(hDC, wPos - Point2(50.0, 10.0), wPos + Point2(50.0, 10.0), buf, DT_CENTER);
				}
			}
		}

		SelectObject(hDC, oldPen);
		DeleteObject(connectionPen);
		if (selectedConnectionIndex != -1) DeleteObject(selectionPen);
	}
}

// -------------------------- Socket draw Interactive Connections ------------------------------------
// this only draws line while the user is dragging around
void hsocket::drawInteractiveConnection(HDC hDC, Point2 pos)
{
	int pSize = CONNECTIONPENSIZE;
	HPEN connectionPen = CreatePen(PS_SOLID, pSize, this->penColor);
	HPEN oldPen = (HPEN)SelectObject(hDC, connectionPen);

	Point2 FROM_socketPos = getSocketPosition(owner, this);
	MoveToEx(hDC, (int)(FROM_socketPos.x + radius / 2), (int)(FROM_socketPos.y + radius / 2), NULL);
	LineTo(hDC, (int)pos.x, (int)pos.y);

	this->owner->drawLabel(hDC, pos - Point2(100.0, 15.0), pos + Point2(100.0, 5.0), this->dataClass, DT_CENTER);

	SelectObject(hDC, oldPen);
	DeleteObject(connectionPen);
}

// ---------------------------- Socket drawSocket --------------------------------------
void hsocket::drawSocket(HDC hDC)
{
	if (isHidden == false && owner->collapsed == false && (*this->owner->sceneData)->zoomScale > 0.4)	// only draw sockets if owner is not collapsed
	{
		Point2 socketPos = getSocketPosition(owner, this);
		float socketSize = getSocketSize(owner);

		HBRUSH oldBrush;
		HBRUSH socketBrush = CreateSolidBrush(this->penColor);
		HBRUSH collapsedSocketBrush = CreateSolidBrush(this->collapsedColor);

		if (type == 0)	// in socket:
		{
			// check if any of the connecting nodes are hidden:
			bool nodesHidden = false;
			for (int i = 0; i < this->toSockets.Count(); i++)
			{
				if (this->toSockets[i]->owner->isHidden == true)
				{
					nodesHidden = true;
					break;
				}
			}

			if (nodesHidden == false)
				oldBrush = (HBRUSH)SelectObject(hDC, socketBrush); //inSocketBrush);
			else
				oldBrush = (HBRUSH)SelectObject(hDC, collapsedSocketBrush);

			Rectangle
			(
				hDC,
				(int)socketPos.x,											// start X
				(int)socketPos.y,											//       Y
				(int)(socketPos.x + socketSize),								// end   X
				(int)(socketPos.y + socketSize)								//       Y
			);
		}

		else			// out socket:
		{
			oldBrush = (HBRUSH)SelectObject(hDC, socketBrush); //outSocketBrush);

			Ellipse
			(
				hDC,
				(int)socketPos.x,											// start X
				(int)socketPos.y,											//       Y
				(int)(socketPos.x + socketSize),								// end   X
				(int)(socketPos.y + socketSize)								//       Y
			);
		}

		// Draw label:
		int textHeight = SOCKETHEIGHT;
		textHeight = (int)(textHeight * (*this->owner->sceneData)->zoomScale);
		Point2 nodeSize = this->owner->getNodeSize(this->owner);

		Point2 textStart;
		Point2 textEnd;
		int textAlign = DT_LEFT;
		if ((type == 0 && flipped == false) || (type == 1 && flipped == true)) // insocket (or flipped outsocket):
		{
			textStart = socketPos + Point2(socketSize * 2 - socketSize / 2, -socketSize / 2) + (labelOffset * (*this->owner->sceneData)->zoomScale);
			textEnd = textStart + Point2(nodeSize.x - socketSize * 2, (float)textHeight);
		}
		else		// outsocket:
		{
			textStart = socketPos + Point2(socketSize * 2, -socketSize / 2) - Point2(nodeSize.x, 0.0f) + (labelOffset * (*this->owner->sceneData)->zoomScale);
			textEnd = textStart + Point2((nodeSize.x - socketSize * 2), (float)textHeight);
			textAlign = DT_RIGHT;
		}

		TSTR socketString = this->label;

		if (type == 1 && this->showVal == true)	// for outsockets add value to string:
		{
			float outFloat;
			TCHAR buf[5];

			// show default value if no controller was set, or controller is not a float:
			if (inputController == NULL)
			{
				outFloat = this->floatVal;
				_stprintf(buf, _T("%g"), outFloat);
			}
			else if (inputController->SuperClassID() == SClass_ID(CTRL_FLOAT_CLASS_ID))
			{
				inputController->GetValue((GetCOREInterface()->GetTime()), &outFloat, FOREVER);
				float mult = pow(10.0f, 3);	// maximum of 2 digits accuracy is enough?
				outFloat = (floorf((outFloat * mult) + 0.5f)) / mult;
				_stprintf(buf, _T("%g"), outFloat);
			}
			else buf[0] = 0;	// makes it so we don't draw contents of buf with socket name

			socketString = socketString + _T(" (") + buf + _T(")");
		}

		// offset text Y to center with socket:
		textStart.y += socketSize - 8.0f; //this->radius/4.0f;

		this->owner->drawLabel(hDC, textStart, textEnd, socketString, textAlign);

		// draw seperator:
		if (drawSeperator)
		{
			// draw a line above the socket:
			Point2 nodePos = this->owner->getNodePosition(this->owner);
			MoveToEx(hDC, (int)nodePos.x, (int)textStart.y, NULL);
			LineTo(hDC, (int)(nodePos.x + nodeSize.x), (int)textStart.y);
		}

		// cleanup:
		SelectObject(hDC, oldBrush);
		DeleteObject(socketBrush);
		DeleteObject(collapsedSocketBrush);
	}
}

// ---------------------------- Socket Get Connection Rectangle --------------------------------------
RECT hsocket::getConnectionRect(hsocket* fromSock, hsocket* toSock)
{
	RECT Rect;
	Point2 sPos = fromSock->getSocketPosition(fromSock->owner, fromSock);
	Point2 ePos = toSock->getSocketPosition(toSock->owner, toSock);

	Rect.left = (LONG)sPos.x;
	Rect.right = (LONG)ePos.x;
	Rect.top = (LONG)sPos.y;
	Rect.bottom = (LONG)ePos.y;
	return Rect;
}

// ---------------------------- schematicNode constructor --------------------------------------
schematicNode::schematicNode()
{
	position = NODEPOS;
	color = SCHEMATICNODECOLOR;
	penColor = NODEEDGECOLOR;
	width = NODEWIDTH;
	height = NODEHEIGHT;
	label = NODELABEL;
	edgeVal = NODEEDGEVAL;
	edgeWidth = EDGEWIDTH;
	isSelected = false;
	collapsed = false;
	m_hBitmap = NULL;
	m_maxBitMap = NULL;
	bitmapPath = _T("");
	info = _T("");
	posLock = false;
	labelOffset = Point2(0.0, 0.0);
	selectable = true;
	cWidth = NODEWIDTH;
	cHeight = NODEHEIGHT;
	showBitmapOnly = false;
	collapsible = true;

	isHidden = false;
	inputNode = NULL;
	parent = NULL;
	drawConnectionsFromParent = false;
	textAlign = DT_CENTER;
	bitmapSize = Point2(80.0f, 80.0f);
	labelColor = NODEEDGECOLOR;
	viewAlign = 0;
	viewAlignOffset = Point2(0.0f, 0.0f);
	showMiniBitmap = true;
	drawLayer = 1;
}

// ---------------------------- schematicNode deconstructor ------------------------------------
schematicNode::~schematicNode()
{
	inputNode = NULL;

	// clean up in / out sockets, who will make sure referencing toSocket connections are properly cleaned up too:
	int sc = inSockets.Count();
	for (int i = 0; i < sc; i++)
	{
		int rI = sc - 1 - i;
		delete inSockets[rI];	// this will properly remove all connections in it's toSockets
		inSockets.Delete(rI, 1);
	}
	inSockets.SetCount(0);

	int osc = outSockets.Count();
	for (int i = 0; i < osc; i++)
	{
		int rI = osc - 1 - i;
		delete outSockets[rI];
		outSockets.Delete(rI, 1);
	}
	outSockets.SetCount(0);

	// let our children know we are gone and adjust their position:
	// we have to loop backwards here, since setParent will remove the child from our children list
	int cc = children.Count();
	for (int i = 0; i < cc; i++)
	{
		int rI = cc - i - 1;
		children[rI]->isHidden = false;		// automatically unhide children if parent is deleted.
		setParent(children[rI], NULL, true);
	}
	// Remove us from our Parent's children list:
	if (parent != NULL)
	{
		for (int i = 0; i < parent->children.Count(); i++)
			if (parent->children[i] == this) parent->children.Delete(i, 1);
	}

	if (m_hBitmap) DeleteObject(m_hBitmap);
	if (m_maxBitMap) m_maxBitMap = NULL;
}

// -------------------------- Socket drawConnections ------------------------------------
// Keep searching until we hit an undidden parent node
// we prevent cyclic looping by always comparing with the very 'firstNode' that started the search.
// if we find a parent that is the firstNode, we know a loop has happened and we stop and return null.
schematicNode* schematicNode::getFirstUnhiddenParent(schematicNode* node, schematicNode* firstNode)
{
	schematicNode *par = NULL;
	if (node != NULL)
	{
		if (node->parent != NULL && node->parent->isHidden == false && node->parent != firstNode)
			par = node->parent;
		else if (node->parent != NULL && node->parent != firstNode)
			par = node->getFirstUnhiddenParent(node->parent, firstNode);
	}

	return par;
}

// -------------------------------- schematicNode getNodePosition --------------------------------
// gets position of node, with or without zoom and pan:
Point2 schematicNode::getNodePosition(schematicNode* sNode, bool viewSpace)
{
	Point2 pos;

	// hidden nodes:
	if (sNode->isHidden == true) pos = Point2(-99999999.9, -99999999.9); // to prevent node hits

	// check if the node is aligned:
	else if (viewAlign == -1)
		pos = Point2((float)(*sceneData)->windowRect.left, (float)(*sceneData)->windowRect.top) + sNode->viewAlignOffset;
	else if (viewAlign == 1)
	{
		Point2 size = sNode->getNodeSize(sNode, viewSpace);
		pos = Point2((*sceneData)->windowRect.right - size.x, (float)(*sceneData)->windowRect.top) + sNode->viewAlignOffset;
	}

	// everything else:
	else
	{
		pos = sNode->position;

		// if we have a parent, then our position is relative to the parent node:
		if (sNode->parent != NULL) pos += sNode->parent->position;

		if (viewSpace == true)
		{
			pos *= (*sNode->sceneData)->zoomScale;
			pos += (*sNode->sceneData)->panAmount;
		}
	}

	return pos;
}

// -------------------------------- schematicNode getNodeSize --------------------------------
// gets size of node, with or without zoom and pan:
Point2 schematicNode::getNodeSize(schematicNode* sNode, bool viewSpace)
{
	Point2 size;

	if (sNode->isHidden == true) size = Point2(-1.0, -1.0);
	else
	{
		// NOT COLLAPSED:
		if (sNode->collapsed == false)
		{
			size.x = (float)sNode->width;
			size.y = (float)sNode->height;
			// if we show a bitmap, include it in the size:
			if (sNode->m_hBitmap) //(sNode->m_maxBitMap && sNode->m_hBitmap)
			{
				//int defSize = BITMAPSIZE;
				size.y += bitmapSize.y; //(defSize * bitmapSize.y);
			}
		}
		// COLLAPSED, WITH MINIBITMAP:
		else if (sNode->collapsed == true && sNode->showMiniBitmap == true)
		{
			size.x = (float)sNode->cWidth;
			size.y = (float)sNode->cHeight;
			if (sNode->m_hBitmap)
			{
				size.y += -14 + MINIBITMAPSIZE;
			}
		}
		// COLLAPSED:
		else	// if a node is collapsed we show the default size:
		{
			size.x = (float)sNode->cWidth;
			size.y = (float)sNode->cHeight;
		}

		if (viewSpace == true)
		{
			size.x *= (*sNode->sceneData)->zoomScale;
			size.y *= (*sNode->sceneData)->zoomScale;
		}
	}
	return size;
}

// -------------------------------- schematicNode getNodeRect --------------------------------
// gets the rectangle the node takes up:
RECT schematicNode::getNodeRect(schematicNode* sNode)
{
	Point2 nodeSize = sNode->getNodeSize(sNode);
	Point2 nodePos = sNode->getNodePosition(sNode);

	RECT drawRect;
	drawRect.left = (LONG)nodePos.x;
	drawRect.right = (LONG)(nodePos.x + nodeSize.x);
	drawRect.top = (LONG)nodePos.y;
	drawRect.bottom = (LONG)(nodePos.y + nodeSize.y);
	return drawRect;
}

// -------------------------------- schematicNode drawLabel --------------------------------
// draws a label:
void schematicNode::drawLabel(HDC hDC, Point2 posTopLeft, Point2 posBottomRight, TSTR sLabel, int align, bool useScale)
{
	if ((*this->sceneData)->zoomScale > 0.3 || useScale == false)
	{
		// Draw node label:
		int oldBkMode = GetBkMode(hDC);
		SetBkMode(hDC, TRANSPARENT);							// temporarly set background to transparent

		// set color of text:
		labelColor = NODEEDGECOLOR;
		int r = GetRValue(this->color);
		int g = GetGValue(this->color);
		int b = GetBValue(this->color);
		int darkenColor = 40;		// if you adjust this, also adjust in DrawNode
		if (!(*this->sceneData)->drawMenuBars) darkenColor = 0;
		if (!(r == 0 && g == 0 && b == 0) && r - darkenColor < 100 && g - darkenColor < 100 && b - darkenColor < 100) labelColor = RGB(200, 200, 200);
		COLORREF oldColor = SetTextColor(hDC, labelColor);

		RECT textLocation;
		textLocation.left = (LONG)posTopLeft.x;
		textLocation.top = (LONG)posTopLeft.y;
		textLocation.right = (LONG)posBottomRight.x;
		textLocation.bottom = (LONG)posBottomRight.y;

		// create font:
		int defHeight = FONTHEIGHT;
		int fHeight = FONTHEIGHT;
		if (useScale == true)
			fHeight = (int)(fHeight * (*this->sceneData)->zoomScale);
		if (fHeight > defHeight + 2) fHeight = defHeight + 2;		// + 2 simple a value I thought looked good

		HFONT labelFont = CreateFont(fHeight,  //fontHeight, 
			0,	//fontWidth
			0,
			0,
			FW_REGULAR,
			0, 0, 0,
			DEFAULT_CHARSET,
			0, 0,
			CLEARTYPE_QUALITY | ANTIALIASED_QUALITY, //0, 
			DEFAULT_PITCH | FF_SWISS,
			_T(""));

		if (labelFont) SelectObject(hDC, labelFont);

		DrawText(hDC, sLabel, (int)_tcslen(sLabel), &textLocation, align | DT_TOP);// | DT_SINGLELINE); //DT_VCENTER

		//clean up:
		if (labelFont) DeleteObject(labelFont);
		SetBkMode(hDC, oldBkMode);
		SetTextColor(hDC, oldColor);
	}
}

// ---------------------------- schematicNode drawNode --------------------------------------
// Draw the rectangle of the node
void schematicNode::drawNode(HDC hDC, bool drawShadow)
{
	if (this->isHidden == false)
	{
		// draw the schematic nodes:

		Point2 nodeSize = this->getNodeSize(this);
		Point2 nodePos = this->getNodePosition(this);

		// check if the node is within the view:
		if (nodePos.x > (*sceneData)->windowRect.right || nodePos.x + nodeSize.x < (*sceneData)->windowRect.left || nodePos.y >(*sceneData)->windowRect.bottom || nodePos.y + nodeSize.y < (*sceneData)->windowRect.top)
			return;

		HDC workHDC = nullptr;
		HBITMAP workhBitmap = nullptr;

		float scaledEdge = edgeVal * (*sceneData)->zoomScale;

		bool transp = (color == RGB(0, 0, 0));	// we need this flag to decide to draw the menu bar

		bool drawAlphaBlend = (*sceneData)->nodeTransparency >= 0.0f && !isSelected && !transp && (*sceneData)->zoomScale > 0.3f;

		if (drawAlphaBlend)
		{
			// we draw the nodes square when the nodes are transparent because we did not want to create a bitmap mask to
			// remove and edge pixels that would show up around the rounded corners
			scaledEdge = 0.0f;

			// prep for alphaBlending our drawings with the hDC:
			// we will do all our drawing in workHDC, and then alpha blend it all to the real hDC in the end
			workHDC = CreateCompatibleDC(hDC);
			SetBkMode(workHDC, TRANSPARENT);
			// we HAVE to put a bitmap into workHDC before we can draw in there:
			workhBitmap = CreateCompatibleBitmap(hDC, (int)(nodePos.x + nodeSize.x + 1.0f), (int)(nodePos.y + nodeSize.y + 1.0f));
			SelectObject(workHDC, workhBitmap);
		}
		else
			workHDC = hDC;

		// drop shadow:
		if (drawShadow && showBitmapOnly == false && !transp)	// no shadow for transparent nodes!
		{
			int backgroundcolor = ColorMan()->GetColor(kBackground);
			int r = GetRValue(backgroundcolor);
			int g = GetGValue(backgroundcolor);
			int b = GetBValue(backgroundcolor);
			if (r > 254) r = 254;	// make sure we don't divide by zero
			if (g > 254) g = 254;
			if (b > 254) b = 254;
			// an attempt to make the shadow color work for both light and dark max scheme:
			int rm = 10 * (255 / (255 - r));
			int gm = 10 * (255 / (255 - g));
			int bm = 10 * (255 / (255 - b));
			r -= rm;
			g -= gm;
			b -= bm;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;

			COLORREF shadowColor = RGB(r, g, b);
			COLORREF shadowColorPen = RGB(r + rm / 2, g + gm / 2, b + bm / 2);	// pen is half as dark to blend off shadow
			HBRUSH shadowBrush = CreateSolidBrush(shadowColor);
			HBRUSH oldBrush = (HBRUSH)SelectObject(workHDC, shadowBrush);
			HPEN shadowPen = CreatePen(PS_SOLID, 1, shadowColorPen);
			HPEN oldPen = (HPEN)SelectObject(workHDC, shadowPen);

			int shadowWidth = 2; //* (*sceneData)->zoomScale;
			RoundRect(workHDC, (int)(nodePos.x - shadowWidth), (int)(nodePos.y + shadowWidth), (int)(nodePos.x + nodeSize.x + shadowWidth), (int)(nodePos.y + shadowWidth + nodeSize.y), (int)scaledEdge, (int)scaledEdge);

			DeleteObject(shadowBrush);
			DeleteObject(shadowPen);
			SelectObject(workHDC, oldBrush);
			SelectObject(workHDC, oldPen);
			return;	// we EXIT here since we draw shadow as the first pass
		}

		// node brush:
		HBRUSH nodeBrush = nullptr;
		if (isSelected == false)
		{
			if (color != RGB(0, 0, 0))
				nodeBrush = CreateSolidBrush(color);
			else	// a completely black brush means we want transparent:
			{
				LOGBRUSH lb;
				lb.lbColor = 0;
				lb.lbHatch = 0;
				lb.lbStyle = BS_NULL;
				nodeBrush = CreateBrushIndirect(&lb);
			}
		}
		else
		{
			COLORREF selColor = SCHEMATICNODESELCOLOR;
			nodeBrush = CreateSolidBrush(selColor);
		}

		// node rectangle:
		HBRUSH oldBrush = (HBRUSH)SelectObject(workHDC, nodeBrush);	// selectObject return old object being replaced
		HPEN outlinePen = CreatePen(PS_SOLID, edgeWidth, penColor);
		HPEN oldPen = (HPEN)SelectObject(workHDC, outlinePen);
		if (showBitmapOnly == false)
			RoundRect(workHDC, (int)nodePos.x, (int)nodePos.y, (int)(nodePos.x + nodeSize.x), (int)(nodePos.y + nodeSize.y), (int)scaledEdge, (int)scaledEdge);

		// get additional label offset:
		Point2 addedOffset = Point2(0.0, 0.0);
		if (this->collapsed == false)
			addedOffset = (this->labelOffset * (*this->sceneData)->zoomScale);

		// Draw menubar:
		if (showBitmapOnly == false)
		{
			int collapsedHeight = NODEHEIGHT;
			if ((*sceneData)->drawMenuBars && addedOffset.y < 5.0 && !transp)	// only draw when the label isn't moved by much
			{
				int singleLineHeight = (int)(15 * (*this->sceneData)->zoomScale);

				COLORREF menubarColor;
				if (isSelected == false)
				{
					int r = GetRValue(color);
					int g = GetGValue(color);
					int b = GetBValue(color);
					int darkenColor = 40;		// if you adjust this, also adjust in DrawLabel
					r -= darkenColor;
					g -= darkenColor;
					b -= darkenColor;
					if (r < 0) r = 0;
					if (g < 0) g = 0;
					if (b < 0) b = 0;
					menubarColor = RGB(r, g, b);
				}
				else
				{
					menubarColor = SCHEMATICNODESELCOLOR;
				}

				HBRUSH menuBrush = CreateSolidBrush(menubarColor);
				HBRUSH oldBrush = (HBRUSH)SelectObject(workHDC, menuBrush);

				int miniBmpOffset = 0;
				if (this->collapsed && this->showMiniBitmap)
				{
					miniBmpOffset = (int)(6 + (*sceneData)->zoomScale * MINIBITMAPSIZE);
				}

				Rectangle(workHDC, (int)(nodePos.x + miniBmpOffset), (int)nodePos.y, (int)(nodePos.x + nodeSize.x), (int)(nodePos.y + singleLineHeight));
				//RoundRect(workHDC, nodePos.x, nodePos.y, (nodePos.x + nodeSize.x) , (nodePos.y + singleLineHeight), 0, 0 );

				DeleteObject(menuBrush);
				SelectObject(workHDC, oldBrush);
			}
		}//end if

	// draw bitmap:
		if (m_hBitmap && (this->collapsed == false || this->showMiniBitmap)) //(m_maxBitMap && m_hBitmap && this->collapsed == false)
		{
			// Get the info for the bitmap.
			BITMAP bm;
			GetObject(m_hBitmap, sizeof(bm), &bm);
			int width = bm.bmWidth;
			int height = bm.bmHeight;

			//RECT rect = this->getNodeRect(this);
			//int bSize = BITMAPSIZE;
			int bSize = (int)(bitmapSize.y * (*sceneData)->zoomScale);
			if (this->collapsed)
			{
				bSize = (int)((*sceneData)->zoomScale * MINIBITMAPSIZE);
			}

			// perhaps our bitmap is larger then our node, if so scale bitmap down:
			int bWidth = bSize;
			int bHeight = bSize;
			if (bWidth >= nodeSize.x) bWidth = (int)(nodeSize.x - BITMAPYOFFSET);
			if (bHeight >= nodeSize.y) bHeight = (int)(nodeSize.y - BITMAPYOFFSET);

			int xPos = (int)(nodePos.x + (nodeSize.x - bWidth) / 2);
			if (this->collapsed) xPos = (int)(nodePos.x + 4);
			int nHeight = NODEHEIGHT;
			if (this->collapsed)
				nHeight = 10;
			nHeight = (int)(nHeight * (*sceneData)->zoomScale);
			int yPos = (int)(nodePos.y + nHeight);
			int bOffset = BITMAPYOFFSET;
			yPos = (int)(yPos - (bOffset * (*sceneData)->zoomScale));		// is an offset to keep the bitmap away from the sockets below

			HDC hMemDC = CreateCompatibleDC(workHDC);	// create memory dc (bitmap is not yet drawn)
			SelectObject(hMemDC, m_hBitmap);

			// Draw memory bitmap to screen (resize if neccesairy):
			StretchBlt(workHDC, xPos, yPos, bWidth, bHeight, hMemDC, 0, 0, width, height, SRCCOPY);

			DeleteDC(hMemDC);
		}

		if (showBitmapOnly == false)
		{
			// Draw node label:
				// we now use nodeSize for entire label so we can have multi-line labels (good for text nodes)
				//int textHeight = NODEHEIGHT;
				//textHeight *= (*this->sceneData)->zoomScale;
			int textHeight = (int)nodeSize.y;
			float subtractMini = 0;
			int tAlign = this->textAlign;
			if (this->collapsed && this->showMiniBitmap)
			{
				addedOffset.x = 10 + (*sceneData)->zoomScale * MINIBITMAPSIZE;
				subtractMini = 12 + (*sceneData)->zoomScale * MINIBITMAPSIZE;
				tAlign = DT_RIGHT;
			}
			this->drawLabel(workHDC, nodePos + addedOffset, (nodePos + addedOffset + Point2(nodeSize.x, (float)textHeight)) - Point2(subtractMini, 0.0f), this->label, tAlign);

			// cleanup:
			SelectObject(workHDC, oldBrush);
			SelectObject(workHDC, oldPen);

			// draw the inSockets:
			for (int i = 0; i < inSockets.Count(); i++)
			{
				inSockets[i]->drawSocket(workHDC);
			}

			// draw the outSockets:
			for (int i = 0; i < outSockets.Count(); i++)
			{
				outSockets[i]->drawSocket(workHDC);
			}

		}//end if

		DeleteObject(nodeBrush);
		DeleteObject(outlinePen);

		// if we are using transparency, blend our node-image onto the real hDC:
		if (drawAlphaBlend)
		{
			BLENDFUNCTION bf;
			bf.BlendOp = AC_SRC_OVER;
			bf.BlendFlags = 0;
			bf.SourceConstantAlpha = (BYTE)((1.0f - (*sceneData)->nodeTransparency) * 255.f);
			bf.AlphaFormat = 0;//AC_SRC_ALPHA; //0x03;	// we only use this if the bitmap had alpha per pixel. But we don't. But if you don't set it to 0, you get really unreliable behaviour

			// for AlphaBlend: The source rectangle must lie completely within the source surface, otherwise an error occurs and the function returns FALSE.
			float subX = 0;
			if (nodePos.x < 0) subX = -nodePos.x;
			float subY = 0;
			if (nodePos.y < 0) subY = -nodePos.y;
			float edgeBuffer = 0.0f;
			if ((*sceneData)->zoomScale < 0.9f) edgeBuffer = 1.0f;	// pad the edges a bit or we loose the outline sometimes
			BOOL drawOK = AlphaBlend(hDC, (int)(nodePos.x + subX), (int)(nodePos.y + subY), (int)(nodeSize.x - subX + edgeBuffer), (int)(nodeSize.y - subY + edgeBuffer), workHDC, (int)(nodePos.x + subX), (int)(nodePos.y + subY), (int)(nodeSize.x - subX + edgeBuffer), (int)(nodeSize.y - subY + edgeBuffer), bf);
			if (drawOK == false)
			{
				// in case AlphaBlend failed to render, render regular:
				BitBlt(hDC, (int)nodePos.x, (int)nodePos.y, (int)nodeSize.x, (int)nodeSize.y, workHDC, (int)nodePos.x, (int)nodePos.y, SRCCOPY);
			}

			DeleteDC(workHDC);
			DeleteObject(workhBitmap);
		}
	}//end if hidden
}

// ---------------------------- schematicNode addSocket --------------------------------------
// add a new socket to node
hsocket* schematicNode::addSocket(HWND m_hWnd, schematicNode* sNode, int sType, TSTR sLabel, bool redraw)
{
	// create new socket:
	hsocket* newSock = new hsocket;
	newSock->type = sType;	// 0 is insocket, 1 is outsocket
	newSock->owner = sNode;
	newSock->label = sLabel;

	if (sType == 0)	// inSocket
	{
		newSock->penColor = SOCKETINCOLOR;
		sNode->inSockets.Append(1, &newSock);
	}
	else			// outSocket
	{
		newSock->penColor = SOCKETOUTCOLOR;
		sNode->outSockets.Append(1, &newSock);
	}

	// set index for socket:
	int totalSockets = sNode->inSockets.Count() + sNode->outSockets.Count();
	newSock->ID = totalSockets - 1; // 0-based

	// calc position:
	float yPos = (float)((totalSockets - 1) * SOCKETHEIGHT);
	yPos += NODEHEIGHT;

	float xPos = (newSock->radius / 2);
	if (sType == 1)
	{
		xPos = sNode->width - newSock->radius * 2;
	}

	newSock->position = Point2(xPos, yPos);

	// adjust height and width of schematic node:
	sNode->height += SOCKETHEIGHT;

	// redraw:
	if (redraw == true && m_hWnd != NULL)
	{
		RECT drawRect = sNode->getNodeRect(sNode);
		InvalidateRect(m_hWnd, &drawRect, FALSE);
	}

	return newSock;
}

// ---------------------------- schematicNode deleteSocket --------------------------------------
// delete socket from node
void schematicNode::deleteSocket(HWND m_hWnd, schematicNode* sNode, int delID, bool redraw)
{
	int inCount = sNode->inSockets.Count();
	int totalCount = inCount + sNode->outSockets.Count();

	// reset indices for other socket below this socket:
	int sc = sNode->inSockets.Count();
	for (int i = 0; i < sc; i++)
	{
		if (sNode->inSockets[i]->ID == delID)
		{
			delete sNode->inSockets[i];
			sNode->inSockets.Delete(i, 1);
		}
		else if (sNode->inSockets[i]->ID > delID) sNode->inSockets[i]->ID -= 1;
	}
	sc = sNode->outSockets.Count();
	for (int i = 0; i < sc; i++)
	{
		if (sNode->outSockets[i]->ID == delID)
		{
			delete sNode->outSockets[i];
			sNode->outSockets.Delete(i, 1);
		}
		else if (sNode->outSockets[i]->ID > delID) sNode->outSockets[i]->ID -= 1;
	}

	// adjust height and width of schematic node:
	if (delID == (totalCount - 1))						// if a socket in the middle gets deleted, we cannot change the size
		sNode->height -= SOCKETHEIGHT;

	// redraw:
	if (redraw == true)
	{
		RECT drawRect = sNode->getNodeRect(sNode); // store rectangle before we delete nodes so we know the old size
		drawRect.bottom += SOCKETHEIGHT;
		InvalidateRect(m_hWnd, &drawRect, FALSE);
	}
}

// ---------------------------- schematicNode socketHitCheck --------------------------------------
bool schematicNode::socketHitCheck(hsocket* sock, Point2 pos)
{
	Point2 socketPos = sock->getSocketPosition(sock->owner, sock);	// relative to schematic node
	float socketSize = sock->getSocketSize(sock->owner);

	float TH = SOCKETTHRESHOLD;

	if (
		socketPos.x - TH < pos.x &&								// nodePos smaller then mousePos
		(socketPos.x + socketSize + TH) > pos.x &&				// nodePos + width bigger then mousePos
		socketPos.y - TH < pos.y &&
		(socketPos.y + socketSize + TH) > pos.y
		)
	{
		return true;
	}

	return false;
}

// -------------------------------- schematicNode flip Socket --------------------------------
// flips a socket right to left and visa versa. This updates the sockets position
void schematicNode::flipSocket(hsocket* sock)
{
	bool flipBool = true;
	if (sock->flipped == true) flipBool = false;

	sock->flipped = flipBool;
	if ((sock->type == 0 && flipBool == false) || (sock->type == 1 && flipBool == true))	// in socket, but we now flip it:
		sock->position.x = sock->radius / 2;
	else
		sock->position.x = sock->owner->width - sock->radius * 2;
}

// -------------------------------- schematicNode set bitmap --------------------------------
void schematicNode::setBitmap(HWND m_hWnd, schematicNode* sNode, Value *val)
{
	if (val == &undefined)
	{
		if (sNode->m_hBitmap) DeleteObject(sNode->m_hBitmap);
		sNode->m_hBitmap = NULL;
		sNode->m_maxBitMap = NULL;
	}
	else
	{
		HWND hWnd = MAXScript_interface->GetMAXHWnd();

		MAXBitMap *mbm = (MAXBitMap*)val;
		type_check(mbm, MAXBitMap, _T("nodeBitmap"));
		sNode->m_maxBitMap = val;

		HDC hDC = GetDC(hWnd);
		PBITMAPINFO bmi = mbm->bm->ToDib(32, NULL, FALSE, TRUE);
		if (sNode->m_hBitmap) DeleteObject(sNode->m_hBitmap);
		sNode->m_hBitmap = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmi->bmiColors, bmi, DIB_RGB_COLORS);
		LocalFree(bmi);
		ReleaseDC(hWnd, hDC);
	}
}

// -------------------------------- schematicNode hide from socket --------------------------------
// if you ever want to support the same collapsing for out sockets, all you should have to change
// is change where it says inSocket to out
void schematicNode::hideFromSocket(hsocket* sock, schematicNode* sourceNode, bool hide, bool first)
{
	if (sock->owner != sourceNode || first == true)
	{
		if (first == false) sock->owner->isHidden = hide;
		bool fHide = ((hide == true) ? false : true); // not
		for (int i = 0; i < sock->toSockets.Count(); i++)
		{
			if (sock->toSockets[i]->owner->isHidden == fHide || first == true)	// we have to do this to prevent looping through a node over and over
			{
				if (sock->toSockets[i]->owner != sourceNode)
				{
					sock->toSockets[i]->owner->isHidden = hide;
					for (int s = 0; s < sock->toSockets[i]->owner->inSockets.Count(); s++)
						sock->toSockets[i]->owner->hideFromSocket(sock->toSockets[i]->owner->inSockets[s], sourceNode, hide, false);
					for (int s = 0; s < sock->toSockets[i]->owner->outSockets.Count(); s++)
						sock->toSockets[i]->owner->hideFromSocket(sock->toSockets[i]->owner->outSockets[s], sourceNode, hide, false);
				}
			}
		}
	}
}

// -------------------------------- schematicNode setParent --------------------------------
// by settings a parent we will move relative to our parent
// this can be used to group nodes together. The 'group' would be the parent node
void schematicNode::setParent(schematicNode* child, schematicNode* par, bool adjustPos)
{
	// unlink from previous parent:
	if (child->parent != NULL)
	{
		if (adjustPos == true) child->position += child->parent->position;	// adjust are position so its no longer relative to our current parent.
		for (int i = 0; i < child->parent->children.Count(); i++)
			if (child->parent->children[i] == child) child->parent->children.Delete(i, 1);
	}

	// Link to new parent:
	// set parent and children arrays and adjust our position relative to parent:
	if (par != NULL && adjustPos == true) child->position -= par->position;
	child->parent = par;
	if (par != NULL) par->children.Append(1, &child);	// add to our new parents children-list
}

