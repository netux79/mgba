/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "menu.h"

#include "util/gui.h"
#include "util/gui/font.h"

#ifdef _3DS
#include <3ds.h>
#endif

DEFINE_VECTOR(GUIMenuItemList, struct GUIMenuItem);

enum GUIMenuExitReason GUIShowMenu(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuItem** item) {
	size_t start = 0;
	size_t lineHeight = GUIFontHeight(params->font);
	size_t pageSize = params->height / lineHeight;
	if (pageSize > 4) {
		pageSize -= 4;
	} else {
		pageSize = 1;
	}
	int cursorOverItem = 0;

	GUIInvalidateKeys(params);
	while (true) {
#ifdef _3DS
		if (!aptMainLoop()) {
			return GUI_MENU_EXIT_CANCEL;
		}
#endif
		uint32_t newInput = 0;
		GUIPollInput(params, &newInput, 0);
		unsigned cx, cy;
		enum GUICursorState cursor = GUIPollCursor(params, &cx, &cy);

		if (newInput & (1 << GUI_INPUT_UP) && menu->index > 0) {
			--menu->index;
		}
		if (newInput & (1 << GUI_INPUT_DOWN) && menu->index < GUIMenuItemListSize(&menu->items) - 1) {
			++menu->index;
		}
		if (newInput & (1 << GUI_INPUT_LEFT)) {
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
			if (item->validStates) {
				if (item->state > 0) {
					unsigned oldState = item->state;
					do {
						--item->state;
					} while (!item->validStates[item->state] && item->state > 0);
					if (!item->validStates[item->state]) {
						item->state = oldState;
					}
				}
			} else if (menu->index >= pageSize) {
				menu->index -= pageSize;
			} else {
				menu->index = 0;
			}
		}
		if (newInput & (1 << GUI_INPUT_RIGHT)) {
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
			if (item->validStates) {
				if (item->state < item->nStates - 1) {
					unsigned oldState = item->state;
					do {
						++item->state;
					} while (!item->validStates[item->state] && item->state < item->nStates - 1);
					if (!item->validStates[item->state]) {
						item->state = oldState;
					}
				}
			} else if (menu->index + pageSize < GUIMenuItemListSize(&menu->items)) {
				menu->index += pageSize;
			} else {
				menu->index = GUIMenuItemListSize(&menu->items) - 1;
			}
		}
		if (cursor != GUI_CURSOR_NOT_PRESENT) {
			if (cx < params->width - 16) {
				int index = (cy / lineHeight) - 2;
				if (index >= 0 && index + start < GUIMenuItemListSize(&menu->items)) {
					if (menu->index != index + start || !cursorOverItem) {
						cursorOverItem = 1;
					}
					menu->index = index + start;
				} else {
					cursorOverItem = 0;
				}
			} else if (cursor == GUI_CURSOR_DOWN || cursor == GUI_CURSOR_DRAGGING) {
				if (cy <= 2 * lineHeight && cy > lineHeight && menu->index > 0) {
					--menu->index;
				} else if (cy <= params->height && cy > params->height - lineHeight && menu->index < GUIMenuItemListSize(&menu->items) - 1) {
					++menu->index;
				} else if (cy <= params->height - lineHeight && cy > 2 * lineHeight) {
					size_t location = cy - 2 * lineHeight;
					location *= GUIMenuItemListSize(&menu->items);
					menu->index = location / (params->height - 3 * lineHeight);
				}
			}
		}

		if (menu->index < start) {
			start = menu->index;
		}
		while ((menu->index - start + 4) * lineHeight > params->height) {
			++start;
		}
		if (newInput & (1 << GUI_INPUT_CANCEL)) {
			break;
		}
		if (newInput & (1 << GUI_INPUT_SELECT) || (cursorOverItem == 2 && cursor == GUI_CURSOR_CLICKED)) {
			*item = GUIMenuItemListGetPointer(&menu->items, menu->index);
			if ((*item)->submenu) {
				enum GUIMenuExitReason reason = GUIShowMenu(params, (*item)->submenu, item);
				if (reason != GUI_MENU_EXIT_BACK) {
					return reason;
				}
			} else {
				return GUI_MENU_EXIT_ACCEPT;
			}
		}
		if (cursorOverItem == 1 && (cursor == GUI_CURSOR_UP || cursor == GUI_CURSOR_NOT_PRESENT)) {
			cursorOverItem = 2;
		}
		if (newInput & (1 << GUI_INPUT_BACK)) {
			return GUI_MENU_EXIT_BACK;
		}

		params->drawStart();
		if (menu->background) {
			menu->background->draw(menu->background, GUIMenuItemListGetPointer(&menu->items, menu->index)->data);
		}
		if (params->guiPrepare) {
			params->guiPrepare();
		}
		unsigned y = lineHeight;
		GUIFontPrint(params->font, 0, y, GUI_ALIGN_LEFT, 0xFFFFFFFF, menu->title);
		if (menu->subtitle) {
			GUIFontPrint(params->font, 0, y * 2, GUI_ALIGN_LEFT, 0xFFFFFFFF, menu->subtitle);
		}
		y += 2 * lineHeight;
		size_t itemsPerScreen = (params->height - y) / lineHeight;
		size_t i;
		for (i = start; i < GUIMenuItemListSize(&menu->items); ++i) {
			int color = 0xE0A0A0A0;
			if (i == menu->index) {
				color = 0xFFFFFFFF;
				GUIFontDrawIcon(params->font, 2, y, GUI_ALIGN_BOTTOM | GUI_ALIGN_LEFT, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_POINTER);
			}
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, i);
			GUIFontPrintf(params->font, 0, y, GUI_ALIGN_LEFT, color, "  %s", item->title);
			if (item->validStates && item->validStates[item->state]) {
				GUIFontPrintf(params->font, params->width, y, GUI_ALIGN_RIGHT, color, "%s ", item->validStates[item->state]);
			}
			y += lineHeight;
			if (y + lineHeight > params->height) {
				break;
			}
		}

