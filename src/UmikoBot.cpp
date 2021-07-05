#include "UmikoBot.h"
#include "core/Permissions.h"

#include "modules/LevelModule.h"
#include "modules/TimezoneModule.h"
#include "modules/CurrencyModule.h"
#include "modules/ModerationModule.h"
#include "modules/FunModule.h"
#include "modules/UserModule.h"
#include "modules/EventModule.h"

#include "Logger.h"

using namespace Discord;

UmikoBot& UmikoBot::Instance()
{
	static UmikoBot bot;
	return bot;
}

UmikoBot::UmikoBot(QObject* parent)
	: Client("umiko-bot", parent)
{
	UStartLogger("umiko.log");
	USetThreadName("Main");
	ULog(ulog::Severity::Debug, "Starting bot...");

	if (!QDir("configs").exists())
		QDir().mkdir("configs");

	GuildSettings::Load("settings.json");
	
	Load();
	m_modules.push_back(new LevelModule(this));
	m_modules.push_back(new TimezoneModule);
	m_modules.push_back(new CurrencyModule(this));
	m_modules.push_back(new ModerationModule);
	m_modules.push_back(new FunModule(this));
	m_modules.push_back(new UserModule);
	m_modules.push_back(new EventModule(this));
	
	Q_FOREACH(Module* module, m_modules)
	{
		module->Load();
	}

	m_timer.setInterval(60 * 1000);
	QObject::connect(&m_timer, &QTimer::timeout, 
		[this]()
	{
		Save();
	});
	
	connect(&getGatewaySocket(), &GatewaySocket::disconnected, this, &UmikoBot::OnDisconnected);

	connect(this, &Client::onMessageCreate,
		[this](const Message& message)
	{
		Q_FOREACH(Module* module, m_modules)
		{
			module->OnMessage(*this, message);
		}

		getChannel(message.channelId()).then(
			[this, message](const Channel& channel)
		{
			GuildSetting setting = GuildSettings::GetGuildSetting(channel.guildId());
			if (channel.guildId() != 0 && !message.author().bot()) // DM
			{
				Q_FOREACH(const Command& command, m_commands)
				{
					if (message.content().startsWith(setting.prefix + command.name) && (command.name == "output" || GuildSettings::OutputAllowed(channel.guildId(), channel.id())))
					{
						command.callback(*this, message, channel);
					}
				}
			}
		});
	
	});

	connect(this, &Client::onGuildCreate,
		[](const Guild& guild)
	{
		GuildSettings::GetGuildSetting(guild.id()); // only creates if it doesn't exist
	});

	connect(this, &Client::onReady,
		[this]()
	{
		GetGuilds();
		m_timer.start();
	});

	connect(this, &Client::onGuildMemberUpdate,
		[this](snowflake_t guild, const QList<snowflake_t>& roles, const User& user, const QString& nick)
	{
		m_guildDatas[guild].userdata[user.id()].username = user.username();
		m_guildDatas[guild].userdata[user.id()].nickname = nick;
	});

	connect(this, &Client::onGuildRoleUpdate,
		[this](snowflake_t guild_id, const Role& role)
	{
		for (int i = 0; i < m_guildDatas[guild_id].roles.size(); i++)
			if (role.id() == m_guildDatas[guild_id].roles[i].id()) 
			{
				m_guildDatas[guild_id].roles[i] = role;
				return;
			}
		m_guildDatas[guild_id].roles.push_back(role);
	});

	connect(this, &Client::onGuildRoleDelete,
		[this](snowflake_t guild_id, snowflake_t role_id)
	{
		for (int i = 0; i < m_guildDatas[guild_id].roles.size(); i++)
			if (role_id == m_guildDatas[guild_id].roles[i].id())
			{
				m_guildDatas[guild_id].roles.erase(m_guildDatas[guild_id].roles.begin() + i);
				return;
			}
	});

	connect(this, &Client::onGuildUpdate,
		[this](const Guild& guild)
	{
		m_guildDatas[guild.id()].ownerId = guild.ownerId();
	});

	connect(this, &Client::onGuildMemberRemove,
		[this](snowflake_t guild_id, const User& user)
	{
		for(auto it = m_guildDatas[guild_id].userdata.begin(); it != m_guildDatas[guild_id].userdata.end(); it++)
			if (it.key() == user.id()) {
				m_guildDatas[guild_id].userdata.erase(it);
				break;
			}
	});

	connect(this, &Client::onGuildMemberAdd,
		[this](const GuildMember& member, snowflake_t guild_id)
	{
		m_guildDatas[guild_id].userdata[member.user().id()].username = member.user().username();
	});

	m_commands.push_back({Commands::GLOBAL_STATUS, "status",
		[this](Client& client,const Message& message, const Channel& channel)
	{
		QStringList args = message.content().split(" ");

		auto printStatus = [this](const Channel& channel, const Message& message, snowflake_t user, QString nickname) {
			GetAvatar(channel.guildId(), user).then(
				[this, message, channel, user, nickname](const QString& icon)
			{
				QString status = "";
				Q_FOREACH(Module* module, m_modules)
				{
					if(GuildSettings::IsModuleEnabled(channel.guildId(), module->GetName(), module->IsEnabledByDefault()))
						module->StatusCommand(status, channel.guildId(), user);
				}

				Embed embed;
				embed.setAuthor(EmbedAuthor(nickname, "", icon));
				embed.setColor(qrand() % 16777216);
				embed.setTitle("Status");
				embed.setDescription(status);

				createMessage(message.channelId(), embed);
			});
		};

		if (args.size() > 1) 
		{
			QList<User> mentions = message.mentions();
			if (mentions.size() > 0)
			{
				printStatus(channel, message, mentions.first().id(), GetName(channel.guildId(), mentions.first().id()));
				return;
			}

			snowflake_t result;

			if ((result = GetUserFromArg(channel.guildId(), args, 1)) == 0) 
				client.createMessage(channel.id(), "Could not find user!");
			else 
				printStatus(channel, message, result, GetName(channel.guildId(), result));
		}
		else
			GetAvatar(channel.guildId(), message.author().id()).then(
				[this, message, channel](const QString& icon)
			{
				QString status = "";
				Q_FOREACH(Module* module, m_modules)
				{
					if (GuildSettings::IsModuleEnabled(channel.guildId(), module->GetName(), module->IsEnabledByDefault()))
						module->StatusCommand(status, channel.guildId(), message.author().id());
				}

				Embed embed;
				embed.setAuthor(EmbedAuthor(GetName(channel.guildId(), message.author().id()), "", icon));
				embed.setColor(qrand() % 16777216);
				embed.setTitle("Status");
				embed.setDescription(status);

				createMessage(message.channelId(), embed);
			});
	}});

	m_commands.push_back({Commands::GLOBAL_HELP, "help",
		[this](Client& client, const Message& message, const Channel& channel)
	{
		QStringList args = message.content().split(" ");
		QString prefix = GuildSettings::GetGuildSetting(channel.guildId()).prefix;
		if (args.size() > 1)
		{
			if (args.size() != 2)
				return;

			QString commandName = args[1];
			if (commandName.startsWith(prefix))
				commandName = QStringRef(&commandName, prefix.size(), commandName.size() - prefix.size()).toString();

			Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help " + commandName);

			QString description = GetCommandHelp(commandName, prefix);
			if (description != "")
				embed.setDescription(description);
			else
				embed.setDescription("Command not found!");

			createMessage(message.channelId(), embed);
		}
		else
		{
			::Permissions::ContainsPermission(*this, channel.guildId(), message.author().id(), CommandPermission::MODERATOR,
				[this, prefix, message, channel](bool result)
			{
				QString description = "";
				[this, &prefix, &description, result, channel, message]() {
					auto forCommand =
						[this, &description, &prefix, result, channel, message](QList<Command> commands) -> bool
					{
						
							for (const Command& command : commands)
							{
								if (result) {
									QString current = "";
									current = "`" + prefix + command.name + "` - " + m_commandsInfo[command.id].briefDescription + "\n";
									if (description.length() + current.length() < 1900)
										description += current;
									else
										return true;
								}
								else {
									if (m_commandsInfo[command.id].adminPermission == true)
										continue;

									QString current = "";
									current = "`" + prefix + command.name + "` - " + m_commandsInfo[command.id].briefDescription + "\n";
									if (description.length() + current.length() < 1900)
										description += current;
									else
										return true;
								}
							}
							return false;
						
					};

					if (!forCommand(m_commands))
						Q_FOREACH(Module* module, m_modules)
							if (forCommand(module->GetCommands()))
								return;
				}();
				description += "\n**Note:** Use `" + prefix + "help <command>` to get the help for a specific command";

				Embed embed;
				embed.setColor(qrand() % 16777216);
				embed.setTitle("Help");
				embed.setDescription(description);

				createMessage(message.channelId(), embed);
			});
		}
	}});

	m_commands.push_back({ Commands::GLOBAL_SET_PREFIX, "setprefix",
		[this](Client& client, const Message& message, const Channel& channel)
	{
		GuildSetting& s = GuildSettings::GetGuildSetting(channel.guildId());
		QStringList args = message.content().split(' ');

		UmikoBot::VerifyAndRunAdminCmd(client, message, channel, 2, args, false, [this, &client, channel, message, args, &s]()
		{
			QString prefix = "";
			for (int i = 1; i < args.size(); i++)
			{
				prefix += args[i];
				if (i < args.size() - 1)
					prefix += " ";
			}

			s.prefix = prefix;

			createMessage(channel.id(), "Prefix is now set to " + s.prefix);
		});
	}});

	m_commands.push_back({ Commands::GLOBAL_MODULE, "module",
		[this](Client& client, const Message& message, const Channel& channel)
	{
		QStringList args = message.content().split(" ");
		QString prefix = GuildSettings::GetGuildSetting(channel.guildId()).prefix;

		auto printHelp = [this, prefix, message]() 
		{
			Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help module");
			QString description = GetCommandHelp("module", prefix);
			embed.setDescription(description);
			createMessage(message.channelId(), embed);
		};

		if (args.size() > 1 && args[1] == "list")
		{
			if (args.size() == 2)
			{
				Embed embed;
				embed.setColor(qrand() % 16777216);
				embed.setTitle("Module list");

				QString description = "";

				Q_FOREACH(const Module* module, m_modules)
				{
					description += module->GetName() + " - " + (GuildSettings::IsModuleEnabled(channel.guildId(), module->GetName(), module->IsEnabledByDefault()) == true ? "enabled" : "disabled") + QString("\n");
				}

				embed.setDescription(description);
				createMessage(message.channelId(), embed);
			}
			else if(args.size() == 3)
			{
				Embed embed;
				embed.setColor(qrand() % 16777216);
				embed.setTitle("Module command list");

				QString description = "";

				bool found = false;

				Q_FOREACH(const Module* module, m_modules)
					if (module->GetName() == args.last())
					{
						found = true;
						Q_FOREACH(const Command& command, module->GetCommands())
							description += prefix + command.name + "\n";
					}

				if (!found)
					description = "Module not found.";
				else if (found && description == "")
					description = "Module has no commands.";
				else
					description += "\n**Note**: Use `" + prefix + "help to get the usage of a command.";

				embed.setDescription(description);
				createMessage(message.channelId(), embed);
			}
			else 
			{
				printHelp();
			}
		} 
		else if (args.size() == 3 && args[1] == "enable")
		{
			UmikoBot::VerifyAndRunAdminCmd(client, message, channel, 3, args, false, [this, &client, channel, message, args]()
			{
				bool found = false;

				Q_FOREACH(const Module* module, m_modules)
					if (module->GetName() == args.last())
					{
						if (GuildSettings::IsModuleEnabled(channel.guildId(), args.last(), module->IsEnabledByDefault()))
						{
							createMessage(message.channelId(), "Module " + args.last() + " is already enabled");
							return;
						}
						else {

							GuildSettings::ToggleModule(channel.guildId(), args.last(), true, module->IsEnabledByDefault());
							found = true;
						}
					}

				if (!found)
					createMessage(message.channelId(), "Could not find module " + args.last());
				else
					createMessage(message.channelId(), "Enabled module " + args.last());
			});
		}
		else if (args.size() == 3 && args[1] == "disable")
		{
			UmikoBot::VerifyAndRunAdminCmd(client, message, channel, 3, args, false, [this, &client, channel, message, args]()
			{
				bool found = false;

				Q_FOREACH(const Module* module, m_modules)
					if (module->GetName() == args.last())
					{
						if (!GuildSettings::IsModuleEnabled(channel.guildId(), args.last(), module->IsEnabledByDefault()))
						{
							createMessage(message.channelId(), "Module " + args.last() + " is already disabled");
							return;
						}
						else {

							GuildSettings::ToggleModule(channel.guildId(), args.last(), false, module->IsEnabledByDefault());
							found = true;
						}
					}

				if (!found)
					createMessage(message.channelId(), "Could not find module " + args.last());
				else
					createMessage(message.channelId(), "Disabled module " + args.last());
			});
		}
		else
		{
			printHelp();
		}

	}});

	m_commands.push_back({Commands::GLOBAL_OUTPUT, "output",
		[this](Client& client, const Message& message, const Channel& channel)
	{
		QStringList args = message.content().split(" ");
		QString prefix = GuildSettings::GetGuildSetting(channel.guildId()).prefix;

		auto printHelp = [this, prefix, message]()
		{
			Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Help output");
			QString description = GetCommandHelp("output", prefix);
			embed.setDescription(description);
			createMessage(message.channelId(), embed);
		};
		if (args.size() > 1 && args[1] == "whitelist")
		{
			UmikoBot::VerifyAndRunAdminCmd(client, message, channel, 2, args, false, [this, &client, channel, message]()
			{
				GuildSetting& s = GuildSettings::GetGuildSetting(channel.guildId());
				for (int i = 0; i < s.outputBlacklistedChannels.size(); i++) {
					if (s.outputBlacklistedChannels[i] == channel.id())
					{
						s.outputBlacklistedChannels.erase(s.outputBlacklistedChannels.begin() + i);
						createMessage(message.channelId(), "Channel removed from blacklisted!");

						return;
					}
				}

				for (int i = 0; i < s.outputWhitelistedChannels.size(); i++) {
					if (s.outputWhitelistedChannels[i] == channel.id())
					{
						createMessage(message.channelId(), "Channel is already whitelisted!");
						return;
					}
				}

				s.outputWhitelistedChannels.push_back(channel.id());
				createMessage(message.channelId(), "Channel has been whitelisted!");

			});
		}
		else if (args.size() > 1 && args[1] == "blacklist")
		{
			UmikoBot::VerifyAndRunAdminCmd(client, message, channel, 2, args, false, [this, &client, channel, message]()
			{
				GuildSetting& s = GuildSettings::GetGuildSetting(channel.guildId());
				for (int i = 0; i < s.outputWhitelistedChannels.size(); i++) {
					if (s.outputWhitelistedChannels[i] == channel.id())
					{
						s.outputWhitelistedChannels.erase(s.outputWhitelistedChannels.begin() + i);
						createMessage(message.channelId(), "Channel removed from whitelisted!");
						return;
					}
				}

				for (int i = 0; i < s.outputBlacklistedChannels.size(); i++) {
					if (s.outputBlacklistedChannels[i] == channel.id())
					{
						createMessage(message.channelId(), "Channel is already blacklisted!");
						return;
					}
				}

				s.outputBlacklistedChannels.push_back(channel.id());
				createMessage(message.channelId(), "Channel has been blacklisted!");
			});
		}
		else
			printHelp();
	} });
}

