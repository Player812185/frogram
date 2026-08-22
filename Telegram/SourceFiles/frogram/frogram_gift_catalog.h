/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "data/data_star_gift.h"

namespace Main {
class Session;
} // namespace Main

namespace Frogram {

inline constexpr auto kGiftCatalogLimit = 256;

class GiftCatalog final {
public:
	explicit GiftCatalog(not_null<Main::Session*> session);

	void remember(const std::vector<Data::StarGift> &gifts);
	[[nodiscard]] std::vector<Data::StarGift> missingFrom(
		const std::vector<Data::StarGift> &current) const;

	[[nodiscard]] QByteArray serialize() const;
	void applySerialized(const QByteArray &serialized);

private:
	void scheduleSave();
	void save();

	const not_null<Main::Session*> _session;

	base::flat_map<uint64, Data::StarGift> _gifts;
	std::vector<uint64> _order;
	base::Timer _saveTimer;
	bool _loaded = false;

};

} // namespace Frogram
