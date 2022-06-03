/*
author: fulavio
*/
#include "data/keywords/data_keywords.h"
#include "base/parse_helper.h"
#include "base/options.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QStandardPaths>
#include <range/v3/all.hpp>

namespace Keywords {
namespace {

void WriteKeywordsFile();

KeywordsList Data;
QueryCount QCount;
StickerCount SCount;

QString CustomFilePath() {
	return cWorkingDir() + qsl("tdata/sticker_keywords.json");
}

int GetCount(const QString &query, const StickerId &id, bool useQueryCount=false) {
	auto count = 0;

	if (useQueryCount) {
		auto queryCount = QCount.find(query);

		if (queryCount != QCount.end()) {
			auto sticker = queryCount->second.find(id);

			if (sticker != queryCount->second.end())
				count = sticker->second;
		}
	}

	else {
		auto stickerCount = SCount.find(id);

		if (stickerCount != SCount.end())
			count = stickerCount->second;
	}

	return count;
}

void ReadCount(const QJsonObject &count) {
	const auto queryCountObj = count.constFind("query");
	const auto stickerCountObj = count.constFind("sticker");

	if (queryCountObj != count.end()) {
		const auto queryCount = queryCountObj->toObject();

		for (auto i = queryCount.constBegin(), e = queryCount.constEnd(); i != e; ++i) {
			if (!(*i).isObject()) {
				LOG(("Bad entry! Error: object expected"));
				continue;
			}

			const auto query = i.key();
			const auto countList = (*i).toObject();

			if (countList.constBegin() == countList.constEnd()) {
				LOG(("Bad entry! Error: count list '%1' empty").arg(query));
				continue;
			}

			StickerCount count_list;

			for (auto c = countList.constBegin(), e = countList.constEnd(); c != e; ++c) {
				auto id = c.key().toULongLong();
				auto count = c.value().toInt();

				count_list.emplace(id, count);
			}

			QCount.emplace(query, count_list);
		}
	}

	if (stickerCountObj != count.end()) {
		const auto stickerCount = stickerCountObj->toObject();
		
		for (auto i = stickerCount.constBegin(), e = stickerCount.constEnd(); i != e; ++i) {
			auto id = i.key().toULongLong();
			auto count = i.value().toInt();

			SCount.emplace(id, count);
		}
	}
}

bool ReadKeywordsFile() {

	QFile file(CustomFilePath());
	QStringList errors;

	if (!file.exists())
		return false;

	const auto guard = gsl::finally([&] {
		if (!errors.isEmpty()) {
			LOG(("While reading file '%1'..."
			).arg(file.fileName()));
		}
	});

	if (!file.open(QIODevice::ReadOnly)) {
		LOG(("Could not read the file!"));
		return false;
	}

	auto error = QJsonParseError{ 0, QJsonParseError::NoError };

	const auto fileread = file.readAll();

	const auto document = QJsonDocument::fromJson(
		base::parse::stripComments(fileread),
		&error);

	file.seek(0);
	const auto vline = QString(file.readLine(99));
	const auto vline_ = QString(file.readLine(99));
	auto v = 3;
	file.close();

	if (error.error != QJsonParseError::NoError) {
		LOG(("Failed to parse! Error: %2"
		).arg(error.errorString()));
		return false;
	} else if (!document.isObject()) {
		LOG(("Failed to parse! Error: object expected"));
		return false;
	}

	QJsonObject keywords;

	if (vline.contains("SATANAS3")) {
		LOG(("SATANAS3"));

		const auto keywordsObj = document.object().constFind("keywords");
		const auto countObj = document.object().constFind("count");

		if (!keywordsObj->isObject()) {
			LOG(("Bad entry! Error: object expected"));
			return false;
		}

		keywords = keywordsObj->toObject();

		if (!(countObj == document.object().constEnd()) && countObj->isObject()) {
			ReadCount(countObj->toObject());
		}
	}
	else if (vline_.contains("SATANAS2")) {
		LOG(("SATANAS2"));
		v = 2;

		keywords = document.object();
	}
	else {
		LOG(("SATANAS"));
		v = 1;

		keywords = document.object();
	}

	for (auto i = keywords.constBegin(), e = keywords.constEnd(); i != e; ++i) {
		if (!(*i).isArray()) {
			LOG(("Bad entry! Error: array of keywords ids expected"));
			continue;
		}

		const auto keyword = i.key();
		const auto stickers = (*i).toArray();

		if (stickers.constBegin() == stickers.constEnd()) {
			LOG(("Bad entry! Error: keyword '%1' empty").arg(keyword));
			continue;
		}

		StickerList sticker_list;

		for (auto id = stickers.constBegin(), e = stickers.constEnd(); id != e; ++id) {

			if (v > 1) {
				if (!id->isString()) {
					LOG(("Bad entry! Error: keyword id string expected"));
					continue;
				}

				sticker_list.push_back(id->toString().toULongLong());
			}

			else {
				if (!id->isObject()) {
					LOG(("Bad entry! Error: object expected"));
					continue;
				}

				auto sId = id->toObject().constFind("id");

				sticker_list.push_back(sId->toString().toULongLong());
			}
		}

		Data.emplace(keyword, sticker_list);
	}

	if (v < 3)
		WriteKeywordsFile();

	return true;
}

void WriteKeywordsFile() {
	QFile file(CustomFilePath());

	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	const char *defaultHeader = R"HEADER(// SATANAS3
)HEADER";