UmikoBot::~UmikoBot()
{
	Save();

	for (Module* module : m_modules)
		delete module;

	ULog(ulog::Severity::Debug, "Bot Terminating...");
	UStopLogger();
}

QString UmikoBot::GetNick(snowflake_t guild, snowflake_t user)
{
	return m_guildDatas[guild].userdata[user].nickname;
}

QString UmikoBot::GetUsername(snowflake_t guild, snowflake_t user)
{
	return m_guildDatas[guild].userdata[user].username;
}

QString UmikoBot::GetName(snowflake_t guild, snowflake_t user)
{
	if (m_guildDatas[guild].userdata[user].nickname != "")
		return m_guildDatas[guild].userdata[user].nickname;
	return m_guildDatas[guild].userdata[user].username;
}

Promise<QString>& UmikoBot::GetAvatar(snowflake_t guild, snowflake_t user)
{
	Promise<QString>* promise = new Promise<QString>();

	getGuildMember(guild, user).then([promise, user](const GuildMember& member)
	{
		QString icon = member.user().avatar();

		if (icon != "")
		{
			icon = "https://cdn.discordapp.com/avatars/" + QString::number(user) + "/" + icon + ".png";
		}
		else
		{
			icon = "https://cdn.discordapp.com/embed/avatars/" + QString::number(member.user().discriminator().toULongLong() % 5) + ".png";
		}

		promise->resolve(icon);
	});

	return (*promise);
}

