/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_hidden_gifts.h"

#include "base/random.h"
#include "core/ui_integration.h"
#include "data/components/credits.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "lottie/lottie_icon.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "payments/payments_checkout_process.h"
#include "payments/payments_form.h"
#include "payments/payments_non_panel_process.h"
#include "ui/abstract_button.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/text/custom_emoji_helper.h"
#include "ui/text/text_utilities.h"
#include "ui/ui_utility.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_credits.h"
#include "styles/style_frogram.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Frogram {
namespace {

constexpr auto kHiddenGiftStars = int64(50);

[[nodiscard]] QString IconPath(uint64 id) {
	return u":/animations/frogram/gifts/%1.tgs"_q.arg(id);
}

[[nodiscard]] int MessageLimit(not_null<Main::Session*> session) {
	return session->appConfig().get<int>(
		u"stargifts_message_length_max"_q,
		255);
}

class GiftTile final : public Ui::AbstractButton {
public:
	GiftTile(
		QWidget *parent,
		not_null<Main::Session*> session,
		const HiddenGift &gift);

private:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;

	std::unique_ptr<Lottie::Icon> _icon;
	Ui::Text::CustomEmojiHelper _helper;
	Ui::Text::String _price;

};

GiftTile::GiftTile(
	QWidget *parent,
	not_null<Main::Session*> session,
	const HiddenGift &gift)
: AbstractButton(parent)
, _icon(Lottie::MakeIcon({
	.path = IconPath(gift.id),
	.sizeOverride = st::frogramHiddenGiftIcon,
	.frame = 0,
}))
, _helper(Core::TextContext({ .session = session })) {
	_price.setMarkedText(
		st::semiboldTextStyle,
		Ui::Text::IconEmoji(&st::starIconEmoji).append(
			' ' + QString::number(gift.stars)),
		kMarkupTextOptions,
		_helper.context());
	resize(st::frogramHiddenGiftSize, st::frogramHiddenGiftSize);
	setPointerCursor(true);
}

void GiftTile::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);

	const auto radius = st::frogramHiddenGiftRadius;
	p.setPen(Qt::NoPen);
	p.setBrush(isOver() ? st::windowBgRipple : st::windowBgOver);
	p.drawRoundedRect(rect(), radius, radius);

	const auto sticker = st::frogramHiddenGiftIcon;
	_icon->paintInCenter(p, QRect(
		(width() - sticker.width()) / 2,
		st::frogramHiddenGiftIconTop,
		sticker.width(),
		sticker.height()));

	const auto padding = st::frogramHiddenGiftPricePadding;
	const auto inner = _price.maxWidth();
	const auto buttonw = inner + padding.left() + padding.right();
	const auto buttonh = st::semiboldTextStyle.font->height
		+ padding.top()
		+ padding.bottom();
	const auto buttonx = (width() - buttonw) / 2;
	const auto buttony = height()
		- buttonh
		- st::frogramHiddenGiftPriceBottom;
	const auto button = QRect(buttonx, buttony, buttonw, buttonh);
	p.setBrush(st::creditsBg3);
	p.drawRoundedRect(button, buttonh / 2., buttonh / 2.);

	p.setPen(st::windowFgActive);
	_price.draw(p, {
		.position = QPoint(buttonx + padding.left(), buttony + padding.top()),
		.availableWidth = inner,
	});
}

void GiftTile::onStateChanged(State was, StateChangeSource source) {
	if (((state() ^ was) & State::Enum::Over) && isOver()) {
		const auto frames = _icon->framesCount();
		if (frames > 1) {
			_icon->animate([=] { update(); }, 0, frames - 1);
		}
	}
}

struct SendOptions {
	TextWithEntities message;
	bool anonymous = false;
};

