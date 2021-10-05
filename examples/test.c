#include "../src/webs.c"

int myFunc0(webs_client* self) {
	printf("(%d) connected!\n", self->id);
	webs_send(self, "gotya8");
	
	return 0;
}

int myFunc1(webs_client* self, char* data, size_t len) {
	printf("(%d) data [ %s ] (%i bytes)\n", self->id, data, len);
	printf("raw: [ ");
	for (int i = 0; i < len; i++)
		switch(data[i]) {
			case ' ' ... '~': printf("%c ", data[i]); break;
			default: printf("%X ", data[i]);
		}
	printf(" ]\n");
	return 0;
}

int myFunc2(webs_client* self) {
	printf("(%d) disconnected!\n", self->id);
	
	return 0;
}

int main(void) {
	webs_start(7752);
	
	WEBS_EVENTS.on_open = myFunc0;
	WEBS_EVENTS.on_data = myFunc1;
	WEBS_EVENTS.on_close = myFunc2;
	
	for(;;);
	
	return 0;
}


