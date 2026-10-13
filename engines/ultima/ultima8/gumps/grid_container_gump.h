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

	GridContainerGump();
	GridContainerGump(uint16 owner, uint32 flags = 0, int32 layer = LAYER_NORMAL);
	~GridContainerGump() override;

	void InitGump(Gump *newparent, bool take_focus = true) override;

	void run() override;

	void PaintThis(RenderSurface *, int32 lerp_factor, bool scaled) override;

	uint16 TraceObjId(int32 mx, int32 my) override;

	bool GetLocationOfItem(uint16 itemid, int32 &gx, int32 &gy,
	                       int32 lerp_factor = 256) override;

	bool StartDraggingItem(Item *item, int mx, int my) override;
	bool DraggingItem(Item *item, int mx, int my) override;

	//! Is this the avatar's backpack, shown on the right side?
	bool isRightPanel() const;

protected:
	void GetItemLocation(int32 lerp_factor) override;

private:
	void updateLayout();
	void assignCells();
	Common::Rect32 getCellRect(int col, int row) const;
	Item *getItemInCell(int col, int row) const;

	int32 _cellSize;
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
