<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "aaa",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "aab",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "aac",  "bar" )      == FALSE, "Unexpected SET reply" );

fail_if( $g->count( "a" ) != 3, "Count should be 3" );

?>
