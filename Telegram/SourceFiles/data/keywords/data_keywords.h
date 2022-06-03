/*
author: fulavio
*/
#pragma once

#include "logs.h"
#include "base/flat_map.h"
#include "data/data_document.h"

namespace Keywords {

using StickerId = quint64;
using StickerList = std::vector<StickerId>;
using KeywordsList = base::flat_map<QString, StickerList>;
using StickerCount = base::flat_map<StickerId, int>;
using QueryCount = base::flat_map<QString, StickerCount>;

struct StickerData {
	StickerId id;
	int count;
};

std::vector<StickerData> Query(const QString &query, bool exact=false);
std::vector<QString> GetKeywords(not_null<DocumentData*> document);
void SetKeywords(std::vector<QString> keywords, not_null<DocumentData*> document);
int SetKeywordCount(const QString &keyword, not_null<DocumentData*> document);

void Start();
void Finish();

} // namespace Keywords
