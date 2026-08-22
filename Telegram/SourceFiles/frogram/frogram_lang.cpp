/*
This file is part of Frogram,
a modification of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "frogram/frogram_lang.h"

#include "lang/lang_file_parser.h"

namespace Frogram {
namespace {

struct Pack {
	QString languageId;
	QString path;
};

[[nodiscard]] const std::vector<Pack> &Packs() {
	static const auto result = std::vector<Pack>{
		{ u"ru"_q, u":/misc/langs/frogram_ru.strings"_q },
	};
	return result;
}

} // namespace

QByteArray LanguagePack(const QString &languageId) {
	const auto base = languageId.split('-').front();
	for (const auto &pack : Packs()) {
		if (pack.languageId == base) {
			return Lang::FileParser::ReadFile(pack.path, pack.path);
		}
	}
	return QByteArray();
}

} // namespace Frogram