snowflake_t UmikoBot::GetUserFromArg(snowflake_t guild, QStringList args, int startIndex) {
	bool ok;
	snowflake_t user = args[startIndex].toULongLong(&ok);

	if (ok)
	{
		QString nick = GetName(guild, user);
		if (nick != "") {
			return user;
		}
	}

	QString name = "";
	for (int i = startIndex; i < args.size(); i++)
	{
		name += args[i];
		if (i < args.size() - 1)
			name += " ";
	}

	struct Match {
		snowflake_t user;
		QString name;
	} perfectNickMatch, partialNickMatch, perfectNameMatch, partialNameMatch;

	perfectNickMatch = { 0 };
	partialNickMatch = { 0 };
	perfectNameMatch = { 0 };
	partialNameMatch = { 0 };

	for (QMap<snowflake_t, UserData>::iterator it = m_guildDatas[guild].userdata.begin(); it != m_guildDatas[guild].userdata.end(); it++)
	{
		if (it.value().nickname == name)
		{
			perfectNickMatch = { it.key(), it.value().nickname };
			break;
		}
		else if (it.value().nickname.startsWith(name))
		{
			partialNickMatch = { it.key(), it.value().nickname };
		}

		if (it.value().username == name)
		{
			perfectNameMatch = { it.key(), GetName(guild, it.key()) };
			break;
		}
		else if (it.value().username.startsWith(name))
		{
			partialNameMatch = { it.key(), GetName(guild, it.key()) };
		}
	}

	if (perfectNickMatch.user != 0)
		return perfectNickMatch.user;
	else if (perfectNameMatch.user != 0)
		return perfectNameMatch.user;
	else if (partialNickMatch.user != 0)
		return partialNickMatch.user;
	else if (partialNameMatch.user != 0)
		return partialNameMatch.user;
	else
		return 0;
}

