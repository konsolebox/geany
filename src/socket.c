/*
 *      socket.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2006 Hiroyuki Yamamoto (author of Sylpheed)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Socket setup and messages handling.
 * The socket allows detection and messages to be sent to the first running instance of Geany.
 * Only the first instance loads session files at startup, and opens files from the command-line.
 */

/*
 * Little dev doc:
 * Each command which is sent between two instances (see send_open_command and
 * socket_lock_input_cb) should have the following scheme:
 * command name\n
 * data\n
 * data\n
 * ...
 * .\n
 * The first thing should be the command name followed by the data belonging to the command and
 * to mark the end of data send a single '.'. Each message should be ended with \n.
 * The command window is only available on Windows and takes no additional data, instead it
 * writes back a Windows handle (HWND) for the main window to set it to the foreground (focus).
 *
 * At the moment the commands window, doclist, open, openro, line and column are available.
 *
 * About the socket files on Unix-like systems:
 * Geany creates a socket in /tmp (or any other directory returned by g_get_tmp_dir()) and
 * a symlink in the current configuration to the created socket file. The symlink is named
 * geany_socket_<hostname>_<displayname> (displayname is the name of the active X display).
 * If the socket file cannot be created in the temporary directory, Geany creates the socket file
 * directly in the configuration directory as a fallback.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SOCKET

#include "socket.h"

#include "app.h"
#include "dialogs.h"
#include "document.h"
#include "encodings.h"
#include "main.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"
#include "win32.h"

#include "gtkcompat.h"

#ifndef G_OS_WIN32
# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <glib/gstdio.h>
#else
# include <winsock2.h>
# include <windows.h>
# include <gdk/gdkwin32.h>
# include <ws2tcpip.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#ifdef G_OS_WIN32
#define REMOTE_CMD_PORT		49876
#define SOCKET_IS_VALID(s)	((s) != INVALID_SOCKET)
#else
#define SOCKET_IS_VALID(s)	((s) >= 0)
#define INVALID_SOCKET		(-1)
#endif
#define BUFFER_LENGTH 4096

struct SocketInfo socket_info;

#ifdef G_OS_WIN32
static gint socket_fd_connect_inet	(gushort port);
static gint socket_fd_open_inet		(gushort port);
static void socket_init_win32		(void);
#else
static gint socket_fd_connect_unix	(const gchar *path);
static gint socket_fd_open_unix		(const gchar *path);
#endif

static gint socket_fd_write			(gint sock, const gchar *buf, gint len);
static gint socket_fd_write_all		(gint sock, const gchar *buf, gint len);
static gint socket_fd_write_cstring (gint fd, const gchar *buf);
static gint socket_fd_write_eot     (gint fd);
static gint socket_fd_get_cstring	(gint sock, gchar *buf, gint len);
static gint socket_fd_check_io		(gint fd, GIOCondition cond);
static gint socket_fd_read			(gint sock, gchar *buf, gint len);
static gint socket_fd_recv			(gint fd, gchar *buf, gint len, gint flags);
static gint socket_fd_close			(gint sock);

static gboolean is_eot              (const gchar *str);

static void send_open_command(gint sock, gint argc, gchar **argv)
{
	gint i;
	gboolean erred = FALSE;

	g_return_if_fail(argc > 1 || cl_options.new_instance_mode == NEW_INSTANCE_EXPLICITLY_DISABLED);
	geany_debug("using running instance of Geany");

	if (argc > 1)
	{
		if (cl_options.goto_line >= 0)
		{
			gchar *line = g_strdup_printf("%d", cl_options.goto_line);
			socket_fd_write_cstring(sock, "line");
			socket_fd_write_cstring(sock, line);
			socket_fd_write_eot(sock);
			g_free(line);
		}

		if (cl_options.goto_column >= 0)
		{
			gchar *col = g_strdup_printf("%d", cl_options.goto_column);
			socket_fd_write_cstring(sock, "column");
			socket_fd_write_cstring(sock, col);
			socket_fd_write_eot(sock);
			g_free(col);
		}
	}

	if (cl_options.no_projects)
		socket_fd_write_cstring(sock, "no-projects");

	/* use "openro" to denote readonly status for new docs */
	socket_fd_write_cstring(sock, cl_options.readonly ?
			(cl_options.favorite ? "openrofav" : "openro") :
			(cl_options.favorite ? "openfav" : "open"));

	for (i = 1; i < argc && argv[i] != NULL && ! erred; i++)
	{
		gchar *filename = main_get_argv_filename(argv[i]);

		/* if the filename is valid or if a new file should be opened is check on the other side */
		if (filename == NULL)
			g_printerr(_("Could not find file '%s'."), filename);
		else if (socket_fd_write_cstring(sock, filename) == -1)
		{
			g_printerr(_("Socket error occurred: %s"), strerror(errno));
			g_printerr(_("This may also indicate that opening of files has been cancelled."));
			erred = TRUE;
		}

		g_free(filename);
	}

	socket_fd_write_eot(sock);
}

