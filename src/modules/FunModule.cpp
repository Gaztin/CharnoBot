#include "FunModule.h"
#include <QtNetwork>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include "core/Permissions.h"
#include "Discord/Patches/MessagePatch.h"
#include <QtCore/QPair>
#include <QtCore/QList>

#define POLL_DEFAULT_TIME 1.0*60.0*60.0		//seconds ~~ 1 hr
#define POLL_MAX_TIME 168.0					//hours   ~~ 1 week
#define POLL_MIN_TIME 1.0/60.0				//hours   ~~ 1 mins

using namespace Discord;

FunModule::FunModule(UmikoBot* client) : Module("funutil", true), m_memeChannel(0), m_client(client)
{

	QObject::connect(&m_MemeManager, &QNetworkAccessManager::finished,
		this, [this](QNetworkReply* reply) {
			auto& client = UmikoBot::Instance();
			if (reply->error()) {
				qDebug() << reply->errorString();
				client.createMessage(m_memeChannel, reply->errorString());

				return;
			}

			QString in = reply->readAll();

			QJsonDocument doc = QJsonDocument::fromJson(in.toUtf8());
			auto obj = doc.object();
			bool isNsfw = obj["nsfw"].toBool();
			if (isNsfw) {
				m_MemeManager.get(QNetworkRequest(QUrl("https://meme-api.herokuapp.com/gimme")));
				return;
			}
			QString title = obj["title"].toString();
			QString url = obj["url"].toString();
			QString author = obj["author"].toString();
			QString postLink = obj["postLink"].toString();
			QString subreddit = obj["subreddit"].toString();

			Embed embed;
			EmbedImage img;
			img.setUrl(url);
			embed.setImage(img);
			embed.setTitle(title);
			EmbedFooter footer;
			footer.setText("Post was made by u/" + author + " on r/" + subreddit + ".\nSee the actual post here: " + postLink);
			embed.setFooter(footer);

			client.createMessage(m_memeChannel, embed);
		});

			QObject::connect(&m_GithubManager, &QNetworkAccessManager::finished,
		this, [this](QNetworkReply* reply) {
			auto& client = UmikoBot::Instance();

			if (reply->error()) {
				qDebug() << reply->errorString();
				client.createMessage(m_GithubChannel, reply->errorString());

				return;
			}

			QString in = reply->readAll();

			QJsonDocument doc = QJsonDocument::fromJson(in.toUtf8());
			auto obj = doc.object();

			QJsonArray items = obj["items"].toArray();

			std::random_device device;
			std::mt19937 rng(device());
			std::uniform_int_distribution<std::mt19937::result_type> dist(0, items.size());

			QJsonObject repo = items[dist(rng)].toObject();

			QString repo_fullname = repo["full_name"].toString();
			QString repo_url = repo["html_url"].toString();
			QString repo_language = repo["language"].toString();
			int repo_stars = repo["stargazers_count"].toInt();

			Embed embed;
			embed.setTitle(repo_fullname);
			embed.setDescription("\nStars: " + QString::number(repo_stars) +
								"\nLanguage: " + repo_language + "\n" +
								repo_url);

			client.createMessage(m_GithubChannel, embed);
		});

	QObject::connect(m_client, &UmikoBot::onMessageReactionAdd, this, &FunModule::onReact);
	QObject::connect(m_client, &UmikoBot::onMessageReactionRemove, this, &FunModule::onUnReact);

	RegisterCommand(Commands::FUN_MEME, "meme", [this](Client& client, const Message& message, const Channel& channel) 
		{

		QStringList args = message.content().split(' ');

		if (args.size() >= 2) {
			client.createMessage(message.channelId(), "**Wrong Usage of Command!** ");
			return;
		}

		m_memeChannel = channel.id();
		m_MemeManager.get(QNetworkRequest(QUrl("https://meme-api.herokuapp.com/gimme")));

		});

	RegisterCommand(Commands::FUN_ROLL, "roll", [this](Client& client, const Message& message, const Channel& channel)
		{
			QStringList args = message.content().split(' ');

			if (args.size() < 2 || args.size() > 3)
			{
				client.createMessage(message.channelId(), "**Wrong Usage of Command!** ");
				return;
			}
			if (args.size() == 2)
			{
				double min = 0;
				double max = args.at(1).toDouble();
				QRegExp re("[+-]?\\d*\\.?\\d+");
				if (!re.exactMatch(args.at(1)))
				{
					client.createMessage(message.channelId(), "**You must roll with numbers!**");
					return;
				}
				if (max > 2147483647 || max < -2147483647)
				{
					client.createMessage(message.channelId(), "**You can't roll that number!**");
					return;
				}
				std::random_device rand_device;
				std::mt19937 gen(rand_device());

				if (max < min)
					std::swap(min, max);

				std::uniform_int_distribution<> dist(min, max);

				QString text = QString("My Value was: **" + QString::number(dist(gen)) + "**");
				client.createMessage(message.channelId(), text);
				return;
			}

			if (args.size() == 3)
			{
				double min = args.at(1).toDouble();
				double max = args.at(2).toDouble();
				QRegExp re("[+-]?\\d*\\.?\\d+");
				if (!re.exactMatch(args.at(1)) || !re.exactMatch(args.at(2)))
				{
					client.createMessage(message.channelId(), "**You must roll with numbers!**");
					return;
				}
				if (max > 2147483647 || min > 2147483647 || max < -2147483647 || min < -2147483647)
				{
					client.createMessage(message.channelId(), "**You can't roll that number!**");
					return;
				}
				if (args.at(1) == args.at(2))
				{
					client.createMessage(message.channelId(), "My Value was: **" + args.at(1) + "**");
					return;
				}

				std::random_device rand_device;
				std::mt19937 gen(rand_device());

				if (max < min)
					std::swap(min, max);

				std::uniform_int_distribution<> dist(min, max);

				QString text = QString("My Value was: **" + QString::number(dist(gen)) + "**");
				client.createMessage(message.channelId(), text);
			}
		});


	RegisterCommand(Commands::FUN_POLL, "poll", [this](Client& client, const Message& message, const Channel& channel) 
	{

		QStringList lines = message.content().split('\n');

		for (auto& line : lines) 
		{
			line = line.trimmed();
		}

		QStringList args = lines.at(0).simplified().split(' ');

		args.pop_front();	//pop the command to just get the args
		lines.pop_front();	//the first line is only used for args

		double pollTime = POLL_DEFAULT_TIME; //in seconds
		long long maxReacts = -1;
		QString pollName = "";

		bool listPolls = false;
		
		bool cancelPoll = false;
		int cancelPollIndex = -1;

		int pollNum =  m_polls[channel.guildId()] == nullptr ? 0 : m_polls[channel.guildId()]->size();

		auto parseArgs = [&pollTime, &maxReacts, &pollName, &listPolls, &cancelPoll, &cancelPollIndex](const QStringList& args, snowflake_t chan) -> bool 
		{ //bool for success
			QRegExp numReg{ "[+]?\\d*\\.?\\d+" };
			QRegExp countReg{ "[+]?\\d*" };
			
			bool newPoll = false;
			bool listPoll = false;

			for (int i = 0; i < args.size(); i++) 
			{
				auto text = args[i].toLower();
				QString next = "";
				if (i + 1 < args.size()) 
				{
					next = args[i + 1];
				}

				if (text == "--maxvotes") 
				{
					if (listPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or list the active polls?**");
						return false;
					}
					if (cancelPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or cancel an existing one?**");
						return false;
					}
					newPoll = true;
					if (next == "" || next.startsWith("--")) 
					{
						UmikoBot::Instance().createMessage(chan, "**No value for maxVotes provided. Go ahead and give me a value don't be lazy.**");
						return false;
					}
					if (!countReg.exactMatch(next)) 
					{
						UmikoBot::Instance().createMessage(chan, "**What's that value for maxVotes? The value should be a positive number!**\n");
						return false;
					}
					long long value = next.toLongLong();
					if (value == 0) 
					{
						UmikoBot::Instance().createMessage(chan, "Do you even want your poll to last?");
						return false;
					}
					maxReacts = value;
					++i;
				}
				else if (text == "--hours") 
				{
					if (listPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or list the active polls?**");
						return false;
					}
					if (cancelPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or cancel an existing one?**");
						return false;
					}
					newPoll = true;
					if (next == "" || next.startsWith("--")) 
					{
						UmikoBot::Instance().createMessage(chan, "**If you want to use the default value for hours, just go ahead and don't type --hours in.\nOh you don't want the default value? Well you gotta type what you want!**");
						return false;
					}
					if (!numReg.exactMatch(next)) 
					{
						UmikoBot::Instance().createMessage(chan, "**How long should the poll last? Please provide a beatiful number...**");
						return false;
					}
					double value = next.toDouble()*60.0*60.0;
					if (value > POLL_MAX_TIME * 60.0 * 60.0) 
					{
						UmikoBot::Instance().createMessage(chan, "**I mean I want your poll to have a *lasting effect*, but don't you think that's a looong time?**");
						return false;
					}
					if (value < POLL_MIN_TIME * 60.0 * 60.0) 
					{
						UmikoBot::Instance().createMessage(chan, "**Oh c'mon what is this tiny hour value!?**");
						return false;
					}
					pollTime = value;
					++i;
				}
				else if (text == "--name") 
				{
					if (listPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or list the active polls?**");
						return false;
					}
					if (cancelPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or cancel an existing one?**");
						return false;
					}
					newPoll = true;
					if (next == "") 
					{
						UmikoBot::Instance().createMessage(chan, "**Sorry, I didn't catch the name. Oh wait! You didn't give me one!**");
						return false;
					}
					if (next.startsWith('"')) 
					{
						//! Should add the last next?
						bool requireNext = true; 
						do 
						{
							pollName += " " + next;
							if (next.endsWith('"')) 
							{
								requireNext = false;
								break;
							}
							if (i+2 >= args.size()) 
							{
								UmikoBot::Instance().createMessage(chan, "**The string you provided never ends.**\nPlease fix that?");
								return false;
							}
							next = args[(++i) + 1];
						} 
						while (!next.endsWith('"'));
						if (requireNext) 
						{
							pollName += " " + next;
						}

						//! remove the quotations
						pollName.remove(1, 1); 
						pollName.chop(1);
					}
					else pollName = next;
					++i;
				}
				else if (text == "--list") 
				{
					if (newPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or list the active polls?**");
						return false;
					}

					if (cancelPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to cancel a poll or list the active polls?**");
						return false;
					}

					listPoll = true;
				}
				else if (text == "--cancel") 
				{
					if (newPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or cancel an existing poll?**");
						return false;
					}

					if (listPoll) 
					{
						UmikoBot::Instance().createMessage(chan, "**I am confused... Do you want to me to make a new poll or cancel an existing poll?**");
						return false;
					}

					cancelPoll = true;

					if (next == "" || next.startsWith("--")) {
						UmikoBot::Instance().createMessage(chan, "**I expect a poll number following the `--cancel` command so that I can cancel the poll.**\n||I know I am pretty awesome, but I can't (yet) read your mind...||");
						return false;
					}
					if (!numReg.exactMatch(next)) {
						UmikoBot::Instance().createMessage(chan, "**The poll number must be a non-negative number...**");
						return false;
					}

					cancelPollIndex = next.toInt();
					
					++i;
				}
				else 
				{
					UmikoBot::Instance().createMessage(chan, "**I didn't expect this string in the arguments:** " + args[i] + "\nPlease fix that?");
					return false;
				}
			}

			listPolls = listPoll;
			return true;
		};

		if (parseArgs(args, message.channelId())) 
		{
			if (!listPolls && !cancelPoll) 
			{

				client.getGuildMember(channel.guildId(), message.author().id())
				.then([=](const GuildMember& member) 
				{
					bool found = false;

					for (auto& allowedRole : m_pollWhitelist[channel.guildId()]) 
					{
						if (member.roles().contains(allowedRole)) 
						{
							found = true;
							break;
						}
					}

					if (!found) 
					{
						UmikoBot::Instance().createMessage(channel.id(), "**You're not allowed to create polls.**");
						return;
					}

					if (lines.size() == 0) 
					{
						UmikoBot::Instance().createMessage(message.channelId(), "**A poll without anything to vote for huh? C'mon you can do better!**");
						return;
					}
					if (lines.size() == 1) 
					{
						UmikoBot::Instance().createMessage(message.channelId(), "**Why would there be a poll if there's only one thing to choose?**");
						return;
					}

					PollOptions options;
					QString desc = "**Here are your options:**\n";

					QRegExp customEmote{ "<:.+:\\d+>" };
					QRegExp customAnimEmote{ "<a:.+:\\d+>" };

					for (auto& option : lines) {
						bool isAnimated = false;
						QString emoji = "";
						int index = option.indexOf(' ');
						emoji = option.mid(0, index);
						QString text = "";
						if (index != -1) text = option.mid(index).trimmed();

						desc += emoji + " : " + text + "\n";

						QString emojiData = emoji;

						if (customEmote.exactMatch(emojiData)) 
						{
							emojiData = emojiData.remove("<:");
							emojiData = emojiData.remove(">");
						}
						else if (customAnimEmote.exactMatch(emojiData)) 
						{
							isAnimated = true;
							emojiData = emojiData.remove("<a:");
							emojiData = emojiData.remove(">");
						}

						//! Check if the current emoji exists already

						for (auto& opt : options) 
						{
							if (opt.emote == emojiData) 
							{
								UmikoBot::Instance().createMessage(message.channelId(), "**You used this reaction choice multiple times:** " + emoji);
								return;
							}
						}

						PollOption opt;
						opt.emote = emojiData;
						opt.desc = text;
						opt.isAnimated = isAnimated;
						options.push_back(opt);
					}

					if (options.length() > 10) 
					{
						UmikoBot::Instance().createMessage(message.channelId(), "**Woah there! That's a lot of options for me to handle!**");
						return;
					}

					desc += "\n**Go ahead! Vote now before the poll ends!**";
					Embed embed;
					embed.setColor(qrand() % 16777216);
					embed.setTitle("Poll#" + QString::number(pollNum) + " " + pollName);
					embed.setDescription(desc);

					snowflake_t guild = channel.guildId();

					UmikoBot::Instance()
					.createMessage(message.channelId(), embed)
					.then(
					[this, pollNum, options, maxReacts, pollName,
						pollTime, guild](const Message& msg) 
					{
						int pos = 0;
						if (m_polls[guild] == nullptr)
							m_polls[guild] = std::make_shared<QList<Poll>>();

						//! Make a new entry for this poll
						PollOptions tempOptions;
						auto poll = std::make_shared<PollSettings>(tempOptions, maxReacts, msg.channelId(), pollName, pollNum, pollTime, m_polls[guild], msg.id());

						m_polls[guild]->push_back(poll);

						//! This is responsible for adding reactions and adding them into the pollOptions
						pollReactAndAdd(options, pos, poll, msg.id(), msg.channelId(), guild);

					});

				});
				
			}
			else if (cancelPoll) 
			{
				auto& serverPolls = m_polls[channel.guildId()];
				if (serverPolls != nullptr && !serverPolls->isEmpty()) 
				{
					bool found = false;

					for (int i = 0; i < serverPolls->size(); i++) {
						if (serverPolls->at(i)->pollNum == cancelPollIndex) {
							auto poll_num = serverPolls->at(i)->pollNum;
							auto poll_name = serverPolls->at(i)->pollName;
							auto notif_chan = serverPolls->at(i)->notifChannel;
							auto poll_msg = serverPolls->at(i)->pollMsg;
							serverPolls->removeAt(i);
							found = true;
							Embed embed;
							embed.setColor(qrand() % 16777216);
							embed.setTitle("Poll#" + QString::number(poll_num) + " " + poll_name);
							embed.setDescription("This poll has been cancelled.");
							MessagePatch patch;
							patch.setEmbed(embed);
							UmikoBot::Instance().deleteAllReactions(notif_chan, poll_msg);
							UmikoBot::Instance().editMessage(notif_chan, poll_msg, patch).then([notif_chan, poll_num](const Message&) {
								UmikoBot::Instance().createMessage(notif_chan, "**Poll#" + QString::number(poll_num)+ " has been cancelled.**");
							});
							break;
						}
					}

					if (!found) 
					{
						client.createMessage(channel.id(), "**No active poll with the provided number exists.**");
					}
				}
				else 
				{
					client.createMessage(channel.id(), "**There are no active polls to cancel.**");
				}
			}
			else 
			{

				if (lines.size() != 0) {
					client.createMessage(message.channelId(), "**I'm curious; why did you pass me more lines if you just wanted to list the active polls?**");
					return;
				}

				Embed embed;
				embed.setColor(qrand() % 16777216);
				embed.setTitle("List of active polls");
				QList<EmbedField> fields;

				auto& serverPolls = m_polls[channel.guildId()];

				if (serverPolls != nullptr && !serverPolls->isEmpty()) 
				{
					for (auto& poll : *serverPolls) {

						std::size_t total = 0;
						for (auto& opt : poll->options) {
							total += opt.count;
						}

						EmbedField field;
						field.setName("Poll#"
							+ QString::number(poll->pollNum)
							+ (poll->pollName == "" ? "" : " " + poll->pollName));
						field.setValue("**Remaining Time: `"
							+ utility::StringifyMilliseconds(poll->timer->remainingTime(), utility::StringMSFormat::MINIMAL) + "`**\n" + "**Total Votes:** `" + QString::number(total) + "`" + "\n[Link](https://discordapp.com/channels/" + QString::number(channel.guildId()) + "/" + QString::number(poll->notifChannel) + "/" + QString::number(poll->pollMsg) + ")");

						fields.push_back(field);
					}
				}
				else 
				{
					embed.setDescription("No polls active.");
					client.createMessage(channel.id(), embed);
					return;
				}

				embed.setFields(fields);

				client.createMessage(channel.id(), embed);
			}
		}

	});

	RegisterCommand(Commands::FUN_GITHUB, "github", [this](Client& client, const Message& message, const Channel& channel) 
	{
		QStringList args = message.content().split(' ');

		m_GithubChannel = channel.id();

		if(args.size() != 1)
		{
			UmikoBot::Instance().createMessage(m_GithubChannel, "**Wrong Usage of Command!**");

			return;
		}

		// For extra randomness
		std::random_device device;
		std::mt19937 rng(device());
		std::uniform_int_distribution<std::mt19937::result_type> ch('A', 'Z');

		m_GithubManager.get(QNetworkRequest(QUrl("https://api.github.com/search/repositories?q=" + QString(QChar((char)ch(rng))))));
	});

	RegisterCommand(Commands::FUN_GIVE_NEW_POLL_ACCESS, "give-new-poll-access", [this](Client& client, const Message& message, const Channel& channel) 
	{
		QStringList args = message.content().split(' ');

		::Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::MODERATOR, [this, args, message, channel](bool result) 
		{
			GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
			if (!result) 
			{
				UmikoBot::Instance().createMessage(message.channelId(), "**You don't have permissions to use this command.**");
				return;
			}
			

			if (args.size() < 2) 
			{
				UmikoBot::Instance().createMessage(channel.id(), "**Wrong Usage of Command!**");
				return;
			}

			snowflake_t guild = channel.guildId();
			snowflake_t chan = channel.id();
			
			auto& roles = message.mentionRoles();

			if (roles.isEmpty()) {
				UmikoBot::Instance().createMessage(channel.id(), "**Wrong Usage of Command!\n**Please mention some role(s) so that I can do the needed.");
				return;
			}

			for (auto& role : roles) {
				if (m_pollWhitelist[guild].contains(role)) continue;
				m_pollWhitelist[guild].push_back(role);
			}

			UmikoBot::Instance().createMessage(channel.id(), "**The roles have been added.**\nPeople with the role(s) can now create new polls!");

		});
		
	});
	
	RegisterCommand(Commands::FUN_TAKE_NEW_POLL_ACCESS, "take-new-poll-access", [this](Client& client, const Message& message, const Channel& channel) 
	{
		QStringList args = message.content().split(' ');

		::Permissions::ContainsPermission(client, channel.guildId(), message.author().id(), CommandPermission::MODERATOR, [this, args, message, channel](bool result) 
		{
			GuildSetting* setting = &GuildSettings::GetGuildSetting(channel.guildId());
			if (!result) 
			{
				UmikoBot::Instance().createMessage(message.channelId(), "**You don't have permissions to use this command.**");
				return;
			}

			if (args.size() < 2) {
				UmikoBot::Instance().createMessage(channel.id(), "**Wrong Usage of Command!**");
				return;
			}

			snowflake_t guild = channel.guildId();
			snowflake_t chan = channel.id();

			auto& roles = message.mentionRoles();

			if (roles.isEmpty()) 
			{
				UmikoBot::Instance().createMessage(channel.id(), "**Wrong Usage of Command!\n**Please mention some role(s) so that I can do the needed.");
				return;
			}

			for (auto& role : roles) 
			{
				m_pollWhitelist[guild].removeOne(role);
			}

			UmikoBot::Instance().createMessage(channel.id(), "**The roles have been removed.**\nPeople with the role(s) can no longer create new polls.");

		});

	});
}

