<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->lock( "foo", 1 )          == FALSE, "Unexpected LOCK reply" );

fail_if( $g->set( "foo", "new" ) != FALSE, "Value should be locked" );

sleep(1);

fail_if( $g->set( "foo", "new" ) == FALSE, "Value should be unlocked" );

$g->quit();

?>