/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_message.c - Error message handling functions
 * Copyright 2004-2015 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufraw.h"
#include <string.h>

/*
 * Every ufraw internal function that might fail should return a status
 * with one of these values:
 * UFRAW_SUCCESS
 * UFRAW_WARNING
 * UFRAW_ERROR
 * The relevant message can be retrived using ufraw_get_message().
 * Even when UFRAW_SUCCESS is returned there could be an information message.
 */
char *ufraw_get_message(ufraw_data *uf)
{
    return uf->message;
}

static void message_append(ufraw_data *uf, char *message)
{
    if (message == NULL) return;
    if (uf->message == NULL) {
        uf->message = g_strdup(message);
        return;
    }
    if (uf->message[strlen(uf->message) - 1] == '\n')
        uf->message = g_strconcat(uf->message, message, NULL);
    else
        uf->message = g_strconcat(uf->message, "\n", message, NULL);
}


/* The following function should be used by ufraw's internal functions. */
void ufraw_message_init(ufraw_data *uf)
{
    uf->status = UFRAW_SUCCESS;
    uf->message = NULL;
}

void ufraw_message_reset(ufraw_data *uf)
{
    uf->status = UFRAW_SUCCESS;
    g_free(uf->message);
    uf->message = NULL;
}

void ufraw_set_error(ufraw_data *uf, const char *format, ...)
{
    uf->status = UFRAW_ERROR;
    if (format != NULL) {
        va_list ap;
        va_start(ap, format);
        char *message = g_strdup_vprintf(format, ap);
        va_end(ap);
        message_append(uf, message);
        g_free(message);
    }
}

void ufraw_set_warning(ufraw_data *uf, const char *format, ...)
{
    // Set warning only if no error was set before
    if (uf->status != UFRAW_ERROR) uf->status = UFRAW_WARNING;
    if (format != NULL) {
        va_list ap;
        va_start(ap, format);
        char *message = g_strdup_vprintf(format, ap);
        va_end(ap);
        message_append(uf, message);
        g_free(message);
    }
}

void ufraw_set_info(ufraw_data *uf, const char *format, ...)
{
    if (format != NULL) {
        va_list ap;
        va_start(ap, format);
        char *message = g_strdup_vprintf(format, ap);
        va_end(ap);
        message_append(uf, message);
        g_free(message);
    }
}

int ufraw_get_status(ufraw_data *uf)
{
    return uf->status;
}

int ufraw_is_error(ufraw_data *uf)
{
    return uf->status == UFRAW_ERROR;
}

// Old error handling, should be removed after being fully implemented.

static char *ufraw_message_buffer(char *buffer, char *message)
{
#ifdef UFRAW_DEBUG
    ufraw_batch_messenger(message);
#endif
    char *buf;
    if (buffer == NULL) return g_strdup(message);
    buf = g_strconcat(buffer, message, NULL);
    g_free(buffer);
    return buf;
}

void ufraw_batch_messenger(char *message)
{
    /* Print the 'ufraw:' header only if there are no newlines in the message
     * (not including possibly one at the end).
     * Otherwise, the header will be printed only for the first line. */
    if (g_strstr_len(message, strlen(message) - 1, "\n") == NULL)
        g_printerr("%s: ", ufraw_binary);
    g_printerr("%s%c", message, message[strlen(message) - 1] != '\n' ? '\n' : 0);
}

char *ufraw_message(int code, const char *format, ...)
{
    // TODO: The following static variables are not thread-safe
    static char *logBuffer = NULL;
    static char *errorBuffer = NULL;
    static gboolean errorFlag = FALSE;
    static void *parentWindow = NULL;
    char *message = NULL;
    void *saveParentWindow;

    if (code == UFRAW_SET_PARENT) {
        saveParentWindow = parentWindow;
        parentWindow = (void *)format;
        return saveParentWindow;
    }
    if (format != NULL) {
        va_list ap;
        va_start(ap, format);
        message = g_strdup_vprintf(format, ap);
        va_end(ap);
    }
    switch (code) {
        case UFRAW_SET_ERROR:
            errorFlag = TRUE;
        case UFRAW_SET_WARNING:
            errorBuffer = ufraw_message_buffer(errorBuffer, message);
        case UFRAW_SET_LOG:
        case UFRAW_DCRAW_SET_LOG:
            logBuffer = ufraw_message_buffer(logBuffer, message);
            g_free(message);
            return NULL;
        case UFRAW_GET_ERROR:
            if (!errorFlag) return NULL;
        case UFRAW_GET_WARNING:
            return errorBuffer;
        case UFRAW_GET_LOG:
            return logBuffer;
        case UFRAW_CLEAN:
            g_free(logBuffer);
            logBuffer = NULL;
        case UFRAW_RESET:
            g_free(errorBuffer);
            errorBuffer = NULL;
            errorFlag = FALSE;
            return NULL;
        case UFRAW_BATCH_MESSAGE:
            if (parentWindow == NULL)
                ufraw_messenger(message, parentWindow);
            g_free(message);
            return NULL;
        case UFRAW_INTERACTIVE_MESSAGE:
            if (parentWindow != NULL)
                ufraw_messenger(message, parentWindow);
            g_free(message);
            return NULL;
        case UFRAW_REPORT:
            ufraw_messenger(errorBuffer, parentWindow);
            return NULL;
        default:
            ufraw_messenger(message, parentWindow);
            g_free(message);
            return NULL;
    }
}
