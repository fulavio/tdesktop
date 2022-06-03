/*
author: fulavio
*/
#include "settings/settings_options.h"

#include "ui/boxes/confirm_box.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/gl/gl_detection.h"
#include "base/options.h"
#include "core/application.h"
#include "chat_helpers/tabbed_panel.h"
#include "lang/lang_keys.h"
#include "media/player/media_player_instance.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "window/window_controller.h"
#include "settings/settings_common.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "chat_helpers/field_autocomplete.h"
#include "data/keywords/data_keywords.h"

namespace ExOption {

const char kOptionStickerSendOnEnter[] = "sticker-send-on-enter";
const char kOptionHideChoosingSticker[] = "hide-choosing-sticker";
const char kOptionStickerKeywordsPrefix[] = "sticker-keywords-prefix";
const char kOptionStickerQueryUseCount[] = "sticker-query-use-count";
const char kOptionReplaceKeywords[] = "replace-keywords";

base::options::toggle StickerSendOnEnter({
    .id = kOptionStickerSendOnEnter,
    .name = "Send sticker on enter",
    .description = "Enviar o primeiro sticker dos resultados inline ao pressionar enter.",
});

base::options::toggle HideChoosingSticker({
    .id = kOptionHideChoosingSticker,
    .name = "Hide Choosing sticker action",
    .description = "Esconde ação de escolhendo um sticker para os contatos.",
});

base::options::toggle StickerKeywordsPrefix({
    .id = kOptionStickerKeywordsPrefix,
    .name = "Allow sugesting stickers without prefix",
    .description = "Permite a sugestão/busca de stickers sem utilizar o prefixo '!'",
});

base::options::toggle StickerQueryUseCount({
    .id = kOptionStickerQueryUseCount,
    .name = "Sticker query use count",
    .description = "Ordena os stickers dos resultados com base no texto query.",
});


base::options::toggle ReplaceKeywords({
    .id = kOptionReplaceKeywords,
    .name = "Replace text messages with a keyword",
    .description = "...",
});

bool stickerSendOnEnter() {
	return StickerSendOnEnter.value();
}
bool hideChoosingSticker() {
	return HideChoosingSticker.value();
}
bool stickerKeywordsPrefix() {
	return StickerKeywordsPrefix.value();
}
bool stickerQueryUseCount() {
	return StickerQueryUseCount.value();
}
bool replaceKeywords() {
	return ReplaceKeywords.value();
}

} // namespace ExOption

namespace Settings {
namespace {

void AddOption(
		not_null<Window::Controller*> window,
		not_null<Ui::VerticalLayout*> container,
		base::options::option<bool> &option,
		rpl::producer<> resetClicks) {
	auto &lifetime = container->lifetime();
	const auto name = option.name().isEmpty() ? option.id() : option.name();
	const auto toggles = lifetime.make_state<rpl::event_stream<bool>>();
	std::move(
		resetClicks
	) | rpl::map_to(
		option.defaultValue()
	) | rpl::start_to_stream(*toggles, lifetime);

	const auto button = AddButton(
		container,
		rpl::single(name),
		(option.relevant()
			? st::settingsButtonNoIcon
			: st::settingsOptionDisabled)
	)->toggleOn(toggles->events_starting_with(option.value()));

	const auto restarter = (option.relevant() && option.restartRequired())
		? button->lifetime().make_state<base::Timer>()
		: nullptr;
	if (restarter) {
		restarter->setCallback([=] {
			window->show(Ui::MakeConfirmBox({
				.text = tr::lng_settings_need_restart(),
				.confirmed = [] { Core::Restart(); },
				.confirmText = tr::lng_settings_restart_now(),
				.cancelText = tr::lng_settings_restart_later(),
			}));
		});
	}
	button->toggledChanges(
	) | rpl::start_with_next([=, &option](bool toggled) {
		if (!option.relevant() && toggled != option.defaultValue()) {
			toggles->fire_copy(option.defaultValue());
			window->showToast(
				tr::lng_settings_experimental_irrelevant(tr::now));
			return;
		}
		option.set(toggled);
		if (restarter) {
			restarter->callOnce(st::settingsButtonNoIcon.toggle.duration);
		}
	}, container->lifetime());

	const auto &description = option.description();
	if (!description.isEmpty()) {
		AddSkip(container, st::settingsCheckboxesSkip);
		AddDividerText(container, rpl::single(description));
		AddSkip(container, st::settingsCheckboxesSkip);
	}
}

void SetupOptions(
		not_null<Window::Controller*> window,
		not_null<Ui::VerticalLayout*> container) {
	AddSkip(container, st::settingsCheckboxesSkip);

	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			tr::lng_settings_experimental_about(),
			st::boxLabel),
		st::settingsDividerLabelPadding);

	auto reset = (Button*)nullptr;
	if (base::options::changed()) {
		const auto wrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();
		AddDivider(inner);
		AddSkip(inner, st::settingsCheckboxesSkip);
		reset = AddButton(
			inner,
			tr::lng_settings_experimental_restore(),
			st::settingsButtonNoIcon);
		reset->addClickHandler([=] {
			base::options::reset();
			wrap->hide(anim::type::normal);
		});
		AddSkip(inner, st::settingsCheckboxesSkip);
	}

	AddDivider(container);
	AddSkip(container, st::settingsCheckboxesSkip);

	const auto addToggle = [&](const char name[]) {
		AddOption(
			window,
			container,
			base::options::lookup<bool>(name),
			(reset
				? (reset->clicks() | rpl::to_empty)
				: rpl::producer<>()));
	};
	
	addToggle(ExOption::kOptionStickerSendOnEnter);
	addToggle(ExOption::kOptionHideChoosingSticker);
	addToggle(ExOption::kOptionStickerKeywordsPrefix);
	addToggle(ExOption::kOptionReplaceKeywords);
	// addToggle(Keywords::kOptionStickerQueryUseCount);
}

} // namespace

ExOptions::ExOptions(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent) {
	setupContent(controller);
}

rpl::producer<QString> ExOptions::title() {
	return rpl::single(qsl("Options"));
}

void ExOptions::setupContent(
		not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	SetupOptions(&controller->window(), content);

	Ui::ResizeFitChild(this, content);
}

} // namespace Settings
