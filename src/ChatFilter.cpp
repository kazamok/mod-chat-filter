#include "ChatFilter.h"
#include "Chat.h"
#include "Log.h"
#include "WorldSessionMgr.h"
#include "AccountMgr.h"
#include "BanMgr.h"
#include "DatabaseEnv.h" // Required for database access
#include <fstream>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <map>
#include <unordered_map>

// Configuration storage
bool cf_enabled;
bool cf_log_enabled;
bool cf_mute_enabled;
// std::vector<std::string> cf_badwords; // Old storage
std::unordered_map<std::string, uint8_t> cf_badwords; // New: word -> level
bool cf_sender_see_message; // Not used yet, but loaded

// Muted players map <PlayerGUID, MuteEndTime>
std::map<uint32, time_t> muted_players;

// This script now handles both config and database loading.
class ChatFilterManagerScript : public WorldScript
{
public:
    ChatFilterManagerScript() : WorldScript("ChatFilterManagerScript") {}

    // Load configuration settings from the .conf file
    void OnAfterConfigLoad(bool reload) override
    {
        cf_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Enable", true);
        cf_log_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Log.Enable", true);
        cf_mute_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Mute.Enable", true);
        // cf_mute_duration is no longer used from config

        if (cf_enabled) {
            LOG_INFO("server.loading", "채팅 필터 모듈이 활성화되었습니다.");
            if (cf_log_enabled) {
                LOG_INFO("server.loading", "채팅 필터 로깅이 활성화되었습니다.");
            }
            if (cf_mute_enabled) {
                LOG_INFO("server.loading", "채팅 필터 자동 처벌 기능이 활성화되었습니다.");
            }
        }
    }

    // Load bad words from the database
    void OnLoadCustomDatabaseTable() override
    {
        if (!cf_enabled)
        {
            return;
        }

        LOG_INFO("server.loading", "데이터베이스에서 금지 단어를 로드합니다...");
        uint32 count = 0;
        cf_badwords.clear();

        // Load both word and level
        QueryResult result = CharacterDatabase.Query("SELECT string, level FROM `mod-chat-filter`");

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                std::string badword = fields[0].Get<std::string>();
                uint8_t level = fields[1].Get<uint8_t>();

                // Convert to lowercase for case-insensitive comparison
                std::transform(badword.begin(), badword.end(), badword.begin(),
                    [](unsigned char c){ return std::tolower(c); });
                
                cf_badwords[badword] = level;
                count++;
            } while (result->NextRow());
        }

        std::stringstream ss;
        ss << ">> " << count << "개의 금지 단어를 로드했습니다.";
        LOG_INFO("server.loading", ss.str().c_str());
    }
};

ChatFilter::ChatFilter() : PlayerScript("ChatFilter") {}

// Returns the punishment level of the bad word found, or -1 if no bad word is found
int8_t ChatFilter::IsBadWord(const std::string& msg)
{
    std::string lower_msg = msg;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(),
        [](unsigned char c){ return std::tolower(c); });

    // Clean the message: remove spaces and common bypass characters
    std::string cleaned_msg;
    for (char c : lower_msg) {
        if (!std::isspace(static_cast<unsigned char>(c)) &&
            c != '!' && c != '@' && c != '#' && c != '$' && c != '%' && c != '^' &&
            c != '&' && c != '*' && c != '(' && c != ')' && c != '-' && c != '=' &&
            c != '+' && c != '[' && c != ']' && c != '{' && c != '}' && c != ';' &&
            c != ':' && c != '\'' && c != '"' && c != ',' && c != '.' && c != '/' &&
            c != '?' && c != '<' && c != '>' && c != '`' && c != '~' && c != '\\' &&
            c != '|')
        {
            cleaned_msg += c;
        }
    }

    for (const auto& pair : cf_badwords) {
        const std::string& badword = pair.first;
        if (cleaned_msg.find(badword) != std::string::npos) {
            return pair.second; // Return the level
        }
    }

    return -1; // No bad word found
}

void ChatFilter::LogMessage(Player* player, const std::string& originalMsg)
{
    if (!cf_log_enabled) {
        return;
    }

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
    if (!cf_enabled || player->IsGameMaster() || lang == LANG_ADDON) {
        return true;
    }

    if (cf_mute_enabled) {
        auto it = muted_players.find(player->GetGUID().GetCounter());
        if (it != muted_players.end()) {
            time_t now = time(nullptr);
            if (now < it->second) {
                long remaining_time = long(it->second - now);
                msg.clear();
                std::stringstream ss;
                ss << "Chat has been temporarily blocked. Please try again in " << remaining_time << " seconds.";
                ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
                return false;
            }
            else {
                muted_players.erase(it);
            }
        }
    }
    
    int8_t badWordLevel = IsBadWord(msg);

    if (badWordLevel != -1)
    {
        LogMessage(player, msg);
        msg.clear(); 

        if (cf_mute_enabled) {
            uint32 mute_duration = 0;
            std::stringstream ss;

            switch (badWordLevel)
            {
                case 0: // Warning: 1 min mute
                    mute_duration = 60;
                    muted_players[player->GetGUID().GetCounter()] = time(nullptr) + mute_duration;
                    ss << "Using profanity will result in a 1 minute chat blocked.";
                    ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
                    break;
                case 1: // Medium: 3 min mute
                    mute_duration = 180;
                    muted_players[player->GetGUID().GetCounter()] = time(nullptr) + mute_duration;
                    ss << "Using profanity will result in a 3 minute chat blocked.";
                    ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
                    break;
                case 2: // Severe: 5 min ban
                {
                    uint32 ban_duration = 300;
                    std::string accountName;
                    uint32 accountId = player->GetSession()->GetAccountId();
                    if (AccountMgr::GetName(accountId, accountName))
                    {
                        // Ban the account for 5 minutes
                        sBan->BanAccount(accountName, "5m", "Use of inappropriate language", "Server");
                        ss << "Using profanity will result in a 5 minute account blocked.";
                        ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
                        // Kick the player
                        player->GetSession()->KickPlayer();
                    }
                    break;
                }
                default:
                    // Fallback for any other level that might be in the DB
                    ChatHandler(player->GetSession()).SendSysMessage("Your message contained inappropriate language and was not sent.");
                    break;
            }
        } else {
            // Muting/punishment is disabled, just block the message
            ChatHandler(player->GetSession()).SendSysMessage("Your message contained inappropriate language and was not sent.");
        }
        return false; // Block message
    }

    return true; // Allow message
}

// Handler for Channel messages
bool ChatFilter::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* /*channel*/)
{
    return OnPlayerCanUseChat(player, type, lang, msg);
}

// Handler for Private messages (Whispers)
bool ChatFilter::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/)
{
    return OnPlayerCanUseChat(player, type, lang, msg);
}

extern "C" void AddSC_mod_chat_filter()
{
    new ChatFilterManagerScript();
    new ChatFilter();
}
