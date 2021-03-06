#include "_ft_internal.h"

///

const char * FT_PUBSUB_TOPIC_EXIT = "$SYS/ft/exit";
const char * FT_PUBSUB_TOPIC_HEARTBEAT = "$SYS/ft/heartbeat";
const char * FT_PUBSUB_TOPIC_POOL_LOWMEM = "$SYS/ft/pool/lowmem";

///

bool ft_pubsub_init(struct ft_pubsub * this)
{
	assert(this != NULL);
	this->subscribers = NULL;
	return true;
}

void ft_pubsub_fini(struct ft_pubsub * this)
{
	assert(this != NULL);
}

bool ft_pubsub_publish(struct ft_pubsub * this, const char * topic, void * data)
{
	if (this == NULL)
	{
		if (ft_context_default != NULL)
		{
			this = &ft_context_default->pubsub;
		}
		if (this == NULL)
		{
			FT_WARN("Default context is not set!");
			return false;
		}
	}

	for (struct ft_subscriber * s = this->subscribers; s != NULL; s = s->next)
	{
		if ((s->topic != topic) || (s->callback == NULL)) continue;

		s->stats.called += 1;
		if (s->callback != NULL) s->callback(s, this, topic, data);
	}

	return true;
}

///

bool ft_subscriber_init(struct ft_subscriber * this, ft_subscriber_cb callback)
{
	assert(this != NULL);
	this->pubsub = NULL;
	this->next = NULL;
	this->callback = callback;
	this->topic = NULL;
	this->stats.called = 0;
	return true;
}

void ft_subscriber_fini(struct ft_subscriber * this)
{
	assert(this != NULL);

	if (this->pubsub != NULL)
		ft_subscriber_unsubscribe(this);

	assert(this->pubsub == NULL);
	assert(this->next == NULL);
}

bool ft_subscriber_subscribe(struct ft_subscriber * this, struct ft_pubsub * pubsub, const char * topic)
{
	assert(this != NULL);
	assert(topic != NULL);

	if (pubsub == NULL)
	{
		if (ft_context_default != NULL)
		{
			pubsub = &ft_context_default->pubsub;
		}
		if (pubsub == NULL)
		{
			FT_WARN("Default context is not set!");
			return false;
		}
	}

	if ((this->topic != NULL) || (this->pubsub != NULL))
	{
		FT_WARN("Already subscribed to topic '%s'", this->topic);
		return false;
	}
	assert(this->next == NULL);

	// Append at the end of the list
	for(struct ft_subscriber ** p = &pubsub->subscribers;; p = &(*p)->next)
	{
		if (*p == NULL)
		{
			*p = this;
			this->topic = topic;
			break;
		}
	}
	assert(this->topic != NULL);

	this->pubsub = pubsub;

	return true;
}

bool ft_subscriber_unsubscribe(struct ft_subscriber * this)
{
	assert(this != NULL);

	if ((this->topic == NULL) || (this->pubsub == NULL))
	{
		FT_WARN("Not subscribed");
		return false;
	}

	for(struct ft_subscriber ** p = &this->pubsub->subscribers; *p != NULL; p = &(*p)->next)
	{
		if (*p != this) continue;

		*p = this->next;
		goto good;
	}

	FT_WARN("Subscriber not found in pubsub!");
	return false;

good:
	this->next = NULL;
	this->topic = NULL;
	this->pubsub = NULL;

	return true;
}
