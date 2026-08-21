/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_settings.h"

#include "core/application.h"
#include "core/core_settings.h"

namespace Frogram {
namespace {

rpl::event_stream<> &ChangesStream() {
	static auto result = rpl::event_stream<>();
	return result;
}

[[nodiscard]] bool Read(const char *key, bool fallback) {
	return Core::App().settings().readPref<bool>(key, fallback);
}

void Write(const char *key, bool value) {
	Core::App().settings().writePref<bool>(key, value);
	ChangesStream().fire({});
}

} // namespace

bool SaveDeleted() {
	return Read(kSaveDeletedPref, true);
}

void SetSaveDeleted(bool value) {
	Write(kSaveDeletedPref, value);
}

bool SaveEdits() {
	return Read(kSaveEditsPref, true);
}

void SetSaveEdits(bool value) {
	Write(kSaveEditsPref, value);
}

bool KeepDeletedInChat() {
	return Read(kKeepInChatPref, true);
}

void SetKeepDeletedInChat(bool value) {
	Write(kKeepInChatPref, value);
}

bool SkipOutgoing() {
	return Read(kSkipOutgoingPref, true);
}

void SetSkipOutgoing(bool value) {
	Write(kSkipOutgoingPref, value);
}

bool SkipChannels() {
	return Read(kSkipChannelsPref, false);
}

void SetSkipChannels(bool value) {
	Write(kSkipChannelsPref, value);
}

rpl::producer<> Changes() {
	return ChangesStream().events();
}

} // namespace Frogram
