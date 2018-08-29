#include "LevelModule.h"
#include "UmikoBot.h"
#include "core/Permissions.h"

LevelModule::LevelModule(UmikoBot* client)
	: Module("levels", true), m_client(client)
{
	m_timer.setInterval(/*300 */ 1000);
	QObject::connect(&m_timer, &QTimer::timeout, 
		[this]()
	{
		for (auto it = m_exp.begin(); it != m_exp.end(); it++)
		{
			for (GuildLevelData& data : it.value())
			{
				if (data.messageCount > 0) 
				{
					data.messageCount = 0;
					data.exp += qrand() % 31 + 15;
				}
			}
		}
	});
	m_timer.start();

	QTime now = QTime::currentTime();
	qsrand(now.msec());

	RegisterCommand(Commands::LEVEL_MODULE_TOP, "top",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel)
	{
		QStringList args = message.content().split(' ');
		GuildSetting s = GuildSettings::GetGuildSetting(channel.guildId());
		QString prefix = s.prefix;

		if (args.first() != prefix + "top")
			return;

		
		if (args.size() == 2) {
			qSort(m_exp[channel.guildId()].begin(), m_exp[channel.guildId()].end(),
				[](const LevelModule::GuildLevelData& v1, const LevelModule::GuildLevelData& v2) -> bool
			{
				return v1.exp > v2.exp;
			});
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Top " + args.back());

			QString desc = "";

			bool ok;
			int count = args.back().toInt(&ok);

			if (!ok) 
			{
				client.createMessage(message.channelId(), "Invalid count");
				return;
			}

			if (count > 30)
				count = 30;

			for (int i = 0; i < count; i++) 
			{
				if (i >= m_exp[channel.guildId()].size())
				{
					embed.setTitle("Top " + QString::number(i));
					break;
				}

				LevelModule::GuildLevelData& curr = m_exp[channel.guildId()][i];
				desc += QString::number(i + 1) + ". ";
				desc += reinterpret_cast<UmikoBot*>(&client)->GetName(channel.guildId(), m_exp[channel.guildId()][i].user);

				unsigned int xp = GetData(channel.guildId(), m_exp[channel.guildId()][i].user).exp;

				unsigned int xpRequirement = s.expRequirement;
				unsigned int level = 1;
				while (xp > xpRequirement && level < s.maximumLevel) {
					level++;
					xp -= xpRequirement;
					xpRequirement *= s.growthRate;
				}

				if (level >= s.maximumLevel)
					level = s.maximumLevel;

				desc += " - Level " + QString::number(level) + "\n";
			}

			embed.setDescription(desc);

			client.createMessage(message.channelId(), embed);
		}
		else if (args.size() == 3)
		{
			qSort(m_exp[channel.guildId()].begin(), m_exp[channel.guildId()].end(),
				[](const LevelModule::GuildLevelData& v1, const LevelModule::GuildLevelData& v2) -> bool
			{
				return v1.exp > v2.exp;
			});

			bool ok1, ok2;
			int count1 = args[1].toInt(&ok1);
			int count2 = args[2].toInt(&ok2);

			if (!ok1 || !ok2)
			{
				client.createMessage(message.channelId(), "Invalid count");
				return;
			}

			if (count2 - count1 > 30)
				count2 = count1 + 30;

			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Top from " + QString::number(count1) + " to " + QString::number(count1 + count2 - 1));

			QString desc = "";

			if (count1 > m_exp[channel.guildId()].size())
			{
				client.createMessage(channel.id(), "Not enough members to create the top.");
				return;
			}

			for (int i = count1 - 1; i < count1 + count2 - 1; i++)
			{
				if (i >= m_exp[channel.guildId()].size())
				{
					embed.setTitle("Top from " + QString::number(count1) + " to " + QString::number(i));
					break;
				}

				LevelModule::GuildLevelData& curr = m_exp[channel.guildId()][i];
				desc += QString::number(i + 1) + ". ";
				desc += reinterpret_cast<UmikoBot*>(&client)->GetName(channel.guildId(), m_exp[channel.guildId()][i].user);

				unsigned int xp = GetData(channel.guildId(), m_exp[channel.guildId()][i].user).exp;

				unsigned int xpRequirement = s.expRequirement;
				unsigned int level = 1;
				while (xp > xpRequirement && level < s.maximumLevel) {
					level++;
					xp -= xpRequirement;
					xpRequirement *= s.growthRate;
				}

				if (level >= s.maximumLevel)
					level = s.maximumLevel;

				desc += " - Level " + QString::number(level) + "\n";
			}

			embed.setDescription(desc);

			client.createMessage(message.channelId(), embed);
		}
		else
		{
			UmikoBot* bot = reinterpret_cast<UmikoBot*>(&client);
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help top");
			QString description = bot->GetCommandHelp("top", prefix);
			embed.setDescription(description);
			bot->createMessage(message.channelId(), embed);
		}
	});

	RegisterCommand(Commands::LEVEL_MODULE_RANK, "rank",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel)
	{
		QStringList args = message.content().split(' ');
		GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
		QString prefix = setting->prefix;

		auto printHelp = [&client, prefix, message]()
		{
			UmikoBot* bot = reinterpret_cast<UmikoBot*>(&client);
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help rank");
			QString description = bot->GetCommandHelp("rank", prefix);
			embed.setDescription(description);
			bot->createMessage(message.channelId(), embed);
		};
		
		if (args.first() != prefix + "rank")
			return;
		
		if (args.size() < 2)
		{
			printHelp();
			return;
		}

		if (args.last() == "list" && args.size() == 2)
		{
			QList<LevelRank> ranks = setting->ranks;
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Rank list");

			QString description = "";
			
			if (ranks.size() == 0)
				description = "No ranks found!";
			else
				for (int i = 0; i < ranks.size(); i++)
					description += ranks[i].name + " id " + QString::number(i) + " minimum level: " + QString::number(ranks[i].minimumLevel) + "\n";
			
			embed.setDescription(description);
			client.createMessage(message.channelId(), embed);
		}
		else if (args[1] == "add" && args.size() > 3)
			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[args, &client, message, channel](bool result)
			{
				GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}

				bool ok;
				unsigned int minimumLevel = args[2].toUInt(&ok);
				if (!ok)
				{
					client.createMessage(message.channelId(), "Invalid minimum level.");
					return;
				}
				QString name = "";
				for (int i = 3; i < args.size(); i++)
				{
					name += args[i];
					if (i < args.size() - 1)
						name += " ";
				}

				LevelRank rank = { name, minimumLevel };

				for (int i = 0; i < setting->ranks.size(); i++)
					if (setting->ranks[i].minimumLevel == minimumLevel)
					{
						client.createMessage(message.channelId(), "Cannot add rank, minimum level already used.");
						return;
					}

				setting->ranks.push_back(rank);
				qSort(setting->ranks.begin(), setting->ranks.end(),
					[](const LevelRank& v1, const LevelRank& v2) -> bool
				{
					return v1.minimumLevel < v2.minimumLevel;
				});

				for (int i = 0; i < setting->ranks.size(); i++)
					if (setting->ranks[i].name == name)
					{
						client.createMessage(message.channelId(), "Added rank " + name + " with id " + QString::number(i));
						break;
					}
			});
		else if (args[1] == "remove" && args.size() == 3)
			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[args, &client, message, channel](bool result)
			{
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}

				GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());

				bool ok;
				unsigned int id = args[2].toUInt(&ok);
				if (!ok)
				{
					client.createMessage(message.channelId(), "Invalid id.");
					return;
				}

				if (id >= setting->ranks.size())
				{
					client.createMessage(message.channelId(), "Id not found.");
					return;
				}

				client.createMessage(message.channelId(), "Deleted rank " + setting->ranks[id].name + " succesfully.");
				setting->ranks.erase(setting->ranks.begin() + id);
			});
		else if (args[1] == "edit" && args.size() >= 4)
			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[args, &client, message, channel, printHelp](bool result)
			{
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}
				GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());

				bool ok;
				unsigned int id = args[3].toUInt(&ok);
				if (!ok)
				{
					client.createMessage(message.channelId(), "Invalid id.");
					return;
				}

				if (id >= setting->ranks.size())
				{
					client.createMessage(message.channelId(), "Id not found.");
					return;
				}

				LevelRank& rank = setting->ranks[id];
				if (args[2] == "name")
				{
					QString name = "";
					for (int i = 4; i < args.size(); i++)
					{
						name += args[i];
						if (i < args.size() - 1)
							name += " ";
					}
					rank.name = name;
					client.createMessage(message.channelId(), "Rank id " + QString::number(id) + " has been succesfully edited.");
				}
				else if (args[2] == "level")
				{
					bool ok;
					unsigned int newlevel = args[4].toUInt(&ok);
					if (!ok)
					{
						client.createMessage(message.channelId(), "Invalid new level.");
						return;
					}

					rank.minimumLevel = newlevel;
					client.createMessage(message.channelId(), "Rank id " + QString::number(id) + " has been succesfully edited.");
				}
				else
				{
					printHelp();
				}
			});
		else
			printHelp();
	});

	RegisterCommand(Commands::LEVEL_MODULE_MAX_LEVEL, "setmaxlevel",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel)
	{
		GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
		QStringList args = message.content().split(' ');
		QString prefix = setting->prefix;

		if (args.first() != prefix + "setmaxlevel")
			return;

		auto printHelp = [&client, prefix, message]()
		{
			UmikoBot* bot = reinterpret_cast<UmikoBot*>(&client);
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help setmaxlevel");
			QString description = bot->GetCommandHelp("setmaxlevel", prefix);
			embed.setDescription(description);
			bot->createMessage(message.channelId(), embed);
		};

		if (args.size() == 2) 
		{
			if (args.last() == "current")
			{
				client.createMessage(message.channelId(), "Maximum level is currently set to " + QString::number(setting->maximumLevel));
				return;
			}

			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[args, &client, message, channel, printHelp, setting](bool result)
			{
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}
				bool ok;
				unsigned int level = args[1].toUInt(&ok);
				if (!ok)
				{
					client.createMessage(message.channelId(), "Invalid level.");
					return;
				}
				setting->maximumLevel = level;
				client.createMessage(message.channelId(), "Maximum level set to " + QString::number(level) + " succesfully!");
			});
		}
		else
			printHelp();
	});

	RegisterCommand(Commands::LEVEL_MODULE_EXP_REQUIREMENT, "setexpreq",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel)
	{
		GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
		QStringList args = message.content().split(' ');
		QString prefix = setting->prefix;

		if (args.first() != prefix + "setexpreq")
			return;

		auto printHelp = [&client, prefix, message]()
		{
			UmikoBot* bot = reinterpret_cast<UmikoBot*>(&client);
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help setexpreq");
			QString description = bot->GetCommandHelp("setexpreq", prefix);
			embed.setDescription(description);
			bot->createMessage(message.channelId(), embed);
		};

		if (args.size() == 2)
		{
			if (args.last() == "current")
			{
				client.createMessage(message.channelId(), "Exp requirement is currently set to " + QString::number(setting->expRequirement));
				return;
			}

			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[args, &client, message, channel, printHelp, setting](bool result)
			{
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}
				bool ok;
				unsigned int expReq = args[1].toUInt(&ok);
				if (!ok)
				{
					client.createMessage(message.channelId(), "Invalid exp requirement.");
					return;
				}
				setting->expRequirement = expReq;
				client.createMessage(message.channelId(), "Exp requirement set to " + QString::number(expReq) + " succesfully!");
			});
		}
		else
			printHelp();
	});

	RegisterCommand(Commands::LEVEL_MODULE_EXP_GROWTH_RATE, "setgrowthrate",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel)
	{
		GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
		QStringList args = message.content().split(' ');
		QString prefix = setting->prefix;

		if (args.first() != prefix + "setgrowthrate")
			return;

		auto printHelp = [&client, prefix, message]()
		{
			UmikoBot* bot = reinterpret_cast<UmikoBot*>(&client);
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help setgrowthrate");
			QString description = bot->GetCommandHelp("setgrowthrate", prefix);
			embed.setDescription(description);
			bot->createMessage(message.channelId(), embed);
		};

		if (args.size() == 2)
		{
			if (args.last() == "current")
			{
				client.createMessage(message.channelId(), "Exp growth is currently set to " + QString::number(setting->growthRate));
				return;
			}

			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[args, &client, message, channel, printHelp, setting](bool result)
			{
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}
				bool ok;
				float growthRate = args[1].toFloat(&ok);
				if (!ok || growthRate < 1)
				{
					client.createMessage(message.channelId(), "Invalid growth rate.");
					return;
				}
				setting->growthRate = growthRate;
				client.createMessage(message.channelId(), "Growth rate set to " + QString::number(growthRate) + " succesfully!");
			});
		}
		else
			printHelp();
	});

	RegisterCommand(Commands::LEVEL_MODULE_EXP_GIVE, "givexp",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel)
	{
		QString prefix = GuildSettings::GetGuildSetting(channel.guildId()).prefix;
		QStringList args = message.content().split(' ');

		if (args.first() != prefix + "givexp")
			return;

		auto printHelp = [&client, prefix, message]()
		{
			UmikoBot* bot = reinterpret_cast<UmikoBot*>(&client);
			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help givexp");
			QString description = bot->GetCommandHelp("givexp", prefix);
			embed.setDescription(description);
			bot->createMessage(message.channelId(), embed);
		};

		if (args.size() >= 2)
		{
			Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::ADMIN,
				[this, args, &client, message, channel, printHelp](bool result)
			{
				if (!result)
				{
					client.createMessage(message.channelId(), "You don't have permissions to use this command.");
					return;
				}
				unsigned int exp = 0;

				if (args[1].endsWith("L"))
				{
					QStringRef substring(&args[1], 0, args[1].size() - 1);
					unsigned int levels = substring.toUInt();
				}
				
			});
		}
		else
			printHelp();
	});
}