void FunModule::onReact(snowflake_t user, snowflake_t channel, snowflake_t message, const Emoji& emoji) const 
{
	auto polls = m_polls;
	UmikoBot::Instance().getChannel(channel).then([user, channel, message, emoji, polls](const Channel& chan) 
	{
		auto guild = chan.guildId();
		if (polls[guild] != nullptr) 
		{
			for (auto poll : *polls[guild]) 
			{
				if (poll->pollMsg == message) 
				{
					UmikoBot::Instance().getUser(user).then([channel, message, emoji, poll](const User& user) 
					{
						if (!user.bot()) 
						{
							QString emojiStr = utility::stringifyEmoji(emoji);
							bool found = false;
							for (auto& option : poll->options) 
							{
								if (option.emote == emojiStr) 
								{
									found = true;
									option.count++;
									break;
								}
							}
							if (!found) 
							{
								//This reaction wasn't one of the choices
								UmikoBot::Instance().deleteUserReaction(channel, message, emojiStr, user.id());
								return;
							}
							if (poll->maxVotes != -1) 
							{
								std::size_t total = 0;
								for (auto it = poll->options.begin(); it != poll->options.end(); ++it) 
								{
									total += it->count;
								}
								if (total == poll->maxVotes) 
								{
									//! Manually invoke timeout for the poll timer
									poll->timer->QTimer::qt_metacall(QMetaObject::InvokeMetaMethod, 5, {});
								}
							}
						}
					});
				}
			}
		}

	});

}

