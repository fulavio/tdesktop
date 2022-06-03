/*
author: fulavio
*/
#pragma once

#include "settings/settings_common.h"

namespace ExOption {

extern const char kOptionStickerSendOnEnter[];
extern const char kOptionHideChoosingSticker[];
extern const char kOptionStickerKeywordsPrefix[];
extern const char kOptionStickerQueryUseCount[];
extern const char kOptionReplaceKeywords[];

bool stickerSendOnEnter();
bool hideChoosingSticker();
bool stickerKeywordsPrefix();
bool stickerQueryUseCount();
bool replaceKeywords();


} // namespace ExOption

namespace Settings {

class ExOptions : public Section<ExOptions> {
public:
	ExOptions(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent(not_null<Window::SessionController*> controller);
};

} // namespace Settings