void LevelModule::OnSave(QJsonDocument& doc) const
{
	QJsonObject json;
	QJsonObject levels;
	QJsonObject backups;

	for (auto it = m_exp.begin(); it != m_exp.end(); it++)
		for (int i = 0; i < it.value().size(); i++) 
			if (m_client->GetName(it.key(), it.value()[i].user) == "")
			{
				bool found = false;
				for (GuildLevelData& data : m_backupexp[it.key()])
					if (data.user == it.value()[i].user) {
						data.exp += it.value()[i].exp;
						found = true;
						break;
					}

				if (!found)
					m_backupexp[it.key()].append(it.value()[i]);

				it.value().erase(it.value().begin() + i);
			}

	for (auto it = m_backupexp.begin(); it != m_backupexp.end(); it++)
		for (int i = 0; i < it.value().size(); i++)
			if (m_client->GetName(it.key(), it.value()[i].user) != "")
			{
				bool found = false;
				for (GuildLevelData& data : m_exp[it.key()])
					if (data.user == it.value()[i].user) {
						data.exp += it.value()[i].exp;
						found = true;
						break;
					}
				
				if (!found)
					m_exp[it.key()].append(it.value()[i]);

				it.value().erase(it.value().begin() + i);
			}

	if(m_exp.size() > 0)
		for (auto it = m_exp.begin(); it != m_exp.end(); it++) 
		{
			QJsonObject level;

			for (int i = 0; i < it.value().size(); i++)
				level[QString::number(it.value()[i].user)] = it.value()[i].exp;

			levels[QString::number(it.key())] = level;
		}

	if(m_backupexp.size() > 0)
		for (auto it = m_backupexp.begin(); it != m_backupexp.end(); it++)
		{
			QJsonObject backup;

			for (int i = 0; i < it.value().size(); i++)
				backup[QString::number(it.value()[i].user)] = it.value()[i].exp;

			backups[QString::number(it.key())] = backup;
		}

	json["levels"] = levels;
	json["backups"] = backups;
	doc.setObject(json);
}

