<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar", 1 ) == FALSE, "Unexpected SET reply" );

sleep(2);

fail_if( $g->get( "foo" ) !== FALSE, "Value should be expired" );

$g->quit();

?>