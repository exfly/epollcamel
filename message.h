#ifndef _CAMEL_MESSAGE_H_INCLUDED_
#define _CAMEL_MESSAGE_H_INCLUDED_

typedef enum MessageType
{
    CMD,
    MSG
} MessageType;

typedef struct Message
{
    MessageType type;
    char body[255];
} Message;

Message message_builder(MessageType type, char* body);
#endif