#ifndef G_OS_WIN32
static void remove_socket_link_full(void)
{
	gchar real_path[512];
	gsize len;

	real_path[0] = '\0';

	/* read the contents of the symbolic link socket_info.file_name and delete it
	 * readlink should return something like "/tmp/geany_socket.499602d2" */
	len = readlink(socket_info.file_name, real_path, sizeof(real_path) - 1);
	if ((gint) len > 0)
	{
		real_path[len] = '\0';
		g_unlink(real_path);
	}
	g_unlink(socket_info.file_name);
}
#endif

static void socket_get_document_list(gint sock)
{
	gchar buf[BUFFER_LENGTH];
	gint n_read;

	if (sock < 0)
		return;

	socket_fd_write_cstring(sock, "doclist");

	while (socket_fd_get_cstring(sock, buf, BUFFER_LENGTH) != -1 && ! is_eot (buf))
		printf("%s\n", buf);
}

#ifndef G_OS_WIN32
static void check_socket_permissions(void)
{
	GStatBuf socket_stat;

	if (g_lstat(socket_info.file_name, &socket_stat) == 0)
	{	/* If the user id of the process is not the same as the owner of the socket
		 * file, then ignore this socket and start a new session. */
		if (socket_stat.st_uid != getuid())
		{
			const gchar *msg = _(
	/* TODO maybe this message needs a rewording */
	"Geany tried to access the Unix Domain socket of another instance running as another user.\n"
	"This is a fatal error and Geany will now quit.");
			g_warning("%s", msg);
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", msg);
			exit(1);
		}
	}
}
#endif

/* (Unix domain) socket support to replace the old FIFO code
 * (taken from Sylpheed, thanks)
 * Returns the created socket, -1 if an error occurred or -2 if another socket exists and files
 * were sent to it. */
gint socket_init(gint argc, gchar **argv)
{
	gint sock;
#ifdef G_OS_WIN32
	HANDLE hmutex;
	HWND hwnd;
	socket_init_win32();
	hmutex = CreateMutexA(NULL, FALSE, "Geany");
	if (! hmutex)
	{
		geany_debug("cannot create Mutex\n");
		return -1;
	}
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		/* To support multiple instances with different configuration directories (as we do on
		 * non-Windows systems) we would need to use different port number s but it might be
		 * difficult to get a port number which is unique for a configuration directory (path)
		 * and which is unused. This port number has to be guessed by the first and new instance
		 * and the only data is the configuration directory path.
		 * For now we use one port number, that is we support only one instance at all. */
		sock = socket_fd_open_inet(REMOTE_CMD_PORT);
		if (sock < 0)
			return 0;
		return sock;
	}

	sock = socket_fd_connect_inet(REMOTE_CMD_PORT);
	if (sock < 0)
		return -1;
#else
	gchar *display_name = NULL;
	const gchar *hostname = g_get_host_name();
	GdkDisplay *display = gdk_display_get_default();
	gchar *p;

	if (display != NULL)
		display_name = g_strdup(gdk_display_get_name(display));
	if (display_name == NULL)
		display_name = g_strdup("NODISPLAY");

	/* these lines are taken from dcopc.c in kdelibs */
	if ((p = strrchr(display_name, '.')) > strrchr(display_name, ':') && p != NULL)
		*p = '\0';
	/* remove characters that may not be acceptable in a filename */
	for (p = display_name; *p; p++)
	{
		if (*p == ':' || *p == '/')
			*p = '_';
	}

	if (socket_info.file_name == NULL)
		socket_info.file_name = g_strdup_printf("%s%cgeany_socket_%s_%s",
			app->configdir, G_DIR_SEPARATOR, hostname, display_name);

	g_free(display_name);

	/* check whether the real user id is the same as this of the socket file */
	check_socket_permissions();

	sock = socket_fd_connect_unix(socket_info.file_name);
	if (sock < 0)
	{
		remove_socket_link_full(); /* deletes the socket file and the symlink */
		return socket_fd_open_unix(socket_info.file_name);
	}
