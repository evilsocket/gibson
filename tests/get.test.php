<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar", 5 ) == FALSE, "Unexpected SET reply" );
fail_if( $g->get( "foo" ) !== "bar", "Unexpected GET reply" );

?>
