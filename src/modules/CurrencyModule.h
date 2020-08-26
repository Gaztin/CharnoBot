#pragma once
#include <qtimer.h>
#include "core/Module.h"
#include "UmikoBot.h"
#include <random>

class UmikoBot;

class CurrencyModule : public Module 
{
private:
	struct UserCurrency 
	{
		snowflake_t userId;
		double currency;
		bool isDailyClaimed;
		unsigned int dailyStreak;
		QTimer* jailTimer;

		UserCurrency(snowflake_t userId, double currency, bool isDailyClaimed, unsigned int dailyStreak)
			: userId(userId), currency(currency), isDailyClaimed(isDailyClaimed), dailyStreak(dailyStreak), jailTimer(new QTimer())
		{
			jailTimer->setSingleShot(true);
		}

		~UserCurrency()
		{
			delete jailTimer;
		}

		UserCurrency(const UserCurrency& other)
			: userId(other.userId), currency(other.currency), isDailyClaimed(other.isDailyClaimed), dailyStreak(other.dailyStreak), jailTimer(new QTimer())
		{
			jailTimer->setSingleShot(true);

			if (other.jailTimer->remainingTime() > 0)
			{
				jailTimer->start(other.jailTimer->remainingTime());
			}
		}

		UserCurrency& operator=(const UserCurrency& other)
		{
			userId = other.userId;
			currency = other.currency;
			isDailyClaimed = other.isDailyClaimed;
			dailyStreak = other.dailyStreak;
			jailTimer = new QTimer();
			jailTimer->setSingleShot(true);

			if (other.jailTimer->remainingTime() > 0)
			{
				jailTimer->start(other.jailTimer->remainingTime());
			}
		}
	};

	//! Map server id with user currency list
	QMap<snowflake_t, QList<UserCurrency>> guildList;

	struct CurrencyConfig {
		double randGiveawayProb{ 0.001 };
		unsigned int freebieExpireTime{ 60 };	//in seconds
		int dailyReward{ 100 };
		int freebieReward{ 300 };
		int gambleReward{ 50 };
		int minGuess{ 0 };
		int maxGuess{ 5 };
		int gambleLoss{ 10 };
		snowflake_t giveawayChannelId{ 0 };
		QString currencyName;
		QString currencySymbol;
		bool isRandomGiveawayDone{ false };
		bool allowGiveaway{ false };
		QTimer* freebieTimer{ nullptr };
		int dailyBonusAmount { 50 };
		int dailyBonusPeriod { 3 };
		int stealSuccessChance { 30 };
		int stealFinePercent { 50 };
		int stealVictimBonusPercent { 30 };
		int stealFailedJailTime { 3 };
	};

	std::random_device random_device;
	std::mt19937 random_engine{ random_device() };
	std::bernoulli_distribution randp;

	QTimer m_timer; //! For dailies, and the giveaway

	QMap<snowflake_t, CurrencyConfig>serverCurrencyConfig;
	
	struct GambleData {
		bool gamble{ false };
		bool doubleOrNothing{ false };
		snowflake_t userId{ 0 };
		int randNum = 0;
		snowflake_t channelId{ 0 };
		double betAmount{ 0 }; //!Use if doubleOrNothing
		QTimer* timer;
	};
	
	//! Map each !gamble (on a server) with its own gamble
	QMap<snowflake_t, GambleData> gambleData;

	UmikoBot* m_client;

private:
	void OnSave(QJsonDocument& doc) const override;
	void OnLoad(const QJsonDocument& doc) override;
	
	UserCurrency getUserData(snowflake_t guild, snowflake_t id) 
	{
		for (auto user : guildList[guild]) 
		{
			if (user.userId == id) 
			{
				return user;
			}
		}

		//! If user is not added to the system, make a new one
		UserCurrency user { id, 0, false, 0 };

		guildList[guild].append(user);
		return guildList[guild].back();
	}
	snowflake_t getUserIndex(snowflake_t guild, snowflake_t id) {

		for (auto it = guildList[guild].begin(); it != guildList[guild].end(); ++it) {
			if (it->userId == id) {
				return std::distance(guildList[guild].begin(), it);
			}
		}
		//! If user is not added to the system, make a new one
		guildList[guild].append(UserCurrency { id, 0, false, 0 });
		return std::distance(guildList[guild].begin(), std::prev(guildList[guild].end()));
	}

	CurrencyConfig& getServerData(snowflake_t guild) 
	{
		return serverCurrencyConfig[guild];
	}

public:
	CurrencyModule(UmikoBot* client);
	void StatusCommand(QString& result, snowflake_t guild, snowflake_t user) override;
	void OnMessage(Discord::Client& client, const Discord::Message& message) override;
};