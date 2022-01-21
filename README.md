# webs
C89 compliant websocket server library.

## Usage

### Starting a Server
```
#include ".../path/to/webs.h"

/* code... */

int main(void) {
	const int port0 = 7752;
	const int port1 = 7754;
	
	/* make two servers */
	webs_server* my_server0 = webs_start(port0);
	webs_server* my_server1 = webs_start(port1);
	
	/* check if servers were successfully created */
	if (!server0 || !server1) {
		printf("error: failed to initialise servers.\n");
		return 1;
	}
	
	/* add event handlers - not necessary, defaults to NULL */
	server0->events.on_open = myFuncOpen0;
	server0->events.on_data = myFuncDate0;
	server0->events.on_close = myFuncClose0;
	server0->events.on_error = myFuncError0;
	server0->events.on_pong = myFuncPong0;
	
	server1->events = {myFuncOpen1, myFuncDate1, myFuncClose1, myFuncError1, NULL, NULL};
	
	/* wait for servers to return from main */
	webs_hold(server0);
	webs_hold(server1);
	
	/* close servers */
	webs_close(server0);
	webs_close(server1);
	
	return 0;
}
```
### Compilation

```
$ cc -c webs.c my_server.c
$ cc -o my_server webs.o my_server.o -lpthread
$ ./my_server
```

### Events
| Event | Description |
|:------|:------------|
| `on_open` | called when a client connects to the server |
| `on_close` | called when a client disconnects from the server |
| `on_data` | called when data is recieved from a client |
| `on_error` | called when an error occurs |
| `on_pong` | called when the server recieves a pong |
| `on_ping` | called when a client pings the server |

### Formatting
when implementing you own functions, each event's function must be structured a certain way, as detailed below, but firstly, I'll include the definitions for `struct webs_client` and `struct webs_server`, in case they are useful.

```
struct webs_client {
	struct webs_server* srv; /* a pointer to the server that the
	                          * client is connected to */
	struct sockaddr_in addr; /* client address */
	pthread_t thread;        /* client's posix thread id */
	size_t id;               /* client's internal id */
	int fd;                  /* client's descriptor */
};

struct webs_server {
	struct webs_event_list events;
	struct webs_client_node* head;
	struct webs_client_node* tail;
	size_t num_clients;
	pthread_t thread;
	size_t id;
	int soc;
};
```

#### `on_open` / `on_close` / `on_ping` / `on_pong`
syntax: `int <my_func_name>(webs_client* <self>);`  
description: `<self>` is a pointer to client data (of type `struct webs_client`).

#### `on_data`
syntax: `int <my_func_name>(webs_client* <self>, char* <data>, size_t <length>);`  
description: `<self>` is as before, `<data>` is a pointer to the recieved data, and `<length>` is the length of this data.  
  
**NOTE**: data is freed when the function returns, doing so yourself may result in a double free.

#### `on_error`
syntax: `int <my_func_name>(webs_client* <self>, enum webs_error <code>);`  
description: `<self>` is as before, and `<code>` is an error code, as described below.

```
enum webs_error {
	WEBS_ERR_NONE = 0,                 /* no error */
	WEBS_ERR_READ_FAILED,              /* failed to read data */
	WEBS_ERR_UNEXPECTED_CONTINUTATION, /* recieved frame marked as continuation with
	                                    * no apparent start frame recieved */
	WEBS_ERR_NO_SUPPORT,               /* frame uses reserved opcode, no support */
	WEBS_ERR_OVERFLOW                  /* frame attempted to contain more than SSIZE_MAX
	                                    * bytes of data */
};
```

### Sending Data
To send data, use either (1) `webs_send(<self>, <string>)`, where `<self>` is as before (forwarded to the send function), and `<string>` is a null-terminating string of data to be sent, or (2) `webs_sendn(<self>, <data_ptr>, <length>)`, where `<self>` is as before, `<data>` is a pointer to data that is to be sent, and `<length>` is how many bytes of data there are.

### Shutting Down
To disconnect a single client use `webs_eject(<self>)`, where `<self>` is the `struct webs_client` pointer of the client. Similarly, to shutdown an entire server use `webs_close(<server>)`, and to block until a server returns from main use `webs_hold(<server>)`.
