/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "data/data_msg_id.h"
#include "data/data_types.h"

class HistoryItem;
struct HistoryMessageEdition;

namespace Main {
class Session;
} // namespace Main

namespace Frogram {

struct MessageVersion {
	TextWithEntities text;
	QString mediaDescription;
	TimeId date = 0;

	[[nodiscard]] bool empty() const {
		return text.empty() && mediaDescription.isEmpty();
	}
};

struct ArchivedMessage {
	FullMsgId itemId;
	PeerId fromId = 0;
	TimeId date = 0;
	TimeId deletedDate = 0;
	std::vector<MessageVersion> versions;

	[[nodiscard]] bool wasDeleted() const {
		return deletedDate != 0;
	}
	[[nodiscard]] bool wasEdited() const {
		return versions.size() > 1;
	}
	[[nodiscard]] const MessageVersion *original() const {
		return versions.empty() ? nullptr : &versions.front();
	}
	[[nodiscard]] const MessageVersion *latest() const {
		return versions.empty() ? nullptr : &versions.back();
	}
};

class MessageArchive final {
public:
	explicit MessageArchive(not_null<Main::Session*> session);
	MessageArchive(const MessageArchive &other) = delete;
	MessageArchive &operator=(const MessageArchive &other) = delete;
	~MessageArchive();

	[[nodiscard]] bool interceptDeletion(not_null<HistoryItem*> item);
	void noteEdition(
		not_null<HistoryItem*> item,
		const HistoryMessageEdition &edition);
	void noteDeletedByUs(const MessageIdsList &ids);
	void noteDeletedByUs(FullMsgId itemId);

	[[nodiscard]] bool deleted(FullMsgId itemId) const;
	[[nodiscard]] TimeId deletedDate(FullMsgId itemId) const;
	[[nodiscard]] bool edited(FullMsgId itemId) const;
	[[nodiscard]] std::vector<MessageVersion> versions(
		FullMsgId itemId) const;
	[[nodiscard]] const ArchivedMessage *lookup(FullMsgId itemId) const;
	[[nodiscard]] std::vector<ArchivedMessage> list(PeerId peerId) const;
	[[nodiscard]] std::vector<PeerId> peers() const;
	[[nodiscard]] int count() const;
	[[nodiscard]] int deletedCount() const;

	void forget(FullMsgId itemId);
	void forgetPeer(PeerId peerId);
	void clear();

	[[nodiscard]] rpl::producer<FullMsgId> itemChanges() const;
	[[nodiscard]] rpl::producer<> allChanges() const;

	[[nodiscard]] QByteArray serialize() const;
	void applySerialized(const QByteArray &serialized);

private:
	[[nodiscard]] ArchivedMessage &ensureEntry(not_null<HistoryItem*> item);
	[[nodiscard]] MessageVersion currentVersion(
		not_null<HistoryItem*> item) const;
	[[nodiscard]] bool worthArchiving(not_null<HistoryItem*> item) const;
	void enforceLimits();
	void scheduleSave();
	void save();

	const not_null<Main::Session*> _session;

	base::flat_map<FullMsgId, ArchivedMessage> _entries;
	base::flat_set<FullMsgId> _deletedByUs;
	std::vector<FullMsgId> _order;

	rpl::event_stream<FullMsgId> _itemChanges;
	rpl::event_stream<> _allChanges;

	base::Timer _saveTimer;
	bool _loaded = false;

};

[[nodiscard]] QString DescribeMedia(not_null<HistoryItem*> item);

} // namespace Frogram
