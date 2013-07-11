<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "aaa",  "bar" )      == FALSE, "Unexpected SET reply" );

$size = $g->sizeof( "aaa" );

fail_if( $size != 3, "SIZEOF should be 3 ( $size )" );

?>
