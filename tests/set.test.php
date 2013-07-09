<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar", 1 ) == FALSE, "Unexpected SET reply" );

sleep(1);

fail_if( $g->get( "foo" ) !== FALSE, "Value should be expired" );

?>
