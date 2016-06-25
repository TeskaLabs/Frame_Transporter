#ifndef __LIBSCCMN_SOCK_EST_H__
#define __LIBSCCMN_SOCK_EST_H__

struct established_socket;

struct established_socket_cb
{
	struct frame * (*get_read_frame)(struct established_socket *);

	// True as a return value means, that the frame has been handed over to upstream protocol
	bool (*read)(struct established_socket *, struct frame * frame);

	void (*state_changed)(struct established_socket *);

	void (*close)(struct established_socket *);
};

struct established_socket
{
	// Common fields
	struct context * context;

	struct
	{
		unsigned int read_connected : 1;  // Socket is read-wise connected
		unsigned int write_connected : 1; // Socket is write-wise connected
		unsigned int write_open : 1;      // Write queue is open for adding new frames
		unsigned int write_ready : 1;     // We can write to the socket (no need to wait for EV_WRITE)
	} flags;

	int ai_family;
	int ai_socktype;
	int ai_protocol;

	struct sockaddr_storage ai_addr;
	socklen_t ai_addrlen;

	struct established_socket_cb * cbs;

	ev_tstamp established_at;
	ev_tstamp read_shutdown_at;

	// Input
	struct ev_io read_watcher;
	struct frame * read_frame;
	size_t read_opportunistic; // When yes, read() callback is triggered for any incoming data
	int read_syserror;

	// Output
	struct ev_io write_watcher;
	struct frame * write_frames; // Queue of write frames 
	struct frame ** write_frame_last;

	// Statistics
	struct
	{
		unsigned int read_events;
		unsigned int write_events;
		unsigned int direct_write_events; //Writes without need of wait for EV_WRITE
		unsigned long read_bytes;
		unsigned long write_bytes;
	} stats;

	// Custom data field
	void * data;
};

bool established_socket_init_accept(struct established_socket *, struct established_socket_cb * cbs, struct listening_socket * listening_socket, int fd, const struct sockaddr * peer_addr, socklen_t peer_addr_len);
void established_socket_fini(struct established_socket *);

bool established_socket_read_start(struct established_socket *);
bool established_socket_read_stop(struct established_socket *);

bool established_socket_write_start(struct established_socket *);
bool established_socket_write_stop(struct established_socket *);

bool established_socket_write(struct established_socket *, struct frame * frame);

bool established_socket_shutdown(struct established_socket *);


#endif // __LIBSCCMN_SOCK_EST_H__
