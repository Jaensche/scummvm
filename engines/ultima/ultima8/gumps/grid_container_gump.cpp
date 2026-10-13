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

#include "ultima/ultima8/gumps/grid_container_gump.h"

#include "ultima/ultima8/gfx/shape.h"
#include "ultima/ultima8/gfx/shape_frame.h"
#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/gfx/texture.h"
#include "ultima/ultima8/ultima8.h"
#include "ultima/ultima8/kernel/kernel.h"
#include "ultima/ultima8/world/container.h"
#include "ultima/ultima8/world/actors/main_actor.h"
#include "ultima/ultima8/world/get_object.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(GridContainerGump)

static const int32 PANEL_MARGIN = 4;

GridContainerGump::GridContainerGump() : ContainerGump(), _cellSize(0) {
}

GridContainerGump::GridContainerGump(uint16 owner, uint32 flags, int32 layer)
	: ContainerGump(nullptr, 0, owner, flags, layer), _cellSize(0) {
}

GridContainerGump::~GridContainerGump() {
}

static bool isOpenLeftPanel(const Gump *g) {
	const GridContainerGump *grid = dynamic_cast<const GridContainerGump *>(g);
	return grid && !grid->IsClosing() && !grid->isRightPanel();
}

void GridContainerGump::InitGump(Gump *newparent, bool take_focus) {
	// Only one container is shown on the left side at a time
	if (!isRightPanel()) {
		Gump *other = Ultima8Engine::get_instance()->getDesktopGump()->FindGump(&isOpenLeftPanel);
		if (other)
			other->Close();
	}

	// Skip the ContainerGump and ItemRelativeGump init, as we don't
	// have a shape and aren't positioned relative to the item.
	Gump::InitGump(newparent, take_focus);
	updateLayout();

	Container *c = getContainer(_owner);
	if (!c) return; // Container gone!?

	for (auto *item : c->_contents) {
		item->enterFastArea();
	}
}

bool GridContainerGump::isRightPanel() const {
	MainActor *avatar = getMainActor();
	return avatar && _owner == avatar->getEquip(ShapeInfo::SE_BACKPACK);
}

void GridContainerGump::updateLayout() {
	if (!_parent)
		return;

	const Common::Rect32 &screen = _parent->getDims();
	int32 width = screen.width() / 3;
	int32 height = screen.height();

	_cellSize = MIN((width - 2 * PANEL_MARGIN) / GRID_COLUMNS,
	                (height - 2 * PANEL_MARGIN) / GRID_ROWS);

	_dims = Common::Rect32(width, height);
	_itemArea = Common::Rect32(GRID_COLUMNS * _cellSize, GRID_ROWS * _cellSize);
	_itemArea.moveTo((width - _itemArea.width()) / 2, (height - _itemArea.height()) / 2);

	_x = isRightPanel() ? screen.width() - width : 0;
	_y = 0;
	_ix = 0;
	_iy = 0;
}

void GridContainerGump::GetItemLocation(int32 lerp_factor) {
	updateLayout();
}

void GridContainerGump::run() {
	Gump::run();

	Container *c = getContainer(_owner);
	if (!c) {
		// Container gone!?
		Close();
		return;
	}

	assignCells();
}

void GridContainerGump::assignCells() {
	Container *c = getContainer(_owner);
	if (!c) return;

	bool showEditorItems = Ultima8Engine::get_instance()->isShowEditorItems();
	bool used[GRID_ROWS][GRID_COLUMNS] = {};
	Common::Array<Item *> unplaced;

	for (auto *item : c->_contents) {
		if (!showEditorItems && item->getShapeInfo()->is_editor())
			continue;

		int32 col, row;
		item->getGumpLocation(col, row);
		if (col < GRID_COLUMNS && row < GRID_ROWS && !used[row][col])
			used[row][col] = true;
		else
			unplaced.push_back(item);
	}

	// Items without a valid cell get the first free one.
	// TODO: Items that don't fit are not shown yet.
	int cell = 0;
	for (auto *item : unplaced) {
		while (cell < GRID_ROWS * GRID_COLUMNS && used[cell / GRID_COLUMNS][cell % GRID_COLUMNS])
			cell++;
		if (cell == GRID_ROWS * GRID_COLUMNS)
			break;

		item->setGumpLocation(cell % GRID_COLUMNS, cell / GRID_COLUMNS);
		used[cell / GRID_COLUMNS][cell % GRID_COLUMNS] = true;
	}
}