void LevelModule::OnLoad(const QJsonDocument& doc)
{
	QJsonObject json = doc.object();

	QJsonObject backups = json["backups"].toObject();
	QJsonObject levels = json["levels"].toObject();

	QStringList guildIds = levels.keys();

	for (const QString& guild : guildIds)
	{
		snowflake_t guildId = guild.toULongLong();

		QJsonObject level = levels[guild].toObject();
		QJsonObject backup = backups[guild].toObject();

		for (const QString& user : level.keys())
			m_exp[guildId].append({ user.toULongLong(), level[user].toInt(), 0 });

		for (const QString& user : backup.keys())
			m_backupexp[guildId].append({ user.toULongLong(), backup[user].toInt(), 0 });
	}
}

void LevelModule::StatusCommand(QString& result, snowflake_t guild, snowflake_t user)
{
	GuildSetting s = GuildSettings::GetGuildSetting(guild);

	unsigned int xp = GetData(guild, user).exp;

	unsigned int xpRequirement = s.expRequirement;
	unsigned int level = 1;
	while (xp > xpRequirement && level < s.maximumLevel) {
		level++;
		xp -= xpRequirement;
		xpRequirement *= s.growthRate;
	}

	if (level >= s.maximumLevel)
	{
		xp = 0;
		level = s.maximumLevel;
		xpRequirement = 0;
	}

	unsigned int rankLevel = level;
	QString rank = "";
	if (s.ranks.size() > 0) {
		for (int i = 0; i < s.ranks.size() - 1; i++)
		{
			if (rankLevel >= s.ranks[i].minimumLevel && rankLevel < s.ranks[i + 1].minimumLevel)
			{
				rank = s.ranks[i].name;
				break;
			}
		}
		if (rank == "")
			rank = s.ranks[s.ranks.size() - 1].name;
		result += "Rank: " + rank + "\n";
	}

	result += "Total exp: " + QString::number(GetData(guild, user).exp) + "\n";


	result += "Level: " + QString::number(level) + "\n";
	if(level == s.maximumLevel)
		result += QString("Exp needed for rankup: Maximum Level\n");
	else
		result += QString("Exp needed for rankup: %1/%2\n").arg(QString::number(xp), QString::number(xpRequirement));
	result += "\n";
}

void LevelModule::OnMessage(Discord::Client& client, const Discord::Message& message) 
{
	Module::OnMessage(client, message);

	client.getChannel(message.channelId()).then(
		[this, message](const Discord::Channel& channel) 
	{
		if (!GuildSettings::IsModuleEnabled(channel.guildId(), GetName(), IsEnabledByDefault()))
			return;

		if (message.author().bot())
			return;

		for (GuildLevelData& data : m_exp[channel.guildId()]) {
			if (data.user == message.author().id()) {
				data.messageCount++;
				return;
			}
		}

		m_exp[channel.guildId()].append({ message.author().id(), 0, 1 });
	});
}

LevelModule::GuildLevelData LevelModule::GetData(snowflake_t guild, snowflake_t user)
{
	for (GuildLevelData data : m_exp[guild])
	{
		if (data.user == user) {
			return data;
		}
	}
	return { user, 0,0 };
}
