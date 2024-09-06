#include "chatlist.hpp"
#include <audio/voice_playback_manager.hpp>
#include <hooks/gjbasegamelayer.hpp>
#include <managers/profile_cache.hpp>
#include <managers/friend_list.hpp>
#include <managers/settings.hpp>
#include <util/ui.hpp>
#include <util/misc.hpp>
#include <data/packets/client/game.hpp>
//#include <ui/game/chat_overlay/overlay.hpp>

using namespace geode::prelude;

bool GlobedChatListPopup::setup() {
    m_noElasticity = true;

	this->setTitle("Chat");
    this->m_title->setScale(.95f);

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    scroll = ScrollLayer::create(ccp(300, 150));
    scroll->setAnchorPoint(ccp(0, 0));
    scroll->ignoreAnchorPointForPosition(false);

    menu = CCMenu::create();

    menu->setPosition({this->m_title->getPositionX(),25});
    this->m_mainLayer->addChild(menu);

    inp = TextInput::create(260, "Message", "chatFont.fnt");
    menu->addChild(inp);

    inp->setCommonFilter(CommonFilter::Any);
    inp->setMaxCharCount(60);

    CCSprite* reviewSpr = CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png");
    reviewSpr->setScale(0.75);

    reviewButton = CCMenuItemSpriteExtra::create(
        reviewSpr,
        this,
        menu_selector(GlobedChatListPopup::onChat)
    );

    menu->addChild(reviewButton);
    menu->setLayout(RowLayout::create());
    menu->setContentWidth(342);
    menu->updateLayout();
    menu->setTouchPriority(-510);

    background = CCScale9Sprite::create("square02_small.png");
    background->setContentSize(scroll->getContentSize());
    background->setOpacity(75);
    background->setPosition(winSize / 2);
    this->addChild(background);
    background->addChild(scroll);

    scroll->m_contentLayer->removeAllChildren();

    this->setTouchEnabled(true);
    CCTouchDispatcher::get()->addTargetedDelegate(this, -129, true);
    CCTouchDispatcher::get()->addTargetedDelegate(scroll, -130, true);

    for (auto message : GlobedGJBGL::get()->m_fields->chatMessages) {
        this->createMessage(message.first, message.second);
    }

    this->schedule(schedule_selector(GlobedChatListPopup::updateChat));

    return true;
}

void GlobedChatListPopup::keyBackClicked()  {
    this->onClose(nullptr);
}

void GlobedChatListPopup::keyDown(cocos2d::enumKeyCodes key)  {
    if (key == KEY_Enter) this->onChat(nullptr);
}

void GlobedChatListPopup::onClose(CCObject*) {
    this->removeFromParent();
}

void GlobedChatListPopup::onChat(CCObject* sender) {
    if (inp->getString().size() != 0) {
        auto& nm = NetworkManager::get();
        nm.send(ChatMessagePacket::create(std::string_view(inp->getString())));

        auto GAM = GJAccountManager::sharedState();

        GlobedGJBGL::get()->m_fields->chatMessages.push_back({GAM->m_accountID, inp->getString()});
        createMessage(GAM->m_accountID, inp->getString());

        //GlobedGJBGL::get()->m_fields->chatOverlay->addMessage(GAM->m_accountID, inp->getString());

        inp->setString("");
    }
}

void GlobedChatListPopup::createMessage(int accountID, const std::string& message) {
    auto& pcm = ProfileCacheManager::get();
    auto& playerStore = GlobedGJBGL::get()->m_fields->playerStore->getAll();

    std::string username = "N/A";

    PlayerAccountData yeah = PlayerAccountData::DEFAULT_DATA;
    // if account ID is ours, then display our username
    if (accountID == GJAccountManager::sharedState()->m_accountID) username = GJAccountManager::sharedState()->m_username;
    // if account ID is in the player cache, get the username from there
    if (pcm.getData(accountID)) username = pcm.getData(accountID).value().name;

    auto cell = GlobedChatCell::create(username, accountID, message);
    cell->setPositionY(5.f);
    cell->setPositionX(5.f);
    scroll->m_contentLayer->addChild(cell);
    scroll->m_contentLayer->setAnchorPoint(ccp(0, 1));

    // move all of them up to accomodate for a new message
    for (GlobedChatCell* gucci : messageCells) {
        gucci->setPositionY(gucci->getPositionY() + GlobedChatCell::CELL_HEIGHT + 3.f);
    }
    messageCells.push_back(cell);

    auto cells = scroll->m_contentLayer->getChildrenCount();
    float height = std::max<float>(scroll->getContentSize().height, GlobedChatCell::CELL_HEIGHT * cells + (3.f * (cells + 1)) + 3.f);
    scroll->m_contentLayer->setContentSize(ccp(scroll->m_contentLayer->getContentSize().width, height));

    //scroll->moveToBottom();
}

void GlobedChatListPopup::updateChat(float dt) {
    // if we have more messages stored then the amount we have displaying, display the rest of them
    auto& messages = GlobedGJBGL::get()->m_fields->chatMessages;
    if (messages.size() > messageCells.size()) {
        this->createMessage(messages.back().first, messages.back().second);
    }
}

GlobedChatListPopup* GlobedChatListPopup::create() {
	auto ret = new GlobedChatListPopup;

	if (ret->initAnchored(POPUP_WIDTH, POPUP_HEIGHT, "GJ_square01.png")) {
		ret->autorelease();
		return ret;
	}

    delete ret;
	return nullptr;
}