#endif

	/* remote command mode, here we have another running instance and want to use it */

#ifdef G_OS_WIN32
	/* first we send a request to retrieve the window handle and focus the window */
	socket_fd_write_cstring(sock, "window");

	if (socket_fd_read(sock, (gchar *)&hwnd, sizeof(hwnd)) == sizeof(hwnd))
		SetForegroundWindow(hwnd);
#endif
	/* now we send the command line args */
	if (argc > 1 || cl_options.new_instance_mode == NEW_INSTANCE_EXPLICITLY_DISABLED)
	{
		send_open_command(sock, argc, argv);
	}

	if (cl_options.list_documents)
	{
		socket_get_document_list(sock);
	}

	socket_fd_close(sock);
	return -2;
}

gint socket_finalize(void)
{
	if (socket_info.lock_socket < 0)
		return -1;

	if (socket_info.lock_socket_tag > 0)
		g_source_remove(socket_info.lock_socket_tag);
	if (socket_info.read_ioc)
	{
		g_io_channel_shutdown(socket_info.read_ioc, FALSE, NULL);
		g_io_channel_unref(socket_info.read_ioc);
		socket_info.read_ioc = NULL;
	}

#ifdef G_OS_WIN32
	WSACleanup();
#else
	if (socket_info.file_name != NULL)
	{
		remove_socket_link_full(); /* deletes the socket file and the symlink */
		g_free(socket_info.file_name);
	}
#endif

	return 0;
}

