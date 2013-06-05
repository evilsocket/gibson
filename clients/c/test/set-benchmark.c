#include <gibson.h>

unsigned long millis() {
	struct timeval time;
	struct tm *tm;

	gettimeofday(&time, NULL );
	tm = localtime(&time.tv_sec);

	return (unsigned long) (tm->tm_sec * 1000 + time.tv_usec / 1000);
}

int main(int argc, char **argv) {
	gbClient client;
	int i = 0, ok = 0, ko = 0, verified = 0;
	unsigned long start = 0, end = 0, run = 10000;
	char key[0xFF] = {0},
		 val[0xFF] = {0};

	// printf("tcp connect  : %d\n", gb_tcp_connect(&client, NULL, 0, 1000));
	printf("unix connect : %d\n", gb_unix_connect(&client, "../../gibson/gibson.sock", 100 ) );

	start = millis();

	for (i = 0; i < run; i++) {

		sprintf( key, "key%d", i );
		sprintf( val, "val%d", i );

		if ( gb_set( &client, key, strlen(key), val, strlen(val), -1 ) == 0 )
			++ok;

		else
			++ko;
	}

	end = millis();

	printf("created %d / %d in %dms\n", ok, run, (unsigned) (end - start));

	start = millis();

	for (i = 0; i < run; i++) {

		sprintf( key, "key%d", i );
		sprintf( val, "val%d", i );

		if( gb_get( &client, key, strlen(key) ) != 0 )
			printf( "Can't verify key '%s' : %d\n", key, client.error );

		else if( strncmp( val, client.reply.buffer, client.reply.size ) != 0 )
			printf( "%s : reply:%s expected:%s\n", key, client.reply.buffer, val );

		else
			++verified;
	}

	end = millis();

	printf( "verified : %d / %d in %dms\n", verified, run, (unsigned) (end - start) );

	gb_disconnect(&client);

	return 0;
}
