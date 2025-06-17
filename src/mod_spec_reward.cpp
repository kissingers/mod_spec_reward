/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

// By BetaYi (QQ:1518429315 ; 1518429315@qq.com) 群：719526711

#include "ScriptMgr.h"
#include "Config.h"
#include <Player.h>
#include "Group.h"
#include "GroupMgr.h"
#include "InstanceScript.h"
#include "Chat.h"
#include "WorldSession.h"
#include "Creature.h"
#include "WorldSessionMgr.h"
#include "Mail.h"
#include "CalendarMgr.h"  // 如果将来需要记录事件时间戳可启用

static bool ModuleEnable, AnnouncerEnable;
static uint32 RewardSpec,MinimalLevel,DungeonToken,RaidToken,TokenCount;
std::string HMessageText;
std::string TMessageText;
std::string DMessageText;
std::string BannerText;

enum SpecType
{
    FLAG_SPEC_HEALER = 0x00000001,
    FLAG_SPEC_DPS    = 0x00000002,
    FLAG_SPEC_TANK   = 0x00000004
};

// Add player scripts
class Spec_Reward : public PlayerScript
{
public:
    Spec_Reward() : PlayerScript("Spec_Reward") { }

    void OnPlayerLogin(Player* player) override
    {
        if (ModuleEnable && AnnouncerEnable)
        {
            ChatHandler(player->GetSession()).SendSysMessage("服务器已启用 |cff4CFF00天赋奖励模块|r。击杀副本Boss将按角色定位奖励代币。");
        }
    }

    void GiveReward(Player* player, uint32 itemId, uint32 count)
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count) == EQUIP_ERR_OK)
        {
            player->StoreNewItem(dest, itemId, count, true);
        }
        else
        {
            MailDraft draft("副本奖励", "尊敬的玩家，

您在副本中的表现获得了奖励，但由于背包已满，系统已将奖励通过邮件发送给您，请注意查收。祝您游戏愉快！");
            Item* item = Item::CreateItem(itemId, count, player);
            if (item)
            {
                draft.AddItem(item);
                draft.SendMailTo(player, MailSender(MAIL_CREATURE, 0), MAIL_CHECK_MASK_COPIED);
            }
        }
    }

    void OnPlayerCreatureKill(Player* player, Creature* boss) override
    {
        if (ModuleEnable && boss->GetLevel() > MinimalLevel && boss->IsDungeonBoss())
        {
            Map* map = player->GetMap();
            std::string tag_colour = "7bbef7";
            std::string plr_colour = "7bbef7";
            bool isRaid = map->IsRaid();

            Map::PlayerList const & playerlist = map->GetPlayers();
            std::ostringstream stream;
            uint32 rewardedCount = 0;

            stream << "[击杀Boss: " << boss->GetNameForLocaleIdx(LOCALE_zhCN) << "]" << std::endl;

            for (auto const& playerRef : playerlist)
            {
                Player* member = playerRef.GetSource();
                if (!member)
                    continue;

                std::string name = member->GetName();
                uint32 itemId = isRaid ? RaidToken : DungeonToken;

                if (member->HasHealSpec() && (RewardSpec & FLAG_SPEC_HEALER))
                {
                    ++rewardedCount;
                    GiveReward(member, itemId, TokenCount);
                    stream << rewardedCount << ". |CFF" << tag_colour << "|r|cff" << plr_colour << name << "|r " << HMessageText << std::endl;
                }
                else if (member->HasTankSpec() && (RewardSpec & FLAG_SPEC_TANK))
                {
                    ++rewardedCount;
                    GiveReward(member, itemId, TokenCount);
                    stream << rewardedCount << ". |CFF" << tag_colour << "|r|cff" << plr_colour << name << "|r " << TMessageText << std::endl;
                }
                else if (member->HasCasterSpec() && (RewardSpec & FLAG_SPEC_DPS))
                {
                    ++rewardedCount;
                    GiveReward(member, itemId, TokenCount);
                    stream << rewardedCount << ". |CFF" << tag_colour << "|r|cff" << plr_colour << name << "|r " << DMessageText << std::endl;
                }
            }

            for (auto const& playerRef : playerlist)
            {
                Player* member = playerRef.GetSource();
                if (member)
                    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, stream.str().c_str(), member);
            }
        }
    }
};

// Initial Conf
class Spec_Reward_Conf : public WorldScript
{
public:
    Spec_Reward_Conf() : WorldScript("Spec_Reward_Conf") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) {
            SetInitialWorldSettings();
        }
    }

    void SetInitialWorldSettings()
    {
        ModuleEnable    = sConfigMgr->GetOption<bool>("Spec_Reward.Enable", true);
        AnnouncerEnable = sConfigMgr->GetOption<bool>("Spec_Reward.Announce", true);
        RewardSpec      = sConfigMgr->GetOption<uint32>("Spec_Reward.Spec", 1);
        MinimalLevel    = sConfigMgr->GetOption<uint32>("Spec_Reward.Level", 80);
        DungeonToken    = sConfigMgr->GetOption<uint32>("Spec_Reward.DungeonToken", 38186);
        RaidToken       = sConfigMgr->GetOption<uint32>("Spec_Reward.RaidToken", 38186);
        TokenCount      = sConfigMgr->GetOption<uint32>("Spec_Reward.TokenCount", 1);
        HMessageText    = sConfigMgr->GetOption<std::string>("Spec_Reward.HealText", "获得了治疗奖励");
        TMessageText    = sConfigMgr->GetOption<std::string>("Spec_Reward.TankText", "获得了坦克奖励");
        DMessageText    = sConfigMgr->GetOption<std::string>("Spec_Reward.DPSText", "获得了输出奖励");
    }
};

// Add all scripts in one
void Addmod_spec_rewardScripts()
{
    new Spec_Reward_Conf();
    new Spec_Reward();
}
