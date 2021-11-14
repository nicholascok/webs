#include "../src/webs.c"

int myFunc0(webs_client* self) {
	printf("(%ld) connected!\n", self->id);
	webs_send(self, "gotya8");
	
	return 0;
}

int myFunc1(webs_client* self, char* data, size_t len) {
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
	if (data[0] == 'C') webs_close();
	
	return 0;
}

int myFunc2(webs_client* self) {
	printf("(%ld) disconnected!\n", self->id);
	
	return 0;
}

int myFunc3(webs_client* self, enum webs_err err) {
	webs_perror(err);
	
	return 0;
}

int main(void) {
	webs_start(7752);
	
	WEBS_EVENTS.on_open = myFunc0;
	WEBS_EVENTS.on_data = myFunc1;
	WEBS_EVENTS.on_close = myFunc2;
	WEBS_EVENTS.on_error = myFunc3;
	
	webs_hold();
	
	return 0;
}


