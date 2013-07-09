<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "aaa",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->sizeof( "aaa" ) != 3, "SIZEOF should be 3" );

?>