void FunModule::onUnReact(snowflake_t user, snowflake_t channel, snowflake_t message, const Emoji& emoji) const 
{
	auto polls = m_polls;
	UmikoBot::Instance().getChannel(channel)
		.then([user, channel, message, emoji, polls](const Channel& chan) 
	{
		auto guild = chan.guildId();
		if (polls[guild] != nullptr) 
		{
			for (auto poll : *polls[guild]) 
			{
				if (poll->pollMsg == message) 
				{
					UmikoBot::Instance().getUser(user).then([channel, message, emoji, poll](const User& user) 
					{
						if (!user.bot()) 
						{
							QString emojiStr = utility::stringifyEmoji(emoji);
							for (auto& option : poll->options) 
							{
								if (option.emote == emojiStr) 
								{
									option.count--;
									break;
								}
							}
						}
					});
				}
			}
		}

	});
}

void FunModule::OnMessage(Client& client, const Message& message) 
{
	for (auto& usr : message.mentions()) 
	{
		if (usr.bot()) 
		{
			snowflake_t chan = message.channelId();
			UmikoBot::Instance().createReaction(chan, message.id(), utility::consts::emojis::reacts::ANGRY_PING)
				.then([chan]
			{
				UmikoBot::Instance().triggerTypingIndicator(chan);
			});
			return;
		}
	}

	Module::OnMessage(client, message);
}

