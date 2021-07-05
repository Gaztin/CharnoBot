#pragma once

#include "core/Module.h"

class ModerationModule: public Module 
{
public:
	ModerationModule();

	void OnMessage(Discord::Client& client, const Discord::Message& message) override;

	void OnSave(QJsonDocument& doc) const override;
	void OnLoad(const QJsonDocument& doc) override;

private:
	bool m_invitationModeration = true;
	QTimer m_warningCheckTimer;

	struct UserWarning
	{
		snowflake_t warnedBy;
		QDateTime when;
		QString message;
		bool expired;

		UserWarning(snowflake_t warnedBy, QString message)
			: warnedBy(warnedBy), when(QDateTime::currentDateTime()), message(message), expired(false)
		{
		}

		UserWarning(snowflake_t warnedBy, QDateTime when, QString message, bool expired)
			: warnedBy(warnedBy), when(when), message(message), expired(expired)
		{
		}
	};

	// Maps user ID to a list of warnings
	QMap<snowflake_t, QList<UserWarning>> warnings;

	unsigned int countWarnings(snowflake_t user, bool countExpired = false);
	void checkWarningsExpiry();
};
