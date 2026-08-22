/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class PeerData;

namespace Ui {
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Frogram {

struct HiddenGift {
	uint64 id = 0;
	int64 stars = 0;
};

[[nodiscard]] const std::vector<HiddenGift> &HiddenGifts();

void ShowHiddenGiftsBox(
	not_null<Window::SessionController*> window,
	not_null<PeerData*> peer);

void AddHiddenGiftsButton(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> window,
	not_null<PeerData*> peer);

} // namespace Frogram
