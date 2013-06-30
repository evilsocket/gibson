<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->ttl( "foo", 1 )     == FALSE, "Unexpected TTL reply" );

sleep(1);

fail_if( $g->get( "foo" ) !== FALSE, "Value should be expired" );

$g->quit();

?>