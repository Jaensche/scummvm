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

#ifndef ULTIMA8_GUMPS_PENTAGRAMGUMP_H
#define ULTIMA8_GUMPS_PENTAGRAMGUMP_H

#include "ultima/ultima8/gumps/gump.h"
#include "ultima/ultima8/misc/classtype.h"

namespace Ultima {
namespace Ultima8 {

/**
 * Panel to set up a sorcery ritual at a pentagram. It has slots for the
 * candles, the focus and the reagents, which stay items in the world: an
 * item put into a slot is moved to the matching spot on the pentagram, and
 * the slots show what is lying on these spots.
 */
class PentagramGump : public Gump {
public:
	ENABLE_RUNTIME_CLASSTYPE()

	static const uint32 PENTAGRAM_SHAPE = 124;

	PentagramGump();
	PentagramGump(uint16 pentagram);
	~PentagramGump() override;

	//! Open the panel for a pentagram, or close it if it is already open
	static void toggle(Item *pentagram);

	void InitGump(Gump *newparent, bool take_focus = true) override;

	void run() override;

	void PaintThis(RenderSurface *, int32 lerp_factor, bool scaled) override;

	uint16 TraceObjId(int32 mx, int32 my) override;

	bool StartDraggingItem(Item *item, int mx, int my) override;
	bool DraggingItem(Item *item, int mx, int my) override;
	void DraggingItemLeftGump(Item *item) override;
	void DropItem(Item *item, int mx, int my) override;

	Gump *onMouseDown(int button, int32 mx, int32 my) override;
	void onMouseClick(int button, int32 mx, int32 my) override;
	void onMouseDouble(int button, int32 mx, int32 my) override;

private:
	enum SlotType {
		SLOT_CANDLE,
		SLOT_FOCUS,
		SLOT_REAGENT
	};

	struct Slot {
		SlotType _type;
		int32 _dx, _dy;        // position relative to the pentagram
		Common::Rect32 _rect;  // position in the panel
		ObjId _item;
	};

	static const int NUM_CANDLES = 5;
	static const int NUM_REAGENTS = 5;
	static const int NUM_SLOTS = NUM_CANDLES + 1 + NUM_REAGENTS;

	void initSlots();
	void updateLayout();
	void updateSlots();
	int getSlotAt(int32 mx, int32 my) const;
	bool canPlace(Item *item, int slot) const;
	void performRitual();

	Slot _slots[NUM_SLOTS];
	Common::Rect32 _buttonRect;
	int32 _cellSize;
	int _dragSlot;
};

} // End of namespace Ultima8
} // End of namespace Ultima

#endif
