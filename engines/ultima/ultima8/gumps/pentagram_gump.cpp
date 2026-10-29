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

#include "ultima/ultima8/gumps/pentagram_gump.h"
#include "ultima/ultima8/gumps/grid_container_gump.h"

#include "ultima/ultima8/gfx/render_surface.h"
#include "ultima/ultima8/gfx/texture.h"
#include "ultima/ultima8/ultima8.h"
#include "ultima/ultima8/kernel/mouse.h"
#include "ultima/ultima8/usecode/uc_list.h"
#include "ultima/ultima8/world/current_map.h"
#include "ultima/ultima8/world/get_object.h"
#include "ultima/ultima8/world/loop_script.h"
#include "ultima/ultima8/world/world.h"
#include "ultima/ultima8/world/actors/main_actor.h"

namespace Ultima {
namespace Ultima8 {

DEFINE_RUNTIME_CLASSTYPE_CODE(PentagramGump)

static const int32 PANEL_MARGIN = 4;
// Distance at which the panel closes
static const int32 CLOSE_RANGE = 512;
// Range around the pentagram to look for candles, foci and reagents
static const uint16 SEARCH_RANGE = 320;

static const uint32 CANDLE_SHAPE = 454;
static const uint32 FOCUS_SHAPE = 397;
static const uint32 REAGENT_SHAPE = 398;

// The following positions are relative to the pentagram and match the
// checks of the pentagram usecode. A lit candle counts within 24 of a point.
static const int32 CANDLE_TOLERANCE = 24;
static const int32 CANDLE_POS[5][2] = {
	{ -70, -70 }, { -201, -41 }, { -266, -167 }, { -167, -266 }, { -41, -201 }
};
// Directions of the candle slots in the panel (times 1000), starting with
// the bottom point to roughly match the pentagram on screen
static const int32 CANDLE_DIR[5][2] = {
	{ 0, 1000 }, { -951, 309 }, { -588, -809 }, { 588, -809 }, { 951, 309 }
};

// Centers the focus on the pentagram. It has to be at least 64 from the
// pentagram position in any direction.
static const int32 FOCUS_POS[2] = { -112, -112 };

// Spots for reagents next to each candle, not blocking the candles or the
// focus. The usecode accepts reagents anywhere on the pentagram.
static const int32 REAGENT_POS[5][2] = {
	{ -106, -74 }, { -185, -77 }, { -230, -159 }, { -159, -230 }, { -77, -185 }
};

static bool isRitualCandle(const Item *item) {
	uint32 type = item->getFrame() & ~3U;
	return item->getShape() == CANDLE_SHAPE && (type == 8 || type == 12);
}

// Same as using an unlit candle
static void lightCandle(Item *candle) {
	if ((candle->getFrame() & 3) == 0)
		candle->setFrame(candle->getFrame() + 1);
}

PentagramGump::PentagramGump() : Gump(), _cellSize(0), _dragSlot(-1) {
	initSlots();
}

PentagramGump::PentagramGump(uint16 pentagram)
	: Gump(0, 0, 5, 5, pentagram, FLAG_ITEM_DEPENDENT, LAYER_NORMAL),
	  _cellSize(0), _dragSlot(-1) {
	initSlots();
}

PentagramGump::~PentagramGump() {
}

void PentagramGump::initSlots() {
	for (int i = 0; i < NUM_SLOTS; i++) {
		Slot &slot = _slots[i];
		if (i < NUM_CANDLES) {
			slot._type = SLOT_CANDLE;
			slot._dx = CANDLE_POS[i][0];
			slot._dy = CANDLE_POS[i][1];
		} else if (i == NUM_CANDLES) {
			slot._type = SLOT_FOCUS;
			slot._dx = FOCUS_POS[0];
			slot._dy = FOCUS_POS[1];
		} else {
			slot._type = SLOT_REAGENT;
			slot._dx = REAGENT_POS[i - NUM_CANDLES - 1][0];
			slot._dy = REAGENT_POS[i - NUM_CANDLES - 1][1];
		}
		slot._item = 0;
	}
}

void PentagramGump::toggle(Item *pentagram) {
	Gump *desktop = Ultima8Engine::get_instance()->getDesktopGump();
	PentagramGump *open = dynamic_cast<PentagramGump *>(desktop->FindGump<PentagramGump>());
	if (open && open->_owner == pentagram->getObjId()) {
		open->Close();
		return;
	}

	PentagramGump *gump = new PentagramGump(pentagram->getObjId());
	gump->InitGump(nullptr);
}

void PentagramGump::InitGump(Gump *newparent, bool take_focus) {
	GridContainerGump::closeLeftPanels();

	Gump::InitGump(newparent, take_focus);
	updateLayout();
	updateSlots();
}

void PentagramGump::updateLayout() {
	if (!_parent)
		return;

	const Common::Rect32 &screen = _parent->getDims();
	int32 width = screen.width() / 3;
	int32 height = screen.height();

	_dims = Common::Rect32(width, height);
	_x = 0;
	_y = 0;

	// Same cell size as the grid panels
	_cellSize = MIN((width - 2 * PANEL_MARGIN) / GridContainerGump::GRID_COLUMNS,
	                (height - 2 * PANEL_MARGIN) / (GridContainerGump::GRID_ROWS + 1));

	// The star fills the width of the panel, with the reagent slots just
	// inside the candle slots. The button is below it.
	int32 size = width - 2 * PANEL_MARGIN;
	int32 cx = width / 2;
	int32 cy = PANEL_MARGIN + size / 2;
	int32 radius = size / 2 - _cellSize / 2;

	for (int i = 0; i < NUM_SLOTS; i++) {
		int32 x = cx, y = cy;
		if (i < NUM_CANDLES) {
			x += radius * CANDLE_DIR[i][0] / 1000;
			y += radius * CANDLE_DIR[i][1] / 1000;
		} else if (i > NUM_CANDLES) {
			int point = i - NUM_CANDLES - 1;
			x += (radius - _cellSize) * CANDLE_DIR[point][0] / 1000;
			y += (radius - _cellSize) * CANDLE_DIR[point][1] / 1000;
		}
		_slots[i]._rect = Common::Rect32(_cellSize, _cellSize);
		_slots[i]._rect.moveTo(x - _cellSize / 2, y - _cellSize / 2);
	}

	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
	int32 top = PANEL_MARGIN + size + _cellSize / 2;
	_buttonRect = Common::Rect32(PANEL_MARGIN, top, width - PANEL_MARGIN, top + font->getFontHeight() + 8);
}

void PentagramGump::updateSlots() {
	for (int i = 0; i < NUM_SLOTS; i++)
		_slots[i]._item = 0;

	Item *pentagram = getItem(_owner);
	if (!pentagram)
		return;

	Point3 pt = pentagram->getLocation();

	UCList itemlist(2);
	LOOPSCRIPT(script, LS_TOKEN_TRUE);
	World::get_instance()->getCurrentMap()->areaSearch(&itemlist, script, sizeof(script),
	                                                   pentagram, SEARCH_RANGE, false);

	Common::Array<Item *> reagents;

	for (uint32 i = 0; i < itemlist.getSize(); i++) {
		Item *item = getItem(itemlist.getuint16(i));
		if (!item || item == pentagram)
			continue;

		Point3 ipt = item->getLocation();
		int32 dx = ipt.x - pt.x;
		int32 dy = ipt.y - pt.y;

		if (item->getShape() == CANDLE_SHAPE) {
			for (int s = 0; s < NUM_CANDLES; s++) {
				if (!_slots[s]._item && ABS(dx - _slots[s]._dx) <= CANDLE_TOLERANCE &&
				        ABS(dy - _slots[s]._dy) <= CANDLE_TOLERANCE) {
					_slots[s]._item = item->getObjId();
					break;
				}
			}
		} else if (item->getShape() == FOCUS_SHAPE) {
			int32 dist = MAX(MAX(ABS(dx), ABS(dy)), ABS(ipt.z - pt.z));
			if (dx < 0 && dy < 0 && dist >= 64 && !_slots[NUM_CANDLES]._item)
				_slots[NUM_CANDLES]._item = item->getObjId();
		} else if (item->getShape() == REAGENT_SHAPE) {
			if (dx <= 0 && dy <= 0 && dx >= -256 && dy >= -256)
				reagents.push_back(item);
		}
	}

	// Reagents on their own spot get that slot, the others any free one
	for (int s = NUM_CANDLES + 1; s < NUM_SLOTS; s++) {
		for (uint i = 0; i < reagents.size(); i++) {
			Point3 ipt = reagents[i]->getLocation();
			if (ipt.x - pt.x == _slots[s]._dx && ipt.y - pt.y == _slots[s]._dy) {
				_slots[s]._item = reagents[i]->getObjId();
				reagents.remove_at(i);
				break;
			}
		}
	}
	for (int s = NUM_CANDLES + 1; s < NUM_SLOTS && !reagents.empty(); s++) {
		if (!_slots[s]._item) {
			_slots[s]._item = reagents.front()->getObjId();
			reagents.remove_at(0);
		}
	}
}

void PentagramGump::run() {
	Gump::run();

	Item *pentagram = getItem(_owner);
	MainActor *avatar = getMainActor();
	if (!pentagram || !avatar) {
		Close();
		return;
	}

	Point3 pt1 = avatar->getLocationAbsolute();
	Point3 pt2 = pentagram->getLocation();
	if (ABS(pt1.x - pt2.x) > CLOSE_RANGE || ABS(pt1.y - pt2.y) > CLOSE_RANGE) {
		Close();
		return;
	}

	updateLayout();
	updateSlots();
}

void PentagramGump::PaintThis(RenderSurface *surf, int32 lerp_factor, bool scaled) {
	surf->fillBlended(TEX32_PACK_RGBA(40, 28, 16, 220), _dims);

	// Lines of the star
	for (int i = 0; i < NUM_CANDLES; i++) {
		const Common::Rect32 &a = _slots[i]._rect;
		const Common::Rect32 &b = _slots[(i + 2) % NUM_CANDLES]._rect;
		surf->drawLine32(TEX32_PACK_RGB(160, 40, 30), (a.left + a.right) / 2, (a.top + a.bottom) / 2,
		                 (b.left + b.right) / 2, (b.top + b.bottom) / 2);
	}

	Common::Rect32 oldClip = surf->getClippingRect();

	for (int i = 0; i < NUM_SLOTS; i++) {
		const Slot &slot = _slots[i];
		Common::Rect32 cell = slot._rect;
		cell.grow(-1);

		uint32 color;
		if (i == _dragSlot)
			color = TEX32_PACK_RGBA(120, 255, 120, 80);
		else if (slot._type == SLOT_CANDLE)
			color = TEX32_PACK_RGBA(255, 200, 120, 48);
		else if (slot._type == SLOT_FOCUS)
			color = TEX32_PACK_RGBA(255, 120, 80, 64);
		else
			color = TEX32_PACK_RGBA(255, 240, 200, 32);
		surf->fillBlended(color, cell);

		Item *item = getItem(slot._item);
		if (!item)
			continue;

		Common::Rect32 clip = slot._rect;
		clip.clip(oldClip);
		surf->setClippingRect(clip);
		GridContainerGump::paintItem(surf, item, slot._rect);
		surf->setClippingRect(oldClip);

		GridContainerGump::paintQuantity(surf, item, slot._rect);
	}

	surf->fillBlended(TEX32_PACK_RGBA(255, 240, 200, 48), _buttonRect);
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
	Common::String text = "Perform ritual";
	GridContainerGump::paintText(surf, text,
	                             _buttonRect.left + (_buttonRect.width() - font->getStringWidth(text)) / 2,
	                             _buttonRect.top + (_buttonRect.height() - font->getFontHeight()) / 2);
}

int PentagramGump::getSlotAt(int32 mx, int32 my) const {
	for (int i = 0; i < NUM_SLOTS; i++) {
		if (_slots[i]._rect.contains(mx, my))
			return i;
	}
	return -1;
}

uint16 PentagramGump::TraceObjId(int32 mx, int32 my) {
	uint16 objId = Gump::TraceObjId(mx, my);
	if (objId && objId != 65535) return objId;

	ParentToGump(mx, my);

	int slot = getSlotAt(mx, my);
	if (slot >= 0 && _slots[slot]._item)
		return _slots[slot]._item;

	return getObjId();
}

bool PentagramGump::canPlace(Item *item, int slot) const {
	Item *pentagram = getItem(_owner);
	MainActor *avatar = getMainActor();
	if (!pentagram || !avatar || slot < 0)
		return false;

	const Slot &s = _slots[slot];
	switch (s._type) {
	case SLOT_CANDLE:
		if (!isRitualCandle(item))
			return false;
		break;
	case SLOT_FOCUS:
		if (item->getShape() != FOCUS_SHAPE)
			return false;
		break;
	case SLOT_REAGENT:
		if (item->getShape() != REAGENT_SHAPE)
			return false;
		break;
	}

	if (s._item && s._item != item->getObjId())
		return false;

	if (!avatar->canReach(pentagram, 128))
		return false;

	Point3 pt = pentagram->getLocation();
	return item->canExistAt(pt.x + s._dx, pt.y + s._dy, pt.z);
}

bool PentagramGump::StartDraggingItem(Item *item, int mx, int my) {
	Item *pentagram = getItem(_owner);
	MainActor *avatar = getMainActor();
	if (!pentagram || !avatar || !avatar->canReach(pentagram, 128))
		return false;

	for (int i = 0; i < NUM_SLOTS; i++) {
		if (_slots[i]._item == item->getObjId()) {
			int32 itemx, itemy;
			GridContainerGump::getItemPaintPosition(item, _slots[i]._rect, itemx, itemy);
			Mouse::get_instance()->setDraggingOffset(mx - itemx, my - itemy);
			return true;
		}
	}
	return false;
}

bool PentagramGump::DraggingItem(Item *item, int mx, int my) {
	_dragSlot = getSlotAt(mx, my);
	if (!canPlace(item, _dragSlot)) {
		_dragSlot = -1;
		return false;
	}

	Mouse::get_instance()->setMouseCursor(Mouse::MOUSE_TARGET);
	return true;
}

void PentagramGump::DraggingItemLeftGump(Item *item) {
	_dragSlot = -1;
}

void PentagramGump::DropItem(Item *item, int mx, int my) {
	_dragSlot = -1;

	int slot = getSlotAt(mx, my);
	if (!canPlace(item, slot))
		return;

	// Put the item on its spot on the pentagram, like dropping it there
	Item *pentagram = getItem(_owner);
	Point3 pt = pentagram->getLocation();
	item->collideMove(pt.x + _slots[slot]._dx, pt.y + _slots[slot]._dy, pt.z, true, true);
	item->fall();

	if (_slots[slot]._type == SLOT_CANDLE)
		lightCandle(item);

	updateSlots();
}

Gump *PentagramGump::onMouseDown(int button, int32 mx, int32 my) {
	Gump *handled = Gump::onMouseDown(button, mx, my);
	if (handled) return handled;

	// Keep clicks from reaching the game world behind the panel
	if (button == Mouse::BUTTON_LEFT || button == Mouse::BUTTON_RIGHT)
		return this;

	return nullptr;
}

void PentagramGump::onMouseClick(int button, int32 mx, int32 my) {
	if (button == Mouse::BUTTON_RIGHT) {
		Close();
		return;
	}

	if (button != Mouse::BUTTON_LEFT)
		return;

	int32 gx = mx, gy = my;
	ParentToGump(gx, gy);
	if (_buttonRect.contains(gx, gy)) {
		performRitual();
		return;
	}

	Item *item = getItem(TraceObjId(mx, my));
	if (item && !Ultima8Engine::get_instance()->isAvatarInStasis())
		item->callUsecodeEvent_look();
}

void PentagramGump::onMouseDouble(int button, int32 mx, int32 my) {
	if (button != Mouse::BUTTON_LEFT)
		return;

	Item *item = getItem(TraceObjId(mx, my));
	if (!item || Ultima8Engine::get_instance()->isAvatarInStasis())
		return;

	if (getMainActor()->canReach(item, 128))
		item->use();
	else
		Mouse::get_instance()->flashCrossCursor();
}

void PentagramGump::performRitual() {
	if (Ultima8Engine::get_instance()->isAvatarInStasis())
		return;

	Item *pentagram = getItem(_owner);
	if (!pentagram)
		return;

	for (int i = 0; i < NUM_CANDLES; i++) {
		Item *candle = getItem(_slots[i]._item);
		if (candle && isRitualCandle(candle))
			lightCandle(candle);
	}

	// The original ritual, as when using the pentagram
	pentagram->callUsecodeEvent_use();
}

} // End of namespace Ultima8
} // End of namespace Ultima