	file.write(defaultHeader);

	auto doc = QJsonObject();
	auto keywords = QJsonObject();
	auto count = QJsonObject();

	for (const auto &[keyword, stickers] : Data) {
		auto stickerArr = QJsonArray();

		for (const auto &id : stickers)
			stickerArr.append(QString::number(id));

		keywords.insert(keyword, stickerArr);
	}

	if (QCount.size() > 0) {
		auto query = QJsonObject();

		for (const auto &[keyword, list] : QCount) {
			auto countObj = QJsonObject();

			for (const auto &[id, cnt] : list)
				countObj.insert(QString::number(id), cnt);

			query.insert(keyword, countObj);
		}

		count.insert("query", query);
	}

	if (SCount.size() > 0) {
		auto sticker = QJsonObject();

		for (const auto &[id, cnt] : SCount)
			sticker.insert(QString::number(id), cnt);

		count.insert("sticker", sticker);
	}

	doc.insert("keywords", keywords);

	if (QCount.size() > 0 || SCount.size() > 0)
		doc.insert("count", count);

	auto document = QJsonDocument();
	document.setObject(doc);
	file.write(document.toJson(QJsonDocument::Compact));
}

void fill() {
	const auto read = ReadKeywordsFile();
	
	LOG(("keywords [%1]").arg(read?qsl("true"):qsl("true")));
}

} // namespace

std::vector<QString> Keywords::GetKeywords(not_null<DocumentData*> document) {
	struct KeywordWithOrder {
		QString keyword;
		int count;
	};
	auto list = std::vector<KeywordWithOrder>();

	for (const auto [keyword, stickers] : Data) {
		const auto sticker = ranges::find_if(stickers, [&](const StickerId &id) {
			return id == document->id;
		});

		if (sticker != stickers.end()) {
			list.push_back({keyword, GetCount(keyword, *sticker, true)});
		}
	}

	ranges::actions::sort(
		list,
		std::greater<>(),
		&KeywordWithOrder::count);

	auto result = ranges::views::all(
		list
	) | ranges::views::transform([](const KeywordWithOrder &keyword) {
		return keyword.keyword;
	}) | ranges::to_vector;

	return std::move(result);
}

void Keywords::SetKeywords(std::vector<QString> keywords, not_null<DocumentData*> document) {
	auto original = GetKeywords(document);
	auto diff = std::vector<QString>();

	for (const auto keyword : keywords) {
		const auto stickers = Data.find(keyword);

		if (stickers != Data.end()) {
			const auto sticker = ranges::find_if(stickers->second, [&](const StickerId &id) {
				return id == document->id;
			});

			if (sticker == stickers->second.end())
				stickers->second.push_back(document->id);
		}
		else {
			StickerList list = {document->id};
			Data.emplace(keyword, list);
		}
	}

	ranges::set_difference(original, keywords, ranges::back_inserter(diff));

	for (const auto key : diff) {
		const auto stickers = Data.find(key);

		if (stickers == Data.end())
			continue;

		if (stickers->second.size() <= 1)
			Data.remove(key);
		else if (stickers != Data.end()) {
			const auto sticker = ranges::find_if(stickers->second, [&](const StickerId &id) {
				return id == document->id;
			});

			if (sticker != stickers->second.end()) {
				stickers->second.erase(sticker);
			}
		}
	}

	WriteKeywordsFile();
}

int Keywords::SetKeywordCount(const QString &keyword, not_null<DocumentData*> document) {
	auto countList = QCount.find(keyword);
	auto sticker = SCount.find(document->id);
	int count = 1;

	if (countList == QCount.end()) {
		StickerCount list;
		list.emplace(document->id, count);
		QCount.emplace(keyword, list);
	}
	else {
		auto &list = countList->second;
		auto sCount = list.find(document->id);

		if (sCount != list.end())
			count = sCount->second++;
		else
			list.emplace(document->id, count);
	}

	count = 1;

	if (sticker == SCount.end())
		SCount.emplace(document->id, count);
	else
		count = sticker->second++;

	WriteKeywordsFile();

	return count;
}

std::vector<StickerData> Keywords::Query(const QString &sQuery, bool exact) {
	const auto query = sQuery.toLower();
	auto result = std::vector<StickerData>();

	if (query.isEmpty())
		return result;

	const auto add = [&](const StickerList &stickers) {
		for (auto sticker : stickers) {
			if (ranges::find_if(result, [&](const StickerData &s) {
				return s.id == sticker;
			}) == result.end()) {
				result.push_back({sticker, GetCount(query, sticker)});
			}
		}
	};

	for (const auto [keyword, stickers] : Data) {
		if (exact) {
			if (keyword.compare(query, Qt::CaseInsensitive) == 0) {
				add(stickers);
			}
		}

		else if (query == "!" || keyword.contains(query, Qt::CaseInsensitive)) {
			add(stickers);
		}
	}

	return std::move(result);
}

void Start() {
	fill();
}

void Finish() {
}

} // namespace Keywords