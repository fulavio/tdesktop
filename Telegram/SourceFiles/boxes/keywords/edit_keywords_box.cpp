/*
author: fulavio
*/
#include "boxes/keywords/edit_keywords_box.h"

#include "chat_helpers/emoji_suggestions_widget.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/input_fields.h"
#include "ui/effects/panel_animation.h"
#include "data/data_document.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "settings/settings_common.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"
#include "window/window_controller.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_window.h"
#include "data/keywords/data_keywords.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "data/data_session.h"

namespace {

class Controller {
public:
	Controller(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window,
		not_null<DocumentData*> document);

	void prepare();

private:
	struct Keyword {
		not_null<Ui::InputField*> field;
		QString keyword;
		int index;
	};

	void setupCover();
	void setupFields();
	void addKeyword(QString keyword, bool remove=true);
	void removeKeyword(int index);
	void updateKeywords();

	std::vector<Keyword> _keywords;
	not_null<Ui::GenericBox*> _box;
	not_null<Window::SessionController*> _window;
	not_null<DocumentData*> _document;
	Fn<void()> _focus;
	Fn<void()> _save;
};

Controller::Controller(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> window,
	not_null<DocumentData*> document)
: _box(box)
, _window(window)
, _document(document) {
}

void Controller::prepare() {
	// setupCover();
	updateKeywords();

	const auto content = _box->verticalLayout();

	_box->setTitle(rpl::single(qsl("Sticker Keywords")));
	_box->setCloseByOutsideClick(false);

	_box->addButton(tr::lng_box_done(), _save);
	_box->addButton(tr::lng_cancel(), [=] { _box->closeBox(); });
	_box->setFocusCallback(_focus);
}

void Controller::updateKeywords() {
	auto keywords = Keywords::GetKeywords(_document);
	QString keywordStr;

	for (auto k : keywords) {
		keywordStr.append(qsl("%1;").arg(k));
	}

	addKeyword(keywordStr);
	setupFields();
}

void Controller::setupFields() {
	const auto getValue = [](not_null<Ui::InputField*> field) {
		return TextUtilities::SingleLine(field->getLastText()).trimmed();
	};

	_focus = [=] {
		for (const auto k : _keywords) {
			if (k.field->getLastText().isEmpty()) {
				k.field->setFocusFast();
				break;
			}
		}
	};

	_save = [=] {
		auto keywordStr = getValue(_keywords.front().field);
		auto keywords = keywordStr.split(';', QString::SkipEmptyParts).toVector().toStdVector();

		Keywords::SetKeywords(std::move(keywords), _document);

		_box->closeBox();
	};
}

void Controller::addKeyword(QString keyword, bool remove) {
	const auto content = _box->verticalLayout();
	int index = (int)_keywords.size();

	// QString title = keyword.isEmpty() ? ("Nova Keyword") : keyword;
	
	const auto input = content->add(
		object_ptr<Ui::InputField>(
			_box,
			st::defaultInputField,
			rpl::single(qsl("Keywords")),
			keyword),
		st::markdownLinkFieldPadding);
	// input->setMaxLength(32);

	_keywords.push_back(Keyword{input, keyword, index});

	QObject::connect(input, &Ui::InputField::changed, [this, index] {
		auto keyword = _keywords[index];
		keyword.keyword = keyword.field->getLastText();
	});
};

// void Controller::setupCover() {
// 	_box->addRow(
// 		object_ptr<Info::Profile::Cover>(
// 			_box,
// 			_user,
// 			_window,
// 			(_phone.isEmpty()
// 				? tr::lng_contact_mobile_hidden()
// 				: rpl::single(Ui::FormatPhone(_phone)))),
// 		style::margins())->setAttribute(Qt::WA_TransparentForMouseEvents);
// }

void Controller::removeKeyword(int index) {
	const auto i = _keywords.begin()+index;

	Assert(i != end(_keywords));
	_keywords.erase(i);

	updateKeywords();
}

} // namespace

void EditKeywordsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window,
		not_null<DocumentData*> document) {
	box->lifetime().make_state<Controller>(box, window, document)->prepare();
}
