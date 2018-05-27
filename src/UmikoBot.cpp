#include "UmikoBot.h"
#include "modules/LevelModule.h"

#include "modules/TimezoneModule.h"
#include "modules/CurrencyModule.h"

UmikoBot::UmikoBot(QObject* parent)
	: Client("umiko-bot", parent)
{
	if (!QDir("configs").exists())
		QDir().mkdir("configs");

	GuildSettings::Load("settings.json");

	m_modules.push_back(new LevelModule);
	m_modules.push_back(new TimezoneModule);
	m_modules.push_back(new CurrencyModule);

	Q_FOREACH(Module* module, m_modules)
	{
		module->Load();
	}

	m_timer.setInterval(/*60 * 60 */ 1000);
	QObject::connect(&m_timer, &QTimer::timeout, 
		[this]()
	{
		Save();
	});
	m_timer.start();

	connect(this, &Client::onMessageCreate,
		[this](const Discord::Message& message)
	{
		Q_FOREACH(Module* module, m_modules)
		{
			module->OnMessage(*this, message);
		}

		getChannel(message.channelId()).then(
			[this, message](const Discord::Channel& channel)
		{
			GuildSetting setting = GuildSettings::GetGuildSetting(channel.guildId());
			if (channel.guildId() != 0 && !message.author().bot()) // DM
			{
				Q_FOREACH(const GlobalCommand& command, m_commands)
				{
					if (message.content().startsWith(setting.prefix + command.name))
					{
						command.callback(message, channel);
					}
				}
			}
		});
	
	});

	connect(this, &Client::onGuildCreate,
		[](const Discord::Guild& guild)
	{
		GuildSettings::AddGuild(guild.id());
	});

	connect(this, &Client::onReady,
		[this]()
	{
		GetGuilds();
	});

	connect(this, &Client::onGuildMemberUpdate,
		[this](snowflake_t guild, const QList<snowflake_t>& roles, const Discord::User& user, const QString& nick)
	{
		if(nick == "")
			m_nicknames[guild][user.id()] = user.username();
		else
			m_nicknames[guild][user.id()] = nick;
	});

	m_commands.push_back({Commands::GLOBAL_STATUS, "status",
		[this](const Discord::Message& message, const Discord::Channel& channel)
	{
		QStringList args = message.content().split(" ");
		if (args.size() > 1) {
			if (args.first() != GuildSettings::GetGuildSetting(channel.guildId()).prefix + "status")
				return;
			for (QMap<snowflake_t, QString>::iterator& it = m_nicknames[channel.guildId()].begin(); it != m_nicknames[channel.guildId()].end(); it++)
			{
				if (it.value() == args.last()) 
				{
					getGuildMember(channel.guildId(), message.author().id()).then(
						[this, message, channel, it](const Discord::GuildMember& member)
					{
						QString status = "";
						Q_FOREACH(Module* module, m_modules)
						{
							module->StatusCommand(status, channel.guildId(), it.key());
						}

						Discord::Embed embed;
						QString icon = "https://cdn.discordapp.com/avatars/" + QString::number(member.user().id()) + "/" + member.user().avatar() + ".png";
						embed.setAuthor(Discord::EmbedAuthor(GetNick(channel.guildId(), it.key()), "", icon));
						embed.setColor(qrand() % 16777216);
						embed.setTitle("Status");
						embed.setDescription(status);

						createMessage(message.channelId(), embed);
					});
				}
			}
		}
		else
			getGuildMember(channel.guildId(), message.author().id()).then(
				[this, message, channel](const Discord::GuildMember& member)
			{
				QString status = "";
				Q_FOREACH(Module* module, m_modules)
				{
					module->StatusCommand(status, channel.guildId(), message.author().id());
				}

				Discord::Embed embed;
				QString icon = "https://cdn.discordapp.com/avatars/" + QString::number(member.user().id()) + "/" + member.user().avatar() + ".png";
				embed.setAuthor(Discord::EmbedAuthor(GetNick(channel.guildId(), message.author().id()), "", icon));
				embed.setColor(qrand() % 16777216);
				embed.setTitle("Status");
				embed.setDescription(status);

				createMessage(message.channelId(), embed);
			});
	}});

}

UmikoBot::~UmikoBot()
{
	Save();

	for (Module* module : m_modules)
		delete module;
}

QString UmikoBot::GetNick(snowflake_t guild, snowflake_t user)
{
	return m_nicknames[guild][user];
}

void UmikoBot::Save()
{
	GuildSettings::Save();

	Q_FOREACH(const Module* module, m_modules)
	{
		module->Save();
	}
}

void UmikoBot::GetGuilds(snowflake_t after)
{
	if (after == 0)
	{
		getCurrentUserGuilds().then(
			[this](const QList<Discord::Guild>& guilds)
		{
			for (size_t i = 0; i < guilds.size(); i++)
			{
				GetGuildsMemberCount(guilds[i].id());
			}

			if (guilds.size() == 100) //guilds size is equal to the limit
			{
				GetGuilds(guilds[guilds.size() - 1].id());
			}
			qDebug("Guild count: %llu", guilds.size());
		});
	}
	else
	{
		getCurrentUserGuilds(0, after).then(
			[this](const QList<Discord::Guild>& guilds)
		{
			for (size_t i = 0; i < guilds.size(); i++)
			{
				GetGuildsMemberCount(guilds[i].id());
			}

			if (guilds.size() == 100) //guilds size is equal to the limit
			{
				GetGuilds(guilds[guilds.size() - 1].id());
			}
			qDebug("Guild count: %llu", guilds.size());
		});
	}
}

void UmikoBot::GetGuildsMemberCount(snowflake_t guild, snowflake_t after)
{
	if (after == 0)
	{
		listGuildMembers(guild, 1000).then(
			[this, guild](const QList<Discord::GuildMember>& members)
		{
			for (size_t i = 0; i < members.size(); i++)
			{
				QString name = members[i].nick();
				if (name == "")
					name = members[i].user().username();
				m_nicknames[guild][members[i].user().id()] = name;
			}

			if (members.size() == 1000) //guilds size is equal to the limit
			{
				GetGuildsMemberCount(guild, members[members.size() - 1].user().id());
			}
			qDebug("Membercount: %llu, %i", guild, members.size());
		});
	}
	else
	{
		listGuildMembers(guild, after, 1000).then(
			[this, guild](const QList<Discord::GuildMember>& members)
		{
			for (size_t i = 0; i < members.size(); i++)
			{
				QString name = members[i].nick();
				if (name == "")
					name = members[i].user().username();
				m_nicknames[guild][members[i].user().id()] = name;
			}

			if (members.size() == 1000) //guilds size is equal to the limit
			{
				GetGuildsMemberCount(guild, members[members.size() - 1].user().id());
			}
			qDebug("Membercount: %llu, %i", guild, members.size());
		});
	}
}