void FunModule::pollReactAndAdd(const PollOptions& options, int pos, const Poll& poll, snowflake_t msg, snowflake_t chan, snowflake_t guild) 
{

	if (pos == options.size()) return;

	UmikoBot::Instance()
		.createReaction(chan, msg, options.at(pos).emote)
		.then([this, pos, options, poll, msg, chan, guild]
		{
			//! Add the reaction if success
			poll->options.push_back(options.at(pos));

			//! Continue with the next reaction
			pollReactAndAdd(options, pos + 1, poll, msg, chan, guild);

		})
		.otherwise([this, pos, options, poll, msg, chan, guild]() {
			//! Remove the poll from the pending list if we couldn't 
			//! add all reactions
			for (int i = 0; i < m_polls[guild]->size(); i++) {
				if (m_polls[guild]->at(i)->pollNum == i) {
					m_polls[guild]->removeAt(i);
					break;
				}
			}
			UmikoBot::Instance().deleteMessage(chan, msg);
			UmikoBot::Instance().createMessage(chan, "**I'm sorry, but one (or more) of the vote options wasn't a reaction.**\nYou might wanna fix that. ||Unless you were trolling, and if that's the case, haha! I am well built!||");
				return;
		});
}

void FunModule::OnSave(QJsonDocument& doc) const 
{
	QJsonObject docObj;

	for (auto& server : m_pollWhitelist.keys()) 
	{
		QJsonObject serverJSON;

		QJsonArray list;
		
		for (auto& roleId : m_pollWhitelist[server]) 
		{
			list.push_back(QString::number(roleId));
		}

		serverJSON["poll-whitelist"] = list;

		docObj[QString::number(server)] = serverJSON;
	}

	doc.setObject(docObj);
}

void FunModule::OnLoad(const QJsonDocument& doc) 
{
	m_pollWhitelist.clear();

	auto docObj = doc.object();

	auto servers = docObj.keys();

	for (auto& server : servers) 
	{
		snowflake_t guild = server.toULongLong();
		auto serverObj = docObj[server].toObject();
		auto list = serverObj["poll-whitelist"].toArray();
		
		for (auto role : list) 
		{
			snowflake_t roleId = role.toString().toULongLong();
			m_pollWhitelist[guild].push_back(roleId);
		}
	}
}
