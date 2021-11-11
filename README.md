# crd_webs
sub-optimal websocket server library for c

## Usage
**Note:** requires compilation with `-lpthread`  
  
To start include the mian file `webs.c` in you program, then to start listening on a port use `webs_start(<port>)`, of course with `<port>` being replaced with the port you wish to listen on.  
  
Then, add your own functions to any of the events listed  below by typing `WEBS_EVENTS.<event> = <my_func>`, again, with `<event>` being one of the listed events, and `<my_func>` being the name of your function.  

### Events
| Event | Description |
|:------|:------------|
| `on_open` | called when a client connects to the server |
| `on_close` | called when a client disconnects from the server |
| `on_data` | called when data is recieved from a client |
| `on_error` | called when an error occurs |
  
### Formatting
when implementing you own functions, each event's function must be structured a certain way, as detailed below, but firstly, I'll include the definition for `struct webs_client`, in case it is useful.

```
struct webs_client {
	struct sockaddr_in addr; // client address
	struct webs_buffer buf_send; // buffer, user can ignore this
	struct webs_buffer buf_recv; // ditto
	pthread_t thread; // client thread id
	long id; // client id
	int fd; // file descriptor for client socket
};
```
#### `on_open` / `on_close`
syntax: `int <my_func_name>(webs_client* <self>);`  
description: `<self>` is a pointer to client data (of type `struct webs_client`).

#### `on_data`
syntax: `int <my_func_name>(webs_client* <self>, char* <data>, size_t <length>);`  
description: `<self>` is as before, `<data>` is a pointer to the recieved data, and `<length>` is the length of this data.  
  
**NOTE**: data is freed when the function returns, doing so yourself may result in a double free.

#### `on_error`
syntax: `int <my_func_name>(webs_client* <self>, int <code>);`  
description: `<self>` is as before, and `<code>` is an error code.

### Sending Data
To send data, use either (1) `webs_send(<self>, <string>)`, where `<self>` is as before (forwarded to the send function), and `<string>` is a null-terminating string of data to be sent, or (2) `webs_sendn(<self>, <data_ptr>, <length>)`, where `<self>` is as before, `<data>` is a pointer to data that is to be sent, and `<length>` is how many bytes of data there are.

### Shutting Down
To disconect a single client use `webs_eject(<self>)`, where `<self>` is the `struct webs_client` pointer of the client. Similarly, to shutdown the entire server used `webs_close()`, and to block until the server closes use `webs_hold()` on the main thread.

### Global Information
Finally, here is the definition for `__webs_global`, which has some potentially pertinant information:

```
struct {
	struct webs_client_node* head; // head of linked list of connected clients
	struct webs_client_node* tail; // likewise, though tail
	size_t num_clients; // number of connected clients
	pthread_t thread; // thread listening for connections
	int soc; // server socket id
} __webs_global;
```
