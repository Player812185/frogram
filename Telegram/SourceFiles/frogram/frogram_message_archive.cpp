/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_message_archive.h"

#include "base/unixtime.h"
#include "core/version.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_edition.h"
#include "frogram/frogram_settings.h"
#include "main/main_session.h"
#include "storage/serialize_common.h"
#include "storage/storage_account.h"

namespace Frogram {
namespace {

constexpr auto kSaveDelay = crl::time(3000);
constexpr auto kFormatVersion = 1;

[[nodiscard]] QByteArray SerializeEntities(const EntitiesInText &entities) {
	if (entities.isEmpty()) {
		return QByteArray();
	}
	auto stream = Serialize::ByteArrayWriter();
	stream << quint32(entities.size());
	for (const auto &entity : entities) {
		stream
			<< quint8(entity.type())
			<< qint32(entity.offset())
			<< qint32(entity.length())
			<< entity.data();
	}
	return std::move(stream).result();
}

[[nodiscard]] EntitiesInText DeserializeEntities(const QByteArray &data) {
	auto result = EntitiesInText();
	if (data.isEmpty()) {
		return result;
	}
	auto stream = Serialize::ByteArrayReader(data);
	auto count = quint32();
	stream >> count;
	if (!stream.ok() || count > quint32(kArchiveLimit)) {
		return result;
	}
	result.reserve(count);
	for (auto i = quint32(); i != count; ++i) {
		auto type = quint8();
		auto offset = qint32();
		auto length = qint32();
		auto entityData = QString();
		stream >> type >> offset >> length >> entityData;
		if (!stream.ok()) {
			return EntitiesInText();
		}
		result.push_back(EntityInText(
			EntityType(type),
			offset,
			length,
			entityData));
	}
	return result;
}

} // namespace

QString DescribeMedia(not_null<HistoryItem*> item) {
	if (const auto media = item->media()) {
		return media->notificationText().text;
	}
	return QString();
}

MessageArchive::MessageArchive(not_null<Main::Session*> session)
: _session(session)
, _saveTimer([=] { save(); }) {
}

MessageArchive::~MessageArchive() {
	if (_saveTimer.isActive()) {
		_saveTimer.cancel();
		save();
	}
}

bool MessageArchive::worthArchiving(not_null<HistoryItem*> item) const {
	if (!item->isRegular() || item->isEphemeral() || item->isScheduled()) {
		return false;
	} else if (item->ttlDestroyAt() || item->isService()) {
		return false;
	} else if (SkipOutgoing() && item->out()) {
		return false;
	} else if (SkipChannels() && item->history()->peer->isBroadcast()) {
		return false;
	}
	return true;
}

MessageVersion MessageArchive::currentVersion(
		not_null<HistoryItem*> item) const {
	return {
		.text = item->originalText(),
		.mediaDescription = DescribeMedia(item),
		.date = item->date(),
	};
}

ArchivedMessage &MessageArchive::ensureEntry(not_null<HistoryItem*> item) {
	const auto itemId = item->fullId();
	const auto i = _entries.find(itemId);
	if (i != end(_entries)) {
		return i->second;
	}
	_order.push_back(itemId);
	return _entries.emplace(itemId, ArchivedMessage{
		.itemId = itemId,
		.fromId = item->from()->id,
		.date = item->date(),
	}).first->second;
}

bool MessageArchive::interceptDeletion(not_null<HistoryItem*> item) {
	const auto itemId = item->fullId();
	if (const auto i = _deletedByUs.find(itemId); i != end(_deletedByUs)) {
		_deletedByUs.erase(i);
		return false;
	} else if (!SaveDeleted() || !worthArchiving(item)) {
		return false;
	}
	auto &entry = ensureEntry(item);
	if (entry.deletedDate) {
		return KeepDeletedInChat();
	}
	entry.deletedDate = base::unixtime::now();
	if (entry.versions.empty()) {
		entry.versions.push_back(currentVersion(item));
	} else {
		auto &last = entry.versions.back();
		if (last.mediaDescription.isEmpty()) {
			last.mediaDescription = DescribeMedia(item);
		}
	}
	enforceLimits();
	scheduleSave();
	_itemChanges.fire_copy(itemId);
	_allChanges.fire({});
	return KeepDeletedInChat();
}

void MessageArchive::noteEdition(
		not_null<HistoryItem*> item,
		const HistoryMessageEdition &edition) {
	if (!SaveEdits() || !worthArchiving(item)) {
		return;
	}
	const auto itemId = item->fullId();
	auto &entry = ensureEntry(item);
	if (entry.versions.empty()) {
		entry.versions.push_back(currentVersion(item));
	}
	const auto &previous = entry.versions.back();
	if (previous.text == edition.textWithEntities) {
		return;
	}
	entry.versions.push_back({
		.text = edition.textWithEntities,
		.date = (edition.editDate > 0)
			? edition.editDate
			: base::unixtime::now(),
	});
	while (entry.versions.size() > kVersionsPerMessageLimit) {
		entry.versions.erase(begin(entry.versions) + 1);
	}
	enforceLimits();
	scheduleSave();
	_itemChanges.fire_copy(itemId);
	_allChanges.fire({});
}

void MessageArchive::noteDeletedByUs(const MessageIdsList &ids) {
	for (const auto &itemId : ids) {
		_deletedByUs.emplace(itemId);
	}
}

void MessageArchive::noteDeletedByUs(FullMsgId itemId) {
	_deletedByUs.emplace(itemId);
}

bool MessageArchive::deleted(FullMsgId itemId) const {
	const auto i = _entries.find(itemId);
	return (i != end(_entries)) && i->second.wasDeleted();
}

TimeId MessageArchive::deletedDate(FullMsgId itemId) const {
	const auto i = _entries.find(itemId);
	return (i != end(_entries)) ? i->second.deletedDate : 0;
}

bool MessageArchive::edited(FullMsgId itemId) const {
	const auto i = _entries.find(itemId);
	return (i != end(_entries)) && i->second.wasEdited();
}

std::vector<MessageVersion> MessageArchive::versions(
		FullMsgId itemId) const {
	const auto i = _entries.find(itemId);
	return (i != end(_entries))
		? i->second.versions
		: std::vector<MessageVersion>();
}

const ArchivedMessage *MessageArchive::lookup(FullMsgId itemId) const {
	const auto i = _entries.find(itemId);
	return (i != end(_entries)) ? &i->second : nullptr;
}

std::vector<ArchivedMessage> MessageArchive::list(PeerId peerId) const {
	auto result = std::vector<ArchivedMessage>();
	for (const auto &[itemId, entry] : _entries) {
		if (!peerId || itemId.peer == peerId) {
			result.push_back(entry);
		}
	}
	ranges::sort(result, [](
			const ArchivedMessage &a,
			const ArchivedMessage &b) {
		const auto aDate = a.deletedDate ? a.deletedDate : a.date;
		const auto bDate = b.deletedDate ? b.deletedDate : b.date;
		return (aDate > bDate);
	});
	return result;
}

std::vector<PeerId> MessageArchive::peers() const {
	auto result = std::vector<PeerId>();
	for (const auto &[itemId, entry] : _entries) {
		if (!ranges::contains(result, itemId.peer)) {
			result.push_back(itemId.peer);
		}
	}
	return result;
}

int MessageArchive::count() const {
	return int(_entries.size());
}

int MessageArchive::deletedCount() const {
	return int(ranges::count_if(_entries, [](const auto &pair) {
		return pair.second.wasDeleted();
	}));
}

void MessageArchive::forget(FullMsgId itemId) {
	const auto i = _entries.find(itemId);
	if (i == end(_entries)) {
		return;
	}
	_entries.erase(i);
	_order.erase(ranges::remove(_order, itemId), end(_order));
	scheduleSave();
	_itemChanges.fire_copy(itemId);
	_allChanges.fire({});
}

void MessageArchive::forgetPeer(PeerId peerId) {
	auto removed = false;
	for (auto i = begin(_entries); i != end(_entries);) {
		if (i->first.peer == peerId) {
			i = _entries.erase(i);
			removed = true;
		} else {
			++i;
		}
	}
	if (!removed) {
		return;
	}
	_order.erase(ranges::remove_if(_order, [&](FullMsgId itemId) {
		return (itemId.peer == peerId);
	}), end(_order));
	scheduleSave();
	_allChanges.fire({});
}

void MessageArchive::clear() {
	if (_entries.empty()) {
		return;
	}
	_entries.clear();
	_order.clear();
	scheduleSave();
	_allChanges.fire({});
}

void MessageArchive::enforceLimits() {
	while (_order.size() > kArchiveLimit) {
		const auto oldest = _order.front();
		_order.erase(begin(_order));
		_entries.remove(oldest);
	}
}

rpl::producer<FullMsgId> MessageArchive::itemChanges() const {
	return _itemChanges.events();
}

rpl::producer<> MessageArchive::allChanges() const {
	return _allChanges.events();
}

void MessageArchive::scheduleSave() {
	if (!_saveTimer.isActive()) {
		_saveTimer.callOnce(kSaveDelay);
	}
}

void MessageArchive::save() {
	_session->local().writeFrogramArchive();
}

QByteArray MessageArchive::serialize() const {
	if (_entries.empty()) {
		return QByteArray();
	}
	auto stream = Serialize::ByteArrayWriter();
	stream
		<< quint32(kFormatVersion)
		<< quint32(AppVersion)
		<< quint32(_entries.size());
	for (const auto &itemId : _order) {
		const auto i = _entries.find(itemId);
		if (i == end(_entries)) {
			continue;
		}
		const auto &entry = i->second;
		stream
			<< quint64(entry.itemId.peer.value)
			<< qint64(entry.itemId.msg.bare)
			<< quint64(entry.fromId.value)
			<< qint32(entry.date)
			<< qint32(entry.deletedDate)
			<< quint32(entry.versions.size());
		for (const auto &version : entry.versions) {
			stream
				<< version.text.text
				<< SerializeEntities(version.text.entities)
				<< version.mediaDescription
				<< qint32(version.date);
		}
	}
	return std::move(stream).result();
}

void MessageArchive::applySerialized(const QByteArray &serialized) {
	if (_loaded) {
		return;
	}
	_loaded = true;
	if (serialized.isEmpty()) {
		return;
	}
	auto stream = Serialize::ByteArrayReader(serialized);
	auto formatVersion = quint32();
	auto appVersion = quint32();
	auto count = quint32();
	stream >> formatVersion >> appVersion >> count;
	if (!stream.ok() || formatVersion != kFormatVersion) {
		LOG(("Frogram Error: Bad archive format version %1."
			).arg(formatVersion));
		return;
	} else if (count > quint32(kArchiveLimit)) {
		LOG(("Frogram Error: Bad archive count %1.").arg(count));
		return;
	}
	auto entries = base::flat_map<FullMsgId, ArchivedMessage>();
	auto order = std::vector<FullMsgId>();
	entries.reserve(count);
	order.reserve(count);
	for (auto i = quint32(); i != count; ++i) {
		auto peerId = quint64();
		auto msgId = qint64();
		auto fromId = quint64();
		auto date = qint32();
		auto deletedDate = qint32();
		auto versionsCount = quint32();
		stream >> peerId >> msgId >> fromId >> date >> deletedDate;
		stream >> versionsCount;
		if (!stream.ok()
			|| versionsCount > quint32(kVersionsPerMessageLimit)) {
			LOG(("Frogram Error: Could not read archive entry."));
			return;
		}
		auto entry = ArchivedMessage{
			.itemId = FullMsgId(PeerId(peerId), MsgId(msgId)),
			.fromId = PeerId(fromId),
			.date = TimeId(date),
			.deletedDate = TimeId(deletedDate),
		};
		entry.versions.reserve(versionsCount);
		for (auto j = quint32(); j != versionsCount; ++j) {
			auto text = QString();
			auto entities = QByteArray();
			auto mediaDescription = QString();
			auto versionDate = qint32();
			stream >> text >> entities >> mediaDescription >> versionDate;
			if (!stream.ok()) {
				LOG(("Frogram Error: Could not read archive version."));
				return;
			}
			entry.versions.push_back({
				.text = TextWithEntities{
					text,
					DeserializeEntities(entities),
				},
				.mediaDescription = mediaDescription,
				.date = TimeId(versionDate),
			});
		}
		order.push_back(entry.itemId);
		entries.emplace(entry.itemId, std::move(entry));
	}
	_entries = std::move(entries);
	_order = std::move(order);
	_allChanges.fire({});
}

} // namespace Frogram