Common::Rect32 GridContainerGump::getCellRect(int col, int row) const {
	Common::Rect32 r(_cellSize, _cellSize);
	r.moveTo(_itemArea.left + col * _cellSize, _itemArea.top + row * _cellSize);
	return r;
}

Item *GridContainerGump::getItemInCell(int col, int row) const {
	Container *c = getContainer(_owner);
	if (!c) return nullptr;

	bool showEditorItems = Ultima8Engine::get_instance()->isShowEditorItems();

	for (auto *item : c->_contents) {
		if (!showEditorItems && item->getShapeInfo()->is_editor())
			continue;

		int32 itemcol, itemrow;
		item->getGumpLocation(itemcol, itemrow);
		if (itemcol == col && itemrow == row)
			return item;
	}
	return nullptr;
}

void GridContainerGump::PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) {
	Container *c = getContainer(_owner);
	if (!c) {
		// Container gone!?
		Close();
		return;
	}

	surf->fillBlended(TEX32_PACK_RGBA(40, 28, 16, 220), _dims);

	for (int row = 0; row < GRID_ROWS; row++) {
		for (int col = 0; col < GRID_COLUMNS; col++) {
			Common::Rect32 cell = getCellRect(col, row);
			cell.grow(-1);
			surf->fillBlended(TEX32_PACK_RGBA(255, 240, 200, 32), cell);
		}
	}

	int32 gameframeno = Kernel::get_instance()->getFrameNum();
	bool showEditorItems = Ultima8Engine::get_instance()->isShowEditorItems();
	Common::Rect32 oldClip = surf->getClippingRect();

	for (auto *item : c->_contents) {
		item->setupLerp(gameframeno);

		if (!showEditorItems && item->getShapeInfo()->is_editor())
			continue;

		int32 col, row;
		item->getGumpLocation(col, row);
		if (col >= GRID_COLUMNS || row >= GRID_ROWS)
			continue;

		const Shape *s = item->getShapeObject();
		assert(s);
		const ShapeFrame *fr = s->getFrame(item->getFrame());
		assert(fr);

		// Center the frame in its cell, cutting off anything too large
		Common::Rect32 cell = getCellRect(col, row);
		int32 itemx = cell.left + (cell.width() - fr->_width) / 2 + fr->_xoff;
		int32 itemy = cell.top + (cell.height() - fr->_height) / 2 + fr->_yoff;

		Common::Rect32 clip = cell;
		clip.clip(oldClip);
		surf->setClippingRect(clip);
		surf->Paint(s, item->getFrame(), itemx, itemy);
		surf->setClippingRect(oldClip);
	}
}

uint16 GridContainerGump::TraceObjId(int32 mx, int32 my) {
	uint16 objId = Gump::TraceObjId(mx, my);
	if (objId && objId != 65535) return objId;

	ParentToGump(mx, my);

	if (_cellSize > 0 && _itemArea.contains(mx, my)) {
		int col = (mx - _itemArea.left) / _cellSize;
		int row = (my - _itemArea.top) / _cellSize;
		Item *item = getItemInCell(col, row);
		if (item)
			return item->getObjId();
	}

	return getObjId();
}

bool GridContainerGump::GetLocationOfItem(uint16 itemid, int32 &gx, int32 &gy,
										  int32 lerp_factor) {
	Item *item = getItem(itemid);
	if (!item || item->getParent() != _owner) return false;

	int32 col, row;
	item->getGumpLocation(col, row);
	if (col >= GRID_COLUMNS || row >= GRID_ROWS) return false;

	Common::Rect32 cell = getCellRect(col, row);
	gx = cell.left + cell.width() / 2;
	gy = cell.top + cell.height() / 2;
	return true;
}

// TODO: Dragging items in and out of the grid is not supported yet
bool GridContainerGump::StartDraggingItem(Item *item, int mx, int my) {
	return false;
}

bool GridContainerGump::DraggingItem(Item *item, int mx, int my) {
	return false;
}

} // End of namespace Ultima8
} // End of namespace Ultima
