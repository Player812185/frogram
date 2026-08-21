/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_msg_id.h"

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Frogram {

void EditHistoryBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session,
	FullMsgId itemId);

void ArchivedPeerBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	PeerId peerId);

[[nodiscard]] bool HasArchiveEntry(
	not_null<Main::Session*> session,
	FullMsgId itemId);

} // namespace Frogram
