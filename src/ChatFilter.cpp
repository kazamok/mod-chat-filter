#include "ChatFilter.h"
#include "Chat.h"
#include "Log.h"
#include "WorldSessionMgr.h"
#include "AccountMgr.h"
#include <fstream>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <map>

// Configuration storage
bool cf_enabled;
bool cf_log_enabled;
bool cf_mute_enabled;
uint32 cf_mute_duration;
std::vector<std::string> cf_badwords;
bool cf_sender_see_message; // Not used yet, but loaded

// Muted players map <PlayerGUID, MuteEndTime>
std::map<uint32, time_t> muted_players;

// Helper to split the badwords string
void split(const std::string& s, char delim, std::vector<std::string>& elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            // Convert to lowercase for case-insensitive comparison
            std::transform(item.begin(), item.end(), item.begin(),
                [](unsigned char c){ return std::tolower(c); });
            elems.push_back(item);
        }
    }
}

class ChatFilterConfig : public WorldScript
{
public:
    ChatFilterConfig() : WorldScript("ChatFilterConfig") {}

    void OnAfterConfigLoad(bool reload) override
    {
        cf_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Enable", true);
        cf_log_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Log.Enable", true);
        cf_mute_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Mute.Enable", true);
        cf_mute_duration = sConfigMgr->GetOption<uint32>("ChatFilter.Mute.Duration", 60);
        std::string badwords_str = sConfigMgr->GetOption<std::string>("ChatFilter.Badwords", "badword1,badword2");
        
        // DEBUG LOG: Print raw badwords_str
        std::stringstream raw_badwords_ss;
        raw_badwords_ss << "설정 파일에서 읽은 raw badwords_str: '" << badwords_str << "'";
        LOG_INFO("server.loading", raw_badwords_ss.str().c_str());

        cf_badwords.clear();
        split(badwords_str, ',', cf_badwords);

        if (cf_enabled) {
            LOG_INFO("server.loading", "채팅 필터 모듈이 활성화되었습니다.");
            if (cf_log_enabled) {
                LOG_INFO("server.loading", "채팅 필터 로깅이 활성화되었습니다.");
            }
            if (cf_mute_enabled) {
                std::stringstream ss;
                ss << "채팅 필터 자동 음소거 기능이 활성화되었습니다. (지속시간: " << cf_mute_duration << "초)";
                LOG_INFO("server.loading", ss.str().c_str());
            }

            // DEBUG LOG: Print loaded badwords
            if (!cf_badwords.empty()) {
                std::stringstream badwords_log_ss;
                badwords_log_ss << "로드된 금지 단어: ";
                for (size_t i = 0; i < cf_badwords.size(); ++i) {
                    badwords_log_ss << "'" << cf_badwords[i] << "'";
                    if (i < cf_badwords.size() - 1) {
                        badwords_log_ss << ", ";
                    }
                }
                LOG_INFO("server.loading", badwords_log_ss.str().c_str());
            } else {
                LOG_INFO("server.loading", "금지 단어 목록이 비어 있습니다.");
            }
        }
    }
};

ChatFilter::ChatFilter() : PlayerScript("ChatFilter") {}

bool ChatFilter::IsBadWord(const std::string& msg)
{
    std::string lower_msg = msg;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(),
        [](unsigned char c){ return std::tolower(c); });

    // DEBUG LOG: Message being checked
    std::stringstream debug_ss;
    debug_ss << "IsBadWord 검사 중: '" << lower_msg << "'";
    LOG_INFO("chatfilter.debug", debug_ss.str().c_str());

    for (const auto& badword : cf_badwords) {
        // DEBUG LOG: Badword comparison
        std::stringstream compare_ss;
        compare_ss << "  금지 단어 '" << badword << "'와 비교 중...";
        LOG_INFO("chatfilter.debug", compare_ss.str().c_str());
        if (lower_msg.find(badword) != std::string::npos) {
            LOG_INFO("chatfilter.debug", "  -> 금지 단어 발견!");
            return true;
        }
    }
    LOG_INFO("chatfilter.debug", "  -> 금지 단어 없음.");
    return false;
}

