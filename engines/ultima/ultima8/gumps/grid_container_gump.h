/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ULTIMA8_GUMPS_GRIDCONTAINERGUMP_H
#define ULTIMA8_GUMPS_GRIDCONTAINERGUMP_H

#include "ultima/ultima8/gumps/container_gump.h"
#include "ultima/ultima8/misc/classtype.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Container gump shown as a fixed full-height panel with a grid of cells.
 * The avatar's backpack is shown on the right side of the screen, any other
 * container on the left. The cell of an item is stored in its gump location.
 */
class GridContainerGump : public ContainerGump {
public:
	ENABLE_RUNTIME_CLASSTYPE()

	static const int GRID_COLUMNS = 5;
	static const int GRID_ROWS = 10;
	static const int MAX_PAGES = 4;

	GridContainerGump();
	GridContainerGump(uint16 owner, uint32 flags = 0, int32 layer = LAYER_NORMAL);
	~GridContainerGump() override;

	void InitGump(Gump *newparent, bool take_focus = true) override;

	void Close(bool no_del = false) override;

	void run() override;

	void Paint(RenderSurface *surf, int32 lerp_factor, bool scaled) override;
	void PaintThis(RenderSurface *, int32 lerp_factor, bool scaled) override;

	uint16 TraceObjId(int32 mx, int32 my) override;
	bool PointOnGump(int mx, int my) override;

	bool GetLocationOfItem(uint16 itemid, int32 &gx, int32 &gy,
	                       int32 lerp_factor = 256) override;

	bool StartDraggingItem(Item *item, int mx, int my) override;
	bool DraggingItem(Item *item, int mx, int my) override;
	void DraggingItemLeftGump(Item *item) override;
	void DropItem(Item *item, int mx, int my) override;

	Gump *onMouseDown(int button, int32 mx, int32 my) override;
	void onMouseClick(int button, int32 mx, int32 my) override;
	void onMouseDouble(int button, int32 mx, int32 my) override;

	//! Is this the avatar's backpack, shown on the right side?
	bool isRightPanel() const;

	//! Is this gump location a grid cell rather than a pixel position
	//! of the original gumps?
	static bool isCellLocation(int32 x, int32 y);

protected:
	void GetItemLocation(int32 lerp_factor) override;

private:
	enum DropAction {
		DROP_NONE,
		DROP_PLACE,         // move to the cell, or the first free one if taken
		DROP_SWAP,          // swap cells with the item in the target cell
		DROP_MERGE,         // merge with the stack in the target cell
		DROP_INTO_CONTAINER // put into the container in the target cell
	};

	static bool isLeftPanel(const Gump *g);
	static bool isVisibleLeftPanel(const Gump *g);

	//! Close the panel through the container's use event, like
	//! double clicking the original gumps, so that game scripts can
	//! e.g. close the lid of a chest.
	void requestClose();

	//! Get the hidden panel of the nearest outer container, if any
	GridContainerGump *getParentPanel() const;

	DropAction getDropAction(Item *item, int32 mx, int32 my, int &col, int &row, Item *&target);
	void getItemPaintPosition(Item *item, int col, int row, int32 &itemx, int32 &itemy) const;
	void updateLayout();
	void assignCells();

	//! Get the cell of an item, false if it doesn't have one
	static bool getItemCell(const Item *item, int32 &col, int32 &row);
	static void setItemCell(Item *item, int32 col, int32 row);

	// Rows count across all pages, so row / GRID_ROWS is the page of a cell
	bool isRowOnPage(int row) const;
	bool getCellAt(int32 mx, int32 my, int &col, int &row) const;
	Common::Rect32 getCellRect(int col, int row) const;
	Item *getItemInCell(int col, int row) const;

	//! Get the page arrow (-1 or 1) at the given point, or 0
	int getPageArrowAt(int32 mx, int32 my) const;
	void changePage(int delta);

	void paintItem(RenderSurface *surf, Item *item, const Common::Rect32 &cell);
	void paintScaled(RenderSurface *surf, const Shape *s, uint32 frameNum, const Common::Rect32 &dest);
	void paintQuantity(RenderSurface *surf, Item *item, const Common::Rect32 &cell);
	void paintFooter(RenderSurface *surf);
	void paintText(RenderSurface *surf, const Common::String &text, int32 x, int32 y);

	int32 _cellSize;
	Common::Rect32 _footerArea;
	int _page;
	int _numPages;
	bool _dragPageFlipped;
	int _dragCol, _dragRow;
	bool _closeRequested;
	int _closeWait;
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
