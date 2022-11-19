/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_cover.h"

#include "data/data_peer_values.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/data_forum_topic.h"
#include "data/stickers/data_custom_emoji.h"
#include "info/profile/info_profile_values.h"
#include "info/profile/info_profile_badge.h"
#include "info/profile/info_profile_emoji_status_panel.h"
#include "info/info_controller.h"
#include "boxes/peers/edit_forum_topic_box.h"
#include "history/view/media/history_view_sticker_player.h"
#include "lang/lang_keys.h"
#include "ui/widgets/labels.h"
#include "ui/text/text_utilities.h"
#include "ui/special_buttons.h"
#include "base/unixtime.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "settings/settings_premium.h"
#include "chat_helpers/stickers_lottie.h"
#include "apiwrap.h"
#include "api/api_peer_photo.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_dialogs.h"

namespace Info::Profile {
namespace {

auto MembersStatusText(int count) {
	return tr::lng_chat_status_members(tr::now, lt_count_decimal, count);
};

auto OnlineStatusText(int count) {
	return tr::lng_chat_status_online(tr::now, lt_count_decimal, count);
};

auto ChatStatusText(int fullCount, int onlineCount, bool isGroup) {
	if (onlineCount > 1 && onlineCount <= fullCount) {
		return tr::lng_chat_status_members_online(
			tr::now,
			lt_members_count,
			MembersStatusText(fullCount),
			lt_online_count,
			OnlineStatusText(onlineCount));
	} else if (fullCount > 0) {
		return isGroup
			? tr::lng_chat_status_members(
				tr::now,
				lt_count_decimal,
				fullCount)
			: tr::lng_chat_status_subscribers(
				tr::now,
				lt_count_decimal,
				fullCount);
	}
	return isGroup
		? tr::lng_group_status(tr::now)
		: tr::lng_channel_status(tr::now);
};

} // namespace

Cover::Cover(
	QWidget *parent,
	not_null<PeerData*> peer,
	not_null<Window::SessionController*> controller)
: Cover(parent, peer, controller, NameValue(peer)) {
}

Cover::Cover(
	QWidget *parent,
	not_null<Data::ForumTopic*> topic,
	not_null<Window::SessionController*> controller)
: Cover(
	parent,
	topic->channel(),
	topic,
	controller,
	TitleValue(topic)) {
}

Cover::Cover(
	QWidget *parent,
	not_null<PeerData*> peer,
	not_null<Window::SessionController*> controller,
	rpl::producer<QString> title)
: Cover(
	parent,
	peer,
	nullptr,
	controller,
	std::move(title)) {
}

[[nodiscard]] const style::InfoProfileCover &CoverStyle(
		not_null<PeerData*> peer,
		Data::ForumTopic *topic) {
	return topic
		? st::infoTopicCover
		: peer->isMegagroup()
		? st::infoProfileMegagroupCover
		: st::infoProfileCover;
}

TopicIconView::TopicIconView(
	not_null<Data::ForumTopic*> topic,
	Fn<bool()> paused,
	Fn<void()> update)
: _topic(topic)
, _paused(std::move(paused))
, _update(std::move(update)) {
	setup(topic);
}

void TopicIconView::paintInRect(QPainter &p, QRect rect) {
	const auto paint = [&](const QImage &image) {
		const auto size = image.size() / style::DevicePixelRatio();
		p.drawImage(
			QRect(
				rect.x() + (rect.width() - size.width()) / 2,
				rect.y() + (rect.height() - size.height()) / 2,
				size.width(),
				size.height()),
			image);
	};
	if (_player && _player->ready()) {
		paint(_player->frame(
			st::infoTopicCover.photo.size,
			QColor(0, 0, 0, 0),
			false,
			crl::now(),
			_paused()).image);
		_player->markFrameShown();
	} else if (!_topic->iconId() && !_image.isNull()) {
		paint(_image);
	}
}

void TopicIconView::setup(not_null<Data::ForumTopic*> topic) {
	setupPlayer(topic);
	setupImage(topic);
}

void TopicIconView::setupPlayer(not_null<Data::ForumTopic*> topic) {
	IconIdValue(
		topic
	) | rpl::map([=](DocumentId id) -> rpl::producer<DocumentData*> {
		if (!id) {
			return rpl::single((DocumentData*)nullptr);
		}
		return topic->owner().customEmojiManager().resolve(
			id
		) | rpl::map([=](not_null<DocumentData*> document) {
			return document.get();
		});
	}) | rpl::flatten_latest(
	) | rpl::map([=](DocumentData *document)
	-> rpl::producer<std::shared_ptr<StickerPlayer>> {
		if (!document) {
			return rpl::single(std::shared_ptr<StickerPlayer>());
		}
		const auto media = document->createMediaView();
		media->checkStickerLarge();
		media->goodThumbnailWanted();

		return rpl::single() | rpl::then(
			document->owner().session().downloaderTaskFinished()
		) | rpl::filter([=] {
			return media->loaded();
		}) | rpl::take(1) | rpl::map([=] {
			auto result = std::shared_ptr<StickerPlayer>();
			const auto sticker = document->sticker();
			if (sticker->isLottie()) {
				result = std::make_shared<HistoryView::LottiePlayer>(
					ChatHelpers::LottiePlayerFromDocument(
						media.get(),
						ChatHelpers::StickerLottieSize::StickerSet,
						st::infoTopicCover.photo.size,
						Lottie::Quality::High));
			} else if (sticker->isWebm()) {
				result = std::make_shared<HistoryView::WebmPlayer>(
					media->owner()->location(),
					media->bytes(),
					st::infoTopicCover.photo.size);
			} else {
				result = std::make_shared<HistoryView::StaticStickerPlayer>(
					media->owner()->location(),
					media->bytes(),
					st::infoTopicCover.photo.size);
			}
			result->setRepaintCallback(_update);
			return result;
		});
	}) | rpl::flatten_latest(
	) | rpl::start_with_next([=](std::shared_ptr<StickerPlayer> player) {
		_player = std::move(player);
		if (!_player) {
			_update();
		}
	}, _lifetime);
}

void TopicIconView::setupImage(not_null<Data::ForumTopic*> topic) {
	rpl::combine(
		TitleValue(topic),
		ColorIdValue(topic)
	) | rpl::map([=](const QString &title, int32 colorId) {
		using namespace Data;
		return ForumTopicIconFrame(colorId, title, st::infoForumTopicIcon);
	}) | rpl::start_with_next([=](QImage &&image) {
		_image = std::move(image);
		_update();
	}, _lifetime);
}

TopicIconButton::TopicIconButton(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<Data::ForumTopic*> topic)
: AbstractButton(parent)
, _view(
		topic,
		[=] { return controller->isGifPausedAtLeastFor(
			Window::GifPauseReason::Layer); },
		[=] { update(); }) {
	resize(st::infoTopicCover.photo.size);
	paintRequest(
	) | rpl::start_with_next([=] {
		auto p = QPainter(this);
		_view.paintInRect(p, rect());
	}, lifetime());
}

Cover::Cover(
	QWidget *parent,
	not_null<PeerData*> peer,
	Data::ForumTopic *topic,
	not_null<Window::SessionController*> controller,
	rpl::producer<QString> title)
: FixedHeightWidget(parent, CoverStyle(peer, topic).height)
, _st(CoverStyle(peer, topic))
, _controller(controller)
, _peer(peer)
, _emojiStatusPanel(peer->isSelf()
	? std::make_unique<EmojiStatusPanel>()
	: nullptr)
, _badge(
	std::make_unique<Badge>(
		this,
		st::infoPeerBadge,
		peer,
		_emojiStatusPanel.get(),
		[=] {
			return controller->isGifPausedAtLeastFor(
				Window::GifPauseReason::Layer);
		}))
, _userpic(topic
	? nullptr
	: object_ptr<Ui::UserpicButton>(
		this,
		controller,
		_peer,
		Ui::UserpicButton::Role::OpenPhoto,
		_st.photo))
, _iconButton(topic
	? object_ptr<TopicIconButton>(this, controller, topic)
	: nullptr)
, _name(this, _st.name)
, _status(this, _st.status)
, _refreshStatusTimer([this] { refreshStatusText(); }) {
	_peer->updateFull();

	_name->setSelectable(true);
	_name->setContextCopyText(tr::lng_profile_copy_fullname(tr::now));

	if (!_peer->isMegagroup()) {
		_status->setAttribute(Qt::WA_TransparentForMouseEvents);
	}

	_badge->setPremiumClickCallback([=] {
		if (const auto panel = _emojiStatusPanel.get()) {
			panel->show(_controller, _badge->widget(), _badge->sizeTag());
		} else {
			::Settings::ShowEmojiStatusPremium(_controller, _peer);
		}
	});
	_badge->updated() | rpl::start_with_next([=] {
		refreshNameGeometry(width());
	}, _name->lifetime());

	initViewers(std::move(title));
	setupChildGeometry();

	if (_userpic) {
		_userpic->uploadPhotoRequests(
		) | rpl::start_with_next([=] {
			_peer->session().api().peerPhoto().upload(
				_peer,
				_userpic->takeResultImage());
		}, _userpic->lifetime());
	} else if (topic->canEdit()) {
		_iconButton->setClickedCallback([=] {
			_controller->show(Box(
				EditForumTopicBox,
				_controller,
				topic->history(),
				topic->rootId()));
		});
	} else {
		_iconButton->setAttribute(Qt::WA_TransparentForMouseEvents);
	}
}

void Cover::setupChildGeometry() {
	widthValue(
	) | rpl::start_with_next([this](int newWidth) {
		if (_userpic) {
			_userpic->moveToLeft(_st.photoLeft, _st.photoTop, newWidth);
		} else {
			_iconButton->moveToLeft(_st.photoLeft, _st.photoTop, newWidth);
		}
		refreshNameGeometry(newWidth);
		refreshStatusGeometry(newWidth);
	}, lifetime());
}

Cover *Cover::setOnlineCount(rpl::producer<int> &&count) {
	std::move(
		count
	) | rpl::start_with_next([this](int count) {
		_onlineCount = count;
		refreshStatusText();
	}, lifetime());
	return this;
}

void Cover::initViewers(rpl::producer<QString> title) {
	using Flag = Data::PeerUpdate::Flag;
	std::move(
		title
	) | rpl::start_with_next([=](const QString &title) {
		_name->setText(title);
		refreshNameGeometry(width());
	}, lifetime());

	_peer->session().changes().peerFlagsValue(
		_peer,
		Flag::OnlineStatus | Flag::Members
	) | rpl::start_with_next(
		[=] { refreshStatusText(); },
		lifetime());
	if (!_peer->isUser()) {
		_peer->session().changes().peerFlagsValue(
			_peer,
			Flag::Rights
		) | rpl::start_with_next(
			[=] { refreshUploadPhotoOverlay(); },
			lifetime());
	} else if (_peer->isSelf()) {
		refreshUploadPhotoOverlay();
	}
}

void Cover::refreshUploadPhotoOverlay() {
	if (!_userpic) {
		return;
	}
	_userpic->switchChangePhotoOverlay([&] {
		if (const auto chat = _peer->asChat()) {
			return chat->canEditInformation();
		} else if (const auto channel = _peer->asChannel()) {
			return channel->canEditInformation();
		}
		return _peer->isSelf();
	}());
}

void Cover::refreshStatusText() {
	auto hasMembersLink = [&] {
		if (auto megagroup = _peer->asMegagroup()) {
			return megagroup->canViewMembers();
		}
		return false;
	}();
	auto statusText = [&]() -> TextWithEntities {
		using namespace Ui::Text;
		auto currentTime = base::unixtime::now();
		if (auto user = _peer->asUser()) {
			const auto result = Data::OnlineTextFull(user, currentTime);
			const auto showOnline = Data::OnlineTextActive(user, currentTime);
			const auto updateIn = Data::OnlineChangeTimeout(user, currentTime);
			if (showOnline) {
				_refreshStatusTimer.callOnce(updateIn);
			}
			return showOnline
				? PlainLink(result)
				: TextWithEntities{ .text = result };
		} else if (auto chat = _peer->asChat()) {
			if (!chat->amIn()) {
				return tr::lng_chat_status_unaccessible({}, WithEntities);
			}
			auto fullCount = std::max(
				chat->count,
				int(chat->participants.size()));
			return { .text = ChatStatusText(fullCount, _onlineCount, true) };
		} else if (auto channel = _peer->asChannel()) {
			auto fullCount = qMax(channel->membersCount(), 1);
			auto result = ChatStatusText(
				fullCount,
				_onlineCount,
				channel->isMegagroup());
			return hasMembersLink
				? PlainLink(result)
				: TextWithEntities{ .text = result };
		}
		return tr::lng_chat_status_unaccessible(tr::now, WithEntities);
	}();
	_status->setMarkedText(statusText);
	if (hasMembersLink) {
		_status->setLink(1, std::make_shared<LambdaClickHandler>([=] {
			_showSection.fire(Section::Type::Members);
		}));
	}
	refreshStatusGeometry(width());
}

Cover::~Cover() {
}

void Cover::refreshNameGeometry(int newWidth) {
	auto nameWidth = newWidth - _st.nameLeft - _st.rightSkip;
	if (const auto widget = _badge->widget()) {
		nameWidth -= st::infoVerifiedCheckPosition.x() + widget->width();
	}
	_name->resizeToNaturalWidth(nameWidth);
	_name->moveToLeft(_st.nameLeft, _st.nameTop, newWidth);
	const auto badgeLeft = _st.nameLeft + _name->width();
	const auto badgeTop = _st.nameTop;
	const auto badgeBottom = _st.nameTop + _name->height();
	_badge->move(badgeLeft, badgeTop, badgeBottom);
}

void Cover::refreshStatusGeometry(int newWidth) {
	auto statusWidth = newWidth - _st.statusLeft - _st.rightSkip;
	_status->resizeToWidth(statusWidth);
	_status->moveToLeft(_st.statusLeft, _st.statusTop, newWidth);
}

} // namespace Info::Profile
