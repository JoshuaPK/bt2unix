
// JPK 03-JUL-2015
// http://www.spinics.net/lists/bluez-devel/msg00109.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <glib.h>
#include <dbus/dbus.h>

#define AGENT_PATH "/agent_sample"

const char* introspect_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\";>\n"
"<node>\n"
"   <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"       <method name=\"Introspect\">\n"
"           <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"       </method>\n"
"   </interface>\n"
"   <interface name=\"org.bluez.Agent\">\n"
"       <method name=\"Release\">\n"
"       </method>\n"
"       <method name=\"RequestPasskey\">\n"
"           <arg type=\"s\" direction=\"out\"/>\n"
"           <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
"       </method>\n"
"       <method name=\"Authorize\">\n"
"           <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
"           <arg name=\"uuid\" type=\"s\" direction=\"in\"/>\n"
"       </method>\n"
"       <method name=\"ConfirmModeChange\">\n"
"           <arg name=\"mode\" type=\"s\" direction=\"in\"/>\n"
"       </method>\n"
"       <method name=\"Cancel\">\n"
"       </method>\n"
"   </interface>\n"
"</node>\n";

static GMainLoop *main_loop = NULL;

static void sig_term(int sig)
{
    g_main_loop_quit(main_loop);
}

static void unregister(DBusConnection *connection, void *user_data)
{
}

static DBusHandlerResult error_message(DBusConnection *conn,
            DBusMessage *msg, char *name, char *str)
{
    DBusMessage *reply;

    reply = dbus_message_new_error(msg, name, str);

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult introspect(DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply;

    reply = dbus_message_new_method_return(msg);
    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &introspect_xml,
            DBUS_TYPE_INVALID);

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult generic_message(DBusConnection *conn,
                        DBusMessage *msg)
{
    DBusMessage *reply;

    reply = dbus_message_new_method_return(msg);
    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult confirm_mode_message(DBusConnection *conn,
                            DBusMessage *msg)
{
    DBusError derr;
    const char *mode;
    char response[17];

    dbus_error_init(&derr);
    if (!dbus_message_get_args(msg, &derr,
                DBUS_TYPE_STRING, &mode,
                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "%s", derr.message);
        dbus_error_free(&derr);
        return  error_message(conn, msg, "org.bluez.Error.Rejected",
                        "Wrong signature");
    }

    printf("Confirm mode change: %s (y/n)\n", mode);
    scanf("%16s", response);

    if (response[0] != 'y')
        return  error_message(conn, msg, "org.bluez.Error.Rejected",
                        "Not Authorized");

    return generic_message(conn, msg);
}

static DBusHandlerResult request_passkey_message(DBusConnection *conn,
                            DBusMessage *msg)
{
    DBusMessage *reply;
    DBusError derr;
    const char *device;
    char passkey[17];
    const char *psk = passkey;

    reply = dbus_message_new_method_return(msg);
    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    dbus_error_init(&derr);
    if (!dbus_message_get_args(msg, &derr,
                DBUS_TYPE_OBJECT_PATH, &device,
                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "%s", derr.message);
        dbus_error_free(&derr);
        return  error_message(conn, msg, "org.bluez.Error.Rejected",
                    "Wrong signature");
    }

    if (device)
        printf("Device: %s\n", device);

    memset(passkey, 0, sizeof(passkey));
    printf("Insert passkey >> ");
    scanf("%16s", passkey);

    dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &psk,
                DBUS_TYPE_INVALID);

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult authorize_message(DBusConnection *conn,
                        DBusMessage *msg)
{
    DBusError derr;
    const char *device, *uuid;
    char response[16];

    dbus_error_init(&derr);
    if (!dbus_message_get_args(msg, &derr,
                DBUS_TYPE_OBJECT_PATH, &device,
                DBUS_TYPE_STRING, &uuid,
                DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Wrong reply signature: %s", derr.message);
        dbus_error_free(&derr);
        return  error_message(conn, msg, "org.bluez.Error.Rejected",
                    "Wrong reply signature");
    }

    printf("Device: %s\nUUID: %s\n Authorize device? (y/n) ", device, uuid);

    scanf("%15s", response);

    if (response[0] != 'y')
        return  error_message(conn, msg,
                "org.bluez.Error.Rejected", "Not Authorized");

    return generic_message(conn, msg);
}

static DBusHandlerResult message_handler(DBusConnection *conn,
                DBusMessage *msg, void *user_data)
{
    const char *method = dbus_message_get_member(msg);
    const char *iface = dbus_message_get_interface(msg);

    if ((strcmp("Introspect", method) == 0) &&
        (strcmp("org.freedesktop.DBus.Introspectable", iface) == 0))
        return introspect(conn, msg);

    if (strcmp("org.bluez.Agent", iface) != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp("Release", method) == 0) {
        g_main_loop_quit(main_loop);
        return generic_message(conn, msg);
    }
    else if (strcmp("RequestPasskey", method) == 0)
        return request_passkey_message(conn, msg);

    else if (strcmp("Authorize", method) == 0)
        return authorize_message(conn, msg);

    else if (strcmp("ConfirmModeChange", method) == 0)
        return confirm_mode_message(conn, msg);

    else if (strcmp("Cancel", method) == 0)
        return generic_message(conn, msg);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable agent_table = {
        .unregister_function    = unregister,
        .message_function   = message_handler,
};

static int register_agent(DBusConnection *conn, char* adapter_path)
{
    DBusMessage* msg;
    char *param = AGENT_PATH;

    msg = dbus_message_new_method_call("org.bluez", adapter_path,
                    "org.bluez.Adapter", "RegisterAgent");
    if (!msg)
        return -ENOMEM;

    if (!dbus_message_append_args(msg,
            DBUS_TYPE_OBJECT_PATH, &param,
            DBUS_TYPE_INVALID))
        goto fail;

    if (!dbus_connection_send(conn, msg, NULL))
        goto fail;

    dbus_message_unref(msg);
    return 0;

fail:
    dbus_message_unref(msg);
    return -ENOMEM;
}

int main(int argc, char *argv[])
{
    DBusConnection *connection;
    DBusError derr;

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sig_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    main_loop = g_main_loop_new(NULL, FALSE);

    dbus_error_init(&derr);
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &derr);
    if (connection == NULL) {
        fprintf(stderr, "Failed to open connection: %s\n",
                            derr.message);
        dbus_error_free(&derr);
        exit(EXIT_FAILURE);
    }

    if (!dbus_connection_register_object_path(connection,
                    AGENT_PATH, &agent_table, NULL)) {
        dbus_connection_unref(connection);
        exit(EXIT_FAILURE);
    }

    if (register_agent(connection, "/hci0") < 0) {
        fprintf(stderr, "Failed to register agent: /hci0\n");
        dbus_connection_unref(connection);
        exit(EXIT_FAILURE);
    }

    dbus_connection_setup_with_g_main(connection, NULL);
    g_main_loop_run(main_loop);
    dbus_connection_unref(connection);

    return 0;
}

