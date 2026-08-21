/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Frogram {

inline constexpr auto kSaveDeletedPref = "frogram_save_deleted";
inline constexpr auto kSaveEditsPref = "frogram_save_edits";
inline constexpr auto kKeepInChatPref = "frogram_keep_deleted_in_chat";
inline constexpr auto kSkipOutgoingPref = "frogram_skip_outgoing";
inline constexpr auto kSkipChannelsPref = "frogram_skip_channels";
inline constexpr auto kArchiveLimit = 20000;
inline constexpr auto kVersionsPerMessageLimit = 32;

[[nodiscard]] bool SaveDeleted();
void SetSaveDeleted(bool value);

[[nodiscard]] bool SaveEdits();
void SetSaveEdits(bool value);

[[nodiscard]] bool KeepDeletedInChat();
void SetKeepDeletedInChat(bool value);

[[nodiscard]] bool SkipOutgoing();
void SetSkipOutgoing(bool value);

[[nodiscard]] bool SkipChannels();
void SetSkipChannels(bool value);

[[nodiscard]] rpl::producer<> Changes();

} // namespace Frogram
