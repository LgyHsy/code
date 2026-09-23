#include "eventhub.h"

static EventHub *hub_table[EVENTHUB_CLASS_MAX];

static EventHub *eventhub_create(void)
{
    EventHub *hub = malloc(sizeof(EventHub));
    if (!hub)
        return NULL;

    hub->subscriptions = NULL;
    hub->event_count = 0;
    return hub;
}

static void eventhub_destroy(EventHub *hub)
{
    if (!hub)
        return;

    EventSubscription *current = hub->subscriptions;
    while (current)
    {
        EventSubscription *next = current->next;
        free(current);
        current = next;
    }

    free(hub);
}

void eventhub_subscribe(EventHubClass cls, const char *event_name, EventHandler handler)
{
    if (cls >= EVENTHUB_CLASS_MAX || !event_name || !handler)
        return;

    EventHub *hub = hub_table[cls];
    if (!hub)
        return;

    EventSubscription *sub = malloc(sizeof(EventSubscription));
    if (!sub)
        return;

    sub->event_name = event_name;
    sub->handler = handler;
    sub->next = hub->subscriptions;
    hub->subscriptions = sub;
    hub->event_count++;
}

void eventhub_publish(EventHubClass cls, const char *event_name, EventResult *event_result, void *data)
{
    if (cls >= EVENTHUB_CLASS_MAX || !event_name)
        return;

    EventHub *hub = hub_table[cls];
    if (!hub)
        return;

    EventSubscription *current = hub->subscriptions;
    while (current)
    {
        if (strcmp(current->event_name, event_name) == 0)
        {
            current->handler(event_result, data);
        }
        current = current->next;
    }
}

bool eventhub_unsubscribe(EventHubClass cls, const char *event_name, EventHandler handler)
{
    if (cls >= EVENTHUB_CLASS_MAX || !event_name || !handler)
        return false;

    EventHub *hub = hub_table[cls];
    if (!hub)
        return false;

    EventSubscription **ptr = &hub->subscriptions;
    bool found = false;

    while (*ptr)
    {
        if ((*ptr)->handler == handler && strcmp((*ptr)->event_name, event_name) == 0)
        {
            EventSubscription *to_remove = *ptr;
            *ptr = (*ptr)->next;
            free(to_remove);
            hub->event_count--;
            found = true;
        }
        else
        {
            ptr = &(*ptr)->next;
        }
    }

    return found;
}

void eventhub_ctrl_init(void)
{
    if (hub_table[EVENTHUB_CLASS_CTRL] == NULL)
        hub_table[EVENTHUB_CLASS_CTRL] = eventhub_create();
}

void eventhub_status_init(void)
{
    if (hub_table[EVENTHUB_CLASS_STATUS] == NULL)
        hub_table[EVENTHUB_CLASS_STATUS] = eventhub_create();
}

void eventhub_media_init(void)
{
    if (hub_table[EVENTHUB_CLASS_MEDIA] == NULL)
        hub_table[EVENTHUB_CLASS_MEDIA] = eventhub_create();
}

void eventhub_init(void)
{
    eventhub_ctrl_init();
    eventhub_status_init();
    eventhub_media_init();
}

void eventhub_ctrl_uninit(void)
{
    if (hub_table[EVENTHUB_CLASS_CTRL])
    {
        eventhub_destroy(hub_table[EVENTHUB_CLASS_CTRL]);
        hub_table[EVENTHUB_CLASS_CTRL] = NULL;
    }
}

void eventhub_status_uninit(void)
{
    if (hub_table[EVENTHUB_CLASS_STATUS])
    {
        eventhub_destroy(hub_table[EVENTHUB_CLASS_STATUS]);
        hub_table[EVENTHUB_CLASS_STATUS] = NULL;
    }
}

void eventhub_media_uninit(void)
{
    if (hub_table[EVENTHUB_CLASS_MEDIA])
    {
        eventhub_destroy(hub_table[EVENTHUB_CLASS_MEDIA]);
        hub_table[EVENTHUB_CLASS_MEDIA] = NULL;
    }
}

void eventhub_uninit(void)
{
    eventhub_ctrl_uninit();
    eventhub_status_uninit();
    eventhub_media_uninit();
}
