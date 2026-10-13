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

#include "graphics/font.h"
#include "graphics/fontman.h"

#include "ultima/ultima8/gumps/grid_container_gump.h"

#include "ultima/ultima8/gfx/shape.h"
#include "ultima/ultima8/gfx/shape_frame.h"
#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/gfx/texture.h"
#include "ultima/ultima8/gfx/palette.h"
#include "ultima/ultima8/gfx/fonts/font_manager.h"
#include "ultima/ultima8/gfx/fonts/shape_font.h"
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
// Game font with small white digits, drawn with extra spacing
static const int QUANTITY_FONT = 3;
// Distance at which panels of containers in the world close
static const int32 CLOSE_RANGE = 256;
// Ticks to wait for a game script to close the panel
static const int CLOSE_TIMEOUT = 30;

GridContainerGump::GridContainerGump() : ContainerGump(), _cellSize(0),
	_page(0), _numPages(1), _dragPageFlipped(false),
	_dragCol(0), _dragRow(0), _closeRequested(false), _closeWait(0) {
}

GridContainerGump::GridContainerGump(uint16 owner, uint32 flags, int32 layer)
	: ContainerGump(nullptr, 0, owner, flags, layer), _cellSize(0),
	  _page(0), _numPages(1), _dragPageFlipped(false),
	  _dragCol(0), _dragRow(0), _closeRequested(false), _closeWait(0) {
}

GridContainerGump::~GridContainerGump() {
}

bool GridContainerGump::isLeftPanel(const Gump *g) {
	const GridContainerGump *grid = dynamic_cast<const GridContainerGump *>(g);
	return grid && !grid->IsClosing() && !grid->_closeRequested && !grid->isRightPanel();
}

bool GridContainerGump::isVisibleLeftPanel(const Gump *g) {
	return isLeftPanel(g) && !g->IsHidden();
}

static bool isInsideContainer(const Container *c, ObjId outer) {
	for (const Container *p = c->getParentAsContainer(); p; p = p->getParentAsContainer()) {
		if (p->getObjId() == outer)
			return true;
	}
	return false;
}

