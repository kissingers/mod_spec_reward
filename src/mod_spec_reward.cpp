/*
 * T 奶 DPS 奖励模块,基于BetaYi的脚本改编和优化  --By 牛皮德来
 */

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

static bool ModuleEnable, AnnouncerEnable;
static uint32 RewardSpec, MinimalLevel, DungeonToken, RaidToken, TokenCount;
std::string HMessageText, TMessageText, DMessageText;

enum SpecType
{
	FLAG_SPEC_TANK   = 0x00000001,
	FLAG_SPEC_HEALER = 0x00000002,
	FLAG_SPEC_DPS    = 0x00000004

};

// Add player scripts
class Spec_Reward : public PlayerScript
{
public:
	Spec_Reward() : PlayerScript("Spec_Reward") {}

	void OnPlayerLogin(Player* player) override
	{
		if (ModuleEnable && AnnouncerEnable)
		{
			ChatHandler(player->GetSession()).SendSysMessage("服务器已启用 |cff4CFF00职业副本奖励 |r模块.");
		}
	}

	void GiveReward(Player* player, uint32 itemId, uint32 count)
	{
		ItemPosCountVec dest;
		if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count) == EQUIP_ERR_OK)
		{
			player->AddItem(itemId, count);
			//player->StoreNewItem(dest, itemId, count, true);
		}
		else
		{
			MailDraft draft("副本奖励", "尊敬的玩家：\n\n由于您的背包已满，系统已通过邮件将奖励发放给您，请注意查收。\n\n感谢您的参与，祝游戏愉快！");

			const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
			if (!itemTemplate)
				return;

			uint32 maxStack = itemTemplate->GetMaxStackSize(); // 获取最大堆叠数
			while (count > 0)
			{
				uint32 sendCount = std::min(count, maxStack);
				Item* item = Item::CreateItem(itemId, sendCount, player);
				if (item)
					draft.AddItem(item);

				count -= sendCount;
			}

			CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
			draft.SendMailTo(trans, MailReceiver(player), MailSender(MAIL_CREATURE, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
			CharacterDatabase.CommitTransaction(trans);
		}
	}

	void OnPlayerCreatureKill(Player* player, Creature* boss) override
	{
		if (ModuleEnable && boss->GetLevel() > MinimalLevel && boss->IsDungeonBoss())
		{
			//lets get the info we want
			Map* map = player->GetMap();
			std::string p_name;
			std::string tag_colour = "7bbef7";
			std::string plr_colour = "7bbef7";
			uint32 tokenItemId = player->GetMap()->IsRaid() ? RaidToken : DungeonToken;

			Map::PlayerList const& playerlist = map->GetPlayers();
			std::ostringstream stream;
			uint32 count = 0;

			stream << "[" << boss->GetNameForLocaleIdx(LOCALE_zhCN) << " 被击杀!]" << std::endl;

			for (Map::PlayerList::const_iterator itr = playerlist.begin(); itr != playerlist.end(); ++itr)
			{
				Player* member = itr->GetSource();
				if (!member)
					continue;

				p_name = member->GetName();
				if (member->HasHealSpec() && RewardSpec & FLAG_SPEC_HEALER)
				{
					GiveReward(member, tokenItemId, TokenCount);
					if (member->IsGameMaster() || !member->isGMVisible())
						continue;
					++count;
					stream << count << ". |CFF" << tag_colour << "|r|cff" << plr_colour << p_name << "|r " << HMessageText << std::endl;
					continue;
				}

				if (member->HasTankSpec() && RewardSpec & FLAG_SPEC_TANK)
				{
					GiveReward(member, tokenItemId, TokenCount);
					if (member->IsGameMaster() || !member->isGMVisible())
						continue;
					++count;
					stream << count << ". |CFF" << tag_colour << "|r|cff" << plr_colour << p_name << "|r " << TMessageText << std::endl;
					continue;
				}
				if (member->HasCasterSpec() && RewardSpec & FLAG_SPEC_DPS)
				{
					GiveReward(member, tokenItemId, TokenCount);
					if (member->IsGameMaster() || !member->isGMVisible())
						continue;
					++count;
					stream << count << ". |CFF" << tag_colour << "|r|cff" << plr_colour << p_name << "|r " << DMessageText << std::endl;
					continue;
				}
			}

			if (count)
			{
				for (Map::PlayerList::const_iterator itr = playerlist.begin(); itr != playerlist.end(); ++itr)
				{
					if (itr->GetSource())
						sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, stream.str().c_str(), itr->GetSource());
				}
			}
		}
	}
};

// Initial Conf
class Spec_Reward_Conf : public WorldScript
{
public:
	Spec_Reward_Conf() : WorldScript("Spec_Reward_Conf") {}

	void OnBeforeConfigLoad(bool /*reload*/) override
	{
		ModuleEnable = sConfigMgr->GetOption<bool>("Spec_Reward.Enable", true);
		AnnouncerEnable = sConfigMgr->GetOption<bool>("Spec_Reward.Announce", true);
		RewardSpec = sConfigMgr->GetOption<uint32>("Spec_Reward.Spec", 3);
		MinimalLevel = sConfigMgr->GetOption<uint32>("Spec_Reward.Level", 80);
		DungeonToken = sConfigMgr->GetOption<uint32>("Spec_Reward.DungeonToken", 38186);
		RaidToken = sConfigMgr->GetOption<uint32>("Spec_Reward.RaidToken", 38186);
		TokenCount = sConfigMgr->GetOption<uint32>("Spec_Reward.TokenCount", 1);
		TMessageText = sConfigMgr->GetOption<std::string>("Spec_Reward.TankText", "--获得坦克额外奖励!");
		HMessageText = sConfigMgr->GetOption<std::string>("Spec_Reward.HealText", "-- 获得治疗额外奖励!");
		DMessageText = sConfigMgr->GetOption<std::string>("Spec_Reward.DPSText", "--获得输出额外奖励!");
	}
};

// Add all scripts in one
void Addmod_spec_rewardScripts()
{
	new Spec_Reward_Conf();
	new Spec_Reward();
}