/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_edit_history_box.h"

#include "base/unixtime.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "frogram/frogram_message_archive.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Frogram {
namespace {

[[nodiscard]] QString FormatMoment(TimeId date) {
	if (!date) {
		return QString();
	}
	const auto parsed = base::unixtime::parse(date);
	return QLocale().toString(parsed, QLocale::ShortFormat);
}

[[nodiscard]] TextWithEntities VersionContent(const MessageVersion &version) {
	auto result = version.text;
	if (!version.mediaDescription.isEmpty()) {
		if (!result.text.isEmpty()) {
			result.append(u"\n"_q);
		}
		result.append(Ui::Text::Italic(version.mediaDescription));
	}
	if (result.text.isEmpty()) {
		result = Ui::Text::Italic(tr::lng_frogram_empty_message(tr::now));
	}
	return result;
}

void AddVersionRow(
		not_null<Ui::GenericBox*> box,
		const QString &title,
		const MessageVersion &version) {
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		rpl::single(title),
		st::boxDividerLabel));
	box->addSkip(st::defaultVerticalListSkip);

	const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		st::boxLabel));
	label->setMarkedText(VersionContent(version));
	label->setSelectable(true);
	box->addSkip(st::boxMediumSkip);
}

} // namespace

bool HasArchiveEntry(
		not_null<Main::Session*> session,
		FullMsgId itemId) {
	const auto entry = session->frogramArchive().lookup(itemId);
	return entry && (entry->wasEdited() || entry->wasDeleted());
}

void EditHistoryBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		FullMsgId itemId) {
	box->setTitle(tr::lng_frogram_edit_history_title());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });

	const auto entry = session->frogramArchive().lookup(itemId);
	if (!entry || entry->versions.empty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_frogram_no_versions(),
			st::boxLabel));
		return;
	}

	const auto count = int(entry->versions.size());
	for (auto i = 0; i != count; ++i) {
		const auto &version = entry->versions[i];
		const auto moment = FormatMoment(version.date);
		const auto title = !i
			? tr::lng_frogram_version_original(tr::now, lt_date, moment)
			: (i == count - 1)
			? tr::lng_frogram_version_current(tr::now, lt_date, moment)
			: tr::lng_frogram_version_edited(tr::now, lt_date, moment);
		AddVersionRow(box, title, version);
	}

	if (entry->wasDeleted()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(tr::lng_frogram_version_deleted(
				tr::now,
				lt_date,
				FormatMoment(entry->deletedDate))),
			st::boxDividerLabel));
	}
}

void ArchivedPeerBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		PeerId peerId) {
	const auto session = &controller->session();
	const auto peer = session->data().peer(peerId);

	box->setTitle(rpl::single(peer->name()));
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	const auto list = session->frogramArchive().list(peerId);
	if (list.empty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_frogram_archive_empty(),
			st::boxLabel));
		return;
	}

	for (const auto &entry : list) {
		const auto author = session->data().peer(entry.fromId);
		const auto moment = FormatMoment(
			entry.deletedDate ? entry.deletedDate : entry.date);
		const auto status = entry.wasDeleted()
			? tr::lng_frogram_status_deleted(tr::now, lt_date, moment)
			: tr::lng_frogram_status_edited(tr::now, lt_date, moment);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(author->name() + u" — "_q + status),
			st::boxDividerLabel));
		box->addSkip(st::defaultVerticalListSkip);

		const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			st::boxLabel));
		if (const auto version = entry.latest()) {
			label->setMarkedText(VersionContent(*version));
		}
		label->setSelectable(true);
		box->addSkip(st::boxMediumSkip);
	}
}

} // namespace Frogram
