#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr std::string_view kSaveKey = "event-rules";
    constexpr std::string_view kSettingKey = "open-events-editor";

    enum class EventType {
        PlayerDeath = 0,
        CheckpointAdded = 1,
        CheckpointRemoved = 2,
        GamemodeChanged = 3,
        EnteredLevel = 4,
        QuitLevel = 5,
    };

    struct EventRule {
        EventType event;
        std::string command;
    };

    std::string trim(std::string in) {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        in.erase(in.begin(), std::find_if(in.begin(), in.end(), notSpace));
        in.erase(std::find_if(in.rbegin(), in.rend(), notSpace).base(), in.end());
        return in;
    }

    std::string eventToString(EventType event) {
        switch (event) {
            case EventType::PlayerDeath: return "Player Death";
            case EventType::CheckpointAdded: return "Checkpoint Added";
            case EventType::CheckpointRemoved: return "Checkpoint Removed";
            case EventType::GamemodeChanged: return "Gamemode Changed";
            case EventType::EnteredLevel: return "Entered Level";
            case EventType::QuitLevel: return "Quit Level";
        }
        return "Unknown";
    }

    std::vector<EventRule> loadRules() {
        std::vector<EventRule> rules;
        auto mod = Mod::get();
        auto raw = mod->template getSavedValue<std::string>(kSaveKey.data(), "");
        if (raw.empty()) return rules;

        std::istringstream stream(raw);
        std::string line;
        while (std::getline(stream, line)) {
            auto sep = line.find('|');
            if (sep == std::string::npos) continue;
            auto eventRaw = line.substr(0, sep);
            auto cmd = line.substr(sep + 1);
            int parsed = -1;
            try {
                parsed = std::stoi(eventRaw);
            } catch (...) {
                continue;
            }
            if (parsed < 0 || parsed > 5) continue;
            rules.push_back({static_cast<EventType>(parsed), cmd});
        }
        return rules;
    }

    void saveRules(std::vector<EventRule> const& rules) {
        std::ostringstream stream;
        for (size_t i = 0; i < rules.size(); ++i) {
            stream << static_cast<int>(rules[i].event) << "|" << rules[i].command;
            if (i + 1 < rules.size()) stream << '\n';
        }
        Mod::get()->setSavedValue<std::string>(kSaveKey.data(), stream.str());
    }

    bool isWineEnvironment() {
#ifdef GEODE_IS_WINDOWS
        if (std::getenv("WINEPREFIX")) return true;
        if (std::getenv("WINELOADERNOEXEC")) return true;
        if (std::getenv("WINEDLLPATH")) return true;
#endif
        return false;
    }

    void runUserCommand(std::string command) {
        auto normalized = trim(command);
        if (normalized.empty()) return;

        if (normalized == "pre.exit") {
            CCDirector::get()->end();
            return;
        }

        if (normalized == "pre.quit") {
            if (auto scene = PlayLayer::get()) {
                scene->onQuit();
            }
            return;
        }

#ifdef GEODE_IS_MACOS
        std::string shellCommand = "zsh -c \"" + normalized + "\"";
        std::system(shellCommand.c_str());
#elif defined(GEODE_IS_WINDOWS)
        if (isWineEnvironment()) {
            std::string shellCommand = "bash -lc \"" + normalized + "\"";
            std::system(shellCommand.c_str());
        }
        else {
            std::string shellCommand = "cmd /C \"" + normalized + "\"";
            std::system(shellCommand.c_str());
        }
#else
        std::string shellCommand = "sh -c \"" + normalized + "\"";
        std::system(shellCommand.c_str());
#endif
    }

    void triggerEvent(EventType event) {
        auto rules = loadRules();
        for (auto const& rule : rules) {
            if (rule.event == event) runUserCommand(rule.command);
        }
    }

    class EventConfigPopup;

    class RuleInputPopup : public Popup {
    protected:
        EventConfigPopup* m_parent = nullptr;
        TextInput* m_eventInput = nullptr;
        TextInput* m_commandInput = nullptr;

        bool init(EventConfigPopup* parent) {
            if (!Popup::init(260.f, 170.f)) return false;
            m_parent = parent;
            this->setTitle("Add Event Rule");

            auto info = CCLabelBMFont::create(
                "Event (1-6): 1 Death, 2 CP+, 3 CP-, 4 Mode, 5 Enter, 6 Quit",
                "goldFont.fnt"
            );
            info->setScale(0.28f);
            info->setAnchorPoint({0.5f, 0.5f});
            info->setPosition({m_size.width / 2.f, m_size.height - 45.f});
            m_mainLayer->addChild(info);

            m_eventInput = TextInput::create(210.f, "Event Number");
            m_eventInput->setCommonFilter(CommonFilter::Int);
            m_eventInput->setMaxCharCount(1);
            m_eventInput->setPosition({m_size.width / 2.f, m_size.height - 70.f});
            m_mainLayer->addChild(m_eventInput);

            m_commandInput = TextInput::create(210.f, "Command / pre.exit / pre.quit");
            m_commandInput->setCommonFilter(CommonFilter::Any);
            m_commandInput->setMaxCharCount(180);
            m_commandInput->setPosition({m_size.width / 2.f, m_size.height - 100.f});
            m_mainLayer->addChild(m_commandInput);

            auto btnSprite = ButtonSprite::create("ADD");
            auto addBtn = CCMenuItemSpriteExtra::create(
                btnSprite, this, menu_selector(RuleInputPopup::onConfirm)
            );
            auto menu = CCMenu::create();
            menu->addChild(addBtn);
            menu->setPosition({m_size.width / 2.f, 28.f});
            m_mainLayer->addChild(menu);
            return true;
        }

        void onConfirm(CCObject*);

    public:
        static RuleInputPopup* create(EventConfigPopup* parent) {
            auto ret = new RuleInputPopup();
            if (ret->init(parent)) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }
    };

    class EventConfigPopup : public Popup {
    protected:
        std::vector<EventRule> m_rules;
        CCLabelBMFont* m_listLabel = nullptr;

        bool init() {
            if (!Popup::init(280.f, 210.f)) return false;
            this->setTitle("Event Commands");
            m_rules = loadRules();

            m_listLabel = CCLabelBMFont::create("", "bigFont.fnt");
            m_listLabel->setAnchorPoint({0.5f, 1.f});
            m_listLabel->setPosition({m_size.width / 2.f, m_size.height - 30.f});
            m_listLabel->setScale(0.35f);
            m_mainLayer->addChild(m_listLabel);
            refreshLabel();

            auto addSprite = ButtonSprite::create("ADD");
            auto clearSprite = ButtonSprite::create("CLEAR");
            auto addBtn = CCMenuItemSpriteExtra::create(
                addSprite, this, menu_selector(EventConfigPopup::onAdd)
            );
            auto clearBtn = CCMenuItemSpriteExtra::create(
                clearSprite, this, menu_selector(EventConfigPopup::onClear)
            );

            auto menu = CCMenu::create();
            menu->addChild(addBtn);
            menu->addChild(clearBtn);
            menu->setPosition({m_size.width / 2.f, 24.f});
            addBtn->setPosition({-55.f, 0.f});
            clearBtn->setPosition({55.f, 0.f});
            m_mainLayer->addChild(menu);
            return true;
        }

        void refreshLabel() {
            std::string text = "No events configured.";
            if (!m_rules.empty()) {
                std::ostringstream stream;
                for (size_t i = 0; i < m_rules.size(); ++i) {
                    stream << (i + 1) << ". " << eventToString(m_rules[i].event)
                           << " -> " << m_rules[i].command;
                    if (i + 1 < m_rules.size()) stream << '\n';
                }
                text = stream.str();
            }
            m_listLabel->setString(text.c_str());
            m_listLabel->limitLabelWidth(220.f, 0.35f, 0.05f);
        }

        void onAdd(CCObject*) {
            createQuickPopup(
                "Choose Event",
                "Pick which event should trigger the command.",
                "Cancel",
                "Next",
                [this](FLAlertLayer*, bool yes) {
                    if (!yes) return;
                    this->openEventTypeStep();
                }
            );
        }

        void openEventTypeStep() {
            createQuickPopup(
                "Event Type",
                "Use these in order:\n1) Death\n2) CP Added\n3) CP Removed\n4) Gamemode\n5) Enter Level\n6) Quit Level\n\nPress OK to continue.",
                "Cancel",
                "OK",
                [this](FLAlertLayer*, bool yes) {
                    if (!yes) return;
                    this->openEventAndCommandInput();
                }
            );
        }

        void openEventAndCommandInput() {
            if (auto popup = RuleInputPopup::create(this)) popup->show();
        }

        void onClear(CCObject*) {
            createQuickPopup(
                "Clear Rules",
                "Remove all configured rules?",
                "No",
                "Yes",
                [this](FLAlertLayer*, bool yes) {
                    if (!yes) return;
                    m_rules.clear();
                    saveRules(m_rules);
                    refreshLabel();
                }
            );
        }

    public:
        static EventConfigPopup* create() {
            auto ret = new EventConfigPopup();
            if (ret->init()) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }

        void addRule(EventRule const& rule) {
            m_rules.push_back(rule);
            saveRules(m_rules);
            refreshLabel();
        }
    };

    void RuleInputPopup::onConfirm(CCObject*) {
        auto eventText = trim(m_eventInput->getString());
        auto command = trim(m_commandInput->getString());
        if (eventText.empty() || command.empty()) {
            FLAlertLayer::create("Missing", "Event and command are required.", "OK")->show();
            return;
        }

        int eventNum = 0;
        try {
            eventNum = std::stoi(eventText);
        }
        catch (...) {
            FLAlertLayer::create("Invalid", "Event must be a number 1 to 6.", "OK")->show();
            return;
        }

        if (eventNum < 1 || eventNum > 6) {
            FLAlertLayer::create("Invalid", "Event must be a number 1 to 6.", "OK")->show();
            return;
        }

        if (m_parent) {
            m_parent->addRule({static_cast<EventType>(eventNum - 1), command});
        }
        this->onClose(nullptr);
    }

    $on_mod(Loaded) {
        static ListenerHandle* sButtonListener = nullptr;
        if (!sButtonListener) {
            sButtonListener = ButtonSettingPressedEventV3(Mod::get(), std::string(kSettingKey)).listen(
                [](std::string_view key) {
                    if (key == "open") {
                        if (auto popup = EventConfigPopup::create()) popup->show();
                    }
                    return ListenerResult::Propagate;
                }
            ).leak();
        }
    }
}

class $modify(RunAtActionPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        triggerEvent(EventType::EnteredLevel);
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        triggerEvent(EventType::PlayerDeath);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
    }

    void storeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::storeCheckpoint(checkpoint);
        triggerEvent(EventType::CheckpointAdded);
    }

    void removeCheckpoint(bool first) {
        PlayLayer::removeCheckpoint(first);
        triggerEvent(EventType::CheckpointRemoved);
    }
};

class $modify(RunAtActionBaseGameLayer, GJBaseGameLayer) {
    void playerWillSwitchMode(PlayerObject* player, GameObject* object) {
        GJBaseGameLayer::playerWillSwitchMode(player, object);
        triggerEvent(EventType::GamemodeChanged);
    }
};

class $modify(RunAtActionPauseLayer, PauseLayer) {
    void onQuit(CCObject* sender) {
        triggerEvent(EventType::QuitLevel);
        PauseLayer::onQuit(sender);
    }
};
