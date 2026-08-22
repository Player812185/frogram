/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_frogram.h"

#include "frogram/frogram_message_archive.h"
#include "frogram/frogram_settings.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/ui_utility.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

class FrogramOptions : public Section<FrogramOptions> {
public:
	FrogramOptions(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent(not_null<Window::SessionController*> controller);

};

FrogramOptions::FrogramOptions(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent(controller);
}

rpl::producer<QString> FrogramOptions::title() {
	return tr::lng_frogram_settings_title();
}

void FrogramOptions::setupContent(
		not_null<Window::SessionController*> controller) {
	const auto container = Ui::CreateChild<Ui::VerticalLayout>(this);

	const auto addToggle = [&](
			rpl::producer<QString> label,
			Fn<bool()> value,
			Fn<void(bool)> setter) {
		const auto button = AddButtonWithIcon(
			container,
			std::move(label),
			st::settingsButtonNoIcon);
		button->toggleOn(rpl::single(value()));
		button->toggledValue(
		) | rpl::filter([=](bool enabled) {
			return (enabled != value());
		}) | rpl::on_next([=](bool enabled) {
			setter(enabled);
		}, button->lifetime());
		return button;
	};

	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, tr::lng_frogram_title());

	addToggle(
		tr::lng_frogram_save_deleted(),
		[] { return Frogram::SaveDeleted(); },
		[](bool value) { Frogram::SetSaveDeleted(value); });
	Ui::AddDividerText(container, tr::lng_frogram_save_deleted_about());

	Ui::AddSkip(container);
	addToggle(
		tr::lng_frogram_keep_in_chat(),
		[] { return Frogram::KeepDeletedInChat(); },
		[](bool value) { Frogram::SetKeepDeletedInChat(value); });
	Ui::AddDividerText(container, tr::lng_frogram_keep_in_chat_about());

	Ui::AddSkip(container);
	addToggle(
		tr::lng_frogram_save_edits(),
		[] { return Frogram::SaveEdits(); },
		[](bool value) { Frogram::SetSaveEdits(value); });
	Ui::AddDividerText(container, tr::lng_frogram_save_edits_about());

	Ui::AddSkip(container);
	addToggle(
		tr::lng_frogram_skip_outgoing(),
		[] { return Frogram::SkipOutgoing(); },
		[](bool value) { Frogram::SetSkipOutgoing(value); });
	addToggle(
		tr::lng_frogram_skip_channels(),
		[] { return Frogram::SkipChannels(); },
		[](bool value) { Frogram::SetSkipChannels(value); });

	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, tr::lng_frogram_archive_section());

	const auto session = &controller->session();
	auto countText = rpl::single(rpl::empty) | rpl::then(
		session->frogramArchive().allChanges()
	) | rpl::map([=] {
		return tr::lng_frogram_archive_count(
			tr::now,
			lt_count,
			session->frogramArchive().count());
	});

	const auto clear = AddButtonWithIcon(
		container,
		tr::lng_frogram_clear_archive(),
		st::settingsAttentionButtonWithIcon);
	clear->addClickHandler([=] {
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_frogram_clear_archive_sure(),
			.confirmed = [=](Fn<void()> close) {
				session->frogramArchive().clear();
				controller->showToast(tr::lng_frogram_cleared(tr::now));
				close();
			},
			.confirmText = tr::lng_frogram_clear_archive(),
			.confirmStyle = &st::attentionBoxButton,
		}));
	});
	Ui::AddDividerText(container, std::move(countText));

	Ui::ResizeFitChild(this, container);
}

} // namespace

Type FrogramId() {
	return FrogramOptions::Id();
}

} // namespace Settings
