#include "../src/webs.c"

int myFunc0(webs_client* self) {
	printf("(%ld) connected!\n", self->id);
	webs_send(self, "gotya8");
	webs_send(self, "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
	
	return 0;
}

int myFunc1(webs_client* self, char* data, ssize_t len) {
	size_t i;
	
	printf("(%ld) data [ %s ] (%lu bytes)\n", self->id, data, len);
	/*
	printf("raw: [ ");
	for (i = 0; i < len; i++)
		if ( data[i] >= ' ' && data[i] <= '~') printf("%c ", data[i]);
		else printf("%X ", data[i]);
	printf("]\n");
	*/
	if (data[0] == 'E') webs_eject(self);
	if (data[0] == 'C') webs_close(self->srv);
	
	return 0;
}

int myFunc2(webs_client* self) {
	printf("(%ld) disconnected!\n", self->id);
	
	return 0;
}

int myFunc3(webs_client* self, enum webs_error err) {
	
	return 0;
}

int main(void) {
	webs_server* server = webs_start(7752);
	if (!server) printf("err\n");
	
	server->events.on_open = myFunc0;
	server->events.on_data = myFunc1;
	server->events.on_close = myFunc2;
	server->events.on_error = myFunc3;
	
	webs_hold(server);
	
	webs_close(server);
	
	return 0;
}