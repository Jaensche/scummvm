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
#include "ultima/ultima8/gfx/fonts/font.h"
#include "ultima/ultima8/gfx/fonts/font_manager.h"
#include "ultima/ultima8/gfx/fonts/rendered_text.h"
#include "ultima/ultima8/ultima8.h"
#include "ultima/ultima8/kernel/kernel.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/world/container.h"
#include "ultima/ultima8/world/actors/main_actor.h"
#include "ultima/ultima8/world/actors/avatar_mover_process.h"
#include "ultima/ultima8/world/get_object.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(GridContainerGump)

static const int32 PANEL_MARGIN = 4;
static const int QUANTITY_FONT = 7;

GridContainerGump::GridContainerGump() : ContainerGump(), _cellSize(0),
	_dragCol(0), _dragRow(0), _quantityText(), _quantityVal() {
}

GridContainerGump::GridContainerGump(uint16 owner, uint32 flags, int32 layer)
	: ContainerGump(nullptr, 0, owner, flags, layer), _cellSize(0),
	  _dragCol(0), _dragRow(0), _quantityText(), _quantityVal() {
}

GridContainerGump::~GridContainerGump() {
	for (int i = 0; i < GRID_ROWS * GRID_COLUMNS; i++)
		delete _quantityText[i];
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

	if (_displayDragging) {
		Common::Rect32 cell = getCellRect(_dragCol, _dragRow);
		cell.grow(-1);
		surf->fillBlended(TEX32_PACK_RGBA(120, 255, 120, 80), cell);
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

		int32 itemx, itemy;
		getItemPaintPosition(item, col, row, itemx, itemy);

		// Cut off anything too large for the cell
		Common::Rect32 cell = getCellRect(col, row);
		Common::Rect32 clip = cell;
		clip.clip(oldClip);
		surf->setClippingRect(clip);
		surf->Paint(item->getShapeObject(), item->getFrame(), itemx, itemy);
		surf->setClippingRect(oldClip);

		paintQuantity(surf, item, cell, col, row);
	}
}

void GridContainerGump::getItemPaintPosition(Item *item, int col, int row, int32 &itemx, int32 &itemy) const {
	const Shape *s = item->getShapeObject();
	assert(s);
	const ShapeFrame *fr = s->getFrame(item->getFrame());
	assert(fr);

	// Center the frame in the cell
	Common::Rect32 cell = getCellRect(col, row);
	itemx = cell.left + (cell.width() - fr->_width) / 2 + fr->_xoff;
	itemy = cell.top + (cell.height() - fr->_height) / 2 + fr->_yoff;
}