		if (itemsPerScreen < GUIMenuItemListSize(&menu->items)) {
			y = 2 * lineHeight;
			GUIFontDrawIcon(params->font, params->width - 8, y, GUI_ALIGN_HCENTER | GUI_ALIGN_BOTTOM, GUI_ORIENT_VMIRROR, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_BUTTON);
			for (; y < params->height - 16; y += 16) {
				GUIFontDrawIcon(params->font, params->width - 8, y, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_TRACK);
			}
			GUIFontDrawIcon(params->font, params->width - 8, y, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_BUTTON);

			size_t top = 2 * lineHeight;
			y = menu->index * (y - top - 16) / GUIMenuItemListSize(&menu->items);
			GUIFontDrawIcon(params->font, params->width - 8, top + y, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_THUMB);
		}

		GUIDrawBattery(params);
		GUIDrawClock(params);

		if (cursor != GUI_CURSOR_NOT_PRESENT) {
			GUIFontDrawIcon(params->font, cx, cy, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_CURSOR);
		}

		if (params->guiFinish) {
			params->guiFinish();
		}
		params->drawEnd();
	}
	return GUI_MENU_EXIT_CANCEL;
}

enum GUICursorState GUIPollCursor(struct GUIParams* params, unsigned* x, unsigned* y) {
	if (!params->pollCursor) {
		return GUI_CURSOR_NOT_PRESENT;
	}
	enum GUICursorState state = params->pollCursor(x, y);
	if (params->cursorState == GUI_CURSOR_DOWN) {
		int dragX = *x - params->cx;
		int dragY = *y - params->cy;
		if (dragX * dragX + dragY * dragY > 25) {
			params->cursorState = GUI_CURSOR_DRAGGING;
			return GUI_CURSOR_DRAGGING;
		}
		if (state == GUI_CURSOR_UP || state == GUI_CURSOR_NOT_PRESENT) {
			params->cursorState = GUI_CURSOR_UP;
			return GUI_CURSOR_CLICKED;
		}
	} else {
		params->cx = *x;
		params->cy = *y;
	}
	if (params->cursorState == GUI_CURSOR_DRAGGING) {
		if (state == GUI_CURSOR_UP || state == GUI_CURSOR_NOT_PRESENT) {
			params->cursorState = GUI_CURSOR_UP;
			return GUI_CURSOR_UP;
		}
		return GUI_CURSOR_DRAGGING;
	}
	params->cursorState = state;
	return params->cursorState;
}

void GUIInvalidateKeys(struct GUIParams* params) {
	for (int i = 0; i < GUI_INPUT_MAX; ++i) {
		params->inputHistory[i] = 0;
	}
}

void GUIDrawBattery(struct GUIParams* params) {
	if (!params->batteryState) {
		return;
	}
	int state = params->batteryState();
	uint32_t color = 0xFF000000;
	if (state == (BATTERY_CHARGING | BATTERY_FULL)) {
		color |= 0xFFC060;
	} else if (state & BATTERY_CHARGING) {
		color |= 0x60FF60;
	} else if (state >= BATTERY_HALF) {
		color |= 0xFFFFFF;
	} else if (state == BATTERY_LOW) {
		color |= 0x30FFFF;
	} else {
		color |= 0x3030FF;
	}

	enum GUIIcon batteryIcon;
	switch (state & ~BATTERY_CHARGING) {
	case BATTERY_EMPTY:
		batteryIcon = GUI_ICON_BATTERY_EMPTY;
		break;
	case BATTERY_LOW:
		batteryIcon = GUI_ICON_BATTERY_LOW;
		break;
	case BATTERY_HALF:
		batteryIcon = GUI_ICON_BATTERY_HALF;
		break;
	case BATTERY_HIGH:
		batteryIcon = GUI_ICON_BATTERY_HIGH;
		break;
	case BATTERY_FULL:
		batteryIcon = GUI_ICON_BATTERY_FULL;
		break;
	default:
		batteryIcon = GUI_ICON_BATTERY_EMPTY;
		break;
	}

	GUIFontDrawIcon(params->font, params->width, 0, GUI_ALIGN_RIGHT, GUI_ORIENT_0, color, batteryIcon);
}

void GUIDrawClock(struct GUIParams* params) {
	char buffer[32];
	time_t t = time(0);
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
	GUIFontPrint(params->font, params->width / 2, GUIFontHeight(params->font), GUI_ALIGN_HCENTER, 0xFFFFFFFF, buffer);
}
