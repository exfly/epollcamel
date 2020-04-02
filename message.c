#include <string.h>
#include "message.h"

Message message_builder(MessageType type, char* body) {

    int len = strlen(body);

    Message msg;
    msg.type = type;
    strncpy(msg.body, body, len);
    msg.body[len] = '\0';
    return msg;
}