#ifdef G_OS_UNIX
static gint socket_fd_connect_unix(const gchar *path)
{
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("fd_connect_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}

static gint socket_fd_open_unix(const gchar *path)
{
	gint sock;
	struct sockaddr_un addr;
	gint val;
	gchar *real_path;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	if (sock < 0)
	{
		perror("sock_open_unix(): socket");
		return -1;
	}

	val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
	{
		perror("setsockopt");
		socket_fd_close(sock);
		return -1;
	}

	/* fix for #1888561:
	 * in case the configuration directory is located on a network file system or any other
	 * file system which doesn't support sockets, we just link the socket there and create the
	 * real socket in the system's tmp directory assuming it supports sockets */
	real_path = g_strdup_printf("%s%cgeany_socket.%08x",
		g_get_tmp_dir(), G_DIR_SEPARATOR, g_random_int());

	if (utils_is_file_writable(real_path) != 0)
	{	/* if real_path is not writable for us, fall back to ~/.config/geany/geany_socket_*_* */
		/* instead of creating a symlink and print a warning */
		g_warning("Socket %s could not be written, using %s as fallback.", real_path, path);
		SETPTR(real_path, g_strdup(path));
	}
	/* create a symlink in e.g. ~/.config/geany/geany_socket_hostname__0 to /tmp/geany_socket.499602d2 */
	else if (symlink(real_path, path) != 0)
	{
		perror("symlink");
		socket_fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, real_path, sizeof(addr.sun_path) - 1);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		socket_fd_close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0)
	{
		perror("listen");
		socket_fd_close(sock);
		return -1;
	}

	g_chmod(real_path, 0600);

	g_free(real_path);

	return sock;
}
#endif

static gint socket_fd_close(gint fd)
{
#ifdef G_OS_WIN32
	return closesocket(fd);
#else
	return close(fd);
#endif
}

#ifdef G_OS_WIN32
static gint socket_fd_open_inet(gushort port)
{
	SOCKET sock;
	struct sockaddr_in addr;
	gchar val;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (G_UNLIKELY(! SOCKET_IS_VALID(sock)))
	{
		geany_debug("fd_open_inet(): socket() failed: %d\n", WSAGetLastError());
		return -1;
	}

	val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
	{
		perror("setsockopt");
		socket_fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		socket_fd_close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0)
	{
		perror("listen");
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}

static gint socket_fd_connect_inet(gushort port)
{
	SOCKET sock;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (G_UNLIKELY(! SOCKET_IS_VALID(sock)))
	{
		geany_debug("fd_connect_inet(): socket() failed: %d\n", WSAGetLastError());
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}

static void socket_init_win32(void)
{
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != NO_ERROR)
		geany_debug("WSAStartup() failed\n");

	return;
}
#endif

static void popup(GtkWidget *window)
{
#ifdef GDK_WINDOWING_X11
	GdkWindow *x11_window = gtk_widget_get_window(window);

	/* Set the proper interaction time on the window. This seems necessary to make
	 * gtk_window_present() really bring the main window into the foreground on some
	 * window managers like Gnome's metacity.
	 * Code taken from Gedit. */
#	if GTK_CHECK_VERSION(3, 0, 0)
	if (GDK_IS_X11_WINDOW(x11_window))
#	endif
	{
		gdk_x11_window_set_user_time(x11_window, gdk_x11_get_server_time(x11_window));
	}
#endif
	gtk_window_present(GTK_WINDOW(window));
#ifdef G_OS_WIN32
	gdk_window_show(gtk_widget_get_window(window));
#endif
}

static gboolean handle_input_filenames(gint sock, GtkWidget *window)
{
	GtkWidget *status_window, *label;
	gchar *utf8_filename, *locale_filename, *message;
	gchar buf[BUFFER_LENGTH];
	GError *error;

	main_status.handling_input_filenames = TRUE;

	if (socket_fd_get_cstring(sock, buf, sizeof(buf)) != -1 && ! is_eot (buf))
	{
		popup(window);
		dialogs_create_cancellable_status_window(&status_window, &label, "Opening Files",
				"Opening files...", TRUE);

		for (int i = 0; i < 4 && gtk_events_pending(); ++i)
			gtk_main_iteration();

		do
		{
			for (int i = 0; i < 4 && gtk_events_pending(); ++i)
				gtk_main_iteration();

			if (status_window == NULL)
			{
				dialogs_show_msgbox(GTK_MESSAGE_WARNING, "Cancelled.");
				main_status.handling_input_filenames = FALSE;
				return FALSE;
			}

			/* we never know how the input is encoded, so do the best auto detection we can */
			if (! g_utf8_validate(buf, -1, NULL))
				utf8_filename = encodings_convert_to_utf8(buf, -1, NULL);
			else
				utf8_filename = g_strdup(buf);

			locale_filename = utils_get_locale_from_utf8(utf8_filename);

			if (locale_filename)
			{
				ui_label_set_text(GTK_LABEL(label), "Opening '%s'...", utf8_filename);

				for (int i = 0; i < 4 && gtk_events_pending(); ++i)
					gtk_main_iteration();

				if (! cl_options.no_projects && g_str_has_suffix(locale_filename, ".geany"))
				{
					if (project_ask_close())
						main_load_project_from_command_line(locale_filename, TRUE);
				}
				else if (! main_handle_filename(locale_filename, &error))
				{
					ui_set_statusbar(TRUE, "%s", error->message);
					dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", error->message);
					g_error_free(error);
				}
			}

			g_free(utf8_filename);
			g_free(locale_filename);
		}
		while (socket_fd_get_cstring(sock, buf, sizeof(buf)) != -1 && ! is_eot (buf));

		gtk_widget_destroy(status_window);
	}

	main_status.handling_input_filenames = FALSE;
	return TRUE;
}

gboolean socket_lock_input_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GtkWidget *window = data;

	gint fd, sock;
	gchar buf[BUFFER_LENGTH];
	struct sockaddr_in caddr;
	socklen_t caddr_len = sizeof(caddr);
	gboolean readonly, favorite;
	gboolean no_projects = FALSE;

	fd = g_io_channel_unix_get_fd(source);
	sock = accept(fd, (struct sockaddr *)&caddr, &caddr_len);

	/* first get the command */
	while (socket_fd_get_cstring(sock, buf, sizeof(buf)) != -1)
	{
		if ((readonly = favorite = strncmp(buf, "openrofav", 10) == 0) ||
				(readonly = strncmp(buf, "openro", 7) == 0) ||
				(favorite = strncmp(buf, "openfav", 8) == 0) ||
				strncmp(buf, "open", 5) == 0)
		{
			cl_options.readonly = readonly;
			cl_options.favorite = favorite;
			cl_options.no_projects = no_projects;

			if (! handle_input_filenames(sock, window))
				break;
		}
		else if (strncmp(buf, "no-projects", 12) == 0)
			no_projects = TRUE;
		else if (strncmp(buf, "doclist", 8) == 0)
		{
			const gchar *filename;
			guint i;

			foreach_document(i)
			{
				filename = DOC_FILENAME(documents[i]);

				if (filename != NULL)
					socket_fd_write_cstring(sock, filename);
			}

			socket_fd_write_eot(sock);
		}
		else if (strncmp(buf, "line", 5) == 0)
		{
			while (socket_fd_get_cstring(sock, buf, sizeof(buf)) != -1 && ! is_eot (buf))
				cl_options.goto_line = atoi(buf);
		}
		else if (strncmp(buf, "column", 7) == 0)
		{
			while (socket_fd_get_cstring(sock, buf, sizeof(buf)) != -1 && ! is_eot (buf))
				cl_options.goto_column = atoi(buf);
		}
#ifdef G_OS_WIN32
		else if (strncmp(buf, "window", 7) == 0)
		{
#	if GTK_CHECK_VERSION(3, 0, 0)
			HWND hwnd = (HWND) gdk_win32_window_get_handle(gtk_widget_get_window(window));
#	else
			HWND hwnd = (HWND) gdk_win32_drawable_get_handle(
				GDK_DRAWABLE(gtk_widget_get_window(window)));
#	endif
			socket_fd_write(sock, (gchar *)&hwnd, sizeof(hwnd));
		}
#endif
	}

	socket_fd_close(sock);
	return TRUE;
}

static gint socket_fd_get_cstring(gint fd, gchar *buf, gint len)
{
	gchar *nulbyte, *bp = buf;
	gint n, p;

	g_return_val_if_fail(len > 0, -1);

	do
	{
		if ((n = socket_fd_recv(fd, bp, len, MSG_PEEK)) <= 0)
			return -1;

		if ((nulbyte = memchr(bp, '\0', n)) != NULL)
			n = nulbyte - bp + 1;

		if ((p = socket_fd_read(fd, bp, n)) < 0)
			return -1;

		g_return_val_if_fail(p == n, -1);
		bp += n;
		len -= n;
	}
	while (nulbyte == NULL && len > 0);

	g_return_val_if_fail(len >= 0, -1);
	g_return_val_if_fail(bp > buf, -1);
	g_return_val_if_fail(*(bp - 1) == '\0', -1);
	return bp - buf;
}

static gint socket_fd_recv(gint fd, gchar *buf, gint len, gint flags)
{
	if (socket_fd_check_io(fd, G_IO_IN) < 0)
		return -1;

	return recv(fd, buf, len, flags);
}

static gint socket_fd_read(gint fd, gchar *buf, gint len)
{
	if (socket_fd_check_io(fd, G_IO_IN) < 0)
		return -1;

#ifdef G_OS_WIN32
	return recv(fd, buf, len, 0);
#else
	return read(fd, buf, len);
#endif
}

static gint socket_fd_check_io(gint fd, GIOCondition cond)
{
	struct timeval timeout;
	fd_set fds;
#ifdef G_OS_UNIX
	gint flags;
#endif

	timeout.tv_sec  = 60;
	timeout.tv_usec = 0;

#ifdef G_OS_UNIX
	/* checking for non-blocking mode */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		perror("fcntl");
		return 0;
	}
	if ((flags & O_NONBLOCK) != 0)
		return 0;
#endif

	FD_ZERO(&fds);
#ifdef G_OS_WIN32
	FD_SET((SOCKET)fd, &fds);
#else
	FD_SET(fd, &fds);
#endif

	if (cond == G_IO_IN)
	{
		select(fd + 1, &fds, NULL, NULL, &timeout);
	}
	else
	{
		select(fd + 1, NULL, &fds, NULL, &timeout);
	}

	if (FD_ISSET(fd, &fds))
	{
		return 0;
	}
	else
	{
		geany_debug("Socket IO timeout");
		return -1;
	}
}

static gint socket_fd_write_cstring(gint fd, const gchar *buf)
{
	return socket_fd_write_all(fd, buf, strlen(buf) + 1);
}

static gint socket_fd_write_all(gint fd, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len)
	{
		n = socket_fd_write(fd, buf, len);
		if (n <= 0)
			return -1;
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}

gint socket_fd_write(gint fd, const gchar *buf, gint len)
{
	if (socket_fd_check_io(fd, G_IO_OUT) < 0)
		return -1;

#ifdef G_OS_WIN32
	return send(fd, buf, len, 0);
#else
	return write(fd, buf, len);
#endif
}

#endif

#define EOT "\4"

gint socket_fd_write_eot(gint fd)
{
	return socket_fd_write_all(fd, EOT, 2);
}

static gboolean is_eot(const gchar *str)
{
	return strncmp(str, EOT, 2) == 0;
}
