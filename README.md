# webs
a simple websocket server library.

## Compilation

```
$ cc -o my_server webs.c my_server.c -lpthread
$ ./my_server
```

## Events

| Event      | Description |
|:-----------|:------------|
| `on_open`  | called when a client connects to a server |
| `on_close` | called when a client disconnects from a server |
| `on_data`  | called when data is recieved from a client |
| `on_error` | called when an error occurs |
| `on_pong`  | called when a server recieves a pong |
| `on_ping`  | called when a client pings a server |

## Handlers

### `on_open`, `on_close`, `on_ping`, `on_pong`

###### Format
`int my_func(webs_client* self);`
  
| Parameter  | Description |
|------------|-------------|
|`self`      | client that triggered the event |

### `on_data`

###### Format
`int my_func(webs_client* self, char* data, size_t length);`
  
| Parameter  | Description |
|------------|-------------|
|`self`      | client that triggered the event |
|`data`      | pointer to recieved data |
|`length`    | number of bytes recieved |

**NOTE**: data is freed when the function returns, doing so yourself is ill-advised.

### `on_error`

###### Format
`int my_func(webs_client* self, enum webs_error code);`
  
| Parameter  | Description |
|------------|-------------|
|`self`      | client that triggered the event |
|`code`      | error code |

Possible value for `code` are,

```
WEBS_ERR_NONE,                     /* no error */
WEBS_ERR_READ_FAILED,              /* failed to read data */
WEBS_ERR_UNEXPECTED_CONTINUTATION, /* recieved frame marked as continuation with
				    *   no apparent start frame recieved */
WEBS_ERR_NO_SUPPORT,               /* frame uses reserved opcode, no support */
WEBS_ERR_OVERFLOW                  /* frame attempted to contain more than SSIZE_MAX
                                    *   bytes of data */
```

## Sending Data

### Strings

###### Format
`webs_send(self, string)`
  
| Parameter  | Description |
|------------|-------------|
|`self`      | client to send data to |
|`string`    | null-terminated string |

### Other

###### Format
`webs_sendn(self, data, length)`
  
| Parameter  | Description |
|------------|-------------|
|`self`      | client to send data to |
|`data`      | pointer to data that is to be sent |
|`length`    | number of bytes to be sent |

## Shutting Down

### Disconnecting a Client

###### Format
`webs_eject(self)`
  
| Parameter  | Description |
|------------|-------------|
|`self`      | client to be ejected |

### Stopping a Server

###### Format
`webs_close(server)`
  
| Parameter  | Description |
|------------|-------------|
|`server`    | server that is to be closed |

### Blocking Until a Server Closes

###### Format
`webs_hold(<server>)`
  
| Parameter  | Description |
|------------|-------------|
|`server`    | server that is to be waited for |
