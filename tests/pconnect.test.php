<?php 

require_once 'testlib.php';

$g = new Gibson();

for( $i = 0; $i < 30; $i++ ){
    fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
}

$g->quit();

fail_if( $g->set( "foo", "bar" ) !== FALSE, "Connection should be closed." );
fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar" ) == FALSE, "Connection should be available." );

$g->quit();

?>
