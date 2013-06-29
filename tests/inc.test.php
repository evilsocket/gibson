<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "1" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->inc( "foo" )           == FALSE, "Unexpected INC reply" );
fail_if( $g->get( "foo" ) !== 2, "Value should be 2" );

$g->quit();

?>