void SendHiddenGift(
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer,
		uint64 giftId,
		SendOptions options) {
	const auto done = [=](Payments::CheckoutResult result) {
		if (result == Payments::CheckoutResult::Paid) {
			window->session().credits().load(true);
			window->showPeerHistory(peer);
		}
	};
	Payments::CheckoutProcess::Start(Payments::InvoiceStarGift{
		.giftId = giftId,
		.randomId = base::RandomValue<uint64>(),
		.message = std::move(options.message),
		.recipient = peer,
		.anonymous = options.anonymous,
	}, done, Payments::ProcessNonPanelPaymentFormFactory(window, done));
}

[[nodiscard]] object_ptr<Ui::RpWidget> MakeTiles(
		not_null<Window::SessionController*> window,
		not_null<PeerData*> peer,
		Fn<SendOptions()> options,
		Fn<void()> chosen) {
	auto result = object_ptr<Ui::RpWidget>((QWidget*)nullptr);
	const auto raw = result.data();
	const auto session = &window->session();

	struct State {
		std::vector<not_null<GiftTile*>> tiles;
	};
	const auto state = raw->lifetime().make_state<State>();
	for (const auto &gift : HiddenGifts()) {
		const auto id = gift.id;
		const auto tile = Ui::CreateChild<GiftTile>(raw, session, gift);
		tile->setClickedCallback([=] {
			SendHiddenGift(window, peer, id, options());
			chosen();
		});
		tile->show();
		state->tiles.push_back(tile);
	}

	raw->widthValue() | rpl::on_next([=](int width) {
		const auto padding = st::frogramHiddenGiftPadding;
		const auto available = width - padding.left() - padding.right();
		const auto single = st::frogramHiddenGiftSize;
		const auto skip = st::frogramHiddenGiftSkip;
		if (available < single) {
			return;
		}
		const auto perRow = std::max(
			(available + skip.x()) / (single + skip.x()),
			1);
		auto left = padding.left();
		auto top = padding.top();
		auto index = 0;
		for (const auto &tile : state->tiles) {
			if (index && !(index % perRow)) {
				left = padding.left();
				top += single + skip.y();
			}
			tile->moveToLeft(left, top, width);
			left += single + skip.x();
			++index;
		}
		raw->resize(width, top + single + padding.bottom());
	}, raw->lifetime());

	return result;
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

	struct State {
		Ui::InputField *message = nullptr;
		Ui::InputField *custom = nullptr;
		bool anonymous = false;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto options = [=] {
		const auto &text = state->message->getTextWithTags();
		return SendOptions{
			.message = TextWithEntities{
				text.text,
				TextUtilities::ConvertTextTagsToEntities(text.tags),
			},
			.anonymous = state->anonymous,
		};
	};
	const auto close = [=] { box->closeBox(); };

	Ui::AddSkip(container);
	Ui::AddDividerText(
		container,
		tr::lng_frogram_hidden_gifts_about(
			lt_name,
			rpl::single(peer->shortName())));

	container->add(MakeTiles(window, peer, options, close));

	state->message = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::giftBoxTextField,
			Ui::InputField::Mode::NoNewlines,
			tr::lng_gift_send_message()),
		st::giftBoxTextPadding);
	state->message->setMaxLength(MessageLimit(session));

	Ui::AddSkip(container);
	if (!peer->isSelf()) {
		container->add(
			object_ptr<Ui::SettingsButton>(
				container,
				tr::lng_gift_send_anonymous(),
				st::settingsButtonNoIcon)
		)->toggleOn(rpl::single(false))->toggledValue(
		) | rpl::on_next([=](bool toggled) {
			state->anonymous = toggled;
		}, container->lifetime());
		Ui::AddSkip(container);
	}
	Ui::AddDivider(container);
	Ui::AddSkip(container);

	Ui::AddSubsectionTitle(container, tr::lng_frogram_hidden_gift_custom());
	state->custom = container->add(
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
		const auto text = state->custom->getLastText().trimmed();
		auto ok = false;
		const auto id = text.toULongLong(&ok);
		if (!ok || !id) {
			state->custom->showError();
			window->showToast(tr::lng_frogram_hidden_gift_bad_id(tr::now));
			return;
		}
		SendHiddenGift(window, peer, id, options());
		close();
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
