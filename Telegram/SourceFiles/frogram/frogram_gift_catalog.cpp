/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_gift_catalog.h"

#include "core/version.h"
#include "data/data_document.h"
#include "main/main_session.h"
#include "storage/serialize_common.h"
#include "storage/serialize_document.h"
#include "storage/storage_account.h"

namespace Frogram {
namespace {

constexpr auto kFormatVersion = 1;
constexpr auto kSaveDelay = crl::time(1000);

[[nodiscard]] bool Sendable(const Data::StarGift &gift) {
	return !gift.unique
		&& !gift.limitedCount
		&& !gift.soldOut
		&& !gift.auction()
		&& gift.stars > 0;
}

} // namespace

GiftCatalog::GiftCatalog(not_null<Main::Session*> session)
: _session(session)
, _saveTimer([=] { save(); }) {
}

void GiftCatalog::remember(const std::vector<Data::StarGift> &gifts) {
	auto changed = false;
	for (const auto &gift : gifts) {
		if (!Sendable(gift)) {
			continue;
		}
		const auto i = _gifts.find(gift.id);
		if (i == end(_gifts)) {
			_gifts.emplace(gift.id, gift);
			_order.push_back(gift.id);
			changed = true;
		} else if (i->second != gift) {
			i->second = gift;
			changed = true;
		}
	}
	while (_order.size() > kGiftCatalogLimit) {
		_gifts.remove(_order.front());
		_order.erase(begin(_order));
		changed = true;
	}
	if (changed) {
		scheduleSave();
	}
}

std::vector<Data::StarGift> GiftCatalog::missingFrom(
		const std::vector<Data::StarGift> &current) const {
	auto known = base::flat_set<uint64>();
	known.reserve(current.size());
	for (const auto &gift : current) {
		known.emplace(gift.id);
	}
	auto result = std::vector<Data::StarGift>();
	for (const auto &id : _order) {
		if (known.contains(id)) {
			continue;
		}
		const auto i = _gifts.find(id);
		if (i != end(_gifts)) {
			result.push_back(i->second);
		}
	}
	ranges::sort(result, ranges::less(), &Data::StarGift::stars);
	return result;
}

QByteArray GiftCatalog::serialize() const {
	if (_gifts.empty()) {
		return QByteArray();
	}
	auto stream = Serialize::ByteArrayWriter();
	stream
		<< quint32(kFormatVersion)
		<< quint32(AppVersion)
		<< quint32(_order.size());
	for (const auto &id : _order) {
		const auto i = _gifts.find(id);
		if (i == end(_gifts)) {
			continue;
		}
		const auto &gift = i->second;
		stream
			<< quint64(gift.id)
			<< qint64(gift.stars)
			<< qint64(gift.starsConverted)
			<< qint64(gift.starsToUpgrade)
			<< qint32(gift.perUserTotal)
			<< qint32(gift.upgradeVariants)
			<< qint32(gift.firstSaleDate)
			<< quint32((gift.requirePremium ? 1 : 0)
				| (gift.peerColorAvailable ? 2 : 0)
				| (gift.upgradable ? 4 : 0)
				| (gift.birthday ? 8 : 0));
		Serialize::Document::writeToStream(
			stream.underlying(),
			gift.document);
	}
	return std::move(stream).result();
}

void GiftCatalog::applySerialized(const QByteArray &serialized) {
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
		LOG(("Frogram Error: Bad gift catalog version %1."
			).arg(formatVersion));
		return;
	} else if (count > quint32(kGiftCatalogLimit)) {
		LOG(("Frogram Error: Bad gift catalog count %1.").arg(count));
		return;
	}
	auto gifts = base::flat_map<uint64, Data::StarGift>();
	auto order = std::vector<uint64>();
	gifts.reserve(count);
	order.reserve(count);
	for (auto i = quint32(); i != count; ++i) {
		auto id = quint64();
		auto stars = qint64();
		auto starsConverted = qint64();
		auto starsToUpgrade = qint64();
		auto perUserTotal = qint32();
		auto upgradeVariants = qint32();
		auto firstSaleDate = qint32();
		auto flags = quint32();
		stream
			>> id
			>> stars
			>> starsConverted
			>> starsToUpgrade
			>> perUserTotal
			>> upgradeVariants
			>> firstSaleDate
			>> flags;
		const auto document = Serialize::Document::readFromStream(
			_session,
			appVersion,
			stream.underlying());
		if (!stream.ok()) {
			LOG(("Frogram Error: Could not read gift catalog content."));
			return;
		} else if (!document || gifts.contains(id)) {
			continue;
		}
		gifts.emplace(id, Data::StarGift{
			.id = id,
			.stars = stars,
			.starsConverted = starsConverted,
			.starsToUpgrade = starsToUpgrade,
			.document = document,
			.perUserTotal = perUserTotal,
			.upgradeVariants = upgradeVariants,
			.firstSaleDate = firstSaleDate,
			.requirePremium = ((flags & 1) != 0),
			.peerColorAvailable = ((flags & 2) != 0),
			.upgradable = ((flags & 4) != 0),
			.birthday = ((flags & 8) != 0),
		});
		order.push_back(id);
	}
	_gifts = std::move(gifts);
	_order = std::move(order);
}

void GiftCatalog::scheduleSave() {
	if (!_saveTimer.isActive()) {
		_saveTimer.callOnce(kSaveDelay);
	}
}

void GiftCatalog::save() {
	_session->local().writeFrogramGifts();
}

} // namespace Frogram