Module* UmikoBot::GetModuleByName(const QString& name)
{
	for (Module* theModule : m_modules)
	{
		if (theModule->GetName() == name)
		{
			return theModule;
		}
	}

	return nullptr;
}

const QList<Role>& UmikoBot::GetRoles(snowflake_t guild)
{
	return m_guildDatas[guild].roles;
}

bool UmikoBot::IsOwner(snowflake_t guild, snowflake_t user)
{
	return m_guildDatas[guild].ownerId == user;
}

QString UmikoBot::GetCommandHelp(QString commandName, QString prefix)
{
	QString description = "";
	auto forCommand =
		[this, &description, &prefix, &commandName](QList<Command> commands)
	{
		for (const Command& command : commands)
		{
			if (command.name == commandName)
			{
				CommandInfo& info = m_commandsInfo[command.id];
				description += "**Command name**: " + commandName + "\n\n";
				description += info.briefDescription + "\n\n";

				QStringList usages = info.usage.split("\n");
				description += "**Usage**: \n";
				if (usages.size() == 0)
					description += "\t`" + prefix + info.usage + "`\n";
				else
					for (const QString& usage : usages)
						description += "\t`" + prefix + usage + "`\n";

				description += "\n" + info.additionalInfo;
				return true;
			}
		}
		return false;
	};

	if (!forCommand(m_commands))
		Q_FOREACH(Module* module, m_modules)
		if (forCommand(module->GetCommands()))
			break;
	return description;
}