void ChatFilter::LogMessage(Player* player, const std::string& originalMsg)
{
    if (!cf_log_enabled) {
        return;
    }

    // Create directory if it doesn't exist
    if (!std::filesystem::is_directory("logs/chatfilter") || !std::filesystem::exists("logs/chatfilter")) {
        std::filesystem::create_directory("logs/chatfilter");
    }

    time_t now = time(0);
    tm* ltm = localtime(&now);

    char date_buf[20];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", ltm);
    std::string log_file = "logs/chatfilter/" + std::string(date_buf) + ".log";

    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", ltm);
    
    std::string accountName;
    if (player->GetSession())
    {
        AccountMgr::GetName(player->GetSession()->GetAccountId(), accountName);
    }

    std::ofstream logfile;
    logfile.open(log_file, std::ios_base::app);
    if (logfile.is_open())
    {
        logfile << "[" << time_buf << "] ";
        logfile << "Player: " << player->GetName() << " ";
        logfile << "(Account: " << accountName << ", GUID: " << player->GetGUID().GetCounter() << ") ";
        logfile << "said: " << originalMsg << std::endl;
        logfile.close();
    }
}

// Handler for Say, Yell, Whisper, etc.
bool ChatFilter::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg)
{
    // DEBUG LOG: OnPlayerCanUseChat triggered. (This must be the absolute first line in this function)
    LOG_INFO("chatfilter.debug", "OnPlayerCanUseChat triggered.");

    // Original content starts here
    std::stringstream chat_debug_ss;
    chat_debug_ss << "OnPlayerCanUseChat: Player '" << player->GetName() << "' sent: '" << msg << "'";
    LOG_INFO("chatfilter.debug", chat_debug_ss.str().c_str());

    if (!cf_enabled || player->IsGameMaster() || lang == LANG_ADDON) {
        if (player->IsGameMaster()) {
            LOG_INFO("chatfilter.debug", "GM이므로 필터링 건너뛰기.");
        }
        if (lang == LANG_ADDON) {
            LOG_INFO("chatfilter.debug", "애드온 메시지이므로 필터링 건너뛰기.");
        }
        return true; // Allow message if filter is disabled, player is GM, or it's an addon message
    }

    // Check if player is muted
    if (cf_mute_enabled) {
        auto it = muted_players.find(player->GetGUID().GetCounter());
        if (it != muted_players.end()) {
            time_t now = time(nullptr);
            if (now < it->second) {
                // Player is still muted
                long remaining_time = long(it->second - now);
                msg.clear();
                std::stringstream ss;
                ss << "채팅이 임시로 금지되었습니다. " << remaining_time << "초 후에 다시 시도하세요.";
                ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
                LOG_INFO("chatfilter.debug", "플레이어 음소거 중. 메시지 차단.");
                return false; // Block message
            }
            else {
                // Mute expired, remove from map
                muted_players.erase(it);
                LOG_INFO("chatfilter.debug", "플레이어 음소거 만료. 음소거 해제.");
            }
        }
    }
    
    if (IsBadWord(msg))
    {
        LogMessage(player, msg);
        msg.clear(); 
        LOG_INFO("chatfilter.debug", "금지 단어 발견, 메시지 차단.");
        
        if (cf_mute_enabled && cf_mute_duration > 0) {
            time_t mute_end_time = time(nullptr) + cf_mute_duration;
            muted_players[player->GetGUID().GetCounter()] = mute_end_time;
            std::stringstream ss;
            ss << "부적절한 언어 사용으로 메시지가 차단되었으며, " << cf_mute_duration << "초 동안 채팅이 금지됩니다.";
            ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
            std::stringstream muted_log_ss;
            muted_log_ss << "플레이어 '" << player->GetName() << "'가 " << cf_mute_duration << "초 동안 음소거되었습니다.";
            LOG_INFO("chatfilter.debug", muted_log_ss.str().c_str());
        } else {
            ChatHandler(player->GetSession()).SendSysMessage("Your message contained inappropriate language and was not sent.");
            LOG_INFO("chatfilter.debug", "금지 단어 발견, 메시지 차단 (음소거 비활성화).");
        }
        return false; // Block message
    } else {
        LOG_INFO("chatfilter.debug", "메시지 필터링 통과.");
    }
    return true; // Allow message
}

// Handler for Channel messages
bool ChatFilter::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* /*channel*/)
{
    // The logic is identical, so we just call the other handler.
    return OnPlayerCanUseChat(player, type, lang, msg);
}

// Handler for Private messages (Whispers)
bool ChatFilter::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/)
{
    // The logic is identical to the general OnPlayerCanUseChat, except for the receiver parameter.
    // Call the general handler.
    return OnPlayerCanUseChat(player, type, lang, msg);
}

extern "C" void AddSC_mod_chat_filter()
{
    new ChatFilterConfig();
    new ChatFilter();
}