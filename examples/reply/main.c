#include <stdlib.h>
#include <stdbool.h>

#include <libsccmn.h>

///

struct exiting_watcher watcher;

///

struct ft_list established_socks; // List of established_socket(s)

bool on_read(struct established_socket * established_sock, struct frame * frame)
{
//	frame_print(frame);	
	frame_flip(frame);
	bool ok = established_socket_write(established_sock, frame);
	if (!ok)
	{
		L_ERROR("Cannot write a frame!");
		frame_pool_return(frame);
	}

	return true;
}

struct established_socket_cb sock_est_sock_cb = 
{
	.get_read_frame = get_read_frame_simple,
	.read = on_read,
	.state_changed = NULL,
	.error = NULL,
};

static void established_sock_stop_each(struct ft_list_node * node, void * data)
{
	established_socket_read_stop((struct established_socket *)node->payload);
	established_socket_write_stop((struct established_socket *)node->payload);
}

static void established_sock_node_fini_cb(struct ft_list_node * node)
{
	established_socket_fini((struct established_socket *)node->payload);
}

///

struct ft_list listen_socks; // List of listening_socket(s)

static bool on_accept_cb(struct listening_socket * listening_socket, int fd, const struct sockaddr * client_addr, socklen_t client_addr_len)
{
	bool ok;

	struct ft_list_node * new_node = ft_list_node_new(sizeof(struct established_socket));
	if (new_node == NULL) return false;

	struct established_socket * established_sock = (struct established_socket *)&new_node->payload;

	ok = established_socket_init_accept(established_sock, &sock_est_sock_cb, listening_socket, fd, client_addr, client_addr_len);
	if (!ok)
	{
		ft_list_node_del(new_node, &established_socks);
		return false;
	}

	// Start read on the socket
	established_socket_set_read_partial(established_sock, true);
	established_socket_read_start(established_sock);

	ft_list_add(&established_socks, new_node);

	return true;
}

struct listening_socket_cb sock_listen_sock_cb =
{
	.accept = on_accept_cb,
};

static bool on_resolve_listen_cb(void * data, struct context * context, struct addrinfo * addrinfo)
{
	struct ft_list_node * new_node = ft_list_node_new(sizeof(struct listening_socket));
	if (new_node == NULL) return false;

	bool ok = listening_socket_init((struct listening_socket *)new_node->payload, &sock_listen_sock_cb, context, addrinfo);
	if (!ok) 
	{
		ft_list_node_del(new_node, &listen_socks);
		return false;
	}

	ft_list_add(&listen_socks, new_node);

	return true;
}

static void listen_sock_node_fini_cb(struct ft_list_node * node)
{
	listening_socket_fini((struct listening_socket *)node->payload);
}

static void listen_sock_start_each(struct ft_list_node * node, void * data)
{
	listening_socket_start((struct listening_socket *)node->payload);
}

static void listen_sock_stop_each(struct ft_list_node * node, void * data)
{
	listening_socket_stop((struct listening_socket *)node->payload);
}

///

static void on_exiting_cb(struct exiting_watcher * watcher, struct context * context)
{
	// Stop established sockets
	ft_list_each(&established_socks, established_sock_stop_each, NULL);

	// Stop listening sockets
	ft_list_each(&listen_socks, listen_sock_stop_each, NULL);
}

static void on_check_cb(struct ev_loop * loop, ev_prepare * check, int revents)
{
	// Check all established socket and remove closed ones
restart:
	for (struct ft_list_node * node = established_socks.head; node != NULL; node = node->next)
	{
		if (established_socket_is_shutdown((struct established_socket *)node->payload))
		{
			ft_list_remove(&established_socks, node);
			goto restart;
		}
	}
}

int main(int argc, char const *argv[])
{
	bool ok;
	int rc;
	struct context context;

	logging_set_verbose(true);
	libsccmn_init();
	
	// Initializa context
	ok = context_init(&context);
	if (!ok) return EXIT_FAILURE;

	// Initialize a list for listening sockets
	ok = ft_list_init(&listen_socks, listen_sock_node_fini_cb);
	if (!ok) return EXIT_FAILURE;

	// Initialize a list for established sockets
	ok = ft_list_init(&established_socks, established_sock_node_fini_cb);
	if (!ok) return EXIT_FAILURE;

	// Prepare listening socket(s)
	rc = listening_socket_create_getaddrinfo(on_resolve_listen_cb, NULL, &context, AF_INET, SOCK_STREAM, "127.0.0.1", "12345");
	if (rc < 0) return EXIT_FAILURE;

	rc = listening_socket_create_getaddrinfo(on_resolve_listen_cb, NULL, &context, AF_UNIX, SOCK_STREAM, "/tmp/sctext.tmp", NULL);
	if (rc < 0) return EXIT_FAILURE;

	// Registed check handler
	ev_prepare prepare_w;
	ev_prepare_init(&prepare_w, on_check_cb);
	ev_prepare_start(context.ev_loop, &prepare_w);
	ev_unref(context.ev_loop);

	// Start listening
	ft_list_each(&listen_socks, listen_sock_start_each, NULL);

	// Register exiting watcher
	context_exiting_watcher_add(&context, &watcher, on_exiting_cb);

	// Enter event loop
	context_evloop_run(&context);

	// Remove established sockets
	ft_list_fini(&established_socks);

	// Remove listening sockets
	ft_list_fini(&listen_socks);

	// Finalize context
	context_fini(&context);

	return EXIT_SUCCESS;
}