QList<Command> UmikoBot::GetAllCommands()
{
	// TDDO(fkp): As this doesn't change, cache it maybe?
	QList<Command> result = m_commands;
	
	for (Module* module : m_modules)
	{
		result.append(module->GetCommands());
	}

	return result;
}

void UmikoBot::VerifyAndRunAdminCmd(Client& client, const Message& message, const Channel& channel, unsigned int requiredNumberOfArgs, const QStringList& args, bool argumentShouldBeANumber, std::function<void()> callback)
{
	::Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::MODERATOR, [args, &client, message, channel, requiredNumberOfArgs, argumentShouldBeANumber, callback](bool result)
	{
		if (!result)
		{
			client.createMessage(message.channelId(), "**You don't have permissions to use this command.**");
			return;
		}

		// TODO(fkp): Maybe we should have a parameter specifying if we want exact/at least $x args.
		// I just don't want to have too many parameters, it's already quite long
		if (args.size() < requiredNumberOfArgs)
		{
			client.createMessage(message.channelId(), "**Wrong Usage of Command!** ");
			return;
		}

		if (args.size() > 1 && argumentShouldBeANumber)
		{
			bool ok;
			// Should be fine to check all numbers agains double, right?
			args.at(1).toDouble(&ok);

			if (!ok)
			{
				client.createMessage(message.channelId(), "**Argument is not a number!**");
				return;
			}
		}

		callback();
	});
}