void GridContainerGump::InitGump(Gump *newparent, bool take_focus) {
	// Only one container is shown on the left side at a time. A container
	// opened from within the shown one hides it until closed again.
	if (!isRightPanel()) {
		Gump *desktop = Ultima8Engine::get_instance()->getDesktopGump();
		GridContainerGump *other = dynamic_cast<GridContainerGump *>(desktop->FindGump(&isVisibleLeftPanel));
		Container *c = getContainer(_owner);
		if (other && c && isInsideContainer(c, other->_owner)) {
			other->HideGump();
		} else {
			// Close the shown panel and all hidden ones behind it
			while ((other = dynamic_cast<GridContainerGump *>(desktop->FindGump(&isLeftPanel))) != nullptr)
				other->requestClose();
		}
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

void GridContainerGump::Close(bool no_del) {
	GridContainerGump *parentPanel = getParentPanel();
	if (parentPanel)
		parentPanel->UnhideGump();

	ContainerGump::Close(no_del);
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

	// Leave room for a footer of one cell for the page controls
	_cellSize = MIN((width - 2 * PANEL_MARGIN) / GRID_COLUMNS,
	                (height - 2 * PANEL_MARGIN) / (GRID_ROWS + 1));
	int32 footerHeight = _cellSize;

	_dims = Common::Rect32(width, height);
	_itemArea = Common::Rect32(GRID_COLUMNS * _cellSize, GRID_ROWS * _cellSize);
	_itemArea.moveTo((width - _itemArea.width()) / 2,
	                 (height - _itemArea.height() - footerHeight) / 2);
	_footerArea = Common::Rect32(_itemArea.left, _itemArea.bottom,
	                             _itemArea.right, _itemArea.bottom + footerHeight);

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

	if (_closeRequested) {
		if (++_closeWait > CLOSE_TIMEOUT)
			Close();
		return;
	}

	// Unlike the original gumps the panel doesn't follow the container,
	// so close it once the avatar walks away.
	MainActor *avatar = getMainActor();
	if (avatar && !isRightPanel()) {
		Point3 pt1 = avatar->getLocationAbsolute();
		Point3 pt2 = c->getTopItem()->getLocationAbsolute();
		if (ABS(pt1.x - pt2.x) > CLOSE_RANGE || ABS(pt1.y - pt2.y) > CLOSE_RANGE) {
			requestClose();
			return;
		}
	}

	assignCells();
}

void GridContainerGump::requestClose() {
	if (_closeRequested)
		return;

	_closeRequested = true;
	_closeWait = 0;

	Item *item = getItem(_owner);
	if (item)
		item->use();
	else
		Close();
}

GridContainerGump *GridContainerGump::getParentPanel() const {
	Container *c = getContainer(_owner);
	if (!c || isRightPanel()) return nullptr;

	for (Container *p = c->getParentAsContainer(); p; p = p->getParentAsContainer()) {
		GridContainerGump *panel = dynamic_cast<GridContainerGump *>(getGump(p->getGump()));
		if (panel && !panel->isRightPanel() && panel->IsHidden())
			return panel;
	}
	return nullptr;
}

void GridContainerGump::assignCells() {
	Container *c = getContainer(_owner);
	if (!c) return;

	bool showEditorItems = Ultima8Engine::get_instance()->isShowEditorItems();
	const int totalRows = MAX_PAGES * GRID_ROWS;
	bool used[totalRows][GRID_COLUMNS] = {};
	Common::Array<Item *> unplaced;
	int lastRow = 0;

	for (auto *item : c->_contents) {
		if (!showEditorItems && item->getShapeInfo()->is_editor())
			continue;

		int32 col, row;
		item->getGumpLocation(col, row);
		if (col < GRID_COLUMNS && row < totalRows && !used[row][col]) {
			used[row][col] = true;
			lastRow = MAX(lastRow, row);
		} else {
			unplaced.push_back(item);
		}
	}

	// Items without a valid cell get the first free one.
	// Items that don't fit on any page are not shown.
	int cell = 0;
	for (auto *item : unplaced) {
		while (cell < totalRows * GRID_COLUMNS && used[cell / GRID_COLUMNS][cell % GRID_COLUMNS])
			cell++;
		if (cell == totalRows * GRID_COLUMNS)
			break;

		item->setGumpLocation(cell % GRID_COLUMNS, cell / GRID_COLUMNS);
		used[cell / GRID_COLUMNS][cell % GRID_COLUMNS] = true;
		lastRow = MAX(lastRow, cell / GRID_COLUMNS);
	}

	// Show the pages in use, plus an empty one if they are all full
	_numPages = lastRow / GRID_ROWS + 1;
	if (_numPages < MAX_PAGES) {
		bool full = true;
		for (int row = (_numPages - 1) * GRID_ROWS; row < _numPages * GRID_ROWS && full; row++) {
			for (int col = 0; col < GRID_COLUMNS && full; col++)
				full = used[row][col];
		}
		if (full)
			_numPages++;
	}
	_page = MIN(_page, _numPages - 1);
}

bool GridContainerGump::isRowOnPage(int row) const {
	return row / GRID_ROWS == _page;
}

bool GridContainerGump::getCellAt(int32 mx, int32 my, int &col, int &row) const {
	if (_cellSize <= 0 || !_itemArea.contains(mx, my))
		return false;

	col = (mx - _itemArea.left) / _cellSize;
	row = _page * GRID_ROWS + (my - _itemArea.top) / _cellSize;
	return true;
}

int GridContainerGump::getPageArrowAt(int32 mx, int32 my) const {
	if (_numPages <= 1 || !_footerArea.contains(mx, my))
		return 0;

	int32 third = _footerArea.width() / 3;
	if (mx < _footerArea.left + third && _page > 0)
		return -1;
	if (mx >= _footerArea.right - third && _page < _numPages - 1)
		return 1;
	return 0;
}

void GridContainerGump::changePage(int delta) {
	_page = CLIP(_page + delta, 0, _numPages - 1);
}

Common::Rect32 GridContainerGump::getCellRect(int col, int row) const {
	Common::Rect32 r(_cellSize, _cellSize);
	r.moveTo(_itemArea.left + col * _cellSize, _itemArea.top + (row % GRID_ROWS) * _cellSize);
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

void GridContainerGump::Paint(RenderSurface *surf, int32 lerp_factor, bool scaled) {
	// Not shown while waiting to be closed
	if (!_closeRequested)
		ContainerGump::Paint(surf, lerp_factor, scaled);
}

bool GridContainerGump::PointOnGump(int mx, int my) {
	return !_closeRequested && ContainerGump::PointOnGump(mx, my);
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

	if (_displayDragging && isRowOnPage(_dragRow)) {
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
		if (col >= GRID_COLUMNS || !isRowOnPage(row))
			continue;

		Common::Rect32 cell = getCellRect(col, row);
		Common::Rect32 clip = cell;
		clip.clip(oldClip);
		surf->setClippingRect(clip);
		paintItem(surf, item, cell);
		surf->setClippingRect(oldClip);

		paintQuantity(surf, item, cell);
	}

	paintFooter(surf);
}

void GridContainerGump::paintItem(RenderSurface *surf, Item *item, const Common::Rect32 &cell) {
	const Shape *s = item->getShapeObject();
	assert(s);
	const ShapeFrame *fr = s->getFrame(item->getFrame());
	assert(fr);

	int32 maxSize = cell.width() - 2;
	if (fr->_width <= maxSize && fr->_height <= maxSize) {
		int32 col, row, itemx, itemy;
		item->getGumpLocation(col, row);
		getItemPaintPosition(item, col, row, itemx, itemy);
		surf->Paint(s, item->getFrame(), itemx, itemy);
		return;
	}

	// Shrink frames too large for the cell, keeping the aspect ratio
	int32 largest = MAX<int32>(fr->_width, fr->_height);
	int32 w = MAX<int32>(1, fr->_width * maxSize / largest);
	int32 h = MAX<int32>(1, fr->_height * maxSize / largest);
	Common::Rect32 dest(w, h);
	dest.moveTo(cell.left + (cell.width() - w) / 2, cell.top + (cell.height() - h) / 2);
	paintScaled(surf, s, item->getFrame(), dest);
}

void GridContainerGump::paintScaled(RenderSurface *surf, const Shape *s, uint32 frameNum, const Common::Rect32 &dest) {
	const ShapeFrame *fr = s->getFrame(frameNum);
	const Palette *pal = s->getPalette();
	if (!fr || !pal)
		return;

	const Graphics::Surface &src = fr->getSurface();
	Graphics::ManagedSurface *raw = surf->getRawSurface();
	const Graphics::PixelFormat &format = raw->format;
	Common::Rect32 clip = surf->getClippingRect();
	int32 ox, oy;
	surf->GetOrigin(ox, oy);

	// Each destination pixel gets the average color of the source pixels it
	// covers, blended by the share of those that aren't transparent.
	for (int32 dy = 0; dy < dest.height(); dy++) {
		int32 sy0 = dy * src.h / dest.height();
		int32 sy1 = MAX(sy0 + 1, (dy + 1) * src.h / dest.height());

		for (int32 dx = 0; dx < dest.width(); dx++) {
			int32 px = dest.left + dx;
			int32 py = dest.top + dy;
			if (!clip.contains(px, py))
				continue;

			int32 sx0 = dx * src.w / dest.width();
			int32 sx1 = MAX(sx0 + 1, (dx + 1) * src.w / dest.width());

			uint32 r = 0, g = 0, b = 0, opaque = 0, total = 0;
			for (int32 sy = sy0; sy < sy1; sy++) {
				const uint8 *line = static_cast<const uint8 *>(src.getBasePtr(0, sy));
				for (int32 sx = sx0; sx < sx1; sx++) {
					total++;
					if (line[sx] == fr->_keycolor)
						continue;

					uint8 cr, cg, cb;
					format.colorToRGB(pal->_native[line[sx]], cr, cg, cb);
					r += cr;
					g += cg;
					b += cb;
					opaque++;
				}
			}
			if (!opaque)
				continue;

			byte *ptr = static_cast<byte *>(raw->getBasePtr(ox + px, oy + py));
			uint32 dstColor = (format.bytesPerPixel == 4) ? *reinterpret_cast<uint32 *>(ptr)
			                                              : *reinterpret_cast<uint16 *>(ptr);
			uint8 dr, dg, db;
			format.colorToRGB(dstColor, dr, dg, db);

			uint32 alpha = opaque * 256 / total;
			uint32 color = format.RGBToColor(
				(r / opaque * alpha + dr * (256 - alpha)) >> 8,
				(g / opaque * alpha + dg * (256 - alpha)) >> 8,
				(b / opaque * alpha + db * (256 - alpha)) >> 8);

			if (format.bytesPerPixel == 4)
				*reinterpret_cast<uint32 *>(ptr) = color;
			else
				*reinterpret_cast<uint16 *>(ptr) = color;
		}
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

void GridContainerGump::paintText(RenderSurface *surf, const Common::String &text, int32 x, int32 y) {
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
	Graphics::ManagedSurface *raw = surf->getRawSurface();
	int32 ox, oy;
	surf->GetOrigin(ox, oy);

	font->drawString(raw, text, ox + x, oy + y, font->getStringWidth(text),
	                 raw->format.RGBToColor(255, 255, 255));
}

void GridContainerGump::paintQuantity(RenderSurface *surf, Item *item, const Common::Rect32 &cell) {
	if (!item->getShapeInfo()->hasQuantity() || item->getQuality() <= 1)
		return;

	Common::String text = Common::String::format("%u", item->getQuality());
	ShapeFont *font = dynamic_cast<ShapeFont *>(FontManager::get_instance()->getGameFont(QUANTITY_FONT));
	if (!font)
		return;

	int32 w = -1, h = 0;
	for (char c : text) {
		const ShapeFrame *fr = font->getFrame(static_cast<uint8>(c));
		w += fr->_width + 1;
		h = MAX<int32>(h, fr->_height);
	}

	int32 x = cell.right - w - 2;
	int32 y = cell.bottom - h - 2;

	// Darken the background to keep the number readable on any item
	surf->fillBlended(TEX32_PACK_RGBA(0, 0, 0, 160), Common::Rect32(x - 1, y - 1, x + w + 1, y + h + 1));

	// The font has no spacing between digits, so draw them one by one
	for (char c : text) {
		const ShapeFrame *fr = font->getFrame(static_cast<uint8>(c));
		surf->Paint(font, static_cast<uint8>(c), x + fr->_xoff, y + fr->_yoff);
		x += fr->_width + 1;
	}
}

void GridContainerGump::paintFooter(RenderSurface *surf) {
	if (_numPages <= 1)
		return;

	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
	Common::String text = Common::String::format("%d of %d", _page + 1, _numPages);
	int32 w = font->getStringWidth(text);
	int32 h = font->getFontHeight();
	paintText(surf, text, _footerArea.left + (_footerArea.width() - w) / 2,
	          _footerArea.top + (_footerArea.height() - h) / 2);

	// Buttons with arrows to the previous and next page
	int32 size = _footerArea.height();
	int32 arrowSize = size / 3;
	int32 cy = _footerArea.top + size / 2;
	const uint32 color = TEX32_PACK_RGB(255, 240, 200);

	if (_page > 0) {
		Common::Rect32 button(_footerArea.left, _footerArea.top, _footerArea.left + size, _footerArea.bottom);
		button.grow(-1);
		surf->fillBlended(TEX32_PACK_RGBA(255, 240, 200, 48), button);
		int32 tip = button.left + (button.width() - arrowSize) / 2;
		for (int32 i = 0; i < arrowSize; i++)
			surf->fill32(color, tip + i, cy - i, 1, 2 * i + 1);
	}

	if (_page < _numPages - 1) {
		Common::Rect32 button(_footerArea.right - size, _footerArea.top, _footerArea.right, _footerArea.bottom);
		button.grow(-1);
		surf->fillBlended(TEX32_PACK_RGBA(255, 240, 200, 48), button);
		int32 tip = button.right - 1 - (button.width() - arrowSize) / 2;
		for (int32 i = 0; i < arrowSize; i++)
			surf->fill32(color, tip - i, cy - i, 1, 2 * i + 1);
	}
}

uint16 GridContainerGump::TraceObjId(int32 mx, int32 my) {
	uint16 objId = Gump::TraceObjId(mx, my);
	if (objId && objId != 65535) return objId;

	ParentToGump(mx, my);

	int col, row;
	if (getCellAt(mx, my, col, row)) {
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
	if (col >= GRID_COLUMNS || !isRowOnPage(row)) return false;

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
		requestClose();
		return;
	}

	int32 gx = mx, gy = my;
	ParentToGump(gx, gy);
	int arrow = getPageArrowAt(gx, gy);
	if (button == Mouse::BUTTON_LEFT && arrow) {
		changePage(arrow);
		return;
	}

	ContainerGump::onMouseClick(button, mx, my);
}

void GridContainerGump::onMouseDouble(int button, int32 mx, int32 my) {
	// Unlike the original gumps, double clicking an empty cell does not
	// use (and thereby close) the container.
	if (TraceObjId(mx, my) == getObjId()) {
		int32 gx = mx, gy = my;
		ParentToGump(gx, gy);
		int arrow = getPageArrowAt(gx, gy);
		if (button == Mouse::BUTTON_LEFT && arrow)
			changePage(arrow);
		return;
	}

	ContainerGump::onMouseDouble(button, mx, my);
}

GridContainerGump::DropAction GridContainerGump::getDropAction(Item *item, int32 mx, int32 my,
                                                               int &col, int &row, Item *&target) {
	target = nullptr;

	Container *c = getContainer(_owner);
	if (!c || !getCellAt(mx, my, col, row))
		return DROP_NONE;

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

	// Holding an item over a page arrow flips the page once
	int arrow = getPageArrowAt(mx, my);
	if (arrow) {
		if (!_dragPageFlipped)
			changePage(arrow);
		_dragPageFlipped = true;
		return false;
	}
	_dragPageFlipped = false;

	Item *target;
	if (getDropAction(item, mx, my, _dragCol, _dragRow, target) == DROP_NONE)
		return false;

	Mouse::get_instance()->setMouseCursor(Mouse::MOUSE_TARGET);
	_displayDragging = true;
	return true;
}

void GridContainerGump::DraggingItemLeftGump(Item *item) {
	ContainerGump::DraggingItemLeftGump(item);
	_dragPageFlipped = false;
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
