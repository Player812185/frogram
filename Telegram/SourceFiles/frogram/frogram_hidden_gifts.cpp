/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_hidden_gifts.h"

#include "api/api_premium.h"
#include "boxes/star_gift_box.h"
#include "chat_helpers/stickers_lottie.h"
#include "core/credits_amount.h"
#include "data/data_peer.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "payments/payments_checkout_process.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_credits.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Frogram {
namespace {

constexpr auto kHiddenGiftStars = int64(50);

[[nodiscard]] QString IconName(uint64 id) {
	return u"frogram/gifts/%1"_q.arg(id);
}

[[nodiscard]] Data::StarGift GiftInfo(
		not_null<Main::Session*> session,
		uint64 id,
		int64 stars,
		const QString &icon) {
	return {
		.id = id,
		.stars = stars,
		.document = ChatHelpers::GenerateLocalTgsSticker(session, icon),
	};
}

[[nodiscard]] auto Descriptors(not_null<Main::Session*> session)
-> std::vector<Info::PeerGifts::GiftDescriptor> {
	auto result = std::vector<Info::PeerGifts::GiftDescriptor>();
	result.reserve(HiddenGifts().size());
	for (const auto &gift : HiddenGifts()) {
		result.push_back(Info::PeerGifts::GiftTypeStars{
			.info = GiftInfo(session, gift.id, gift.stars, IconName(gift.id)),
		});
	}
	return result;
}

void ShowSendBox(
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer,
		Data::StarGift info) {
	window->show(Box(
		Ui::SendGiftBox,
		window,
		peer,
		std::make_shared<Api::PremiumGiftCodeOptions>(peer),
		Info::PeerGifts::GiftTypeStars{ .info = std::move(info) },
		nullptr));
}

void SendGiftById(
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer,
		uint64 id) {
	const auto session = &window->session();
	const auto weak = base::make_weak(window);
	Ui::RequestOurForm(window->uiShow(), MTP_inputInvoiceStarGift(
		MTP_flags(0),
		peer->input(),
		MTP_long(id),
		MTPTextWithEntities()
	), [=](
			uint64 formId,
			CreditsAmount price,
			std::optional<Payments::CheckoutResult> failure) {
		const auto strong = weak.get();
		if (!strong || failure) {
			return;
		}
		ShowSendBox(strong, peer, GiftInfo(
			session,
			id,
			price.whole(),
			u"my_gifts_empty"_q));
	});
}

void HiddenGiftsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer) {
	box->setTitle(tr::lng_frogram_hidden_gifts());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	const auto session = &window->session();
	const auto container = box->verticalLayout();

	Ui::AddSkip(container);
	Ui::AddDividerText(
		container,
		tr::lng_frogram_hidden_gifts_about(
			lt_name,
			rpl::single(peer->shortName())));

	container->add(Ui::MakeGiftsList({
		.window = window,
		.peer = peer,
		.gifts = rpl::single(Ui::GiftsDescriptor{
			Descriptors(session),
			std::make_shared<Api::PremiumGiftCodeOptions>(peer),
		}),
	}));

	Ui::AddDivider(container);
	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, tr::lng_frogram_hidden_gift_custom());

	const auto field = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::giftBoxTextField,
			Ui::InputField::Mode::SingleLine,
			tr::lng_frogram_hidden_gift_custom_placeholder()),
		st::giftBoxTextPadding);

	const auto send = container->add(
		object_ptr<Ui::SettingsButton>(
			container,
			tr::lng_frogram_hidden_gift_custom_send(),
			st::settingsButtonNoIcon));
	send->setClickedCallback([=] {
		auto ok = false;
		const auto id = field->getLastText().trimmed().toULongLong(&ok);
		if (!ok || !id) {
			field->showError();
			window->showToast(tr::lng_frogram_hidden_gift_bad_id(tr::now));
			return;
		}
		SendGiftById(window, peer, id);
		box->closeBox();
	});

	Ui::AddSkip(container);
	Ui::AddDividerText(
		container,
		tr::lng_frogram_hidden_gift_custom_about());
}

} // namespace

const std::vector<HiddenGift> &HiddenGifts() {
	static const auto result = [] {
		const auto ids = std::array<uint64, 11>{ {
			5922558454332916696ULL,
			5956217000635139069ULL,
			5801108895304779062ULL,
			5800655655995968830ULL,
			5866352046986232958ULL,
			5893356958802511476ULL,
			5935895822435615975ULL,
			5969796561943660080ULL,
			6026193266406327981ULL,
			5974210632977745012ULL,
			6046178578163303744ULL,
		} };
		auto list = std::vector<HiddenGift>();
		list.reserve(ids.size());
		for (const auto id : ids) {
			list.push_back({ .id = id, .stars = kHiddenGiftStars });
		}
		return list;
	}();
	return result;
}

void ShowHiddenGiftsBox(
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer) {
	window->show(Box(HiddenGiftsBox, window, peer));
}

void AddHiddenGiftsButton(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer) {
	Ui::AddSkip(container);
	container->add(
		object_ptr<Ui::SettingsButton>(
			container,
			tr::lng_frogram_hidden_gifts(),
			st::settingsButtonNoIcon)
	)->setClickedCallback([=] {
		ShowHiddenGiftsBox(window, peer);
	});
	Ui::AddSkip(container);
}

} // namespace Frogram
