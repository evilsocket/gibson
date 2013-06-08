#include <gibson.h>

int main(int argc, char **argv) {
	gbClient client;
	gbMultiBuffer mb;

	int i = 0;

	printf("unix connect : %d\n", gb_unix_connect(&client, "../../../gibson.sock", 100 ) );

	gb_set( &client, "numa", 4, "a", 1, -1 );
	gb_set( &client, "numb", 4, "b", 1, -1 );
	gb_set( &client, "numc", 4, "c", 1, -1 );
	gb_set( &client, "numd", 4, "d", 1, -1 );

	gb_mget( &client, "num", 3 );

	gb_reply_multi( &client, &mb );
	for( i = 0; i < mb.count; i++ ){
		printf( "%s : %s\n", mb.keys[i], mb.values[i].buffer );
	}

	gb_reply_multi_free(&mb);

	gb_disconnect(&client);

	return 0;
}