void UmikoBot::OnDisconnected()
{
	getGatewaySocket().reconnectToGateway();
}

void UmikoBot::Save()
{
	GuildSettings::Save();

	Q_FOREACH(const Module* module, m_modules)
	{
		module->Save();
	}
}

void UmikoBot::Load()
{
#define Command(x) {#x, x}
	using namespace Commands;
	static QMap<QString, unsigned int> commandIds
	{
		Command(GLOBAL_STATUS),
		Command(GLOBAL_HELP),
		Command(GLOBAL_SET_PREFIX),
		Command(GLOBAL_MODULE),
		Command(GLOBAL_OUTPUT),

		Command(LEVEL_MODULE_TOP),
		Command(LEVEL_MODULE_RANK),
		Command(LEVEL_MODULE_MAX_LEVEL),
		Command(LEVEL_MODULE_EXP_REQUIREMENT),
		Command(LEVEL_MODULE_EXP_GROWTH_RATE),
		Command(LEVEL_MODULE_EXP_GIVE),
		Command(LEVEL_MODULE_EXP_TAKE),
		Command(LEVEL_MODULE_BLOCK_EXP),
		
		Command(USER_MODULE_WHO_IS),
		Command(USER_MODULE_I_AM),
		Command(USER_MODULE_ACHIEVEMENTS),

		Command(TIMEZONE_MODULE_TIMEOFFSET),

		Command(MODERATION_INVITATION_TOGGLE),
		Command(MODERATION_WARN),
		Command(MODERATION_WARNINGS),
		Command(MODERATION_WARNINGS_ALL),

		Command(CURRENCY_WALLET),
		Command(CURRENCY_DAILY),
		Command(CURRENCY_GAMBLE),
		Command(CURRENCY_CLAIM),
		Command(CURRENCY_GIFT),
		Command(CURRENCY_SET_PRIZE_CHANNEL),
		Command(CURRENCY_SET_NAME),
		Command(CURRENCY_SET_SYMBOL),
		Command(CURRENCY_SET_DAILY),
		Command(CURRENCY_SET_GAMBLE_LOSS),
		Command(CURRENCY_SET_GAMBLE_MAX_GUESS),
		Command(CURRENCY_SET_GAMBLE_MIN_GUESS),
		Command(CURRENCY_SET_GAMBLE_REWARD),
		Command(CURRENCY_SET_PRIZE),
		Command(CURRENCY_SET_PRIZE_EXPIRY),
		Command(CURRENCY_SET_PRIZE_PROB),
		Command(CURRENCY_RICH_LIST),
		Command(CURRENCY_DONATE),
		Command(CURRENCY_STEAL),
		Command(CURRENCY_COMPENSATE),
		Command(CURRENCY_SET_STEAL_SUCCESS_CHANCE),
		Command(CURRENCY_SET_STEAL_FINE_PERCENT),
		Command(CURRENCY_SET_STEAL_VICTIM_BONUS),
		Command(CURRENCY_SET_STEAL_JAIL_HOURS),
		Command(CURRENCY_SET_DAILY_BONUS_AMOUNT),
		Command(CURRENCY_SET_DAILY_BONUS_PERIOD),
		Command(CURRENCY_BRIBE),
		Command(CURRENCY_SET_BRIBE_SUCCESS_CHANCE),
		Command(CURRENCY_SET_MAX_BRIBE_AMOUNT),
		Command(CURRENCY_SET_LEAST_BRIBE_AMOUNT),

		Command(EVENT_LAUNCH),
		Command(EVENT_END),
		Command(EVENT),
		Command(EVENT_GIVE_NEW_ACCESS),
		Command(EVENT_TAKE_NEW_ACCESS),
		Command(EVENT_SET_HRHR_STEAL_SUCCESS_CHANCE),
		Command(EVENT_SET_LRLR_STEAL_SUCCESS_CHANCE),
		Command(EVENT_GET_REWARD),
		Command(EVENT_BUY_TICKETS),
		Command(EVENT_TICKET),
		Command(EVENT_SET_TICKET_PRICE),
		Command(EVENT_SET_USER_MAX_TICKET),


		Command(FUN_MEME),
		Command(FUN_POLL),
		Command(FUN_GITHUB),
		Command(FUN_GIVE_NEW_POLL_ACCESS),
		Command(FUN_TAKE_NEW_POLL_ACCESS),
		Command(FUN_ROLL)
	};

	QFile file("commands.json");
	if (file.open(QIODevice::ReadOnly))
	{
		QByteArray data = file.readAll();

		QJsonDocument doc(QJsonDocument::fromJson(data));
		QJsonObject json = doc.object();
		QStringList commandsIdentifier = json.keys();
		for (const QString& identifier : commandsIdentifier)
		{
			QJsonObject current = json.value(identifier).toObject();
			CommandInfo info;
			info.briefDescription = current["brief"].toString();
			info.usage = current["usage"].toString();
			info.additionalInfo = current["additional"].toString();
			info.adminPermission = current["admin"].toBool();

			m_commandsInfo[commandIds[identifier]] = info;
		}
	}
	else
	{
		ULog(ulog::Severity::Warning, "Could not open commands.json");
		// Decide if we should generate them on the fly or something
		// or just terminate the bot
	}
}

