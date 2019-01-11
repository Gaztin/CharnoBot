#pragma once
#include "core/Module.h"

class ModerationModule : public Module
{
public:
	ModerationModule();

	void OnMessage(Discord::Client& client, const Discord::Channel& channel, const Discord::Message& message) override;

private:
	void OnSave(QJsonDocument& doc) const override;
	void OnLoad(const QJsonDocument& doc) override;

	struct BannedKeyword
	{
		QString text;
		int deleteMessageDays;
	};

	struct Settings
	{
		Settings() : bannedKeywordMinLength(8) {}

		QList<BannedKeyword> bannedKeywords;
		int bannedKeywordMinLength;
	};

	QMap<snowflake_t, Settings> m_settings;
};