void GridContainerGump::paintQuantity(RenderSurface *surf, Item *item, const Common::Rect32 &cell, int col, int row) {
	if (!item->getShapeInfo()->hasQuantity() || item->getQuality() <= 1)
		return;

	int index = row * GRID_COLUMNS + col;
	uint16 quantity = item->getQuality();

	if (!_quantityText[index] || _quantityVal[index] != quantity) {
		delete _quantityText[index];
		Font *font = FontManager::get_instance()->getGameFont(QUANTITY_FONT);
		unsigned int remaining;
		_quantityText[index] = font->renderText(Common::String::format("%u", quantity), remaining);
		_quantityVal[index] = quantity;
	}

	int32 w, h;
	_quantityText[index]->getSize(w, h);
	_quantityText[index]->draw(surf, cell.right - w - 1, cell.bottom - h - 1);
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

Gump *GridContainerGump::onMouseDown(int button, int32 mx, int32 my) {
	Gump *handled = Gump::onMouseDown(button, mx, my);
	if (handled) return handled;

	// Keep clicks from reaching the game world behind the panel
	if (button == Mouse::BUTTON_LEFT || button == Mouse::BUTTON_RIGHT)
		return this;

	return nullptr;
}

void GridContainerGump::onMouseClick(int button, int32 mx, int32 my) {
	if (button == Mouse::BUTTON_RIGHT) {
		Close();
		return;
	}

	ContainerGump::onMouseClick(button, mx, my);
}

void GridContainerGump::onMouseDouble(int button, int32 mx, int32 my) {
	// Unlike the original gumps, double clicking an empty cell does not
	// use (and thereby close) the container.
	if (TraceObjId(mx, my) == getObjId())
		return;

	ContainerGump::onMouseDouble(button, mx, my);
}

GridContainerGump::DropAction GridContainerGump::getDropAction(Item *item, int32 mx, int32 my,
                                                               int &col, int &row, Item *&target) {
	target = nullptr;

	Container *c = getContainer(_owner);
	if (!c || _cellSize <= 0 || !_itemArea.contains(mx, my))
		return DROP_NONE;

	col = (mx - _itemArea.left) / _cellSize;
	row = (my - _itemArea.top) / _cellSize;
	target = getItemInCell(col, row);

	if (!target || target == item)
		return c->CanAddItem(item, true) ? DROP_PLACE : DROP_NONE;

	if (item->getShapeInfo()->hasQuantity() && item->canMergeWith(target))
		return DROP_MERGE;

	Container *targetcontainer = dynamic_cast<Container *>(target);
	if (targetcontainer)
		return targetcontainer->CanAddItem(item, true) ? DROP_INTO_CONTAINER : DROP_NONE;

	if (item->getParent() == _owner)
		return DROP_SWAP;

	return c->CanAddItem(item, true) ? DROP_PLACE : DROP_NONE;
}

bool GridContainerGump::StartDraggingItem(Item *item, int mx, int my) {
	Container *c = getContainer(_owner);
	if (!c) return false;

	// check if the container the item is in is in range
	MainActor *avatar = getMainActor();
	if (!avatar->canReach(c, 128)) return false;

	int32 col, row, itemx, itemy;
	item->getGumpLocation(col, row);
	getItemPaintPosition(item, col, row, itemx, itemy);
	Mouse::get_instance()->setDraggingOffset(mx - itemx, my - itemy);

	return true;
}

bool GridContainerGump::DraggingItem(Item *item, int mx, int my) {
	_displayDragging = false;

	Container *c = getContainer(_owner);
	if (!c) return false;

	// check if the container is in range
	MainActor *avatar = getMainActor();
	if (!avatar->canReach(c, 128)) return false;

	Item *target;
	if (getDropAction(item, mx, my, _dragCol, _dragRow, target) == DROP_NONE)
		return false;

	Mouse::get_instance()->setMouseCursor(Mouse::MOUSE_TARGET);
	_displayDragging = true;
	return true;
}

void GridContainerGump::DropItem(Item *item, int mx, int my) {
	_displayDragging = false;

	int col, row;
	Item *target;
	DropAction action = getDropAction(item, mx, my, col, row, target);

	// Holding the run key (shift by default) asks how many items of a
	// stack to move. The keymapper consumes the key itself, so the
	// keyboard modifier state can't be used here.
	AvatarMoverProcess *mover = Ultima8Engine::get_instance()->getAvatarMoverProcess();
	bool split = mover && mover->hasMovementFlags(AvatarMoverProcess::MOVE_RUN) &&
	             item->getShapeInfo()->hasQuantity() && item->getQuality() > 1;

	switch (action) {
	case DROP_NONE:
		break;

	case DROP_MERGE:
		if (split)
			splitItem(item, target);
		else
			mergeItems(item, target);
		break;

	case DROP_INTO_CONTAINER:
		item->moveToContainer(dynamic_cast<Container *>(target));
		item->randomGumpLocation();
		break;

	case DROP_SWAP: {
		int32 oldcol, oldrow;
		item->getGumpLocation(oldcol, oldrow);
		target->setGumpLocation(oldcol, oldrow);
		item->setGumpLocation(col, row);
		break;
	}

	case DROP_PLACE: {
		Container *c = getContainer(_owner);
		Item *placed = item;

		if (split && target != item) {
			placed = createSplitTarget(item);
			if (!placed)
				break;
			placed->moveToContainer(c);
			splitItem(item, placed);
		} else if (item->getParent() != _owner) {
			item->moveToContainer(c);
		}

		if (target && target != placed)
			placed->randomGumpLocation();
		else
			placed->setGumpLocation(col, row);
		break;
	}
	}
}

} // End of namespace Ultima8
} // End of namespace Ultima