void UmikoBot::GetGuilds(snowflake_t after)
{
	auto processGuilds = [this](const QList<Guild>& guilds)
	{
		for (int i = 0; i < guilds.size(); i++)
		{
			getGuildRoles(guilds[i].id()).then(
				[this, guilds, i](const QList<Role>& roles)
			{
				m_guildDatas[guilds[i].id()].roles = roles;
				GetGuildMemberInformation(guilds[i].id());
			});

			getGuild(guilds[i].id()).then(
				[this](const Guild& guild)
			{
				m_guildDatas[guild.id()].ownerId = guild.ownerId();
			});
		}

		if (guilds.size() == 100) //guilds size is equal to the limit
		{
			GetGuilds(guilds[guilds.size() - 1].id());
		}
		ULog(ulog::Severity::Debug, UFString("Guild count: %llu", guilds.size()));
	};

	if (after == 0)
		getCurrentUserGuilds().then(processGuilds);
	else
		getCurrentUserGuilds(0, after).then(processGuilds);
}

void UmikoBot::GetGuildMemberInformation(snowflake_t guild, snowflake_t after)
{
	auto processMembers = [this, guild](const QList<GuildMember>& members)
	{
		for (int i = 0; i < members.size(); i++)
		{
			m_guildDatas[guild].userdata[members[i].user().id()].nickname = members[i].nick();
			m_guildDatas[guild].userdata[members[i].user().id()].username = members[i].user().username();
		}

		if (members.size() == 1000) //guilds size is equal to the limit
		{
			GetGuildMemberInformation(guild, members[members.size() - 1].user().id());
		}

		ULog(ulog::Severity::Debug, UFString("Guild ID: %llu", guild));
		ULog(ulog::Severity::Debug, UFString("Member count: %i", members.size()));
	};

	if (after == 0)
		listGuildMembers(guild, 1000).then(processMembers);
	else
		listGuildMembers(guild, 1000, after).then(processMembers);
}
